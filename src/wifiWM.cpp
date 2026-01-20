/*
 * Conexion a la red wifi usando WifiManager
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

//mensaje de wifi OK con SSID
const char* wifiOKmsg() {
    static char buffer[70]; // Un poco más de margen por si el SSID es largo
    snprintf(buffer, sizeof(buffer), "<<<<--- WiFi conectada (%s) --->>>>", WiFi.SSID().c_str());
    return buffer;
}

//llamado cuando WiFiManager sale del modo configuracion
void saveWifiCallback() {
    LOG_INFO("[CALLBACK] fired");
    // Eliminamos el temporizador y apagamos el led indicador de modo AP
    setParpadeo(tic_APLed, APAGA, LEDB);
    lcd.infoclear("conectando WIFI");
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, LEDG);
}

//llamado cuando WiFiManager entra en modo configuracion
void configModeCallback (WiFiManager *myWiFiManager) {
  LOG_INFO("[CALLBACK] fired");
  // apagamos el LED indicador de wifi
  setParpadeo(tic_WifiLed, APAGA, LEDG);
  // Empezamos el temporizador que hará parpadear el LED indicador de AP
  setParpadeo(tic_APLed, NORMAL, parpadeoLedPWM, LEDB);
  lcd.infoclear("   modo -AP- :", DEFAULTBLINK, LOWBIP, 1); //lo señalamos en display
  lcd.info("\"Ardomo\" activado", 3);
}

//llamado cuando WiFiManager recibe parametros adicionales
void saveParamCallback()
{
  LOG_INFO("[CALLBACK] fired");
  LOG_INFO("Should save config");
  saveConfig = true;
  wm.stopConfigPortal();
}

//lamado antes de empezar carga del sketch via OTA
void preOtaUpdateCallback()
{
  LOG_INFO("[CALLBACK] fired");
  lcd.infoclear("OTA in progress", DEFAULTBLINK, LOWBIP, 1);
}

//evento llamado en caso de desconexion de la wifi
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  if (Estado.connected) LOG_ERROR("WiFi lost connection. Reason: ", info.wifi_sta_disconnected.reason);
  else LOG_DEBUG("WiFi lost connection. Reason: ", info.wifi_sta_disconnected.reason);
  Estado.errorInformado = true; // bloquea futuros LOG_WARN/ERROR
  setConnected(false);
}

//evento llamado en caso de conexion de la wifi
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  LOG_INFO(wifiOKmsg());
  logStatus(wifiOKmsg());
  if (Estado.estado == STANDBY) {
    lcd.info("STANDBY",1);  //restaura pantalla (borra msg de reconexion)
    showTemp();  // muestra temperatura ambiente en standby
  }
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
  saveConfig = false;
  if(initFlags.initWifi) {
    wm.resetSettings(); //borra wifi guardada
    //delay(300);
    PRINTLN("[setupRedWM] encoderSW pulsado y multirriego en GRUPO3 --> borramos red WIFI");
    lcd.infoclear("red WIFI borrada", DEFAULTBLINK, LOWBIP, 1); //señala borrado wifi
  }
  // explicitly set mode, esp defaults to STA+AP   
  WiFi.mode(WIFI_STA);
  //esp_wifi_set_ps( WIFI_PS_NONE );  // Set current WiFi power save type (Default is WIFI_PS_MIN_MODEM)
  //WiFi.setTxPower(WIFI_POWER_19_5dBm); // ajusta la potencia de transmision wifi al maximo
  wm.setHostname(HOSTNAME); 
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
  wm.setTitle("Version: " + String(FW_VERSION));
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
  if(Estado.noWIFI) return;
  lcd.infoclear("conectando WIFI");
  ledPWM(LEDR,OFF);   // Apagamos LEDR
  setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, LEDG); // y empezamos el temporizador que hará parpadear el LED indicador de wifi
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
  setParpadeo(tic_APLed, APAGA, LEDB);
  //si no hemos podido conectar y existe una red wifi salvada,reintentamos hasta 20 seg.
  // (para caso corte de corriente)
  if (Estado.recoverableError && wm.getWiFiIsSaved()) {
    lcd.infoclear("conectando WIFI");
    PRINTLN("[setupRedWM] Hay wifi salvada -> reintentamos la conexion");
    int j=0;
    Estado.recoverableError = false;
    setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, LEDG);
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      WiFi.reconnect(); 
      delay(2000);
      j++;
      if(j == MAXCONNECTRETRY) {
        Estado.recoverableError = true;
        LOG_ERROR("Fallo en la reconexión");
        break;
      }
    }
  }
  // dejamos LEDB segun estado de modoDEMO
  Estado.modoDEMO ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
  //detenemos parpadeo led wifi
  setParpadeo(tic_WifiLed, PARAR);
  if (checkWifi()) {
    PRINTLN("\n[setupRedWM]  >>  Conectado a SSID: ", WiFi.SSID().c_str());
    PRINTLN(  "[setupRedWM]  >>      IP address: ", WiFi.localIP());
    PRINTLN(  "[setupRedWM]  >>      RSSI:", WiFi.RSSI(), "dBm  (",  wm.getRSSIasQuality(WiFi.RSSI()),"%)\n");
    // LOG_DEBUG(" >>      Autoreconnect:", WiFi.getAutoReconnect(), " (1 = enabled)");
    int msgl = snprintf(buff, MAXBUFF, "wifi OK: %s", WiFi.SSID().c_str());
    lcd.info(buff, 1, msgl);
  }
  else if(!Estado.modoDEMO) {
     statusError(E1, RECUPERABLE); //si no hemos podido conectar a la wifi señalamos error
     LOG_ERROR("SIN conexion wifi");
  }
    // ----------------------------- save the custom parameters
  if (saveConfig) {
    strcpy(config.domoticz_ip, custom_domoticz_server.getValue());
    strcpy(config.domoticz_port, custom_domoticz_port.getValue());
    strcpy(config.ntpServer, custom_ntpserver.getValue());
    strcpy(config.TZ, custom_timezone.getValue());
  }
  //dejamos activado evento de desconexion o conexion ?? (wifi events):
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  // WiFi.removeEvent(WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
} //fin setupRedWM

/**
 * @brief activa portal para configuracion red wifi y/o parámetros de conexion
 * 
 * @param config 
 */
