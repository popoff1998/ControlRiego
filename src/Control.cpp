#define __MAIN__
#include "Control.h"
#include <EEPROM.h>

S_BOTON *boton;
S_BOTON *ultimoBoton;

#ifdef NODEMCU
  WiFiUDP ntpUDP;
#endif

NTPClient timeClient(ntpUDP,ntpServer);

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
                VERIFY=true; 
  #endif
  #ifdef DEVELOP
                NONETWORK=false;
                VERIFY=true; 
  #endif
  #ifdef DEMO
                NONETWORK=true;
                VERIFY=false;
  #endif

  Serial.begin(115200);
  Serial.print("\n\n");
  Serial.println("CONTROL RIEGO V" + String(VERSION) + "    Built on " __DATE__ " at " __TIME__ );
  strncpy(version_n, VERSION, 10); //eliminamos "." y "-" para mostrar version en el display
  std::remove(std::begin(version_n),std::end(version_n),'.');
  std::remove(std::begin(version_n),std::end(version_n),'-');
  Serial.print("Startup reason: ");Serial.println(ESP.getResetReason());
  #ifdef TRACE
    Serial.println("TRACE: in setup");
  #endif
  //Para el display
  #ifdef DEBUG
   Serial.println("Inicializando display");
  #endif
  display = new Display(DISPCLK,DISPDIO);
  display->clearDisplay();
  //Para el encoder
  #ifdef DEBUG
   Serial.println("Inicializando Encoder");
  #endif
  Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  //Para el Configure le paso encoder y display porque lo usara.
  #ifdef DEBUG
   Serial.println("Inicializando Configure");
  #endif
  configure = new Configure(display);
  //Para el CD4021B
  initCD4021B();
  //Para el 74HC595
  initHC595();
  //preparo indicadores de inicializaciones opcionales
  setupInit();
  //Inicializo la EEPROM (escribo en ella si asi se indica y/o se lee de ella valores configurados)
  procesaEeprom();
  //led encendido
  led(LEDR,ON);
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  setupRedWM();
  if (saveConfig) {
    EEPROM.commit();
    saveConfig = false;
  }
  #ifdef EXTRADEBUG
    Serial.printf("estado LEDR: %d \n",ledStatusId(LEDR));
    Serial.printf("estado LEDG: %d \n",ledStatusId(LEDG));
    Serial.printf("estado LEDB: %d \n",ledStatusId(LEDB));
  #endif
  delay(2500);
  //Ponemos en hora
  timeClient.begin();
  initClock();
  //Inicializamos lastRiegos (registro fecha y riego realizado)
  initLastRiegos();
  //Cargamos factorRiegos
  initFactorRiegos();
  //Deshabilitamos el hold de Pause
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
  //Llamo a parseInputs CLEAR para eliminar prepulsaciones antes del bucle loop
  parseInputs(CLEAR);
  // estado final en funcion de la conexion
  setupEstado();
  //inicializamos apuntador estructura multi (posicion del selector multirriego):
  multi = getMultibyId(getMultiStatus());
  //lanzamos supervision periodica estado cada 10 seg.
  tic_verificaciones.attach_scheduled(10, flagVerificaciones);
  standbyTime = millis();
  #ifdef TRACE
    Serial.println("TRACE: ending setup");
  #endif

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

/**---------------------------------------------------------------
 * Tratamiento boton pulsado (si ha cambiado de estado)
 */
