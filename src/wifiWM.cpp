/*
 * Nuevo metodo de conexion a la red wifi usando WifiManager
 *  - no se codifican en el pgm (wifissid.h) las redes wifi y sus pw
*/
#include "Control.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h >
#include <WiFiManager.h>
#include <Ticker.h>

//#define ledWifi             LEDG   
//#define ledAP               LEDB

Ticker tic_WifiLed;
Ticker tic_APLed;

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

void setupRedWM()  // conexion a la red por medio de WifiManager
{
   #ifdef NET_MQTTCLIENT
    MqttClient.setClient(client);
    MqttClient.setServer(MQTT_SERVER,1883);
  #endif
  #ifdef MEGA256
    Ethernet.begin(mac,ip,gateway,subnet);
  #endif
  #ifdef NODEMCU
    bool wifiSW = false;
    connected = false;
    falloAP = false;
    //verificamos si encoderSW esta pulsado (estado OFF) y selector de multirriego esta en posicion:
    //   - Grupo2 (GOTEOS)
    //   (no se verifica - Grupo3 (TODO)
    // --> en ese caso borramos red wifi almacenada en el ESP8266
    if (testButton(bENCODER, OFF) && testButton(bGOTEOS,ON)) wifiSW = true;
    else wifiSW = false;
    if(wifiSW) {
      Serial.println("encoderSW pulsado y multirriego en GOTEOS --> borramos red WIFI");
      WiFi.disconnect(); //borra wifi guardada
      wifiClearSignal(5);
      led(LEDR,ON);
    }
    // explicitly set mode, esp defaults to STA+AP   
    WiFi.mode(WIFI_STA); 
    // Creamos una instancia de la clase WiFiManager
    WiFiManager wm;
    // Descomentar para resetear configuración
    //wm.resetSettings();
    // Empezamos el temporizador que hará parpadear el LED indicador de wifi
    tic_WifiLed.attach(0.2, parpadeoLedWifi);
    //sets timeout until configuration portal gets turned off
    wm.setConfigPortalTimeout(180);
    // callbacks
    wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveWifiCallback);
    // activamos modo AP y portal cautivo y comprobamos si se establece la conexión
    if(!wm.autoConnect("Ardomo")){
      Serial.println("Fallo en la conexión (timeout)");
      falloAP = true;
      WiFi.mode(WIFI_STA); 
      //ESP.reset();
      delay(1000);
    }
    /* 
     * Podemos continuar hasta aqui por tres razones:
     *   - nos hemos conectado a la red wifi almacenada
     *   - nos hemos podido conectara a la red wifi que hemos introducido en la web de configuracion
     *   - no nos hemos podido conectar a la red wifi almacenada o no habia y el modo configuracion ha 
     *     dado timeout
     */
    // Eliminamos el temporizador y dejamos LEDB segun estado de NONETWORK
    tic_APLed.detach();
    NONETWORK ? led(LEDB,ON) : led(LEDB,OFF);

    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.printf("Wifi conectado a SSID: %s\n", WiFi.SSID().c_str());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
      // Eliminamos el temporizador y encendemos el led indicador de wifi
      tic_WifiLed.detach();
      led(LEDG,ON);
      connected = true;
    }
    else {
      // apagamos el LED indicador de wifi
      tic_WifiLed.detach();
      led(LEDG,OFF);
      connected = false;
    }
  #endif
}

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
    // apagamos el LED indicador de wifi
    Serial.println("[ERROR] No estamos conectados a la wifi");
    led(LEDG,OFF);
    connected = false;
    return false;
  }
}