void startConfigPortal() 
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
    strcpy(config.TZ, custom_timezone.getValue());
  }
  // Eliminamos el temporizador y dejamos LEDB segun estado de modoDEMO
  setParpadeo(tic_APLed, PARAR);
  Estado.modoDEMO ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
  lcd.infoclear("reconectando WIFI");
  setParpadeo(tic_WifiLed, PARAR);
  checkWifi();  // ¿TODO es necesario?
}

//set del estado de la conexion wifi y del led indicador si procede
void setConnected(bool state) {
  Estado.connected = state;
  if (Estado.estado != ERROR && Estado.estado != PAUSE && Estado.estado != CONFIGURANDO) ledPWM(LEDG,state);
}

// verificacion estado de la conexion wifi
int checkWifi(bool level) {
  //LOG_TRACE("in checkWifi");
  if(WiFi.status() == WL_CONNECTED) {
    // detenemos su parpadeo por si lo tuviera activo y encendemos el LEDG indicador de wifi
    setParpadeo(tic_WifiLed, FIJO, LEDG);
    if (!Estado.connected) {
      LOG_INFO(wifiOKmsg());
      logStatus(wifiOKmsg());  
      Estado.connected = true;
      Estado.errorInformado = false; //reiniciamos bloqueo futuros LOG_WARN/ERROR
    }
    return level==true ? wm.getRSSIasQuality(WiFi.RSSI()) : true; // devuelve nivel señal wifi si level es true 
  }
  else {
    if (!Estado.errorInformado) LOG_ERROR(" ** [ERROR] No estamos conectados a la wifi");
    // detenemos su parpadeo por si lo tuviera activo y apagamos el LEDG indicador de wifi
    setParpadeo(tic_WifiLed, APAGA, LEDG);  
    Estado.connected = false;
    Estado.errorInformado = true; // bloquea futuros LOG_ERROR
    return false;
  }
}