void procesaBotones()
{
  #ifdef EXTRATRACE
    Serial.println("TRACE: in procesaBotones");
  #endif
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  //NOTA: el encoderSW esta en estado HIGH en reposo y en estado LOW cuando esta pulsado
  bool encoderSW;
  (!Boton[bId2bIndex(bENCODER)].estado) ? encoderSW = true : encoderSW = false;
  //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multiriego
  if (!multiSemaforo) {
    boton = NULL;
    boton = parseInputs(READ);
  }
  else multiSemaforo = false;
  // si no se ha pulsado ningun boton salimos
  if(boton == NULL) return;
  //Si estamos en reposo pulsar cualquier boton solo nos saca de ese estado
  if (reposo && boton->id != bSTOP) {
    Serial.println("Salimos de reposo -1-");
    reposo = false;
    displayOFF = false;
    standbyTime = millis();
    if(Estado.estado == STOP) display->print("stop");
    else StaticTimeUpdate();
    return;
  }
  //a partir de aqui procesamos solo los botones con flag ACTION
  if (!boton->flags.action) return;
  //En estado error salimos sin procesar el boton, a menos que:
  //   - pulsemos Pause y pasamos a modo NONETWORK
  //   - pulsemos Stop y en este caso reseteamos 
  if(Estado.estado == ERROR) 
  {
    //Si estamos en error y pulsamos pausa, nos ponemos en estado NONETWORK para test
    if(boton->id == bPAUSE && boton->estado) {  //evita procesar el release del pause
      Estado.estado = STANDBY;
      NONETWORK = true;
      Serial.println("estado en ERROR y PAUSA pulsada pasamos a modo NONETWORK y reseteamos");
      bip(2);
      led(LEDB,ON);
      //reseteos varios:
      stopAllRiego(false); //para apagar leds activos
      tic_parpadeoLedON.detach(); //detiene parpadeo led ON por si estuviera activado
      led(LEDR,ON);               // y lo deja fijo
      multiriego = false;
      multiSemaforo = false;
      errorOFF = false;
      displayOFF = false;
      standbyTime = millis();
      StaticTimeUpdate();
    }
    //Si estamos en ERROR y pulsamos STOP, reseteamos
    if(boton->id == bSTOP) {
      Serial.println("ERROR + STOP --> Reset.....");
      longbip(3);
      ESP.restart();  
    }
    return;  //cualquier otro boton que no sea Stop o Pausa en estado ERROR salimos sin procesarlo
  }
  //Procesamos el boton pulsado:
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
              Serial.println("[ConF] configurando tiempo riego por defecto");
              delay(500);
              boton = NULL;
            }
            break;
          case STANDBY:
              if(encoderSW) {  //si encoderSW+Pause --> conmutamos estado NONETWORK
                // es necesario? puede dar problemas? exception(28)?
                //boton = NULL; a ver que pasa si lo quitamos
                if (NONETWORK) {
                    NONETWORK = false;
                    Serial.println("encoderSW+PAUSE pasamos a modo NORMAL y leemos factor riegos");
                    bip(2);
                    led(LEDB,OFF);
                    display->print("----");
                    initFactorRiegos();
                    if(VERIFY && Estado.estado != ERROR) stopAllRiego(true); //verificamos operativa OFF para los IDX's 
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
                //boton = NULL; a ver que pasa si lo quitamos 
                delay(3000);
                ultimosRiegos(HIDE);
              }
              standbyTime = millis();
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
                //boton = NULL; a ver que pasa si lo quitamos
                holdPause = false;
                #ifdef DEBUG
                  Serial.printf("#01 savedValue: %d  value: %d \n",savedValue,value);
                #endif
                savedValue = value;
              }
              else {   //si esta pulsado encoderSW hacemos un soft reset
                Serial.println("ConF + encoderSW + PAUSA --> Reset.....");
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
      if (boton->estado) {  //si hemos PULSADO STOP
        //De alguna manera esta regando y hay que parar
        if (Estado.estado == REGANDO || Estado.estado == MULTIREGANDO || Estado.estado == PAUSE) {
          display->print("stop");
          stopAllRiego(true);
          T.StopTimer();
          bip(3);
          display->blink(DEFAULTBLINK);
          multiriego = false;
          multiSemaforo = false;
          Estado.estado = STOP;
        }
        else {
          //Lo hemos pulsado en standby - seguro antinenes
          display->print("stop");
          bip(1);
          longbip(1);
          stopAllRiego(true);  // apagar leds y parar riegos ¿?
          standbyTime = millis();
          if (Estado.estado == ERROR) break; //error en stopAllRiego
          Estado.estado = STOP;
          //pasamos directamente a reposo
          Serial.println("Stanby + Stop : Entramos en reposo");
          reposo = true;
          displayOFF = false;
        }
      }
      //si hemos liberado STOP: salimos del estado stop o de configuracion
      if (!boton->estado && (Estado.estado == STOP || Estado.estado == CONFIGURANDO)) {
        if (savedValue>0) {
            #ifdef DEBUG
            Serial.printf("#02 savedValue: %d  value: %d \n",savedValue,value);
            #endif
            value = savedValue;  // para que restaure reloj aunque no salvemos con pause el valor configurado
        }
        StaticTimeUpdate();
        led(Boton[bId2bIndex(bCONFIG)].led,OFF);
        //Serial.println("Salimos de reposo -2-");
        reposo = false; //por si salimos de stop antinenes
        displayOFF = false;
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
          Serial.printf( "en configuracion de MULTIRRIEGO, getMultibyId devuelve: %s \n" , multi->desc);
          Serial.printf( "                                           multi->size: %d \n" , multi->size);
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
          Serial.printf( "en MULTIRRIEGO, getMultibyId devuelve: %s \n" , multi->desc);
          Serial.printf( "                          multi->size: %d \n" , multi->size);
          Serial.printf( "en MULTIRRIEGO, encoderSW status  : %d \n", encoderSW );
        #endif
        // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
        // y mostramos en el display la version del programa.
        if (encoderSW) {
          display->print(version_n);
          displayGrupo(multi->serie, multi->size);
          multiriego = false;
          #ifdef DEBUG
            Serial.printf( "en MULTIRRIEGO + encoderSW, display de grupo: %s tamaño: %d \n", multi->desc , multi->size );
          #endif
          StaticTimeUpdate();
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
            #ifdef DEBUG
              Serial.printf("Minutos: %d Segundos: %d FMinutos: %d FSegundos: %d\n",minutes,seconds,fminutes,fseconds);
            #endif
            ultimoBoton = boton;
            //if (fminutes == 0 && fseconds == 0) {
            if ((fminutes == 0 && fseconds == 0) || boton->idx == 0) {
              Estado.estado = TERMINANDO; // nos saltamos este riego
              display->print("-00-");
              break;
            }
            T.SetTimer(0,fminutes,fseconds);
            T.StartTimer();
            initRiego(boton->id);
            if(Estado.estado != ERROR) Estado.estado = REGANDO; // para que no borre ERROR
        }
        else {  // mostramos en el display el factor de riego del boton pulsado
          int index = bId2bIndex(boton->id);
          led(Boton[index].led,ON);
          #ifdef DEBUG
            Serial.printf("Boton: %s Factor de riego: %d \n", boton->desc,factorRiegos[idarrayRiego(boton->id)]);
            Serial.printf("          boton.index: %d \n", index);
            Serial.printf("          boton(%d).led: %d \n", index, Boton[index].led);
            Serial.printf("#04 savedValue: %d  value: %d \n",savedValue,value);
          #endif
          savedValue = value;
          value = factorRiegos[idarrayRiego(boton->id)];
          display->print(value);
          delay(2000);
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
          Serial.printf("[ConF] configurando IDX boton: %s \n",boton->desc);
          configure->configureIdx(bId2bIndex(boton->id));
          value = boton->idx;
        }
      }
  }
}

