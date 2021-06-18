#define __MAIN__
#include "Control.h"
#include <EEPROM.h>
#include <wifissid.h>

S_BOTON *boton;
S_BOTON *ultimoBoton;
bool ret;

#ifdef RELEASE
 bool NONETWORK=false;
#endif

#ifdef DEVELOP
 bool NONETWORK=false;
#endif

#ifdef DEMO
 bool NONETWORK=true;
#endif

#ifdef MEGA256
  EthernetUDP ntpUDP;
#endif

#ifdef NODEMCU
  WiFiUDP ntpUDP;
  ESP8266WiFiMulti WiFiMulti;
  bool connected;
#endif

NTPClient timeClient(ntpUDP,"192.168.100.60");

TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);
TimeChangeRule *tcr;
time_t utc;
// Initial check
void check(void)
{
  initLeds();
  display->check(2);
}
//CheckBuzzer
void checkBuzzer(void)
{

}
//Para JSON
StaticJsonBuffer<2000> jsonBuffer;

uint idarrayRiego(uint16_t id)
{
  for (uint i=0;i<NUMRIEGOS;i++) {
    if(COMPLETO[i] == id) return i;
  }
  return 999;
}

void timerIsr()
{
  Encoder->service();
}

void initEeprom() {
  int i,botonAddr;
  bool eeinitialized=0;

  #ifdef NODEMCU
    EEPROM.begin(sizeof(__eeprom_data));
  #endif
  EEPROM.get(0,eeinitialized);
  botonAddr = offsetof(__eeprom_data, botonIdx[0]);
  #ifdef DEBUG
    Serial << endl;
    Serial << "sizeof(__eeprom_data)= " << sizeof(__eeprom_data) << endl;
    Serial << "eeinitialized= " << eeinitialized << endl;
    Serial << "FORCEINITEEPROM= " << FORCEINITEEPROM << endl;
    Serial << "boton0 offset= " << botonAddr << endl;
  #endif
  if( eeinitialized == 0 || FORCEINITEEPROM == 1) {
    Serial.println("Inicializando la EEPROM");
    for(i=0;i<16;i++) {
      EEPROM.put(botonAddr,Boton[i].idx);
      Serial << "escribiendo boton " << i << " : " << Boton[i].idx << " address: " << botonAddr << endl;
      botonAddr += 2;
    }
    uint8_t mr,sr;
    minutes = DEFAULTMINUTES;
    seconds = DEFAULTSECONDS;
    value = ((seconds==0)?minutes:seconds); //para que use esos valores
    Serial << "SAVE EEPROM TIME, minutes: " << minutes << " seconds: " << seconds << endl;
    EEPROM.put(offsetof(__eeprom_data, minutes),minutes);
    EEPROM.put(offsetof(__eeprom_data, seconds),seconds);
    EEPROM.put(offsetof(__eeprom_data, initialized),(uint8_t)1);   // marca la eeprom como inicializada
    #ifdef NODEMCU
      bool bRc = EEPROM.commit();
      if(bRc) Serial.println("Write eeprom successfully");
      else    Serial.println("Write eeprom error");
    #endif                
    EEPROM.get(offsetof(__eeprom_data, minutes),mr);
    EEPROM.get(offsetof(__eeprom_data, seconds),sr);
    #ifdef DEBUG
      uint16_t boton0;  //dbg
      uint16_t boton1;  //dbg
      int boton0Addr, boton1Addr; //dbg    
      Serial << "OFFinitialized: " << offsetof(__eeprom_data, initialized) << endl;
      boton0Addr = offsetof(__eeprom_data, botonIdx[0]);  //dbg
      EEPROM.get(offsetof(__eeprom_data, botonIdx[0]), boton0);  //dbg
      boton1Addr = offsetof(__eeprom_data, botonIdx[1]);  //dbg
      EEPROM.get(offsetof(__eeprom_data, botonIdx[1]), boton1);  //dbg
      Serial << "leido boton 0 : " << boton0 << " address: " << boton0Addr << endl;  //dbg
      Serial << "leido boton 1 : " << boton1 << " address: " << boton1Addr << endl;  //dbg
      Serial << "OFFm: " << offsetof(__eeprom_data, minutes) << " OFFs: " << offsetof(__eeprom_data, seconds) << endl;
      Serial << "READ EEPROM TIME, minutes: " << mr << " seconds: " << sr << endl;
    #endif
  }
  else {
    Serial.println("Leyendo valores de la EEPROM");
    for(i=0;i<16;i++) {
      EEPROM.get(botonAddr,Boton[i].idx);
      Serial << "leido boton " << i << " : " << Boton[i].idx << " address: " << botonAddr << endl;
      botonAddr += sizeof(Boton[i].idx);
    }
    EEPROM.get(offsetof(__eeprom_data, minutes),minutes);
    EEPROM.get(offsetof(__eeprom_data, seconds),seconds);
    Serial << "READ EEPROM TIME, minutes: " << minutes << " seconds: " << seconds << endl;
    value = ((seconds==0)?minutes:seconds);
    StaticTimeUpdate();
  }
}

