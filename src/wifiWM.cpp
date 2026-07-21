/*
 * Conexion a la red wifi usando WifiManager
*/
#include "Control.h"

#define ledWifi  LEDG   
#define ledAP    LEDB

Ticker tic_WifiLed;
Ticker tic_APLed;

#ifdef DEVELOP
  int timeout = 30;  //config portal timeout para pruebas (segundos)
#else
  int timeout = 180;  //config portal timeout para produccion (segundos) 
#endif

// Personalizacion del html que se muestra en el portal AP (texto de los botones, placeholders, validaciones, etc.)

const char* custom_head_element = 
    "<style>input::placeholder { font-style: italic; opacity: 0.6; }</style>"
    "<script>"
    "document.addEventListener('DOMContentLoaded', function() {"
    "  var replaceText = function(selector, newText) {"
    "    var el = document.querySelector(selector);"
    "    if(el) el.innerHTML = newText;"
    "  };"
    "  replaceText('form[action=\"/wifi\"] button', 'Configure WiFi & Parms');"
    "  replaceText('form[action=\"/update\"] button', 'FW Update');"
    "  replaceText('form[action=\"/erase\"] button', 'Erase WIFI');"
    "});"
    "</script>";
    

// Pattern para validar IPs y puertos en los campos de configuración (usado en el portal AP de WiFiManager y en el webserver)
#define RX_OCTETO "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
#define RX_IP_BASE "^(" RX_OCTETO "\\." RX_OCTETO "\\." RX_OCTETO "\\." RX_OCTETO ")$|^([a-zA-Z0-9\\-]+\\.local)$"
// #define RX_PORT_BASE "^([1-9][0-9]{0,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$"

// Atributos HTML completos de esos campos, con validación y mensajes de error personalizados
const char* IP_ATTRS = "pattern='" RX_IP_BASE "' title='IP o host.local' required";
const char* PORT_ATTRS = "type='number' min='1' max='65535' placeholder='" DFLT_SCD_PORT "'";
// const char* PORT_ATTRS = "pattern='" RX_PORT_BASE "' title='Puerto (1-65535)' placeholder='" DFLT_SCD_PORT "'";

// Creamos una instancia de la clase WiFiManager

WiFiManager wm;

WiFiManagerParameter custom_SCD_server("SCD_ip", "Domoticz ip (requerida)", "", sizeof(config.SCD_ip)-1,IP_ATTRS); // custom input attrs (ip mask)
WiFiManagerParameter custom_SCD_port("SCD_port", "puerto", DFLT_SCD_PORT, sizeof(config.SCD_port)-1, PORT_ATTRS); // custom input attrs (port mask)
WiFiManagerParameter custom_SCD_user("SCD_user", "user", "", sizeof(config.SCD_user)-1);
WiFiManagerParameter custom_SCD_password("SCD_password", "password", "", sizeof(config.SCD_password)-1);
WiFiManagerParameter custom_ntpserver("ntpServer", "NTP server", "", sizeof(config.ntpServer)-1, "placeholder='" NTPSERVER_SPAIN "'");
WiFiManagerParameter custom_timezone("timeZone", "timeZone", "", sizeof(config.TZ)-1, "placeholder='" TZ_Europe_Madrid "'");

//mensaje de wifi reconectando
static const char* MSG_WIFI_CONN = "conectando WIFI";

//mensaje de wifi OK con SSID
const char* wifiOKmsg(bool compact = false) {
  static char buffer[70]; // Un poco más de margen por si el SSID es largo
  if (compact) snprintf(buffer, MAXBUFF, "wifi OK: %s", WiFi.SSID().c_str());
  else snprintf(buffer, sizeof(buffer), "<<<<     WiFi conectada a %s     >>>>", WiFi.SSID().c_str());
  return buffer;
}

