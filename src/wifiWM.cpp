/*
 * Nuevo metodo de conexion a la red wifi usando WifiManager
 *  - no se codifican en el pgm (wifissid.h) las redes wifi y sus pw
*/
#include "Control.h"

//#define ledWifi             LEDG   
//#define ledAP               LEDB

Ticker tic_WifiLed;
Ticker tic_APLed;

int timeout = 180;  //config portal timeout

// Creamos una instancia de la clase WiFiManager

WiFiManager wm;


WiFiManagerParameter custom_domoticz_server("domoticz_ip", "Domoticz_ip");
WiFiManagerParameter custom_domoticz_port("domoticz_port", "puerto");
WiFiManagerParameter custom_ntpserver("ntpServer", "NTP_server");

void parpadeoLedWifi() {
  sLEDG = !sLEDG;
  ledPWM(LEDG,sLEDG);
}

void parpadeoLedAP() {
  sLEDB = !sLEDB;
  ledPWM(LEDB,sLEDB);
}

//llamado cuando WiFiManager sale del modo configuracion
void saveWifiCallback() {
    LOG_INFO("[CALLBACK] saveWifiCallback fired");
    // Eliminamos el temporizador y apagamos el led indicador de modo AP
    tic_APLed.detach();
    ledPWM(LEDB,OFF);
    lcd.infoclear("conectando WIFI");
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
}

//llamado cuando WiFiManager entra en modo configuracion
void configModeCallback (WiFiManager *myWiFiManager) {
  LOG_INFO("[CALLBACK] configModeCallback fired");
  // apagamos el LED indicador de wifi
  tic_WifiLed.detach();
  ledPWM(LEDG,OFF);
  // Empezamos el temporizador que hará parpadear el LED indicador de AP
  tic_APLed.attach(0.5, parpadeoLedAP);
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
 LOG_INFO("WiFi conectada");
}

//evento llamado en caso de desconexion de la wifi
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
 LOG_ERROR("WiFi lost connection. Reason: ", info.wifi_sta_disconnected.reason);
 WiFi.reconnect();
 if (checkWifi()) {
  LOG_INFO("Trying to Reconnect: success");
  return;
 } 
 else LOG_ERROR("Trying to Reconnect: failed");
 delay(3000);
}

// conexion a la red por medio de WifiManager
void setupRedWM(Config_parm &config)
{
  #ifdef DEVELOP
    wm.setDebugOutput(true, WM_DEBUG_DEV);
    //wm.debugPlatformInfo();
  #endif  
  connected = false;
  falloAP = false;
  saveConfig = false;
  if(initFlags.initWifi) {
    wm.resetSettings(); //borra wifi guardada
    //delay(300);
    LOG_INFO("encoderSW pulsado y multirriego en GRUPO3 --> borramos red WIFI");
    lcd.infoclear("red WIFI borrada", DEFAULTBLINK, LOWBIP, 1); //señala borrado wifi
  }
  // explicitly set mode, esp defaults to STA+AP   
  WiFi.mode(WIFI_STA);
  wm.setHostname(HOSTNAME); 
  // Descomentar para resetear configuración
  //wm.resetSettings();
  // Empezamos el temporizador que hará parpadear el LED indicador de wifi
  tic_WifiLed.attach(0.2, parpadeoLedWifi);
  lcd.infoclear("conectando WIFI");
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
  custom_domoticz_server.setValue(config.domoticz_ip, 40);
  custom_domoticz_port.setValue(config.domoticz_port, 5);
  custom_ntpserver.setValue(config.ntpServer, 40);
  // activamos modo AP y portal cautivo y comprobamos si se establece la conexión
  if(!wm.autoConnect("Ardomo")) {
    LOG_WARN("Fallo en la conexión (timeout)");
    falloAP = true;
    delay(1000);
  }
  /* 
    * Podemos continuar hasta aqui por tres razones:
    *   - nos hemos conectado a la red wifi almacenada
    *   - nos hemos podido conectara a la red wifi que hemos introducido en la web de configuracion
    *   - no nos hemos podido conectar a la red wifi almacenada o no habia y el modo configuracion ha 
    *     dado timeout (falloAP=true)
    */
  // detenemos parpadeo led AP y borramos -AP- del display (caso de que se hubiera activado antes AP)
  tic_APLed.detach();
  //si no hemos podido conectar y existe una red wifi salvada,reintentamos hasta 20 seg.
  // (para caso corte de corriente)
  if (falloAP && wm.getWiFiIsSaved()) {
    lcd.infoclear("conectando WIFI");
    LOG_INFO("Hay wifi salvada -> reintentamos la conexion");
    int j=0;
    falloAP = false;
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      WiFi.reconnect(); 
      delay(2000);
      j++;
      if(j == MAXCONNECTRETRY) {
        falloAP = true;
        LOG_ERROR("Fallo en la reconexión");
        statusError(E1);
        break;
      }
    }
  }
  // dejamos LEDB segun estado de NONETWORK
  NONETWORK ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
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
    // ----------------------------- save the custom parameters
  if (saveConfig) {
    strcpy(config.domoticz_ip, custom_domoticz_server.getValue());
    strcpy(config.domoticz_port, custom_domoticz_port.getValue());
    strcpy(config.ntpServer, custom_ntpserver.getValue());
  }
  //dejamos activado evento de desconexion o conexion ?? (wifi events):
  //WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  //WiFi.removeEvent(WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

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
  // Eliminamos el temporizador y dejamos LEDB segun estado de NONETWORK
  tic_APLed.detach();
  NONETWORK ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
  lcd.infoclear("reconectando WIFI");
  tic_WifiLed.detach();
  checkWifi();
}

// verificacion estado de la conexion wifi
bool checkWifi() {
  LOG_TRACE("in checkWifi");
  if(WiFi.status() == WL_CONNECTED) {
    ledPWM(LEDG,ON);  // Encendemos el LED indicador de wifi
    connected = true;
    return true;
  }
  else {
    LOG_ERROR(" ** [ERROR] No estamos conectados a la wifi");
    ledPWM(LEDG,OFF);  // apagamos el LED indicador de wifi
    connected = false;
    return false;
  }
}