void initFactorRiegos()
{
  #ifdef TRACE
    Serial.println("TRACE: in initFactorRiegos");
  #endif

  for(uint i=0;i<NUMRIEGOS;i++) {
    factorRiegos[i]=getFactor(Boton[bId2bIndex(COMPLETO[i])].idx);
  }
  #ifdef VERBOSE
    //Leemos los valores para comprobar que lo hizo bien
    for(uint i=0;i<NUMRIEGOS;i++) {
      Serial.printf("FACTOR %d: %d \n",i,factorRiegos[i]);
    }
  #endif
}

void timeByFactor(int factor,uint8_t *fminutes, uint8_t *fseconds)
{
  //Aqui convertimos minutes y seconds por el factorRiegos
  uint tseconds = (60*minutes) + seconds;
  //factorizamos
  tseconds = (tseconds*factor)/100;
  //reconvertimos
  *fminutes = tseconds/60;
  *fseconds = tseconds%60;
}


void setup()
{
  Serial.begin(9600);
  Serial.println(" ");
  Serial.println("CONTROL RIEGO V2");
  #ifdef TRACE
    Serial.println("TRACE: in setup");
  #endif
  //Para el display
  #ifdef DEBUG
   Serial.println("Inicializando display");
  #endif
  display = new Display(DISPCLK,DISPDIO);
  #ifdef DEBUG
   Serial.println("Display inicializado");
  #endif
  display->print("----");
  //Para el encoder
  #ifdef DEBUG
   Serial.println("Inicializando Encoder");
  #endif
   display->print("----");
  Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  #ifdef MEGA256
    Timer1.initialize(1000);
    Timer1.attachInterrupt(timerIsr);
  #endif
  //Para el Configure le paso encoder y display porque lo usara.
  #ifdef DEBUG
   Serial.println("Inicializando Configure");
  #endif
  display->print("----");
  configure = new Configure(display);
  //Para el BUZZER
  //pinMode(BUZZER, OUTPUT);
  //Para el CD4021B
  initCD4021B();
  //Para el 74HC595
  initHC595();
  //led encendido
  led(LEDR,ON);
  //Chequeo de perifericos de salida
  check();
  //Para la red
  delay(1000);
  #ifdef NET_MQTTCLIENT
    MqttClient.setClient(client);
    MqttClient.setServer(MQTT_SERVER,1883);
  #endif
  #ifdef MEGA256
    Ethernet.begin(mac,ip,gateway,subnet);
  #endif
  #ifdef NODEMCU
    for (int i=0;i < sizeof(ssid)/sizeof(ssid[0]); i++) {
      int j=0;
      WiFiMulti.addAP(ssid[i],pass);
      #ifdef DEBUG
              Serial.println("");
              Serial.print("Intentando conectar a:   ");
              Serial.print(ssid[i]);
      #endif
      connected = true;
      while(WiFiMulti.run() != WL_CONNECTED) {
        led(LEDG,ON);
        Serial.print(".");
        delay(200);
        led(LEDG,OFF);
        delay(200);
        j++;
        if(j == MAXCONNECTRETRY) {
          connected = false;
          break;
        }
      }
      if(connected) {
        Serial.println("");
        Serial.printf("Wifi conectado a SSID: %s\n", WiFi.SSID().c_str());
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        led(LEDG,ON);
        break;
      }
    }
  #endif
  delay(1500);
  //Ponemos en hora
  timeClient.begin();
  timeClient.update();
  setTime(timeClient.getEpochTime());
  //Inicializo la EEPROM
  initEeprom();
  //Iniciamos lastRiegos
  for(uint i=0;i<NUMRIEGOS;i++) {
    lastRiegos[i] = 0;
  }
  //Iniciamos factorRTiegos
  initFactorRiegos();
  //Deshabilitamos el hold de Pause
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
  if(!connected) {
    Serial.println("ERROR DE CONEXION WIFI");
    if(!NONETWORK) {
      Estado.estado = ERROR;
      display->print("Err9");
      longbip(3);
    }
    // Si estamos en modo NONETWORK pasamos a STANDBY aunque no exista conexión wifi y
    // encendemos led wifi en color azul
    else {
      Estado.estado = STANDBY;
      bip(2);
      led(LEDB,ON);
    }
  }
  else {
    Estado.estado = STANDBY;
    bip(1);
  }
  standbyTime = millis();
  reposo = false;
  //Llamo a parseInputs una vez para eliminar prepulsaciones antes del bucle loop
  parseInputs();
}

