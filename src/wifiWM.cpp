/*
 * Nuevo metodo de conexion a la red wifi usando WifiManager
 *  - no se codifican en el pgm (wifissid.h) las redes wifi y sus pw
*/
#include "Control.h"

//#define ledWifi             LEDG   
//#define ledAP               LEDB

Ticker tic_WifiLed;
Ticker tic_APLed;

#ifdef DEVELOP
  int timeout = 20;  //config portal timeout para pruebas
#else
  int timeout = 180;  //config portal timeout para produccion  
#endif

// Creamos una instancia de la clase WiFiManager

WiFiManager wm;


WiFiManagerParameter custom_domoticz_server("domoticz_ip", "Domoticz_ip");
WiFiManagerParameter custom_domoticz_port("domoticz_port", "puerto");
WiFiManagerParameter custom_ntpserver("ntpServer", "NTP_server");
WiFiManagerParameter custom_timezone("timeZone", "timezone");

//llamado cuando WiFiManager sale del modo configuracion
void saveWifiCallback() {
    LOG_INFO("[CALLBACK] saveWifiCallback fired");
    // Eliminamos el temporizador y apagamos el led indicador de modo AP
    tic_APLed.detach();
    ledPWM(LEDB,OFF);
    lcd.infoclear("conectando WIFI");
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    tic_WifiLed.attach(RAPIDO, parpadeoLedWifi);
}

//llamado cuando WiFiManager entra en modo configuracion
void configModeCallback (WiFiManager *myWiFiManager) {
  LOG_INFO("[CALLBACK] configModeCallback fired");
  // apagamos el LED indicador de wifi
  tic_WifiLed.detach();
  ledPWM(LEDG,OFF);
  // Empezamos el temporizador que hará parpadear el LED indicador de AP
  tic_APLed.attach(NORMAL, parpadeoLedAP);
  lcd.infoclear("   modo -AP- :", DEFAULTBLINK, LOWBIP, 1); //lo señalamos en display
  lcd.info("\"Ardomo\" activado", 3);
}

//llamado cuando WiFiManager recibe parametros adicionales
void saveParamCallback()
{
  LOG_INFO("[CALLBACK] saveParamCallback fired");
  LOG_INFO("Should save config");
  saveConfig = true;
  wm.stopConfigPortal();
}

//lamado antes de empezar carga del sketch via OTA
void preOtaUpdateCallback()
{
  LOG_INFO("[CALLBACK] setPreOtaUpdateCallback fired");
  lcd.infoclear("OTA in progress", DEFAULTBLINK, LOWBIP, 1);
}

//evento llamado en caso de conexion de la wifi
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
 LOG_INFO("    <<<<---  WiFi conectada  --->>>>");
 connected = true;
}

//evento llamado en caso de desconexion de la wifi
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
 LOG_ERROR("WiFi lost connection. Reason: ", info.wifi_sta_disconnected.reason);
//  WiFi.reconnect();
//  if (checkWifi()) {
//   LOG_INFO("Trying to Reconnect: success");
//   return;
//  } 
//  else LOG_ERROR("Trying to Reconnect: failed");
//  delay(3000);
}

