#define __MAIN__
#include <Control.h>
#include <EEPROM.h>
#include <wifissid.h>

S_BOTON *boton;
S_BOTON *ultimoBoton;
bool ret;

#ifdef MEGA256
  EthernetUDP ntpUDP;
#endif

#ifdef NODEMCU
  WiFiUDP ntpUDP;
  ESP8266WiFiMulti WiFiMulti;
#endif

NTPClient timeClient(ntpUDP,"192.168.100.60");

TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);
TimeChangeRule *tcr;
time_t utc;

void timerIsr()
{
  Encoder->service();
}

void initEeprom() {
  int i,botonAddr;
  bool eeinitialized;

  #ifdef NODEMCU
    EEPROM.begin(sizeof(__eeprom_data));
  #endif
  EEPROM.get(0,eeinitialized);
  botonAddr = offsetof(__eeprom_data, botonIdx[0]);
  if( eeinitialized == 0 || FORCEINITEEPROM == 1) {
    Serial.println("Inicializando la EEPROM");
    for(i=0;i<16;i++) {
      EEPROM.put(botonAddr,Boton[i].idx);
      botonAddr += 2;
    }
    EEPROM.put(offsetof(__eeprom_data, minutes),DEFAULTMINUTES);
    EEPROM.put(offsetof(__eeprom_data, seconds),DEFAULTSECONDS);
    #ifdef NODEMCU
      EEPROM.commit();
    #endif
  }
  else {
    Serial.println("Leyendo valores de la EEPROM");
    for(i=0;i<16;i++) {
      EEPROM.get(botonAddr,Boton[i].idx);
      //Serial << "leido boton " << i << " : " << Boton[i].idx << " address: " << botonAddr << endl;
      botonAddr += sizeof(Boton[i].idx);
    }
    EEPROM.get(offsetof(__eeprom_data, minutes),minutes);
    EEPROM.get(offsetof(__eeprom_data, seconds),seconds);
    //Serial << "READ EEPROM TIME, minutes: " << minutes << " seconds: " << seconds << endl;
    value = ((seconds==0)?minutes:seconds);
    StaticTimeUpdate();
  }
}

void setup()
{
  Serial.begin(9600);
  //Para el display
  display = new Display(DISPCLK,DISPDIO);
  display->print("----");
  //Para el encoder
  Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  #ifdef MEGA256
    Timer1.initialize(1000);
    Timer1.attachInterrupt(timerIsr);
  #endif
  #ifdef NODEMCU
    WiFiMulti.addAP(ssid,pass);
    while(WiFiMulti.run() != WL_CONNECTED) {
       Serial.print(".");
       delay(200);
    }
    Serial.println("");
    Serial.println("WiFi connectado");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(200);
  #endif
  //Para el Configure le paso encoder y display porque lo usara.
  configure = new Configure(display);
  //Para el BUZZER
  pinMode(BUZZER, OUTPUT);
  //Para el CD4021B
  initCD4021B();
  //Para el 74HC595
  initHC595();
  //Para la red
  delay(1000);
  #ifdef NET_MQTTCLIENT
    MqttClient.setClient(client);
    MqttClient.setServer(MQTT_SERVER,1883);
  #endif
  #ifdef MEGA256
    Ethernet.begin(mac,ip,gateway,subnet);
  #endif
  delay(1500);
  //Ponemos en hora
  timeClient.begin();
  timeClient.update();
  setTime(timeClient.getEpochTime());
  //Estado inicial
  Estado.estado = STANDBY;
  //Inicializo la EEPROM
  initEeprom();
  //Iniciamos lastRiegos
  for(int i=0;i<5;i++) {
    lastRiegos[i] = 0;
  }
  //Deshabilitamos el hold de Pause
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
  bip(1);
}

void ultimosRiegos(int modo)
{
  switch(modo) {
    case SHOW:
      time_t t;
      utc = timeClient.getEpochTime();
      t = CE.toLocal(utc,&tcr);
      display->printTime(hour(t),minute(t));
      for(int i=0;i<5;i++) {
        if(lastRiegos[i] > previousMidnight(t)) {
            led(Boton[bId2bIndex(COMPLETO[i])].led,ON);
        }
      }
      break;
    case HIDE:
      StaticTimeUpdate();
      for(unsigned int i=0;i<sizeof(COMPLETO)/2;i++) {
        led(Boton[bId2bIndex(COMPLETO[i])].led,OFF);
      }
      break;
  }
}

void loop()
{
  procesaBotones();
  procesaEstados();
}

