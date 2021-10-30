/*
 * Nuevo metodo de conexion a la red wifi usando WifiManager
 *  - no se codifican en el pgm (wifissid.h) las redes wifi y sus pw
*/
#include "Control.h"

//#define ledWifi             LEDG   
//#define ledAP               LEDB

Ticker tic_WifiLed;
Ticker tic_APLed;

// Creamos una instancia de la clase WiFiManager
WiFiManager wm;

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
  Serial.println("[CALLBACK] saveCallback fired");
    // Eliminamos el temporizador y apagamos el led indicador de modo AP
    tic_APLed.detach();
    led(LEDB,OFF);
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
}

//llamado cuando WiFiManager entra en modo configuracion
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("[CALLBACK] configModeCallback fired");
  // apagamos el LED indicador de wifi
  tic_WifiLed.detach();
  led(LEDG,OFF);
  // Empezamos el temporizador que hará parpadear el LED indicador de AP
  tic_APLed.attach(0.5, parpadeoLedAP);
}

//llamado cuando WiFiManager recibe parametros adicionales
void saveParamCallback()
{
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("Should save config");
  saveConfig = true;
}

// conexion a la red por medio de WifiManager
void setupRedWM()
{
  connected = false;
  falloAP = false;
  saveConfig = false;
  if(initFlags.initWifi) {
    //WiFi.disconnect(); //borra wifi guardada (no funciona con V3.2.0 de platform espressif)
    wm.resetSettings(); //borra wifi guardada
    delay(300);
    Serial.println("encoderSW pulsado y multirriego en GRUPO3 --> borramos red WIFI");
    //señala borrado wifi
    longbip(3);
  }
  // explicitly set mode, esp defaults to STA+AP   
  WiFi.mode(WIFI_STA); 
  // Descomentar para resetear configuración
  //wm.resetSettings();
  // Empezamos el temporizador que hará parpadear el LED indicador de wifi
  tic_WifiLed.attach(0.2, parpadeoLedWifi);
  //sets timeout until configuration portal gets turned off
  wm.setConfigPortalTimeout(180);
  //parametros custom de configuracion en la pagina web de wifi
  WiFiManagerParameter custom_domoticz_server("serverAddress", "Domoticz_ip" ,serverAddress, 40);
  WiFiManagerParameter custom_domoticz_port("DOMOTICZPORT", "puerto", DOMOTICZPORT, 5);
  WiFiManagerParameter custom_ntpserver("ntpServer", "NTP_server", ntpServer, 40);
  wm.addParameter(&custom_domoticz_server);
  wm.addParameter(&custom_domoticz_port);
  wm.addParameter(&custom_ntpserver);
  // callbacks
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveWifiCallback);
  wm.setSaveParamsCallback(saveParamCallback);
  //muestra version en el titulo de la pagina web inicial
  wm.setTitle("Version: " + String(VERSION));
  // activamos modo AP y portal cautivo y comprobamos si se establece la conexión
  if(!wm.autoConnect("Ardomo")){
    Serial.println("Fallo en la conexión (timeout)");
    falloAP = true;
    //WiFi.mode(WIFI_STA); 
    //ESP.reset();
    delay(1000);
  }
  /* 
    * Podemos continuar hasta aqui por tres razones:
    *   - nos hemos conectado a la red wifi almacenada
    *   - nos hemos podido conectara a la red wifi que hemos introducido en la web de configuracion
    *   - no nos hemos podido conectar a la red wifi almacenada o no habia y el modo configuracion ha 
    *     dado timeout (falloAP=true)
    */
  // Eliminamos el temporizador y dejamos LEDB segun estado de NONETWORK
  tic_APLed.detach();
  NONETWORK ? led(LEDB,ON) : led(LEDB,OFF);
  //si no hemos podido conectar reintentamos hasta 10 seg. (para caso corte de corriente)
  if (falloAP) {
    int j=0;
    falloAP = false;
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
      j++;
      if(j == MAXCONNECTRETRY) {
        falloAP = true;
        Serial.println("Fallo en la reconexión (timeout 10 seg.)");
        break;
      }
    }
  }
  //detenemos parpadeo led wifi
  tic_WifiLed.detach();
  if(WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWifi conectado a SSID: %s\n", WiFi.SSID().c_str());
    Serial.print(" IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf(" RSSI: %d dBm \n\n", WiFi.RSSI());
    // Encendemos el led indicador de wifi
    led(LEDG,ON);
    connected = true;
  }
  else {
    // Apagamos el LED indicador de wifi
    led(LEDG,OFF);
    connected = false;
  }
  // ----------------------------- save the custom parameters to eeprom
  if (saveConfig) {
    strcpy(serverAddress, custom_domoticz_server.getValue());
    strcpy(DOMOTICZPORT, custom_domoticz_port.getValue());
    strcpy(ntpServer, custom_ntpserver.getValue());
    eepromWriteRed();
  }
}

// verificacion estado de la conexion wifi
bool checkWifi() {
  #ifdef TRACE
    Serial.println("TRACE: in checkWifi");
  #endif
  if(WiFi.status() == WL_CONNECTED) {
    // Encendemos el LED indicador de wifi
    led(LEDG,ON);
    connected = true;
    return true;
  }
  else {
    Serial.println("[ERROR] No estamos conectados a la wifi");
    // apagamos el LED indicador de wifi
    led(LEDG,OFF);
    connected = false;
    return false;
  }
}