// conexion a la red por medio de WifiManager
void setupRedWM(Config_parm &config, S_initFlags &initFlags)
{
  #ifdef DEVELOP
    wm.setDebugOutput(true, WM_DEBUG_DEV);
    //wm.debugPlatformInfo();
  #endif  
  connected = false;
  recoverableError = false;
  saveConfig = false;
  if(initFlags.initWifi) {
    wm.resetSettings(); //borra wifi guardada
    //delay(300);
    LOG_INFO("encoderSW pulsado y multirriego en GRUPO3 --> borramos red WIFI");
    lcd.infoclear("red WIFI borrada", DEFAULTBLINK, LOWBIP, 1); //señala borrado wifi
  }
  // explicitly set mode, esp defaults to STA+AP   
  WiFi.mode(WIFI_STA);
  //esp_wifi_set_ps( WIFI_PS_NONE );  // Set current WiFi power save type (Default is WIFI_PS_MIN_MODEM)
  //WiFi.setTxPower(WIFI_POWER_19_5dBm); // ajusta la potencia de transmision wifi al maximo
  wm.setHostname(HOSTNAME); 
  // Descomentar para resetear configuración
  //wm.resetSettings();
  
  //sets timeout until configuration portal gets turned off
  wm.setConfigPortalTimeout(timeout);
  // callbacks
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveWifiCallback);
  wm.setSaveParamsCallback(saveParamCallback);
  wm.setPreOtaUpdateCallback(preOtaUpdateCallback);
  //if this is set, it will exit after config, even if connection is unsuccessful
  wm.setBreakAfterConfig(true);
  //muestra version en el titulo de la pagina web inicial
  wm.setTitle("Version: " + String(VERSION));
  //pagina de parametros independiente
  wm.setParamsPage(true);
  wm.addParameter(&custom_domoticz_server);
  wm.addParameter(&custom_domoticz_port);
  wm.addParameter(&custom_ntpserver);
  wm.addParameter(&custom_timezone);
  custom_domoticz_server.setValue(config.domoticz_ip, 40);
  custom_domoticz_port.setValue(config.domoticz_port, 5);
  custom_ntpserver.setValue(config.ntpServer, 40);
  custom_timezone.setValue(config.TZ, 100);
  if(noWIFI) return;
  lcd.infoclear("conectando WIFI");
  tic_WifiLed.attach(RAPIDO, parpadeoLedWifi); // Empezamos el temporizador que hará parpadear el LED indicador de wifi
  ledPWM(LEDR,OFF);   // y apagamos LEDR
  // activamos conexion wifi y comprobamos si se establece
  if(!wm.autoConnect("Ardomo")) {
    LOG_WARN("Fallo en la conexión (timeout)");
    recoverableError = true;
    delay(1000);
  }
  /* 
    * Podemos continuar hasta aqui por tres razones:
    *   - nos hemos conectado a la red wifi almacenada
    *   - nos hemos podido conectara a la red wifi que hemos introducido en la web de configuracion
    *   - no nos hemos podido conectar a la red wifi almacenada o no habia y el modo configuracion ha 
    *     dado timeout (recoverableError=true)
    */
  // detenemos parpadeo led AP (caso de que se hubiera activado antes AP)
  tic_APLed.detach();
  ledPWM(LEDB,OFF);   // y lo apagamos
  //si no hemos podido conectar y existe una red wifi salvada,reintentamos hasta 20 seg.
  // (para caso corte de corriente)
  if (recoverableError && wm.getWiFiIsSaved()) {
    lcd.infoclear("conectando WIFI");
    LOG_INFO("Hay wifi salvada -> reintentamos la conexion");
    int j=0;
    recoverableError = false;
    tic_WifiLed.attach(RAPIDO, parpadeoLedWifi);
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      WiFi.reconnect(); 
      delay(2000);
      j++;
      if(j == MAXCONNECTRETRY) {
        recoverableError = true;
        LOG_ERROR("Fallo en la reconexión");
        break;
      }
    }
  }
  // dejamos LEDB segun estado de modoDEMO
  modoDEMO ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
  //detenemos parpadeo led wifi
  tic_WifiLed.detach();
  if (checkWifi()) {
    LOG_INFO(" >>  Conectado a SSID: ", WiFi.SSID().c_str());
    LOG_INFO(" >>      IP address: ", WiFi.localIP());
    LOG_INFO(" >>      RSSI:", WiFi.RSSI(), "dBm  (",  wm.getRSSIasQuality(WiFi.RSSI()),"%)");
    LOG_DEBUG(" >>      Autoreconnect:", WiFi.getAutoReconnect(), " (1 = enabled)");
    int msgl = snprintf(buff, MAXBUFF, "wifi OK: %s", WiFi.SSID().c_str());
    lcd.info(buff, 1, msgl);
  }
  else if(!modoDEMO) statusError(E1, RECUPERABLE); //si no hemos podido conectar a la wifi señalamos error
    // ----------------------------- save the custom parameters
  if (saveConfig) {
    strcpy(config.domoticz_ip, custom_domoticz_server.getValue());
    strcpy(config.domoticz_port, custom_domoticz_port.getValue());
    strcpy(config.ntpServer, custom_ntpserver.getValue());
    strcpy(config.TZ, custom_timezone.getValue());
  }
  //dejamos activado evento de desconexion o conexion ?? (wifi events):
  // WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  // WiFi.removeEvent(WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
} //fin setupRedWM

/**
 * @brief activa portal para configuracion red wifi y/o parámetros de conexion
 * 
 * @param config 
 */
void starConfigPortal(Config_parm &config) 
{
  wm.setConfigPortalTimeout(timeout);
  if (!wm.startConfigPortal("Ardomo")) {
    LOG_INFO(" exit or hit timeout");
  }
    // ----------------------------- save the custom parameters
  if (saveConfig) {
    strcpy(config.domoticz_ip, custom_domoticz_server.getValue());
    strcpy(config.domoticz_port, custom_domoticz_port.getValue());
    strcpy(config.ntpServer, custom_ntpserver.getValue());
  }
  // Eliminamos el temporizador y dejamos LEDB segun estado de modoDEMO
  tic_APLed.detach();
  modoDEMO ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
  lcd.infoclear("reconectando WIFI");
  tic_WifiLed.detach();
  checkWifi();
}