void procesaBotones()
{
  //Procesamos los botones
  if (!multiSemaforo) {
    //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multiriego
    boton = NULL;
    boton = parseInputs();
  }
  else {
    multiSemaforo = false;
  }

  if(boton != NULL) {
    //Si estamos en reposo solo nos saca de ese estado
    if (reposo && boton->id != bSTOP) {
      Serial.println("Salimos de reposo");
      reposo = false;
      StaticTimeUpdate();
    }
    else if (boton->flags.action) {
      switch (boton->id) {
        //Primero procesamos los botones singulares, el resto van por default
        case bPAUSE:
          if (Estado.estado != STOP) {
            //No procesamos los release del boton salvo en STOP
            if(!boton->estado) break;
            switch (Estado.estado) {
              case REGANDO:
                bip(1);
                Estado.estado = PAUSE;
                if(ultimoBoton) stopRiego(ultimoBoton->id);
                T.PauseTimer();
                break;
              case PAUSE:
                bip(2);
                if(ultimoBoton) initRiego(ultimoBoton->id);
                T.ResumeTimer();
                Estado.estado = REGANDO;
                break;
              case CONFIGURANDO:
                if(!configure->configuringIdx() && !configure->configuringTime()) {
                  configure->configureTime();
                  delay(500);
                  boton = NULL;
                }
                break;
              case STANDBY:
                ultimosRiegos(SHOW);
                boton = NULL;
                delay(3000);
                ultimosRiegos(HIDE);
            }
          }
          else {
            //Procesamos el posible hold del boton pause
            if(boton->estado) {
              if(!holdPause) {
                countHoldPause = millis();
                holdPause = true;
              }
              else {
                if((millis() - countHoldPause) > HOLDTIME) {
                  configure->start();
                  longbip(1);
                  Estado.estado = CONFIGURANDO;
                  boton = NULL;
                  holdPause = false;
                }
              }
            }
            //Si lo hemos soltado quitamos holdPause
            else holdPause = false;
          }
          break;
        case bSTOP:
          if (boton->estado) {
            if (Estado.estado == REGANDO || Estado.estado == MULTIREGANDO || Estado.estado == PAUSE) {
              display->print("stop");
              stopAllRiego();
              T.StopTimer();
              bip(3);
              display->blink(DEFAULTBLINK);
              multiriego = false;
              multiSemaforo = false;
              Estado.estado = STOP;
            }
            else {
              //Lo hemos pulsado en standby
              bip(3);
              display->print("stop");
              stopAllRiego();
              reposo = false;
              Estado.estado = STOP;
            }
          }

          if (!boton->estado && (Estado.estado == STOP || Estado.estado == CONFIGURANDO)) {
            StaticTimeUpdate();
            led(Boton[bId2bIndex(bCONFIG)].led,OFF);
            Estado.estado = STANDBY;
          }
          standbyTime = millis();
          break;
        case bMULTIRIEGO:
          if (Estado.estado == STANDBY && !multiriego) {
            bip(4);
            uint16_t multiStatus = getMultiStatus();
            multiriego = true;
            multi.actual = 0;
            switch(multiStatus) {
              case bCOMPLETO:
                multi.serie = COMPLETO;
                multi.size = sizeof(COMPLETO)/2;
                multi.id = bCOMPLETO;
                strcpy((char *)"COMPLETO",multi.desc);
                break;
              case bCESPED:
                multi.serie = CESPED;
                multi.size = sizeof(CESPED)/2;
                multi.id = bCESPED;
                strcpy((char *)"CESPED",multi.desc);
                break;
              case bGOTEOS:
                multi.serie = GOTEOS;
                multi.size = sizeof(GOTEOS)/2;
                multi.id = bGOTEOS;
                strcpy((char *)"GOTEOS",multi.desc);
                break;
            }
            //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
            led(Boton[bId2bIndex(multi.id)].led,ON);
            boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
          }
          //Aqui no hay break para que riegue
        default:
          if (Estado.estado == STANDBY) {
            bip(2);
            T.SetTimer(0,minutes,seconds);
            T.StartTimer();
            ultimoBoton = boton;
            initRiego(boton->id);
            //Estado.estado = REGANDO;
          }
          //Configuramos el idx
          if (Estado.estado == CONFIGURANDO) {
            savedValue = value;
            configure->configureIdx(bId2bIndex(boton->id));
            value = boton->idx;
          }
      }
    }
  }
}