// copia los parametros de conexion wifi a los parametros personalizados de WiFiManager (para mostrarlos en el portal AP)
void copyConfigToCustomParams() {
  custom_SCD_server.setValue(config.SCD_ip, sizeof(config.SCD_ip)-1);
  custom_SCD_port.setValue(config.SCD_port, sizeof(config.SCD_port)-1);
  custom_SCD_user.setValue(config.SCD_user, sizeof(config.SCD_user)-1);
  custom_SCD_password.setValue(config.SCD_password, sizeof(config.SCD_password)-1);
  custom_ntpserver.setValue(config.ntpServer, sizeof(config.ntpServer)-1);
  custom_timezone.setValue(config.TZ, sizeof(config.TZ)-1);
}

// copia los parametros personalizados de WiFiManager a config, usando defaults si alguno esta vacio
void copyCustomParamsToConfig() {
  const char* val;
  strlcpy(config.SCD_ip, custom_SCD_server.getValue(), sizeof(config.SCD_ip));
  val = custom_SCD_port.getValue();
  strlcpy(config.SCD_port, (val && val[0]) ? val : DFLT_SCD_PORT, sizeof(config.SCD_port));
  strlcpy(config.SCD_user, custom_SCD_user.getValue(), sizeof(config.SCD_user));
  strlcpy(config.SCD_password, custom_SCD_password.getValue(), sizeof(config.SCD_password));
  val = custom_ntpserver.getValue();
  strlcpy(config.ntpServer, (val && val[0]) ? val : NTPSERVER_SPAIN, sizeof(config.ntpServer));
  val = custom_timezone.getValue();
  strlcpy(config.TZ, (val && val[0]) ? val : TZ_Europe_Madrid, sizeof(config.TZ));
  if (config.SCD_ip[0] == '\0') LOG_WARN("IP de Domoticz no definida");
  LOG_DEBUG("Config actualizado: SCD_ip=", config.SCD_ip, ", SCD_port=", config.SCD_port, ", ntpServer=", config.ntpServer, ", timeZone=", config.TZ);
}

//llamado cuando WiFiManager sale del modo configuracion
void saveWifiCallback() {
    LOG_INFO("[CALLBACK] fired, should save config");
    saveConfigRequired = true;
    // Eliminamos el temporizador y apagamos el led indicador de modo AP
    setLed(tic_APLed, APAGA, ledAP);
    lcd.infoclear(MSG_WIFI_CONN);
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, ledWifi);
}

//llamado cuando WiFiManager entra en modo configuracion
void configModeCallback (WiFiManager *myWiFiManager) {
  LOG_INFO("[CALLBACK] fired");
  // apagamos el LED indicador de wifi
  setLed(tic_WifiLed, APAGA, ledWifi);
  // Empezamos el temporizador que hará parpadear el LED indicador de AP
  setParpadeo(tic_APLed, NORMAL, parpadeoLedPWM, ledAP);
  lcd.infoclear("   modo -AP- :", 1, LOWBIP, 1); //lo señalamos en display
  lcd.info("\"Ardomo\" activado", 3);
}

//llamado cuando WiFiManager recibe parametros adicionales (de pagina independiente de parametros)
// void saveParamCallback()
// {
//   LOG_INFO("[CALLBACK] fired");
//   LOG_INFO("Should save config");
//   saveConfigRequired = true;
//   wm.stopConfigPortal();
// }

//llamado antes de empezar carga del sketch via OTA
void preOtaUpdateCallback()
{
  LOG_INFO("[CALLBACK] fired");
  lcd.infoclear("OTA in progress", BLINKDISPLAY, LOWBIP, 1);
  #ifdef DISPLAYOTA
    // actualizamos el progreso en el display
    Update.onProgress([](unsigned int progress, unsigned int total) {
        static int lastPercent = -1;
        int percentage = (progress / (total / 100));
        if (percentage != lastPercent) {
            lastPercent = percentage;
            lcd.setCursor(16, 0);
            lcd.printf("%d%%", percentage);
            #ifdef DEVELOP
            Serial.printf("OTA Progress: %d%%\r", percentage);
            #endif
        }
    });
  #endif
}

//evento llamado en caso de desconexion de la wifi
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  if (Estado.connected) {
    LOG_ERROR("WiFi lost connection. Reason: ", info.wifi_sta_disconnected.reason);
    Estado.errorInformado = true; // bloquea futuros LOG_WARN/ERROR
    setConnected(false);
  }
  else LOG_DEBUG("WiFi lost connection. Reason: ", info.wifi_sta_disconnected.reason);
}