bool wifiReconnect () {
    LOG_INFO("----  INTENTANDO RECONEXION WIFI  ----");
    setParpadeo(tic_WifiLed, RAPIDO, parpadeoLedPWM, LEDG);
    lcd.info("conectando WIFI",1);
    // WiFi.reconnect(); 
    WiFi.disconnect();
    delay(3000);
    WiFi.begin();
    delay(3000);
    setParpadeo(tic_WifiLed, PARAR);
    if (checkWifi()) {
      int msgl = snprintf(buff, MAXBUFF, "wifi OK: %s", WiFi.SSID().c_str());
      lcd.info(buff, 1, msgl);
      if (Estado.estado == STANDBY) {
          lcd.info("STANDBY",1);  //restaura pantalla (borra msg de reconexion)
          showTemp();  // muestra temperatura ambiente en standby
      }
      return true;
    } else return false;
}    

bool VerifyRecoveryWifi(bool checkReconInterval) {
  //LOG_TRACE("");
  //en modoDEMO sin conexion no verificamos (DEMO sin wifi)
  if (Estado.modoDEMO && !Estado.connected) return true;
  lcd.displayON(); //por si estuviera parpadeando(apagado) por error en pantalla
  /*
    Si no estamos conectados a la wifi, intentamos reconexion cada RECONNECTINTERVAL minutos.
    Normalmente no se ejecutara, ya que el evento WiFiStationConnected se ejecutara
    cuando se recupere la conexion a la wifi, pero por si acaso lo dejamos (algunos fallos wifi del ESP32
    no generan el evento de conexion y no se recupera la conexion automaticamente).
    */
    if(!Estado.connected && checkReconInterval) {
      // Intentamos reconectar
      if(wifiReconnect()) {
        LOG_INFO(wifiOKmsg());
        logStatus(wifiOKmsg());
        Estado.errorInformado = false; //reiniciamos bloqueo futuros LOG_WARN/ERROR 
      } else if (!Estado.errorInformado) {
                LOG_WARN("Reconnect failed, reintentando cada ", RECONNECTINTERVAL, " minutos");
                Estado.errorInformado = true; 
            }
    }
  //  Verificamos estado actual de la wifi (y display wifi level si procede)
    #ifdef DEVELOP
    int wifilevel = checkWifi(true); // conectado a wifi?
    #else
    int wifilevel = checkWifi(config.showwifilevel); // conectado a wifi?
    #endif
    if(wifilevel) {
      LOG_TRACE("Wifi OK, nivel=",wifilevel,"%");
      if (config.showwifilevel && Estado.estado == STANDBY) {
         LOG_TRACE("showwifilevel=",config.showwifilevel,"wifilevel=",wifilevel);
         if(wifilevel==100) wifilevel=99; 
         lcd.setCursor(0,3);
         snprintf(buff,MAXBUFF,"%02d%%",wifilevel);
         lcd.print(buff); 
      }
    } else if (config.showwifilevel && Estado.estado == STANDBY) {lcd.setCursor(0,3);lcd.print("--%");} //borramos nivel wifi si se mostraba
    /*
      Caso de haber recuperado la conexion wifi despues del Setup leemos factor riegos.
      Si este diese error de conexion con Domoticz, se dejara el flag Estado.recoverableError activado
      y VerifyRecoverySCD será llamada en procesaEstadoError cada RECONNECTINTERVAL 
      para seguir reintentando hasta que se recupere la conexion.
    */    
    if (Estado.connected && Estado.recoverableError) {
      LOG_INFO("conexion Wifi recuperada despues Setup, leemos factor riegos");
      ledPWM(LEDG,OFF);  // TODO comprobar si es necesario
      setStateMachine(STANDBY); // reseteamos estado ERROR
      initFactorRiegos(); //en caso de producirse error con esta funcion ya dejara este activado
      setupEstadoFinal();
    }
    if (Estado.connected && !Estado.error) return true;
      else return false;
}  

void pararLedsWifiAP() {
    setParpadeo(tic_WifiLed, PARAR);
    setParpadeo(tic_APLed, PARAR);
}