// verificacion estado de la conexion wifi
int checkWifi(bool level) {
  //LOG_TRACE("in checkWifi");
  if(WiFi.status() == WL_CONNECTED) {
    tic_WifiLed.detach();  // detenemos su parpadeo por si lo tuviera activo
    ledPWM(LEDG,ON);  // Encendemos el LED indicador de wifi
    connected = true;
    return level==true ? wm.getRSSIasQuality(WiFi.RSSI()) : true; // devuelve nivel señal wifi si level es true 
  }
  else {
    LOG_ERROR(" ** [ERROR] No estamos conectados a la wifi");
    ledPWM(LEDG,OFF);  // apagamos el LED indicador de wifi
    connected = false;
    return false;
  }
}

bool wifiReconnect () {
    LOG_WARN("----  INTENTANDO RECONEXION WIFI  ----");
    tic_WifiLed.attach(RAPIDO, parpadeoLedWifi);
    lcd.info("conectando WIFI",1);
    // WiFi.reconnect(); 
    WiFi.disconnect();
    delay(3000);
    WiFi.begin();
    delay(3000);
    tic_WifiLed.detach();
    if (checkWifi()) {
      int msgl = snprintf(buff, MAXBUFF, "wifi OK: %s", WiFi.SSID().c_str());
      lcd.info(buff, 1, msgl);
      return true;
    } else return false;
}    

bool wifiVerifyRecovery(Config_parm &config, S_Estado &Estado) {
  //LOG_TRACE("");
  //en modoDEMO sin conexion no verificamos (DEMO sin wifi)
  if (modoDEMO && !connected) return true;
  lcd.displayON(); //por si estuviera parpadeando(apagado) por error en pantalla
  /*
    Si no estamos conectados a la wifi, intentamos reconexion cada RECONNECTINTERVAL minutos.
    Normalmente no se ejecutara, ya que el evento WiFiStationConnected se ejecutara
    cuando se recupere la conexion a la wifi, pero por si acaso lo dejamos (algunos fallos wifi del ESP32
    no generan el evento de conexion y no se recupera la conexion automaticamente).

  */
    if(!connected && checkReconInterval) {
      if(wifiReconnect()) LOG_INFO("Wifi reconectada OK"); //reconectamos a la wifi
        else LOG_WARN("Reconnect failed, esperando ",RECONNECTINTERVAL," minutos para volver a intentar");
    }
  /*
    Verificamos estado actual de la wifi 
    (y display wifi level si procede)
  */  
    #ifdef DEVELOP
    int wifilevel = checkWifi(true); // conectado a wifi?
    #else
    int wifilevel = checkWifi(config.showwifilevel); // conectado a wifi?
    #endif
    if(wifilevel) {
      LOG_DEBUG("Wifi verificada OK, nivel=",wifilevel,"%");
      if (config.showwifilevel && Estado.estado == STANDBY) {
         LOG_DEBUG("showwifilevel=",config.showwifilevel,"wifilevel=",wifilevel);
         if(wifilevel==100) wifilevel=99; 
         lcd.setCursor(0,3);
         snprintf(buff,MAXBUFF,"%02d%%",wifilevel);
         lcd.print(buff); 
      }
    } else if (config.showwifilevel && Estado.estado == STANDBY) {lcd.setCursor(0,3);lcd.print("--%");} //borramos nivel wifi si se mostraba
    /*
      Caso de haber recuperado la conexion wifi despues del Setup leemos factor riegos.
      Si este diese error de conexion con Domoticz, se dejara el flag recoverableError activado
      y domoticzVerifyRecovery será llamada en procesaEstadoError cada RECONNECTINTERVAL 
      para seguir reintentando hasta que se recupere la conexion.
    */    
    if (connected && recoverableError) {
      LOG_INFO("conexion Wifi recuperada despues Setup, leemos factor riegos");
      ledPWM(LEDG,OFF);
      Estado.estado = STANDBY; //borramos estado ERROR
      Estado.error = NOERROR; //reseteamos error
      recoverableError = false; //reseteamos error recuperable
      initFactorRiegos(); //en caso de producirse error con esta funcion ya dejara este activado
      setupEstado();
    }
    if (connected && !Estado.error) return true;
      else return false;
}  