void ultimosRiegos(int modo)
{
  switch(modo) {
    case SHOW:
      time_t t;
      utc = timeClient.getEpochTime();
      t = CE.toLocal(utc,&tcr);
      for(uint i=0;i<NUMRIEGOS;i++) {
        if(lastRiegos[i] > previousMidnight(t)) {
            led(Boton[bId2bIndex(COMPLETO[i])].led,ON);
        }
      }
      display->printTime(hour(t),minute(t));
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
  #ifdef EXTRATRACE
    Serial.println("TRACE: in loop");
  #endif

  procesaBotones();
  procesaEstados();
}

void procesaBotones()
{
  #ifdef EXTRATRACE
    Serial.println("TRACE: in procesaBotones");
  #endif
  //Procesamos los botones
  if (!multiSemaforo) {
    //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multiriego
    boton = NULL;
    boton = parseInputs();
  }
  else {
    multiSemaforo = false;
  }

  //En estado error salimos a menos que pulsemos bPause y pasamos a modo NONETWORK
  if(Estado.estado == ERROR) 
  {
    if(boton != NULL) {
      //Si estamos en error y pulsamos pausa, nos ponemos en estado NONETWORK para test
      if(boton->id == bPAUSE) {
        Estado.estado = STANDBY;
        NONETWORK = true;
        Serial.println("estado en ERROR y PAUSA pulsada pasamos a modo NONETWORK y reseteamos");
        bip(2);
        led(LEDB,ON);
        //TODO: ¿es necesario mas reseteo de estados o leds?
        multiriego = false;
        multiSemaforo = false;
        StaticTimeUpdate();
      }
    }
    return;
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
                if(Estado.estado != ERROR) Estado.estado = REGANDO; // para que no borre ERROR
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
            //De alguna manera esta regando y hay que parar
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
              //Lo hemos pulsado en standby - seguro antinenes
              bip(3);
              display->print("stop");
              stopAllRiego();
              reposo = false;
              Estado.estado = STOP;
            }
          }
          //Salimos del estado stop o de configuracion
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
                strcpy(multi.desc,(char *)"COMPLETO");
                break;
              case bCESPED:
                multi.serie = CESPED;
                multi.size = sizeof(CESPED)/2;
                multi.id = bCESPED;
                strcpy(multi.desc,(char *)"CESPED");
                break;
              case bGOTEOS:
                multi.serie = GOTEOS;
                multi.size = sizeof(GOTEOS)/2;
                multi.id = bGOTEOS;
                strcpy(multi.desc,(char *)"GOTEOS");
                break;
            }
            //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
            //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
            Serial.printf("MULTIRRIEGO iniciado: %s \n", multi.desc);
            led(Boton[bId2bIndex(multi.id)].led,ON);
            boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
          }
          //Aqui no hay break para que riegue
        default:
          if (Estado.estado == STANDBY) {
            bip(2);
            //cambia minutes y seconds en funcion del factor de cada sector de riego
            uint8_t fminutes=0,fseconds=0;
            if(multiriego) {
              timeByFactor(factorRiegos[idarrayRiego(boton->id)],&fminutes,&fseconds);
            }
            else {
              fminutes = minutes;
              fseconds = seconds;
            }
            //
            T.SetTimer(0,fminutes,fseconds);
            T.StartTimer();
            ultimoBoton = boton;
            initRiego(boton->id);
            #ifdef DEBUG
              Serial.printf("Minutos: %d Segundos: %d FMinutos: %d FSegundos: %d\n",minutes,seconds,fminutes,fseconds);
            #endif
            if(Estado.estado != ERROR) Estado.estado = REGANDO; // para que no borre ERROR
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
  #ifdef EXTRATRACE
    Serial.println("TRACE: in procesaEstados");
  #endif

  switch (Estado.estado) {
    case CONFIGURANDO:
      if (boton != NULL) {
        if (boton->flags.action) {
          Serial << "En estado CONFIGURANDO pulsado ACTION" << endl;
          switch(boton->id) {
            case bMULTIRIEGO:
              break;
            case bPAUSE:
              if(!boton->estado)
              {
                Serial << "Release de Pause" << endl;
                return;
              }
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
      blinkPause();
      break;
    case REGANDO:
      tiempoTerminado = T.Timer();
      if (T.TimeHasChanged()) refreshTime();
      if (tiempoTerminado == 0) {
        Estado.estado = TERMINANDO;
      }
      break;
    case STANDBY:
      Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
      //Apagamos el display si ha pasado el lapso y no estamos en error
      if (reposo) standbyTime = millis();
      else {
        if (millis() > standbyTime + (1000 * STANDBYSECS) && Estado.estado != ERROR ) {
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
          //Simular la pulsacion del siguiente boton de la serie de multiriego
          boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
          multiSemaforo = true;
        }
        else {
          longbip(3);
          multiriego = false;
          multiSemaforo = false;
            Serial.printf("MULTIRRIEGO %s terminado \n", multi.desc);
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
      //Serial << "En procesaencoder idx";
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
  uint arrayIndex = idarrayRiego(id);
  time_t t;

  if(arrayIndex == 999) return;
  Serial << "Iniciando riego: " << Boton[index].desc << endl;
  led(Boton[index].led,ON);
  utc = timeClient.getEpochTime();
  t = CE.toLocal(utc,&tcr);
  lastRiegos[arrayIndex] = t;
  domoticzSwitch(Boton[index].idx,(char *)"On");
  #ifdef DEBUG
    Serial.print("initRiego acaba en estado: ");
    Serial.println(Estado.estado);
  #endif
}

void stopRiego(uint16_t id)
{
  //Esta funcion mandara el mensaje a domoticz de desactivar el boton
  int index = bId2bIndex(id);
  #ifdef DEBUG
  Serial << "Terminando riego: " << Boton[index].desc << endl;
  #endif
  domoticzSwitch(Boton[index].idx,(char *)"Off");
  if (Estado.estado != ERROR) Serial << "Terminado OK riego: " << Boton[index].desc << endl;
  #ifdef DEBUG
    Serial.print("stopRiego acaba en estado: ");
    Serial.println(Estado.estado);
  #endif
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

void checkBuzzzer(void)
{

}

void bip(int veces)
{
  for (int i=0; i<veces;i++) {
    led(BUZZER,ON);
    delay(50);
    led(BUZZER,OFF);
    delay(50);
  }
}

void longbip(int veces)
{
  for (int i=0; i<veces;i++) {
    led(BUZZER,ON);
    delay(750);
    led(BUZZER,OFF);
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

// modificacion 1 para que en caso de estado ERROR no borre el código de error del display
void StaticTimeUpdate(void)
{
  if (Estado.estado == ERROR) return;
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;
  display->printTime(minutes, seconds);
}

void refreshDisplay()
{
  display->refreshDisplay();
  //display->printTime(T.ShowMinutes(),T.ShowSeconds());
}

void refreshTime()
{
  display->printTime(T.ShowMinutes(),T.ShowSeconds());
}

bool checkWifiConnected()
{
  #ifdef TRACE
    Serial.println("TRACE: in checkWifiConnected");
  #endif
  int i=0;
  if(NONETWORK) {
     led(LEDB,ON);
     return false;
  }
  //Comprobamos si estamos conectados, error en caso contrario
  while(WiFiMulti.run() != WL_CONNECTED) {
    i++;
    if(i<5) continue;
    else {
      #ifdef DEBUG
        Serial.println("Error: No estamos conectados a la wifi");
      #endif
      display->print("Err1");
      Estado.estado = ERROR;
      led(LEDG,OFF);
      return false;
    }
  }
  led(LEDG,ON);
  return true;
}

int getFactor(uint16_t idx)
{
  #ifdef TRACE
    Serial.println("TRACE: in getFactor");
  #endif
  //Ante cualquier problema devolvemos 100% para no factorizar ese riego
  #ifdef NODEMCU
    if(!checkWifiConnected()) return 100;
  #endif

  char JSONMSG[200]="/json.htm?type=devices&rid=%d";
  char message[250];
  sprintf(message,JSONMSG,idx);
  String response;

  #ifdef NODEMCU
    String tmpStr = "http://" + String(serverAddress) + ":" + DOMOTICZPORT + String(message);
    #ifdef DEBUG
      Serial.print("TMPSTR: ");Serial.println(tmpStr);
    #endif
    httpclient.begin(tmpStr);
    int httpCode = httpclient.GET();
    if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
        response = httpclient.getString();
        #ifdef EXTRADEBUG
          Serial.print("RESPONSE: ");Serial.println(response);
        #endif
      }
    }
    else {
      if(!NONETWORK && Estado.estado != ERROR) {
        Estado.estado = ERROR;
        display->print("Err8");
        #ifdef DEBUG
          Serial.printf("GETFACTOR IDX: %d [HTTP] GET... failed, error: %s\n", idx,httpclient.errorToString(httpCode).c_str());
        #endif
      }
      return 100;
    }
  #endif

  #ifdef MEGA256
    int statusCode = 0;
    httpclient.get(message);
    statusCode = httpclient.responseStatusCode();
    response = httpclient.responseBody();
    if(statusCode != 200){
      #ifdef DEBUG
        Serial.print("Status code: ");
        Serial.println(statusCode);
      #endif
      display->print("Err2");
      Estado.estado = ERROR;
      httpclient.stop();
      return 100;
    }
  #endif

  //Procesamos
  int pos = response.indexOf("\"status\" : \"ERR\"");
  if(pos != -1) {
    if(NONETWORK) return 100;
    #ifdef DEBUG
      Serial.println("SE HA DEVUELTO ERROR");
    #endif
    display->print("Err4");
    Estado.estado = ERROR;
    return 100;
  }

  #ifdef MEGA256
    httpclient.stop();
  #endif

  #ifdef NODEMCU
    httpclient.end();
  #endif

  //if(Estado.estado != ERROR && strcmp(msg,"On") == 0) Estado.estado = REGANDO;

  //Teoricamente ya tenemos en response el JSON, lo procesamos
  //pero acabo de darme cuenta que si preguntamos por un rid como el 0 que devuelve cosas, puede dar una excepcion
  //asi que hay que controlarlo
  const size_t bufferSize = 2*JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(16) + JSON_OBJECT_SIZE(46) + 1500;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject &root = jsonBuffer.parseObject(response);
  if (!root.success()) {
    #ifdef DEBUG
      Serial.println("parseObject() failed");
    #endif
    return 100;
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *factorstr = root["result"][0]["Description"];
  if(factorstr == NULL) {
    //El rid (idx) seguramente era 0 o parecido, así que devolvemos 100
    #ifdef VERBOSE
      Serial.printf("El idx %d no se ha podido leer del JSON\n",idx);
    #endif
    return 100;
  }

  long int factor = strtol(factorstr,NULL,10);
  if(factor == 0L) return 100;
  else return (int)factor;
}

void domoticzSwitch(int idx,char *msg)
{
  #ifdef TRACE
    Serial.println("TRACE: in domoticzSwitch");
  #endif

  #ifdef NODEMCU
    if(!checkWifiConnected()) return;
  #endif

  #ifdef NET_DIRECT
    char JSONMSG[200]="GET /json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s HTTP/1.1\r\n\r\n";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    if (!client.available())
    {
      clientConnect();
    }
    #ifdef DEBUG
      Serial.println(message);
    #endif
    client.println(message);
    char c = client.read();
    #ifdef DEBUG
      Serial.println(c)
    #endif
    client.stop();
  #endif

  #ifdef NET_HTTPCLIENT
    char JSONMSG[200]="/json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    String response;
    #ifdef MEGA256
      int statusCode = 0;
      httpclient.get(message);
      statusCode = httpclient.responseStatusCode();
      response = httpclient.responseBody();
      if(statusCode != 200){
        #ifdef DEBUG
          Serial.print("Status code: ");
          Serial.println(statusCode);
        #endif
        display->print("Err2");
        Estado.estado = ERROR;
        httpclient.stop();
        return;
      }
    #endif

    #ifdef NODEMCU
      String tmpStr = "http://" + String(serverAddress) + ":" + DOMOTICZPORT + String(message);
      httpclient.begin(tmpStr);
      int httpCode = httpclient.GET();
      if(httpCode > 0) {
        if(httpCode == HTTP_CODE_OK) {
          response = httpclient.getString();
          #ifdef EXTRADEBUG
            Serial.println(response);
          #endif
        }
      }
      else {
        if(NONETWORK) return;
        Estado.estado = ERROR;
        display->print("Err3");
        #ifdef DEBUG
          Serial.printf("DOMOTICZSWITH IDX: %d [HTTP] GET... failed, error: %s\n", idx, httpclient.errorToString(httpCode).c_str());
        #endif
        return;
      }
    #endif

    int pos = response.indexOf("\"status\" : \"ERR\"");
    if(pos != -1) {
      if(NONETWORK) return;
      #ifdef DEBUG
        Serial.println("SE HA DEVUELTO ERROR");
      #endif
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