void procesaEstados()
{
  switch (Estado.estado) {
    case CONFIGURANDO:
      if (boton != NULL) {
        if (boton->flags.action) {
          switch(boton->id) {
            case bMULTIRIEGO:
              break;
            case bPAUSE:
              if(!boton->estado) return;
              if(configure->configuringTime()) {
                uint8_t m,s;
                Serial << "SAVE EEPROM TIME, minutes: " << minutes << " seconds: " << seconds << endl;
                EEPROM.put(offsetof(__eeprom_data, minutes),minutes);
                EEPROM.put(offsetof(__eeprom_data, seconds),seconds);
                #ifdef NODEMCU
                  EEPROM.commit();
                #endif
                longbip(2);
                EEPROM.get(offsetof(__eeprom_data, minutes),m);
                EEPROM.get(offsetof(__eeprom_data, seconds),s);
                Serial << "OFFm: " << offsetof(__eeprom_data, minutes) << " OFFs: " << offsetof(__eeprom_data, seconds) << endl;
                Serial << "READ EEPROM TIME, minutes: " << m << " seconds: " << s << endl;
                configure->stop();
              }
              if(configure->configuringIdx()) {
                Serial << "SAVE EEPROM IDX" << endl;
                Boton[configure->getActualIdxIndex()].idx = value;
                EEPROM.put(offsetof(__eeprom_data, botonIdx[0]) + 2*configure->getActualIdxIndex(),value);
                #ifdef NODEMCU
                  EEPROM.commit();
                #endif
                value = savedValue;
                longbip(2);
                configure->stop();
              }
              break;
            case bSTOP:
              if(!boton->estado) {
                configure->stop();
                Estado.estado = STANDBY;
                standbyTime = millis();
              }
              break;
          }
        }
      }
      else procesaEncoder();
      break;
    case ERROR:
      display->blink(DEFAULTBLINK);
      break;
    case REGANDO:
      tiempoTerminado = T.Timer();
      if (T.TimeHasChanged()) refreshDisplay();
      if (tiempoTerminado == 0) {
        Estado.estado = TERMINANDO;
      }
      break;
    case STANDBY:
      Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
      //Apagamos el display si ha pasado el lapso
      if (reposo) standbyTime = millis();
      else {
        if (millis() > standbyTime + (1000 * STANDBYSECS) ) {
          Serial.println("Entramos en reposo");
          reposo = true;
          display->clearDisplay();
        }
      }
      procesaEncoder();
      break;
    case TERMINANDO:
      bip(5);
      stopRiego(ultimoBoton->id);
      display->blink(DEFAULTBLINK);
      led(Boton[bId2bIndex(ultimoBoton->id)].led,OFF);
      StaticTimeUpdate();
      standbyTime = millis();
      reposo = false;
      if(Estado.estado != ERROR) Estado.estado = STANDBY;

      //Comprobamos si estamos en un multiriego
      if (multiriego) {
        multi.actual++;
        if (multi.actual < multi.size) {
          boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
          multiSemaforo = true;
        }
        else {
          longbip(3);
          multiriego = false;
          multiSemaforo = false;
          Serial.println("MULTIRIEGO TERMINADO");
          led(Boton[bId2bIndex(multi.id)].led,OFF);
        }
      }
      break;
    case STOP:
      //En stop activamos el comportamiento hold de pausa
      Boton[bId2bIndex(bPAUSE)].flags.holddisabled = false;
      break;
    case PAUSE:
      blinkPause();
      break;
  }
}

void procesaEncoder()
{
  #ifdef NODEMCU
    Encoder->service();
  #endif
  if(Estado.estado == CONFIGURANDO && configure->configuringIdx()) {
      value -= Encoder->getValue();
      if (value > 1000) value = 1000;
      if (value <  1) value = 1;
      display->print(value);
      return;
  }

  if (Estado.estado == CONFIGURANDO && !configure->configuringTime()) return;

  if(!reposo) StaticTimeUpdate();
  value -= Encoder->getValue();
  //Estamos en el rango de minutos
  if(seconds == 0 && value>0) {
    if (value > MAXMINUTES)  value = MAXMINUTES;
    if (value != minutes) {
      minutes = value;
    } else return;
  }
  else {
    //o bien estamos en el rango de segundos o acabamos de entrar en el
    if(value<60 && value>=MINSECONDS) {
      if (value != seconds) {
        seconds = value;
      } else return;
    }
    else if (value >=60) {
      value = minutes = 1;
      seconds = 0;
    }
    else if(minutes == 1) {
      value = seconds = 59;
      minutes = 0;
    }
    else
    {
      value = seconds = MINSECONDS;
      minutes = 0;
    }
  }
  reposo = false;
  StaticTimeUpdate();
  standbyTime = millis();
}

void initRiego(uint16_t id)
{
  //Esta funcion mandara el mensaje a domoticz de activar el boton
  int index = bId2bIndex(id);
  time_t t;

  Serial << "Iniciando riego: " << Boton[index].desc << endl;
  led(Boton[index].led,ON);
  for (int i=0;i<5;i++) {
    if(COMPLETO[i] == id) {
      utc = timeClient.getEpochTime();
      t = CE.toLocal(utc,&tcr);
      lastRiegos[i] = t;
    }
  }
  domoticzSwitch(Boton[index].idx,(char *)"On");
}