/**---------------------------------------------------------------
 * Proceso en funcion del estado
 */
void procesaEstados()
{
  #ifdef EXTRATRACE
    Serial.println("TRACE: in procesaEstados");
  #endif

  switch (Estado.estado) {
    case CONFIGURANDO:
      if (boton != NULL) {
        if (boton->flags.action) {
          Serial.println( "En estado CONFIGURANDO pulsado ACTION" );
          switch(boton->id) {
            case bMULTIRIEGO:
              break;
            case bPAUSE:
              if(!boton->estado)
              {
                Serial.println( "Release de Pause" );
                return;
              }
              if(configure->configuringTime()) {
                uint8_t m=0,s=0;
                Serial.printf( "SAVE EEPROM TIME, minutes: %d  secons: %d \n", minutes, seconds);
                EEPROM.put(offsetof(__eeprom_data, minutes),minutes);
                EEPROM.put(offsetof(__eeprom_data, seconds),seconds);
                #ifdef NODEMCU
                  EEPROM.commit();
                #endif
                longbip(2);
                EEPROM.get(offsetof(__eeprom_data, minutes),m);
                EEPROM.get(offsetof(__eeprom_data, seconds),s);
                Serial.printf( "OFFm: %d  OFFs: %d \n", offsetof(__eeprom_data, minutes), offsetof(__eeprom_data, seconds));
                Serial.printf( "READ EEPROM TIME, minutes: %d  secons: %d \n", m , s);
                configure->stop();
              }
              if(configure->configuringIdx()) {
                Serial.printf( "SAVE EEPROM IDX value: %d \n", value);
                Boton[configure->getActualIdxIndex()].idx = (uint16_t)value;
                EEPROM.put(offsetof(__eeprom_data, botonIdx[0]) + 2*configure->getActualIdxIndex(),(uint16_t)value);
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
                Serial.printf( "SAVE EEPROM Multi : %s  tamaño: %d \n", multi->desc , multi->size);
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
                // ¿deberia ir aqui value = savedValue; para caso de salir sin salvar???
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
      //Apagamos el display si ha pasado el lapso
      if (reposo) standbyTime = millis();
      else {
        if (millis() > standbyTime + (1000 * STANDBYSECS)) {
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
      if (Estado.estado == ERROR) break; //no continuamos si se ha producido error al parar el riego
      display->blink(DEFAULTBLINK);
      led(Boton[bId2bIndex(ultimoBoton->id)].led,OFF);
      StaticTimeUpdate();
      standbyTime = millis();
      //Serial.println("Salimos de reposo -3-");
      //reposo = false;
      Estado.estado = STANDBY;

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
      //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
      if(reposo && !displayOFF) {
        if (millis() > standbyTime + (4 * 1000 * STANDBYSECS) && reposo) {
          display->clearDisplay();
          displayOFF = true;
        }
      }
      break;
    case PAUSE:
      blinkPause();
      break;
  }
}

/**---------------------------------------------------------------
 * Estado final al acabar Setup en funcion de la conexion a la red
 */
void setupEstado() 
{
  #ifdef TRACE
    Serial.println("TRACE: in setupEstado");
  #endif
  // Si estamos en modo NONETWORK pasamos a STANDBY aunque no exista conexión wifi o estemos en ERROR
  if (NONETWORK) {
    Estado.estado = STANDBY;
    bip(2);
    return;
  }
  // Si estado actual es ERROR seguimos así
  if (Estado.estado == ERROR) {
    return;
  }
  // Si estamos conectados pasamos a STANDBY o STOP (caso de estar pulsado este al inicio)
  if (checkWifi()) {
    if (Boton[bId2bIndex(bSTOP)].estado) {
      Estado.estado = STOP;
      display->print("stop");
      bip(1);
      longbip(1);
    }
    else Estado.estado = STANDBY;
    bip(1);
    return;
  }
  //si no estamos conectados a la red y no estamos en modo NONETWORK pasamos a estado ERROR
  statusError("Err1", 3);
}

/**---------------------------------------------------------------
 * verificamos si encoderSW esta pulsado (estado OFF) y selector de multirriego esta en:
 *    - Grupo1 (arriba) --> en ese caso inicializariamos la eeprom
 *    - Grupo3 (abajo) --> en ese caso borramos red wifi almacenada en el ESP8266
 */
void setupInit(void) {
  #ifdef TRACE
    Serial.println("TRACE: in setupInit");
  #endif
  //S_initFlags initFlags = 0;
  if (testButton(bENCODER, OFF)) {
    if (testButton(bGRUPO1,ON)) {
      initFlags.initEeprom = true;
      Serial.println("encoderSW pulsado y multirriego en GRUPO1  --> flag de init EEPROM true");
      eepromWriteSignal(6);
    }
    if (testButton(bGRUPO3,ON)) {
      initFlags.initWifi = true;
      Serial.println("encoderSW pulsado y multirriego en GRUPO3  --> flag de init WIFI true");
      wifiClearSignal(6);
    }
  }
};

/**---------------------------------------------------------------
 * Chequeo de perifericos
 */
void check(void)
{
  display->print("----");
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

/**---------------------------------------------------------------
 * Lectura y/o escritura de la eeprom
 */
void procesaEeprom() 
{
  int i,botonAddr;
  int grupoAddr;
  bool eeinitialized=0;
  n_Grupos = nGrupos();

  #ifdef NODEMCU
    EEPROM.begin(sizeof(__eeprom_data) + sizeof(_eeprom_group)*n_Grupos);
  #endif

  EEPROM.get(0,eeinitialized);
  botonAddr = offsetof(__eeprom_data, botonIdx[0]);
  
  #ifdef DEBUG
    Serial.printf( "\n tamaño de la eeprom : %d \n", sizeof(__eeprom_data) + sizeof(_eeprom_group)*n_Grupos );
    Serial.printf( "\t eeinitialized= %d \n", eeinitialized );
    Serial.printf( "\t FORCEINITEEPROM= %d \n", FORCEINITEEPROM );
    Serial.printf( "\t initEeprom= %d \n", initFlags.initEeprom );
    Serial.printf( "\t boton0 offset= %d \n", botonAddr );
  #endif

  if( eeinitialized == 0 || FORCEINITEEPROM == 1 || initFlags.initEeprom) {
    Serial.println(">>>>>>>>>>>>>>  Inicializando la EEPROM  <<<<<<<<<<<<<<");
    //escribe valores de los IDX de los botones
    for(i=0;i<16;i++) {
      EEPROM.put(botonAddr,Boton[i].idx);
      Serial.printf( "escribiendo boton %d : %d address %d \n", i , Boton[i].idx, botonAddr );
      botonAddr += 2;
    }
    //escribe tiempo de riego por defecto
    minutes = DEFAULTMINUTES;
    seconds = DEFAULTSECONDS;
    Serial.printf( "escrito tiempo riego por defecto, minutos: %d segundos: %d \n",minutes ,seconds );
    EEPROM.put(offsetof(__eeprom_data, minutes),minutes);
    EEPROM.put(offsetof(__eeprom_data, seconds),seconds);
    //escribe parametros conexion a Domoticz y ntp
    //eepromWriteRed();
    //escribe parametros conexion a Domoticz y ntp solo si FORCEINITEEPROM o no inicializada
    if(eeinitialized == 0 || FORCEINITEEPROM == 1) eepromWriteRed();
    //escribe grupos de multirriego y su tamaño
    eepromWriteGroups();
    // marca la eeprom como inicializada
    EEPROM.put(offsetof(__eeprom_data, initialized),(uint8_t)1);   
    #ifdef NODEMCU
      bool bRc = EEPROM.commit();
      if(bRc) Serial.println("Write eeprom OK");
      else    Serial.println("Write eeprom error");
    #endif                
    #ifdef DEBUG
      Serial.printf( "OFFinitialized: %d \n", offsetof(__eeprom_data, initialized));
      Serial.printf( "OFFm: %d OFFS: %d \n", offsetof(__eeprom_data, minutes), offsetof(__eeprom_data, seconds));
      Serial.printf( "OFFnumgroups: %d \n", offsetof(__eeprom_data, numgroups));
    #endif
    //señala la escritura de la eeprom
    if(bRc) longbip(3);
  }
  //siempre leemos valores de la eeprom para cargar las variables correspondientes:
  Serial.println("<<<<<<<<<<<<<<<<<<<<<   Leyendo valores de la EEPROM   >>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  //leemos valores de los IDX de los botones
  Serial.println("Leyendo idx de los botones");
  botonAddr = offsetof(__eeprom_data, botonIdx[0]);
  for(i=0;i<16;i++) {
    EEPROM.get(botonAddr,Boton[i].idx);
    #ifdef VERBOSE
      Serial.printf( "leido boton %d idx: %d address: %d \n", i, Boton[i].idx, botonAddr);
    #endif
    botonAddr += sizeof(Boton[i].idx);
  }
  //leemos tiempo de riego por defecto
  EEPROM.get(offsetof(__eeprom_data, minutes),minutes);
  EEPROM.get(offsetof(__eeprom_data, seconds),seconds);
  Serial.printf( "leido tiempo riego por defecto, minutos: %d segundos: %d \n",minutes ,seconds );
  value = ((seconds==0)?minutes:seconds);
  StaticTimeUpdate();
  //leemos parametros conexion a Domoticz y ntp
  EEPROM.get(offsetof(__eeprom_data, serverAddress),serverAddress);
  EEPROM.get(offsetof(__eeprom_data, DOMOTICZPORT),DOMOTICZPORT);
  EEPROM.get(offsetof(__eeprom_data, ntpServer),ntpServer);
  Serial.println("leidos parametros de conexion a domoticz en la eeprom");
  Serial.printf( " - Domoticz ip: %s puerto: %s \n", serverAddress, DOMOTICZPORT);
  Serial.printf( " - NTP server: %s \n", ntpServer );
  //leemos grupos de multirriego
  EEPROM.get(offsetof(__eeprom_data, numgroups),n_Grupos); //numero de grupos de multirriego
  Serial.printf( "leido n_Grupos de la eeprom: %d grupos \n", n_Grupos );
  #ifdef DEBUG
      int k;
      k = offsetof(__eeprom_data, groups[0].size);
      Serial.printf( "offset Grupo1.size  : %d \n" , k );
      k = offsetof(__eeprom_data, groups[0].serie);
      Serial.printf( "offset Grupo1.serie  : %d \n" , k );
      k = offsetof(__eeprom_data, groups[1].size);
      Serial.printf( "offset Grupo2.size  : %d \n" , k );
      k = offsetof(__eeprom_data, groups[1].serie);
      Serial.printf( "offset Grupo2.serie  : %d \n" , k );
      k = offsetof(__eeprom_data, groups[2].size);
      Serial.printf( "offset Grupo3.size  : %d \n" , k );
      k = offsetof(__eeprom_data, groups[2].serie);
      Serial.printf( "offset Grupo3.serie  : %d \n" , k );
  #endif
  grupoAddr = offsetof(__eeprom_data, groups[0]);
  for(i=0;i<n_Grupos;i++) {
    multi = getMultibyIndex(i);
    EEPROM.get(grupoAddr,multi->size);
    #ifdef VERBOSE
      Serial.printf( "leyendo elementos Grupo%d : %d elementos \n" , i+1 , multi->size );
    #endif
    grupoAddr += 4;
    for (int j=0;j < multi->size; j++) {
      EEPROM.get(grupoAddr,multi->serie[j]);
      grupoAddr += 2;
    }
  }
  printMultiGroup();    
    
}

/**---------------------------------------------------------------
 * Lee factores de riego del domoticz
 */
void initFactorRiegos()
{
  #ifdef TRACE
    Serial.println("TRACE: in initFactorRiegos");
  #endif
  //inicializamos a valor 100 por defecto para caso de error
  for(uint i=0;i<NUMRIEGOS;i++) {
    factorRiegos[i]=100;
  }
  //leemos factores del Domoticz
  for(uint i=0;i<NUMRIEGOS;i++) {
    factorRiegos[i]=getFactor(Boton[bId2bIndex(COMPLETO[i])].idx);
    if(Estado.estado == ERROR) break; //al primer error salimos
  }
  #ifdef VERBOSE
    //Leemos los valores para comprobar que lo hizo bien
    Serial.print("Factores de riego ");
    factorRiegosOK ? Serial.println("leidos: ") :  Serial.println("(simulados): ");
    for(uint i=0;i<NUMRIEGOS;i++) {
      Serial.printf("factor ZONA%d: %d \n",i+1,factorRiegos[i]);
    }
  #endif
}

//Aqui convertimos minutes y seconds por el factorRiegos
void timeByFactor(int factor,uint8_t *fminutes, uint8_t *fseconds)
{
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
    setTime(timeClient.getEpochTime());
    timeOK = true;
    Serial.print("initClock: NTP time recibido OK  (UTC) --> " + timeClient.getFormattedTime());
    time_t t = CE.toLocal(now(),&tcr);
    Serial.printf("  local --> %d:%d:%d \n" ,hour(t),minute(t),second(t));
  }  
   else {
     Serial.println("[ERROR] initClock: no se ha recibido time por NTP");
     timeOK = false;
    }
}

//muestra lo regado desde las 0h encendiendo sus leds y mostrando hora o apagandolos
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

//escribe en la eeprom grupos de multirriego y su tamaño
void eepromWriteGroups() 
{
  int grupoAddr;
  grupoAddr = offsetof(__eeprom_data, groups[0]);
  EEPROM.put(offsetof(__eeprom_data, numgroups),n_Grupos); //numero de grupos de multirriego
  for(int i=0;i<n_Grupos;i++) {
    multi = getMultibyIndex(i);
    Serial.printf( "escribiendo elementos Grupo%d : %d elementos \n" , i+1 , multi->size );
    EEPROM.put(grupoAddr,multi->size);
    grupoAddr += 4;
    for (int j=0;j < multi->size; j++) {
      EEPROM.put(grupoAddr,multi->serie[j]);
      grupoAddr += 2;
    }
  }
}

//escribe parametros conexion a Domoticz y ntp
void eepromWriteRed() 
{
  Serial.println("Escribiendo parametros de conexion a domoticz en la eeprom");
  EEPROM.put(offsetof(__eeprom_data, serverAddress),serverAddress);
  EEPROM.put(offsetof(__eeprom_data, DOMOTICZPORT),DOMOTICZPORT);
  EEPROM.put(offsetof(__eeprom_data, ntpServer),ntpServer);
}

//conmuta estado LEDR, LEDG y LEDB para atenuarlos
void dimmerLeds()
{
  if (reposo) { 
    led(LEDR,OFF);
    led(LEDG,OFF);
    led(LEDB,OFF);
    delay(1);
    led(LEDR,ON);
    if(connected) led(LEDG,ON);
    if(NONETWORK) led(LEDB,ON);
  }   
}

//lee encoder para actualizar el display
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
      //if (value <  1) value = 1;
      if (value <  1) value = 0; //permitimos IDX=0 para desactivar ese boton
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
  //Serial.println("Salimos de reposo -4-");
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

//Inicia el riego correspondiente al idx
bool initRiego(uint16_t id)
{
  int index = bId2bIndex(id);
  #ifdef DEBUG
    Serial.printf("Boton: %s boton.index: %d \n", boton->desc, bId2bIndex(boton->id));
  #endif
  uint arrayIndex = idarrayRiego(id);
  time_t t;
  if(arrayIndex == 999) return false;
  Serial.printf( "Iniciando riego: %s \n", Boton[index].desc);
  led(Boton[index].led,ON);
  utc = timeClient.getEpochTime();
  t = CE.toLocal(utc,&tcr);
  lastRiegos[arrayIndex] = t;
  return domoticzSwitch(Boton[index].idx,(char *)"On");
}

//Termina el riego correspondiente al idx
bool stopRiego(uint16_t id)
{
  int index = bId2bIndex(id);
  #ifdef DEBUG
  Serial.printf( "Terminando riego: %s \n", Boton[index].desc);
  #endif
  domoticzSwitch(Boton[index].idx,(char *)"Off");
  if (Estado.estado != ERROR && !simErrorOFF) Serial.printf( "Terminado OK riego: %s \n" , Boton[index].desc );
  else {     //avisa de que no se ha podido terminar un riego
    if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
      if (simErrorOFF) statusError("Err5",5); // simula error si simErrorOFF es true
      errorOFF = true;  // recordatorio error
      tic_parpadeoLedON.attach(0.2,parpadeoLedON);
    } 
    return false;
  }
  return true;
}

//Pone a off todos los leds de riegos y detiene riegos (solo si la llamamos con true)
void stopAllRiego(bool stop)
{
  //Apago los leds de multiriego
  led(Boton[bId2bIndex(multi->id)].led,OFF);
  //Apago los leds de riego
  for(unsigned int i=0;i<sizeof(COMPLETO)/2;i++) {
    led(Boton[bId2bIndex(COMPLETO[i])].led,OFF);
    if (stop) {  //paramos todas las zonas de riego
      if(!stopRiego(COMPLETO[i])) { 
        break; //al primer error salimos
      }  
    }
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

/**---------------------------------------------------------------
 * Comunicacion con Domoticz usando httpGet
 */
String httpGetDomoticz(String message) 
{
  #ifdef TRACE
    Serial.println("TRACE: in httpGetDomoticz");
  #endif
  String tmpStr = "http://" + String(serverAddress) + ":" + DOMOTICZPORT + String(message);
  #ifdef DEBUG
    Serial.print("TMPSTR: ");Serial.println(tmpStr);
  #endif
  httpclient.begin(client, tmpStr); // para v3.0.0 de platform esp8266
  String response = "{}";
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
    if(Estado.estado != ERROR) {
      Estado.estado = ERROR;
      #ifdef DEBUG
        Serial.printf("[ERROR] httpGetDomoticz: ERROR comunicando con Domoticz error: %s\n", httpclient.errorToString(httpCode).c_str()); 
      #endif
    }
    return "Err2";
  }
  //vemos si la respuesta indica status error
  int pos = response.indexOf("\"status\" : \"ERR");
  if(pos != -1) {
    #ifdef DEBUG
      Serial.println("[ERROR] httpGetDomoticz: SE HA DEVUELTO ERROR"); 
    #endif
    Estado.estado = ERROR;
    return "ErrX";
  }
  httpclient.end();
  Serial.println("TRACE: in httpGetDomoticz **return");
  return response;
}

/**---------------------------------------------------------------
 * lee factor de riego del Domoticz, almacenado en campo comentarios
 */
int getFactor(uint16_t idx)
{
  #ifdef TRACE
    Serial.println("TRACE: in getFactor");
  #endif
  factorRiegosOK = false;
  if(!checkWifi()) {
    if(NONETWORK) return 100; //si estamos en modo NONETWORK devolvemos 100 y no damos error
    else {
      statusError("Err1",3);
      return 0;
    }
  }
  // si el IDX es 0 devolvemos 0 sin procesarlo (boton no asignado)
  if(idx == 0) return 0;
  char JSONMSG[200]="/json.htm?type=devices&rid=%d";
  char message[250];
  sprintf(message,JSONMSG,idx);
  String response = httpGetDomoticz(message);
  //procesamos la respuesta para ver si se ha producido error:
  if (Estado.estado == ERROR && response.startsWith("Err")) {
    if (NONETWORK) {  //si estamos en modo NONETWORK devolvemos 100 y no damos error
      Estado.estado = STANDBY;
      return 100;
    }
    if(response == "ErrX") statusError("Err3",3);
    else statusError(response,3);
    #ifdef DEBUG
      Serial.printf("GETFACTOR IDX: %d [HTTP] GET... failed\n", idx);
    #endif
    return 100;
  }
  /* Teoricamente ya tenemos en response el JSON, lo procesamos
     Si el IDX no existe Domoticz no devuelve error, asi que hay que controlarlo
     Ante cualquier problema (no de error), devolvemos 100% para no factorizar ese riego
     si VERIFY=false, o Err2 si VERIFY=true
  */
  const size_t bufferSize = 2*JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(16) + JSON_OBJECT_SIZE(46) + 1500;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject &root = jsonBuffer.parseObject(response);
  if (!root.success()) {
    #ifdef DEBUG
      Serial.println("parseObject() failed");
    #endif
    if(!VERIFY) return 100;
    else {
      statusError("Err2",3);
      return 100;
    }
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *factorstr = root["result"][0]["Description"];
  if(factorstr == NULL) {
    //El rid (idx) no esta definido en el Domoticz
    #ifdef VERBOSE
      Serial.printf("El idx %d no se ha podido leer del JSON\n",idx);
    #endif
    if(!VERIFY) return 100;
    else {
      statusError("Err2",3);
      return 100;
    }
  }
  //si hemos leido correctamente (numero, campo vacio o solo con comentarios)
  //consideramos leido OK el factor riego. En los dos ultimos casos se
  //devuelve valor por defecto 100.
  factorRiegosOK = true;
  long int factor = strtol(factorstr,NULL,10);
  //controlamos devolver 0 solo si se ha puesto explicitamente
  if (factor == 0) {
    if (strlen(factorstr) == 0) return 100;    //campo comentarios vacio -> por defecto 100
    if (!isdigit(factorstr[0])) return 100;    //comentarios no comienzan por 0
  }
  return (int)factor;
}

/**---------------------------------------------------------------
 * Envia a domoticz orden de on/off del idx correspondiente
 */
bool domoticzSwitch(int idx,char *msg)
{
  #ifdef TRACE
    Serial.println("TRACE: in domoticzSwitch");
  #endif
  //if(NONETWORK) return true; //simulamos que ha ido OK
  if(NONETWORK || idx == 0) return true; //simulamos que ha ido OK
  if(!checkWifi()) {
    statusError("Err1",3);
    return false;
  }
  char JSONMSG[200]="/json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s";
  char message[250];
  sprintf(message,JSONMSG,idx,msg);
  String response = httpGetDomoticz(message);
  //procesamos la respuesta para ver si se ha producido error:
  if (Estado.estado == ERROR && response.startsWith("Err")) {
    if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
      if(response == "ErrX") {
        if(strcmp(msg,"On") == 0 )statusError("Err4",3); //error al iniciar riego
        else statusError("Err5",5); //error al parar riego
      }
      else statusError(response,3); //otro error
    }
    Serial.printf("DOMOTICZSWITH IDX: %d fallo en %s\n", idx, msg);
    return false;
  }
  //if(Estado.estado != ERROR && strcmp(msg,"On") == 0) Estado.estado = REGANDO;
  return true;
}

/**---------------------------------------------------------------
 * lectura del puerto serie para debug
 */
void leeSerial() 
{
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

void flagVerificaciones() 
{
  flagV = ON; //aqui solo activamos flagV para no usar llamadas a funciones bloqueantes en Ticker
}

/**---------------------------------------------------------------
 * verificaciones periodicas de estado (wifi, hora correcta, ...)
 */
void Verificaciones() 
{   
  #ifdef DEBUG
    leeSerial();  // para ver si simulamos algun tipo de error
  #endif
  if (!flagV) return;      //si no activada por Ticker salimos sin hacer nada
  Serial.print(".");
  if (errorOFF) bip(2);  //recordatorio error grave
  if (!NONETWORK && (Estado.estado == STANDBY || (Estado.estado == ERROR && !connected))) {
    if (checkWifi()) Estado.estado = STANDBY; //verificamos si seguimos connectados a la wifi
    if (connected && falloAP) {
      Serial.println("Wifi conectada despues Setup, leemos factor riegos");
      falloAP = false;
      initFactorRiegos(); //esta funcion ya dejara el estado correspondiente
    }
    if (!timeOK && connected) initClock();    //verificamos time
  }
  flagV = OFF;
}

/**---------------------------------------------------------------
 * pasa a estado ERROR
 */
void statusError(String errorID, int n) 
{    
  Estado.estado = ERROR;
  Serial.printf("[statusError]: %s \n", errorID.c_str());
  display->print(errorID.c_str());
  longbip(n);
}

void parpadeoLedON()
{
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