//evento llamado en caso de conexion de la wifi
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  logStatus(wifiOKmsg());
  Estado.errorInformado = false; //reiniciamos bloqueo futuros LOG_WARN/ERROR
  setConnected(true);
}

// conexion a la red por medio de WifiManager
void setupRedWM(S_initFlags &initFlags)
{
  #ifdef DEVELOP
    wm.setDebugOutput(true, WM_DEBUG_DEV);
    //wm.debugPlatformInfo();
  #endif  
  Estado.connected = false;
  Estado.recoverableError = false;
  saveConfigRequired = false;
  if(initFlags.initWifi) {
    wm.resetSettings(); //borra wifi guardada
    //delay(300);
    PRINTLN("[setupRedWM] encoderSW pulsado y multirriego en GRUPO3 --> borramos red WIFI");
    lcd.infoclear("red WIFI borrada", BLINKDISPLAY, LOWBIP, 1); //señala borrado wifi
  }
  // explicitly set mode, esp defaults to STA+AP   
  WiFi.mode(WIFI_STA);
  //esp_wifi_set_ps( WIFI_PS_NONE );  // Set current WiFi power save type (Default is WIFI_PS_MIN_MODEM)
  //WiFi.setTxPower(WIFI_POWER_19_5dBm); // ajusta la potencia de transmision wifi al maximo
  wm.setHostname(HOSTNAME); 
  wm.setConfigPortalTimeout(timeout); //sets timeout until configuration portal gets turned off
  wm.setAPClientCheck(true);  // avoid timeout if client connected to softap
  wm.setMinimumSignalQuality(25);  // set min RSSI (percentage) to show in scans, null = 8%
  // callbacks
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveWifiCallback);
  // wm.setSaveParamsCallback(saveParamCallback);
  wm.setPreOtaUpdateCallback(preOtaUpdateCallback);
  //if this is set, it will exit after config, even if connection is unsuccessful
  wm.setBreakAfterConfig(true);
  //muestra version en el titulo de la pagina web inicial
  wm.setTitle("Version: " + String(FW_VERSION));
  wm.setCustomHeadElement(custom_head_element); // custom html to add to head, can be a js script to change styles and text in the page
  // Orden de los ítems del menú principal (no compatible con setParamsPage)
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator)
  const char* menu[] = {"wifi","exit","sep","info","update","erase"}; // (if param is in menu, params will not show up in wifi page!)
  wm.setMenu(menu,6);
  wm.setShowInfoErase(false); // oculta el botón "erase" de la pagina de informacion (si se muestra en el menu)
  wm.setShowInfoUpdate(false); // oculta el botón "update" de la pagina de informacion (si se muestra en el menu)
  //parametros adicionales en la misma pagina que la wifi o en una pagina independiente
  // wm.setParamsPage(false); // muestra los parametros adicionales en la misma pagina que la wifi (default)
  // wm.setParamsPage(true); // muestra los parametros adicionales en una pagina independiente
  wm.addParameter(&custom_SCD_server);
  wm.addParameter(&custom_SCD_port);
  wm.addParameter(&custom_SCD_user);
  wm.addParameter(&custom_SCD_password);
  wm.addParameter(&custom_ntpserver);
  wm.addParameter(&custom_timezone);
  copyConfigToCustomParams();
  if(Estado.noWIFI) return;
  lcd.infoclear(MSG_WIFI_CONN);
  ledPWM(LEDR,OFF);   // Apagamos LEDR
  setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, ledWifi); // y empezamos el temporizador que hará parpadear el LED indicador de wifi
  // activamos conexion wifi y comprobamos si se establece
  if(!wm.autoConnect("Ardomo")) {
    PRINTLN("[setupRedWM] Fallo en la conexión (timeout)");
    Estado.recoverableError = true;
    delay(1000);
  }
  /* 
    * Podemos continuar hasta aqui por tres razones:
    *   - nos hemos conectado a la red wifi almacenada
    *   - nos hemos podido conectara a la red wifi que hemos introducido en la web de configuracion
    *   - no nos hemos podido conectar a la red wifi almacenada o no habia y el modo configuracion ha 
    *     dado timeout (Estado.recoverableError=true)
    */
  // detenemos parpadeo y apagamos led AP (caso de que se hubiera activado antes AP)
  setLed(tic_APLed, APAGA, ledAP);
  //si no hemos podido conectar y existe una red wifi salvada,reintentamos hasta 20 seg.
  // (para caso corte de corriente)
  if (Estado.recoverableError && wm.getWiFiIsSaved()) {
    lcd.infoclear(MSG_WIFI_CONN);
    PRINTLN("[setupRedWM] Hay wifi salvada -> reintentamos la conexion");
    int j=0;
    Estado.recoverableError = false;
    setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, ledWifi);
    WiFi.reconnect(); // reintentamos conexion a la wifi salvada (asincrono, no bloqueante)
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      delay(2000);
      j++;
      if(j == MAXCONNECTRETRY) {
        Estado.recoverableError = true;
        LOG_WARN("Fallo en la reconexión");
        break;
      }
    }
  }
  if (checkWifi()) {
    PRINTLN("\n[setupRedWM]  >>  Conectado a SSID: ", WiFi.SSID().c_str());
    PRINTLN(  "[setupRedWM]  >>      IP address: ", WiFi.localIP());
    PRINTLN(  "[setupRedWM]  >>      RSSI:", WiFi.RSSI(), "dBm  (",  wm.getRSSIasQuality(WiFi.RSSI()),"%)\n");
    lcd.info(wifiOKmsg(SHORT), 1);
  }
  else if(!Estado.modoDEMO) {
     statusError(E1, RECUPERABLE); //si no hemos podido conectar a la wifi señalamos error
     LOG_ERROR("SIN conexion wifi");
  }
  // dejamos led RGB segun la situacion final
  setLedStatus();
  // copia parametros del portal AP a la config wifi
  if (saveConfigRequired) copyCustomParamsToConfig();
  //dejamos activado evento de desconexion o conexion ?? (wifi events):
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  // WiFi.removeEvent(WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
} //fin setupRedWM