void stopRiego(uint16_t id)
{
  //Esta funcion mandara el mensaje a domoticz de desactivar el boton
  int index = bId2bIndex(id);
  domoticzSwitch(Boton[index].idx,(char *)"Off");
}

void stopAllRiego()
{
  //Esta funcion pondra a off todos los botones
  //Apago los leds de multiriego
  led(Boton[bId2bIndex(multi.id)].led,OFF);
  //Apago los leds de riego
  for(unsigned int i=0;i<sizeof(COMPLETO)/2;i++) {
    led(Boton[bId2bIndex(COMPLETO[i])].led,OFF);
    stopRiego(COMPLETO[i]);
  }
}

void bip(int veces)
{
  for (int i=0; i<veces;i++) {
    analogWrite(BUZZER, 255);
    delay(50);
    analogWrite(BUZZER, 0);
    delay(50);
  }
}

void longbip(int veces)
{
  for (int i=0; i<veces;i++) {
    analogWrite(BUZZER, 255);
    delay(750);
    analogWrite(BUZZER, 0);
    delay(100);
  }
}

void blinkPause()
{
  if (!displayOff) {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      display->clearDisplay();
      displayOff = true;
      lastBlinkPause = millis();
    }
  }
  else {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      refreshDisplay();
      displayOff = false;
      lastBlinkPause = millis();
    }
  }
}

void StaticTimeUpdate(void)
{
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;
  display->printTime(minutes, seconds);
}

void refreshDisplay()
{
  display->printTime(T.ShowMinutes(),T.ShowSeconds());
}

void domoticzSwitch(int idx,char *msg)
{
  #ifdef NODEMCU
  int i=0;
  //Comprobamos si estamos conectados, error en caso contrario
  while(WiFiMulti.run() != WL_CONNECTED) {
    i++;
    if(i<5) continue;
    else {
      Serial.println("Error: No estamos conectados a la wifi");
      display->print("Err1");
      Estado.estado = ERROR;
      return;
    }
  }
  #endif

  #ifdef NET_DIRECT
    char JSONMSG[200]="GET /json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s HTTP/1.1\r\n\r\n";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    //Serial.println(message);
    if (!client.available())
    {
      clientConnect();
    }
    Serial.println(message);
    client.println(message);
    char c = client.read();
    Serial.println(c)
    client.stop();
  #endif

  #ifdef NET_HTTPCLIENT
    char JSONMSG[200]="/json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    //Serial.println(message);
    String response;
    int statusCode = 0;
    #ifdef MEGA256
      httpclient.get(message);
      statusCode = httpclient.responseStatusCode();
      response = httpclient.responseBody();
      if(statusCode != 200){
        Serial.print("Status code: ");
        Serial.println(statusCode);
        display->print("Err2");
        Estado.estado = ERROR;
        httpclient.stop();
        return;
      }
    #endif

    #ifdef NODEMCU
      String tmpStr = "http://" + String(serverAddress) + ":8080" + String(message);
      httpclient.begin(tmpStr);
      int httpCode = httpclient.GET();
      if(httpCode > 0) {
        if(httpCode == HTTP_CODE_OK) {
          response = httpclient.getString();
          Serial.println(response);
        }
      }
      else {
        Estado.estado = ERROR;
        display->print("Err3");
        Serial.printf("[HTTP] GET... failed, error: %s\n", httpclient.errorToString(httpCode).c_str());
        return;
      }
    #endif

    int pos = response.indexOf("\"status\" : \"ERR\"");
    if(pos != -1) {
      Serial.println("SE HA DEVUELTO ERROR");
      display->print("Err4");
      Estado.estado = ERROR;
      return;
    }

    #ifdef MEGA256
      httpclient.stop();
    #endif

    #ifdef NODEMCU
      httpclient.end();
    #endif

    if(Estado.estado != ERROR && strcmp(msg,"On") == 0) Estado.estado = REGANDO;
  #endif

  #ifdef NET_MQTTCLIENT
    if (!MqttClient.connected()){
      mqttReconnect();
    }
    MqttClient.publish("out","HOLA");
    //MqttClient.loop();
  #endif
}

#ifdef NET_DIRECT
void clientConnect()
{
  while(1)
  {
    delay(1000);
    Serial.println("Conectando a domoticz ...");
    if (client.connect(server,port)) {
      Serial.println("conectado");
      break;
    }
    else {
      Serial.println("Conexion fallida");
      client.stop();
    }
  }
}
#endif

#ifdef NET_MQTTCLIENT
void mqttReconnect()
{
  // Loop until we're reconnected
  while (!MqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MqttClient.connect("arduinoClient")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      MqttClient.publish("outTopic","hello world");
      // ... and resubscribe
      MqttClient.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(MqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
#endif
