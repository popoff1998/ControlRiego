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

void parpadeoLedWifi(){
  byte estado = ledStatusId(LEDG);
  led(LEDG,!estado);
}

void parpadeoLedAP(){
  byte estado = ledStatusId(LEDB);
  led(LEDB,!estado);
}

//llamado cuando WiFiManager sale del modo configuracion
void saveWifiCallback() {
  Serial.println(F("[CALLBACK] saveWifiCallback fired"));
    // Eliminamos el temporizador y apagamos el led indicador de modo AP
    tic_APLed.detach();
    led(LEDB,OFF);
    infoDisplay("----", NOBLINK, BIP, 0);
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
}

//llamado cuando WiFiManager entra en modo configuracion
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("[CALLBACK] configModeCallback fired"));
  // apagamos el LED indicador de wifi
  tic_WifiLed.detach();
  led(LEDG,OFF);
  // Empezamos el temporizador que hará parpadear el LED indicador de AP
  tic_APLed.attach(0.5, parpadeoLedAP);
  infoDisplay("-AP-", DEFAULTBLINK, LONGBIP, 1); //lo señalamos en display
}

//llamado cuando WiFiManager recibe parametros adicionales
void saveParamCallback()
{
  Serial.println(F("[CALLBACK] saveParamCallback fired"));
  Serial.println(F("Should save config"));
  saveConfig = true;
  wm.stopConfigPortal();
}

//lamado antes de empezar carga del sketch via OTA
void preOtaUpdateCallback()
{
  Serial.println(F("[CALLBACK] setPreOtaUpdateCallback fired"));
  infoDisplay("####", DEFAULTBLINK, LONGBIP, 1);
}

// conexion a la red por medio de WifiManager
void setupRedWM(Config_parm &config)
{
  connected = false;
  falloAP = false;
  saveConfig = false;
  if(initFlags.initWifi) {
    wm.resetSettings(); //borra wifi guardada
    //delay(300);
    Serial.println(F("encoderSW pulsado y multirriego en GRUPO3 --> borramos red WIFI"));
    infoDisplay("CLEA", DEFAULTBLINK, LONGBIP, 1); //señala borrado wifi
  }
  // explicitly set mode, esp defaults to STA+AP   
  WiFi.mode(WIFI_STA); 
  // Descomentar para resetear configuración
  //wm.resetSettings();
  // Empezamos el temporizador que hará parpadear el LED indicador de wifi
  tic_WifiLed.attach(0.2, parpadeoLedWifi);
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
    Serial.println(F("Fallo en la conexión (timeout)"));
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
  infoDisplay("----", NOBLINK, BIP, 0);
  //si no hemos podido conectar y existe una red wifi salvada,reintentamos hasta 20 seg.
  // (para caso corte de corriente)
  if (falloAP && wm.getWiFiIsSaved()) {
    Serial.println(F("Hay wifi salvada -> reintentamos la conexion"));
    int j=0;
    falloAP = false;
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      delay(2000);
      j++;
      if(j == MAXCONNECTRETRY) {
        falloAP = true;
        Serial.println(F("Fallo en la reconexión"));
        break;
      }
    }
  }
  // dejamos LEDB segun estado de NONETWORK
  NONETWORK ? led(LEDB,ON) : led(LEDB,OFF);
  //detenemos parpadeo led wifi
  tic_WifiLed.detach();
  if (checkWifi()) {
    Serial.printf("\nWifi conectado a SSID: %s\n", WiFi.SSID().c_str());
    Serial.print(F(" IP address: "));
    Serial.println(WiFi.localIP());
    Serial.printf(" RSSI: %d dBm  (%d%%)\n\n", WiFi.RSSI(), wm.getRSSIasQuality(WiFi.RSSI()));
  }
    // ----------------------------- save the custom parameters
  if (saveConfig) {
    strcpy(config.domoticz_ip, custom_domoticz_server.getValue());
    strcpy(config.domoticz_port, custom_domoticz_port.getValue());
    strcpy(config.ntpServer, custom_ntpserver.getValue());
  }
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
    Serial.println(F(" exit or hit timeout"));
  }
    // ----------------------------- save the custom parameters
  if (saveConfig) {
    strcpy(config.domoticz_ip, custom_domoticz_server.getValue());
    strcpy(config.domoticz_port, custom_domoticz_port.getValue());
    strcpy(config.ntpServer, custom_ntpserver.getValue());
  }
  // Eliminamos el temporizador y dejamos LEDB segun estado de NONETWORK
  tic_APLed.detach();
  NONETWORK ? led(LEDB,ON) : led(LEDB,OFF);
  infoDisplay("----", NOBLINK, BIP, 0);
  tic_WifiLed.detach();
  checkWifi();
}

// verificacion estado de la conexion wifi
bool checkWifi() {
  #ifdef TRACE
    Serial.println(F("TRACE: in checkWifi"));
  #endif
  if(WiFi.status() == WL_CONNECTED) {
    // Encendemos el LED indicador de wifi
    led(LEDG,ON);
    connected = true;
    return true;
  }
  else {
    Serial.println(F("[ERROR] No estamos conectados a la wifi"));
    // apagamos el LED indicador de wifi
    led(LEDG,OFF);
    connected = false;
    return false;
  }
}