/**
 * @brief activa portal para configuracion red wifi y/o parámetros de conexion
 */
void startConfigPortal() 
{
  wm.setConfigPortalTimeout(timeout);
  if (!wm.startConfigPortal("Ardomo")) {
    LOG_INFO(" exit or hit timeout");
  }
  // copia parametros del portal AP a la config wifi
  if (saveConfigRequired) copyCustomParamsToConfig();
  // deja led RGB segun la situacion final
  setLedStatus();
  checkWifi();  // TODO ¿es necesario?
  delay(config.msgdisplaymillis);
}

//set del estado de la conexion wifi info en display y led indicador si procede
void setConnected(bool state) {
  LOG_DEBUG("setConnected: ", state, "Estado:", Estado.estado);
  Estado.connected = state;
  // Ajusta la UI (led status)
  setLedStatus();
  // Ajusta la UI (display) si no estamos en el Setup en caso de conexion
  if (state && !Estado.inSetup) {
      if (Estado.estado == STANDBY) {
          lcd.info(wifiOKmsg(SHORT),2);
          delay(config.msgdisplaymillis);
          setUI(STANDBY);  //restaura pantalla (borra msg de reconexion)
      }
      else lcd.infoclear(wifiOKmsg(SHORT), 1); // borra pantalla y muestra wifi OK en display primera linea
      if (!timeOK) setClock(); // sincronizamos reloj al conectar wifi (si no se ha sincronizado ya en el Setup)
    }        
}

