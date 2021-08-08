#define __MAIN__
#include "Control.h"
#include <EEPROM.h>
//#include <wifissid.h>

S_BOTON *boton;
S_BOTON *ultimoBoton;
bool ret;
bool flagV = OFF;
int ledState = LOW;
bool timeOK = false;
bool factorRiegosOK = false;
bool errorOFF = false;
bool simErrorOFF = false;

#ifdef MEGA256
  EthernetUDP ntpUDP;
#endif

#ifdef NODEMCU
  WiFiUDP ntpUDP;
  //ESP8266WiFiMulti WiFiMulti;
#endif

NTPClient timeClient(ntpUDP,"192.168.100.60");

TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);
TimeChangeRule *tcr;
time_t utc;
Ticker tic_parpadeoLedON;  //para parpadeo led ON (LEDR)
Ticker tic_verificaciones; //para verificaciones periodicas


//Para JSON
StaticJsonBuffer<2000> jsonBuffer;

/*----------------------------------------------*
 *               Setup inicial                  *
 *----------------------------------------------*/
void setup()
{
#ifdef RELEASE
               NONETWORK=false; 
#endif
#ifdef DEVELOP
               NONETWORK=false;
#endif
#ifdef DEMO
               NONETWORK=true;
#endif

  Serial.begin(115200);
  Serial.print("\n\n");
  Serial.println("CONTROL RIEGO V1.3.1");
  Serial.print("Startup reason: ");Serial.println(ESP.getResetReason());
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
  //Para el CD4021B
  initCD4021B();
  //Para el 74HC595
  initHC595();
  //Inicializo la EEPROM (escribo en ella si asi se indica y/o se lee de ella valores configurados)
  procesaEeprom();
  //led encendido
  led(LEDR,ON);
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  //delay(1000);
  //setupRed();
  setupRedWM();
  #ifdef EXTRADEBUG
    Serial.printf("estado LEDR: %d \n",ledStatusId(LEDR));
    Serial.printf("estado LEDG: %d \n",ledStatusId(LEDG));
    Serial.printf("estado LEDB: %d \n",ledStatusId(LEDB));
  #endif
  delay(2000);
  //Ponemos en hora
  timeClient.begin();
  initClock();
  //Inicializamos lastRiegos (registro fecha y riego realizado)
  initLastRiegos();
  //Cargamos factorRiegos
  initFactorRiegos();
  //Deshabilitamos el hold de Pause
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
  //si no estamos conectados a la red y no estamos en modo NONETWORK pasamos a estado ERROR
  if(!connected) {
    Serial.println("[ERROR] Setup: ERROR DE CONEXION WIFI");
    if(!NONETWORK) {
      Estado.estado = ERROR;
      display->print("Err1");
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
  //inicializamos apuntador estructura multi (posicion del selector multirriego):
  multi = getMultibyId(getMultiStatus());
  //lanzamos supervision periodica estado cada 10 seg.
  tic_verificaciones.attach_scheduled(10, flagVerificaciones);
}

/*----------------------------------------------*
 *            Bucle principal                   *
 *----------------------------------------------*/
void loop()
{
  #ifdef EXTRATRACE
    Serial.println("TRACE: in loop");
  #endif

  procesaBotones();
  dimmerLeds();
  procesaEstados();
  dimmerLeds();
  Verificaciones();
}

/*----------------------------------------------*
 *                 Funciones                    *
 *----------------------------------------------*/

void procesaBotones()
{
  #ifdef EXTRATRACE
    Serial.println("TRACE: in procesaBotones");
  #endif
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  if (!Boton[bId2bIndex(bENCODER)].estado) encoderSW = true;
  else encoderSW = false;
  //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multiriego
  if (!multiSemaforo) {
    boton = NULL;
    boton = parseInputs();
  }
  else {
    multiSemaforo = false;
  }
  //Procesamos el boton pulsado:
  if(boton != NULL) {
    //En estado error salimos sin procesar el boton, a menos que pulsemos Pause y pasamos a modo NONETWORK
    // o pulsemos Stop y en este caso reseteamos
    if(Estado.estado == ERROR) 
    {
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
      //Si estamos en ERROR y pulsamos STOP, reseteamos
      if(boton->id == bSTOP) {
        Serial.println("Reset..");
        longbip(3);
        ESP.restart();  
      }
      return;
    }
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
                if(!configure->configuringIdx() && !configure->configuringTime() && !configure->configuringMulti()) {
                  configure->configureTime();
                  delay(500);
                  boton = NULL;
                }
                break;
              case STANDBY:
                  if(encoderSW) {  //si encoderSW+Pause --> conmutamos estado NONETWORK
                    if (NONETWORK) {
                        NONETWORK = false;
                        Serial.println("encoderSW+PAUSE pasamos a modo NORMAL");
                        bip(2);
                        led(LEDB,OFF);
                    }
                    else {
                        NONETWORK = true;
                        Serial.println("encoderSW+PAUSE pasamos a modo NONETWORK (DEMO)");
                        bip(2);
                        led(LEDB,ON);
                    }
                  }
                  else {    // muestra hora y ultimos riegos
                    ultimosRiegos(SHOW);
                    boton = NULL;
                    delay(3000);
                    ultimosRiegos(HIDE);
                  }
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
                  if(!encoderSW) { //pasamos a modo Configuracion
                    configure->start();
                    longbip(1);
                    Estado.estado = CONFIGURANDO;
                    boton = NULL;
                    holdPause = false;
                    #ifdef DEBUG
                      Serial.printf("#01 savedValue: %d  value: %d \n",savedValue,value);
                    #endif
                    savedValue = value;
                  }
                  else {   //si esta pulsado encoderSW hacemos un soft reset
                    Serial.println("Reset..");
                    longbip(3);
                    ESP.restart();  // Hard RESET: ESP.reset()
                  }
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
            if (savedValue>0) {
                #ifdef DEBUG
                Serial.printf("#02 savedValue: %d  value: %d \n",savedValue,value);
                #endif
                value = savedValue;  // para que restaure reloj aunque no salvemos con pause el valor configurado
            }
            StaticTimeUpdate();
            led(Boton[bId2bIndex(bCONFIG)].led,OFF);
            Estado.estado = STANDBY;
          }
          standbyTime = millis();
          break;
        case bMULTIRIEGO:
            //Configuramos el grupo de multirriego activo
          if (Estado.estado == CONFIGURANDO) {
            #ifdef DEBUG
              Serial.printf("#03 savedValue: %d  value: %d \n",savedValue,value);
            #endif
            savedValue = value;
            configure->configureMulti();
            multi = getMultibyId(getMultiStatus()); 
            #ifdef DEBUG
              Serial << "en configuracion de MULTIRRIEGO, getMultibyId devuelve: " << multi->desc << endl;
              Serial << "                                        multi->size: " << multi->size << endl;
            #endif            
            displayGrupo(multi->serie, multi->size);
            multi->size = 0 ; // borramos grupo actual
            display->print("push");
            break;
          }
          if (Estado.estado == STANDBY && !multiriego) {
            bip(4);
            multiriego = true;
            multi = getMultibyId(getMultiStatus());
            multi->actual = 0;
            #ifdef DEBUG
              Serial << "en MULTIRRIEGO, getMultibyId devuelve : " << multi->desc << endl;
              Serial << "                       multi->size: " << multi->size << endl;
              Serial << "en MULTIRRIEGO, encoderSW status  : " << encoderSW << endl;
            #endif
            // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
            if (encoderSW) {
              displayGrupo(multi->serie, multi->size);
              multiriego = false;
              #ifdef DEBUG
                Serial << "en MULTIRRIEGO + encoderSW, display de grupo: " << multi->desc << " tamaño : " << multi->size << endl;
              #endif            
              break;              
            }  
            else {
              //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
              //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
              Serial.printf("MULTIRRIEGO iniciado: %s \n", multi->desc);
              led(Boton[bId2bIndex(multi->id)].led,ON);
              boton = &Boton[bId2bIndex(multi->serie[multi->actual])];
            }
            //Aqui no hay break para que comience multirriego por default
          }
        default:
          if (Estado.estado == STANDBY) {
            if (!encoderSW) {  //iniciamos el riego correspondiente al boton seleccionado
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
                #ifdef DEBUG
                  Serial.printf("Minutos: %d Segundos: %d FMinutos: %d FSegundos: %d\n",minutes,seconds,fminutes,fseconds);
                #endif
                ultimoBoton = boton;
                if (fminutes == 0 && fseconds == 0) {
                  Estado.estado = TERMINANDO; // nos saltamos este riego
                  break;
                }
                T.SetTimer(0,fminutes,fseconds);
                T.StartTimer();
                initRiego(boton->id);
                if(Estado.estado != ERROR) Estado.estado = REGANDO; // para que no borre ERROR
            }
            else {  // mostramos en el display el factor de riego del boton pulsado
              int index = bId2bIndex(boton->id);
              //led(Boton[index].led,ON); //para que funcione encendido led 16
              led(Boton[index].led,ON);
              #ifdef DEBUG
                Serial.printf("Boton: %s Factor de riego: %d \n", boton->desc,factorRiegos[idarrayRiego(boton->id)]);
                Serial.printf("          boton.index: %d \n", index);
                Serial.printf("          boton(%d).led: %d \n", index, Boton[index].led);
              #endif
              #ifdef DEBUG
                Serial.printf("#04 savedValue: %d  value: %d \n",savedValue,value);
              #endif
              savedValue = value;
              value = factorRiegos[idarrayRiego(boton->id)];
              display->print(value);
              delay(2000);
              // boton = NULL;  //--> provoca reset
              #ifdef DEBUG
                Serial.printf("#05 savedValue: %d  value: %d \n",savedValue,value);
              #endif
              value = savedValue;  // para que restaure reloj
              led(Boton[index].led,OFF);
              StaticTimeUpdate();
            }
          }
          if (Estado.estado == CONFIGURANDO ) {
            if (configure->configuringMulti()) {  //Configuramos el multirriego seleccionado
              if (multi->size < 16) {
                    #ifdef DEBUG
                      Serial.printf("#07 savedValue: %d  value: %d \n",savedValue,value);
                    #endif
                    savedValue = value;
                    multi->serie[multi->size] = boton->id;
                    multi->size++;
                    led(Boton[bId2bIndex(boton->id)].led,ON);
              }
            }
            else {          //Configuramos el idx
              #ifdef DEBUG
                Serial.printf("#06 savedValue: %d  value: %d \n",savedValue,value);
              #endif
              savedValue = value;
              configure->configureIdx(bId2bIndex(boton->id));
              value = boton->idx;
            }
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
                #ifdef DEBUG
                  Serial.printf("#08 savedValue: %d  value: %d \n",savedValue,value);
                #endif
                value = savedValue;
                longbip(2);
                configure->stop();
              }
              if(configure->configuringMulti()) {
                Serial << "SAVE EEPROM Multi :" << multi->desc << " tamaño : " << multi->size << endl;
                //graba en la eeprom los 3 grupos
                eepromWriteGroups();
                #ifdef NODEMCU
                  EEPROM.commit();
                #endif
                #ifdef DEBUG
                  Serial.printf("#09 savedValue: %d  value: %d \n",savedValue,value);
                #endif
                value = savedValue;
                ultimosRiegos(HIDE);
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
        multi->actual++;
        if (multi->actual < multi->size) {
          //Simular la pulsacion del siguiente boton de la serie de multiriego
          boton = &Boton[bId2bIndex(multi->serie[multi->actual])];
          multiSemaforo = true;
        }
        else {
          longbip(3);
          multiriego = false;
          multiSemaforo = false;
            Serial.printf("MULTIRRIEGO %s terminado \n", multi->desc);
          led(Boton[bId2bIndex(multi->id)].led,OFF);
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

// Initial check
void check(void)
{
  initLeds();
  display->check(1);
}

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

void procesaEeprom() {
  int i,botonAddr;
  int grupoAddr;
  bool eeinitialized=0;
  n_Grupos = nGrupos();
  //verificamos si encoderSW esta pulsado (estado OFF) y selector de multirriego esta en posicion:
  //   - Grupo1 (CESPED)
  //   (no se verifica - Grupo3 (TODO)
  // --> en ese caso inicializariamos la eeprom
  if (testButton(bENCODER, OFF) && testButton(bCESPED,ON))
  {
    eepromSW = true;
    Serial.println("encoderSW pulsado y multirriego en CESPED  --> Inicializando la EEPROM");
  }
  else eepromSW = false;  

  #ifdef NODEMCU
    EEPROM.begin(sizeof(__eeprom_data) + sizeof(_eeprom_group)*n_Grupos);
  #endif

  EEPROM.get(0,eeinitialized);
  botonAddr = offsetof(__eeprom_data, botonIdx[0]);
  
  #ifdef DEBUG
    Serial << endl;
    Serial << "tamaño de la eeprom : " << sizeof(__eeprom_data) + sizeof(_eeprom_group)*n_Grupos << endl;
    Serial << "eeinitialized= " << eeinitialized << endl;
    Serial << "FORCEINITEEPROM= " << FORCEINITEEPROM << endl;
    Serial << "eepromSW= " << eepromSW << endl;
    Serial << "boton0 offset= " << botonAddr << endl;
  #endif

  if( eeinitialized == 0 || FORCEINITEEPROM == 1 || eepromSW) {
    Serial.println(">>>>>>>>>>>>>>  Inicializando la EEPROM  <<<<<<<<<<<<<<");
    //escribe valores de los IDX de los botones
    for(i=0;i<16;i++) {
      EEPROM.put(botonAddr,Boton[i].idx);
      Serial << "escribiendo boton " << i << " : " << Boton[i].idx << " address: " << botonAddr << endl;
      botonAddr += 2;
    }
    //escribe tiempo de riego por defecto
    minutes = DEFAULTMINUTES;
    seconds = DEFAULTSECONDS;
    Serial << "escrito tiempo riego por defecto, minutos: " << minutes << " segundos: " << seconds << endl;
    EEPROM.put(offsetof(__eeprom_data, minutes),minutes);
    EEPROM.put(offsetof(__eeprom_data, seconds),seconds);
    // marca la eeprom como inicializada
    EEPROM.put(offsetof(__eeprom_data, initialized),(uint8_t)1);   
    //escribe grupos de multirriego y su tamaño
        eepromWriteGroups();
    #ifdef NODEMCU
      bool bRc = EEPROM.commit();
      if(bRc) Serial.println("Write eeprom OK");
      else    Serial.println("Write eeprom error");
    #endif                
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
      Serial << "OFFnumgroups: " << offsetof(__eeprom_data, numgroups) << endl;
    #endif
    //señala la escritura de la eeprom
    if(bRc) {
      eepromWriteSignal(5);
      longbip(3);
    }
  }
  //siempre leemos valores de la eeprom para cargar las variables correspondientes:
  Serial.println("<<<<<<<<<<<<<<<<<<<<<   Leyendo valores de la EEPROM   >>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  //leemos valores de los IDX de los botones
  Serial.println("Leyendo idx de los botones");
  botonAddr = offsetof(__eeprom_data, botonIdx[0]);
  for(i=0;i<16;i++) {
    EEPROM.get(botonAddr,Boton[i].idx);
    #ifdef VERBOSE
      Serial << "leido boton " << i << " idx : " << Boton[i].idx << " address: " << botonAddr << endl;
    #endif
    botonAddr += sizeof(Boton[i].idx);
  }
  //leemos tiempo de riego por defecto
  EEPROM.get(offsetof(__eeprom_data, minutes),minutes);
  EEPROM.get(offsetof(__eeprom_data, seconds),seconds);
  Serial << "leido tiempo riego por defecto, minutos: " << minutes << " segundos: " << seconds << endl;
  value = ((seconds==0)?minutes:seconds);
  StaticTimeUpdate();
  //leemos grupos de multirriego
  EEPROM.get(offsetof(__eeprom_data, numgroups),n_Grupos); //numero de grupos de multirriego
  Serial << "leido n_Grupos de la eeprom: " << n_Grupos << " grupos" << endl;
  #ifdef DEBUG
      int k;
      k = offsetof(__eeprom_data, groups[0].size);
      Serial << "offset Grupo1.size  : " << k << endl;
      k = offsetof(__eeprom_data, groups[0].serie);
      Serial << "offset Grupo1.serie  : " << k << endl;
      k = offsetof(__eeprom_data, groups[1].size);
      Serial << "offset Grupo2.size  : " << k << endl;
      k = offsetof(__eeprom_data, groups[1].serie);
      Serial << "offset Grupo2.serie  : " << k << endl;
      k = offsetof(__eeprom_data, groups[2].size);
      Serial << "offset Grupo3.size  : " << k << endl;
      k = offsetof(__eeprom_data, groups[2].serie);
      Serial << "offset Grupo3.serie  : " << k << endl;
  #endif
  grupoAddr = offsetof(__eeprom_data, groups[0]);
  for(i=0;i<n_Grupos;i++) {
    multi = getMultibyIndex(i);
    EEPROM.get(grupoAddr,multi->size);
    #ifdef VERBOSE
      Serial << "leyendo elementos Grupo" << i+1 << " : " << multi->size << " elementos " << endl;
    #endif
    grupoAddr += 4;
    for (int j=0;j < multi->size; j++) {
      EEPROM.get(grupoAddr,multi->serie[j]);
      grupoAddr += 2;
    }
  }
  printMultiGroup();    
    
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
    if (!factorRiegosOK) Serial.print("(simulados) ");
    Serial.println("Factores de riego leidos:");
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

void initClock()
{
  if (timeClient.update()) {
    Serial.print("initClock: NTP time recibido OK  --> ");
    Serial.println(timeClient.getFormattedTime());
    setTime(timeClient.getEpochTime());
    timeOK = true;
  }  
   else {
     Serial.println("[ERROR] initClock: no se ha recibido time por NTP");
     timeOK = false;
    }
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

void eepromWriteGroups() {
    //escribe en la eeprom grupos de multirriego y su tamaño
    int grupoAddr;
    grupoAddr = offsetof(__eeprom_data, groups[0]);
    EEPROM.put(offsetof(__eeprom_data, numgroups),n_Grupos); //numero de grupos de multirriego
    for(int i=0;i<n_Grupos;i++) {
      multi = getMultibyIndex(i);
      Serial << "escribiendo elementos Grupo" << i+1 << " : " << multi->size << " elementos " << endl;
      EEPROM.put(grupoAddr,multi->size);
      grupoAddr += 4;
      for (int j=0;j < multi->size; j++) {
        EEPROM.put(grupoAddr,multi->serie[j]);
        grupoAddr += 2;
      }
    }
}

void dimmerLeds()
{
  if (reposo) { 
     //conmuta estado LEDR, LEDG y LEDB para atenuarlos
     led(LEDR,OFF);
     led(LEDG,OFF);
     led(LEDB,OFF);
     delay(1);
     led(LEDR,ON);
     if(connected) led(LEDG,ON);
     if(NONETWORK) led(LEDB,ON);
     //standbyTime = millis();
  }   
}

void procesaEncoder()
{
  #ifdef NODEMCU
    Encoder->service();
  #endif
  if(Estado.estado == CONFIGURANDO && configure->configuringIdx()) {
      #ifdef EXTRATRACE
       Serial.println("TRACE: en procesaencoder idx");
      #endif
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

void initLastRiegos()
{
  for(uint i=0;i<NUMRIEGOS;i++) {
   lastRiegos[i] = 0;
  }
}

void initRiego(uint16_t id)
{
  //Esta funcion mandara el mensaje a domoticz de activar el boton
  int index = bId2bIndex(id);
  #ifdef DEBUG
    Serial.printf("Boton: %s boton.index: %d \n", boton->desc, bId2bIndex(boton->id));
  #endif
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
  if (Estado.estado != ERROR && !simErrorOFF) Serial << "Terminado OK riego: " << Boton[index].desc << endl;
  else {     //avisa de que no se ha podido terminar un riego
    if (simErrorOFF) {  // simula error si simErrorOFF es true
      Estado.estado = ERROR;
      display->print("Err3");
      errorOFF = true;  // recordatorio error
    }
    tic_parpadeoLedON.attach(0.2,parpadeoLedON);
    longbip(5);  
  }
  #ifdef DEBUG
    Serial.print("stopRiego acaba en estado: ");
    Serial.println(Estado.estado);
  #endif
}

void stopAllRiego()
{
  //Esta funcion pondra a off todos los botones
  //Apago los leds de multiriego
  led(Boton[bId2bIndex(multi->id)].led,OFF);
  //Apago los leds de riego
  for(unsigned int i=0;i<sizeof(COMPLETO)/2;i++) {
    led(Boton[bId2bIndex(COMPLETO[i])].led,OFF);
    stopRiego(COMPLETO[i]);
  }
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

void StaticTimeUpdate(void)
{
  if (Estado.estado == ERROR) return; //para que en caso de estado ERROR no borre el código de error del display
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;
  display->printTime(minutes, seconds);
}

void refreshDisplay()
{
  display->refreshDisplay();
}

void refreshTime()
{
  display->printTime(T.ShowMinutes(),T.ShowSeconds());
}

/*
void setupRed()
{
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

}
*/

bool checkWifiConnected()
{
  #ifdef TRACE
    Serial << "TRACE: in checkWifiConnectedWM , NONETWORK = " << NONETWORK << endl;
  #endif
  if(NONETWORK) { // en modo NONETWORK return false pero no damos error
     led(LEDB,ON);
     return false;
  }
  //Comprobamos si estamos conectados, error en caso contrario
  if(checkWifi()) return true;
   else {
      Serial.println("[ERROR] checkWifiConnected: ERROR DE CONEXION WIFI");
      display->print("Err1");
      Estado.estado = ERROR;
      led(LEDG,OFF);
      return false;
    }
}

int getFactor(uint16_t idx)
{
  #ifdef TRACE
    Serial.println("TRACE: in getFactor");
  #endif
  factorRiegosOK = false;
  //Ante cualquier problema devolvemos 100% para no factorizar ese riego
  #ifdef NODEMCU
    if(!checkWifi()) return 100;
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
    //httpclient.begin(tmpStr);
    httpclient.begin(client, tmpStr); // para v3.0.0 de platform esp8266
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
        display->print("Err2");
        #ifdef DEBUG
          Serial.println("[ERROR] getFactor: ERROR comunicando con Domoticz"); 
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
      display->print("Err9");
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
      Serial.println("[ERROR] getFactor: SE HA DEVUELTO ERROR"); 
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
  //if(factor == 0L) return 100;  
  //else return (int)factor;
  factorRiegosOK = true; //se devuelve un factor riego leido OK
  return (int)factor;
}

bool domoticzSwitch(int idx,char *msg)
{
  #ifdef TRACE
    Serial.println("TRACE: in domoticzSwitch");
  #endif

  #ifdef NODEMCU
    if(!checkWifiConnected()) return false;
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
        display->print("Err9");
        Estado.estado = ERROR;
        httpclient.stop();
        return;
      }
    #endif

    #ifdef NODEMCU
      String tmpStr = "http://" + String(serverAddress) + ":" + DOMOTICZPORT + String(message);
      //httpclient.begin(tmpStr);
      httpclient.begin(client, tmpStr); // para v3.0.0 de platform esp8266
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
        if(NONETWORK) return true;
        Estado.estado = ERROR;
        display->print("Err3");
        #ifdef DEBUG
          Serial.println("[ERROR] domoticzSwitch: SE HA DEVUELTO ERROR"); 
          Serial.printf("DOMOTICZSWITH IDX: %d [HTTP] GET... failed, error: %s\n", idx, httpclient.errorToString(httpCode).c_str());
        #endif
        return false;
      }
    #endif

    int pos = response.indexOf("\"status\" : \"ERR\"");
    if(pos != -1) {
      if(NONETWORK) return true;
      #ifdef DEBUG
        Serial.println("[ERROR] domoticzSwitch: SE HA DEVUELTO ERROR"); 
      #endif
      display->print("Err4");
      Estado.estado = ERROR;
      return false;
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
  return true;
}

void leeSerial() {
  if (Serial.available() > 0) {
    // lee cadena de entrada
    String inputSerial = Serial.readString();
    int inputNumber = inputSerial.toInt();
    if ((!inputNumber || inputNumber>2) && inputNumber != 9) {
        Serial.println("Teclee: ");
        Serial.println("   1 - simular error NTP");
        Serial.println("   2 - simular error apagar riego");
        Serial.println("   9 - anular simulacion errores");
    }
    switch (inputNumber) {
          case 1:
              Serial.println("   1 - simular error NTP");
              timeOK = false;
              break;
          case 2:
              Serial.println("   2 - simular error apagar riego");
              simErrorOFF = true;
              break;
          case 9:
              Serial.println("   9 - anular simulacion errores");
              simErrorOFF = false;
              timeOK = true;                         
    }
  }
}

void flagVerificaciones() {
  flagV = ON; //solo activamos flagV para no usar llamadas a funciones bloqueantes en Ticker
}

void Verificaciones() {   //verificaciones periodicas de estado wifi y hora correcta
  #ifdef DEBUG
    leeSerial();  // para ver si simulamos algun tipo de error
  #endif
  if (!flagV) return;      //si no activada por Ticker salimos sin hacer nada
  Serial.print(".");
  if (errorOFF) bip(2);  //recordatorio error grave
  if (!NONETWORK && (Estado.estado == STANDBY || (Estado.estado == ERROR && !connected))) {
    if (checkWifi()) Estado.estado = STANDBY; //verificamos wifi
    if (!timeOK && connected) initClock();    //verificamos time
    //if (!factorRiegosOK && connected) initFactorRiegos();    //verificamos factor riegos
  }
  flagV = OFF;
}

void parpadeoLedON(){
  byte estado = ledStatusId(LEDR);
  led(LEDR,!estado);
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