// Verificacion estado de la conexion wifi
// Si level es true devuelve nivel de señal wifi, si es false solo verifica conexion wifi
// y devuelve true si hay conexion o false si no la hay
int checkWifi(bool level) {
  // Hay conexion wifi: si no la habia previamente, informamos recuperacion
  if(WiFi.status() == WL_CONNECTED) {
    if (!Estado.connected) {
      logStatus(wifiOKmsg());
      Estado.errorInformado = false; // reiniciamos bloqueo futuros LOG_WARN/ERROR
      setConnected(true);
    }
    return level==true ? wm.getRSSIasQuality(WiFi.RSSI()) : true; 
  }
  // No hay conexion wifi: informamos error si no se habia informado previamente
  else {
    if (Estado.connected) {
        if (!Estado.errorInformado) {
          LOG_ERROR(" ** [ERROR] No estamos conectados a la wifi");
          Estado.errorInformado = true; // bloquea futuros LOG_ERROR
        }
        setConnected(false);
    }      
    return false;
  }
}

bool wifiReconnect () {
    LOG_INFO("----  INTENTANDO RECONEXION WIFI  ----");
    setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, ledWifi);
    if (Estado.estado == STANDBY) lcd.info(MSG_WIFI_CONN,2);
    else {
      lcd.info(MSG_WIFI_CONN, 1); // muestra mensaje de reconexion
      lcd.info("",2); // y borra segunda linea
    }
    WiFi.disconnect();
    delay(3000);
    WiFi.begin();
    delay(3000);
    setLedStatus(); // elimina parpadeo led wifi
    return checkWifi();
}    

bool VerifyRecoveryWifi(bool checkRecon) {
  //en modoDEMO sin conexion no verificamos (DEMO sin wifi)
  if (Estado.modoDEMO && !Estado.connected) return true;
  lcd.displayON(); //por si estuviera parpadeando(apagado) por error en pantalla
  /*
    Si no estamos conectados a la wifi, intentamos reconexion cada RECONNECTINTERVAL minutos.
    El evento WiFiStationConnected se deberia ejecutar cuando se recupere la conexion a la wifi,
    pero no siempre es asi (algunos fallos wifi del ESP32
    no generan el evento de conexion y no se recupera la conexion automaticamente).
  */
    if(!Estado.connected && checkRecon) {
      if(wifiReconnect()) {
        Estado.errorInformado = false; //reiniciamos bloqueo futuros LOG_WARN/ERROR 
      } else if (!Estado.errorInformado) {
                LOG_WARN("Reconnect failed, reintentando cada ", RECONNECTINTERVAL, " minutos");
                Estado.errorInformado = true; 
            }
    }
  //  Verificamos estado actual de la wifi (y display wifi level si procede)
    int wifilevel = checkWifi(config.showwifilevel); // conectado a wifi?
    if(Estado.estado == STANDBY) showWifiLevel(wifilevel); // muestra nivel wifi en standby si se ha configurado para mostrarlo{
    /*
      Caso de haber recuperado la conexion wifi despues del Setup leemos factor riegos.
      Si este diese error de conexion con Domoticz, se dejara el flag Estado.recoverableError activado
      y VerifyRecoverySCD será llamada en procesaEstadoError cada RECONNECTINTERVAL 
      para seguir reintentando hasta que se recupere la conexion con Domoticz.
    */    
    if (Estado.connected && Estado.recoverableError) {
      LOG_INFO("conexion Wifi recuperada despues Setup, leemos factor riegos");
      setStateMachine(STANDBY); // reseteamos estado ERROR sin cambios en el display
      lcd.clear(BORRA2H); // borramos mensaje de error previo en display
      checkAndInitFactorRiegos(); //en caso de producirse error con esta funcion ya dejara este activado
      setupEstadoFinal();
    }
    return (Estado.connected && !Estado.error);
}  

// muestra nivel de señal wifi en display (si se ha configurado para mostrarlo)
void showWifiLevel(int wifilevel) {
    lcd.setCursor(0, 3);
    // Si la opción de mostrar está desactivada, limpiamos el área y salimos
    if (!config.showwifilevel) {lcd.print("   "); return;}
    if (wifilevel > 0) {
        if (wifilevel >= 100) wifilevel = 99;
        LOG_TRACE("Wifi OK, nivel=", wifilevel, "%");
        lcd.printf("%02d%%", wifilevel);
    } 
    else lcd.print("--%"); // Caso de pérdida de señal o wifilevel == 0
}