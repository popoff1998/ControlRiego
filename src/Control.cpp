#define __MAIN__
#include "Control.h"

bool clean_FS = false;
const char *parmFile = "/config_parm.json";       // fichero de parametros activos
const char *defaultFile = "/config_default.json"; // fichero de parametros por defecto

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
                NONETWORK=true;
                VERIFY=true; 
  #endif
  #ifdef DEMO
                NONETWORK=true;
                VERIFY=false;
  #endif

  Serial.begin(115200);
  Serial.println("\n\n CONTROL RIEGO V" + String(VERSION) + "    Built on " __DATE__ " at " __TIME__ );
  Serial.print(F("Startup reason: "));Serial.println(ESP.getResetReason());
  #ifdef TRACE
    Serial.println(F("TRACE: in setup"));
  #endif
  //Para el display
  #ifdef DEBUG
   Serial.println(F("Inicializando display"));
  #endif
  display = new Display(DISPCLK,DISPDIO);
  display->clearDisplay();
  //Para el encoder
  #ifdef DEBUG
   Serial.println(F("Inicializando Encoder"));
  #endif
  Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  //Para el Configure le paso encoder y display porque lo usara.
  #ifdef DEBUG
   Serial.println(F("Inicializando Configure"));
  #endif
  configure = new Configure(display);
  //Para el CD4021B
  initCD4021B();
  //Para el 74HC595
  initHC595();
  //preparo indicadores de inicializaciones opcionales
  setupInit();
  //led encendido
  led(LEDR,ON);
  //setup parametros configuracion
  #ifdef EXTRADEBUG
    printFile(parmFile);
  #endif
  setupParm();
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  setupRedWM(config);
  if (saveConfig) {
    if (saveConfigFile(parmFile, config))  bipOK(3);;
    saveConfig = false;
  }
  #ifdef WEBSERVER
    setupWS();
  #endif
  delay(2000);
  //Ponemos en hora
  timeClient.begin();
  delay(500);
  initClock();
  //Inicializamos lastRiegos (registro fecha y riego realizado)
  initLastRiegos();
  //Cargamos factorRiegos
  initFactorRiegos();
  //Deshabilitamos el hold de Pause
  Boton[bID_bIndex(bPAUSE)].flags.holddisabled = true;
  //Llamo a parseInputs CLEAR para eliminar prepulsaciones antes del bucle loop
  parseInputs(CLEAR);
  //Estado final en funcion de la conexion
  setupEstado();
  #ifdef EXTRADEBUG
    printMulti();
    printFile(parmFile);
  #endif
  //lanzamos supervision periodica estado cada 10 seg.
  tic_verificaciones.attach_scheduled(10, flagVerificaciones);
  standbyTime = millis();
  #ifdef TRACE
    Serial.println(F("TRACE: ending setup"));
  #endif
}


/*----------------------------------------------*
 *            Bucle principal                   *
 *----------------------------------------------*/

void loop()
{
  #ifdef EXTRATRACE
    Serial.print(F("L"));
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
    Serial.print(F("B"));
  #endif
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  //NOTA: el encoderSW esta en estado HIGH en reposo y en estado LOW cuando esta pulsado
  (!Boton[bID_bIndex(bENCODER)].estado) ? encoderSW = true : encoderSW = false;
  //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multiriego
  if (multiSemaforo) multiSemaforo = false;
  else  boton = parseInputs(READ);  //vemos si algun boton ha cambiado de estado
  // si no se ha pulsado ningun boton salimos
  if(boton == NULL) return;
  //Si estamos en reposo pulsar cualquier boton solo nos saca de ese estado
  // (salvo STOP que si actua y se procesa mas adelante)
  if (reposo && boton->id != bSTOP) {
    Serial.println(F("Salimos de reposo"));
    reposo = false;
    displayOff = false;
    standbyTime = millis();
    if(Estado.estado == STOP) display->print("StoP");
    else StaticTimeUpdate();
    return;
  }
  //En estado error no actuan los botones
  // (salvo PAUSE y STOP que se procesan en procesaEstadoError)
  if(Estado.estado == ERROR) return;
  //a partir de aqui procesamos solo los botones con flag ACTION
  if (!boton->flags.action) return;
  //Procesamos el boton pulsado:
  switch (boton->id) {
    //Primero procesamos los botones singulares, el resto van por default
    case bPAUSE:
      procesaBotonPause();
      break;
    case bSTOP:
      procesaBotonStop();
      break;
    case bMULTIRIEGO:
      if (!procesaBotonMultiriego()) break;
      //Aqui no hay break para que comience multirriego por default
    default:
      procesaBotonZona();
  }
}


/**---------------------------------------------------------------
 * Proceso en funcion del estado
 */
void procesaEstados()
{
  #ifdef EXTRATRACE
    Serial.print(F("E"));
  #endif

  switch (Estado.estado) {
    case CONFIGURANDO:
      procesaEstadoConfigurando();
      break;
    case ERROR:
      procesaEstadoError();
      blinkPause();
      break;
    case REGANDO:
      procesaEstadoRegando();
      break;
    case TERMINANDO:
      procesaEstadoTerminando();
      break;
    case STANDBY:
      procesaEstadoStandby();
      break;
    case STOP:
      procesaEstadoStop();
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
    Serial.println(F("TRACE: in setupEstado"));
  #endif
  //inicializamos apuntador estructura multi (posicion del selector multirriego):
  if(!setMultibyId(getMultiStatus(), config) || !config.initialized) {
    statusError(E0, 3);  //no se ha podido cargar parámetros de ficheros -> señalamos el error
  return;
  }
  // Si estamos en modo NONETWORK pasamos a STANDBY (o STOP si esta pulsado) aunque no exista conexión wifi o estemos en ERROR
  if (NONETWORK) {
    if (Boton[bID_bIndex(bSTOP)].estado) {
      Estado.estado = STOP;
      infoDisplay("StoP", NOBLINK, LONGBIP, 1);
    }
    else Estado.estado = STANDBY;
    Estado.fase = CERO;
    bip(2);
    return;
  }
  // Si estado actual es ERROR seguimos así
  if (Estado.estado == ERROR) {
    return;
  }
  // Si estamos conectados pasamos a STANDBY o STOP (caso de estar pulsado este al inicio)
  if (checkWifi()) {
    if (Boton[bID_bIndex(bSTOP)].estado) {
      Estado.estado = STOP;
      infoDisplay("StoP", NOBLINK, LONGBIP, 1);
    }
    else Estado.estado = STANDBY;
    Estado.fase = CERO;
    bip(1);
    return;
  }
  //si no estamos conectados a la red y no estamos en modo NONETWORK pasamos a estado ERROR
  statusError(E1, 3);
}


/**---------------------------------------------------------------
 * verificamos si encoderSW esta pulsado (estado OFF) y selector de multirriego esta en:
 *    - Grupo1 (arriba) --> en ese caso cargamos los parametros del fichero de configuracion por defecto
 *    - Grupo3 (abajo) --> en ese caso borramos red wifi almacenada en el ESP8266
 */
void setupInit(void) {
  #ifdef TRACE
    Serial.println(F("TRACE: in setupInit"));
  #endif
  if (testButton(bENCODER, OFF)) {
    if (testButton(bGRUPO1,ON)) {
      initFlags.initParm = true;
      Serial.println(F("encoderSW pulsado y multirriego en GRUPO1  --> flag de load default PARM true"));
      loadDefaultSignal(6);
    }
    if (testButton(bGRUPO3,ON)) {
      initFlags.initWifi = true;
      Serial.println(F("encoderSW pulsado y multirriego en GRUPO3  --> flag de init WIFI true"));
      wifiClearSignal(6);
    }
  }
};


void procesaBotonPause(void)
{
  if (Estado.estado != STOP) {
    //No procesamos los release del boton salvo en STOP
    if(!boton->estado) return;
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
      case STANDBY:
          boton = NULL; //lo borramos para que no sea tratado más adelante en procesaEstados
          if(encoderSW) {  //si encoderSW+Pause --> conmutamos estado NONETWORK
            if (NONETWORK) {
                NONETWORK = false;
                Serial.println(F("encoderSW+PAUSE pasamos a modo NORMAL y leemos factor riegos"));
                bip(2);
                led(LEDB,OFF);
                display->print("----");
                initFactorRiegos();
                if(VERIFY && Estado.estado != ERROR) stopAllRiego(true); //verificamos operativa OFF para los IDX's 
            }
            else {
                NONETWORK = true;
                Serial.println(F("encoderSW+PAUSE pasamos a modo NONETWORK (DEMO)"));
                bip(2);
                led(LEDB,ON);
            }
          }
          else {    // muestra hora y ultimos riegos
            ultimosRiegos(SHOW);
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
            ledConf(ON);
            Estado.estado = CONFIGURANDO;
            Serial.println(F("Stop + hold PAUSA --> modo ConF()"));
            boton = NULL;
            holdPause = false;
            savedValue = value;
          }
          else {   //si esta pulsado encoderSW hacemos un soft reset
            Serial.println(F("Stop + encoderSW + PAUSA --> Reset....."));
            longbip(3);
            ESP.restart();  // Hard RESET: ESP.reset()
          }
        }
      }
    }
    //Si lo hemos soltado quitamos holdPause
    else holdPause = false;
  }
};


void procesaBotonStop(void)
{
  if (boton->estado) {  //si hemos PULSADO STOP
    if (Estado.estado == REGANDO || Estado.estado == MULTIREGANDO || Estado.estado == PAUSE) {
      //De alguna manera esta regando y hay que parar
      display->print("StoP");
      T.StopTimer();
      if (!stopAllRiego(true)) {   //error en stopAllRiego
        boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
        return; 
      }
      infoDisplay("StoP", DEFAULTBLINK, BIP, 6);;
      Estado.estado = STOP;
      multiriego = false;
      multiSemaforo = false;
    }
    else {
      //Lo hemos pulsado en standby - seguro antinenes
      infoDisplay("StoP", NOBLINK, BIP, 3);
      // apagar leds y parar riegos (por si riego activado externamente)
      if (!stopAllRiego(true)) {   //error en stopAllRiego
        boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
        return; 
      }
      Estado.estado = STOP;
      reposo = true; //pasamos directamente a reposo
      displayOff = false;
    }
  }
  //si hemos liberado STOP: salimos del estado stop
  //(dejamos el release del STOP en modo ConF para que actue el codigo de procesaEstadoConfigurando
  if (!boton->estado && Estado.estado == STOP) {
    StaticTimeUpdate();
    reposo = false; //por si salimos de stop antinenes
    displayOff = false;
    Estado.estado = STANDBY;
  }
  standbyTime = millis();
}


bool procesaBotonMultiriego(void)
{
  if (Estado.estado == STANDBY && !multiriego) {
    #ifdef DEBUG
      int n_grupo = setMultibyId(getMultiStatus(), config);
      Serial.printf( "en MULTIRRIEGO, setMultibyId devuelve: Grupo%d (%s) multi.size=%d \n" , n_grupo, multi.desc, *multi.size);
      for (int k=0; k < *multi.size; k++) Serial.printf( "       multi.serie: x%x \n" , multi.serie[k]);
      Serial.printf( "en MULTIRRIEGO, encoderSW status  : %d \n", encoderSW );
    #endif
    // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
    // y mostramos en el display la version del programa.
    if (encoderSW) {
      char version_n[10];
      strncpy(version_n, VERSION, 10); //eliminamos "." y "-" para mostrar version en el display
      std::remove(std::begin(version_n),std::end(version_n),'.');
      std::remove(std::begin(version_n),std::end(version_n),'-');
      display->print(version_n);
      displayGrupo(multi.serie, *multi.size);
      #ifdef DEBUG
        Serial.printf( "en MULTIRRIEGO + encoderSW, display de grupo: %s tamaño: %d \n", multi.desc , *multi.size );
      #endif
      StaticTimeUpdate();
      return false;    //para que procese el BREAK al volver a procesaBotones         
    }  
    else {
      //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
      //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
      bip(4);
      multiriego = true;
      multi.actual = 0;
      Serial.printf("MULTIRRIEGO iniciado: %s \n", multi.desc);
      led(Boton[bID_bIndex(*multi.id)].led,ON);
      boton = &Boton[bID_bIndex(multi.serie[multi.actual])];
    }
  }
  return true;   //para que continue con el case DEFAULT al volver
}


void procesaBotonZona(void)
{
  int zIndex = bID_zIndex(boton->id);
  if (zIndex == 999) return; //el boton no es de ZONA ¿es necesaria esta comprobacion?
  int bIndex = bID_bIndex(boton->id); 
  if (Estado.estado == STANDBY) {
    if (!encoderSW) {  //iniciamos el riego correspondiente al boton seleccionado
        bip(2);
        //cambia minutes y seconds en funcion del factor de cada sector de riego
        uint8_t fminutes=0,fseconds=0;
        if(multiriego) {
          timeByFactor(factorRiegos[zIndex],&fminutes,&fseconds);
        }
        else {
          fminutes = minutes;
          fseconds = seconds;
        }
        #ifdef DEBUG
          Serial.printf("Minutos: %d Segundos: %d FMinutos: %d FSegundos: %d\n",minutes,seconds,fminutes,fseconds);
        #endif
        ultimoBoton = boton;
        // si tiempo factorizado de riego es 0 o IDX=0, nos saltamos este riego
        if ((fminutes == 0 && fseconds == 0) || boton->idx == 0) {
          Estado.estado = TERMINANDO;
          led(Boton[bIndex].led,ON); //para que se vea que zona es 
          display->print("-00-");
          return;
        }
        T.SetTimer(0,fminutes,fseconds);
        T.StartTimer();
        initRiego(boton->id);
        if(Estado.estado != ERROR) Estado.estado = REGANDO; // para que no borre ERROR
    }
    else {  // mostramos en el display el factor de riego del boton pulsado
      led(Boton[bIndex].led,ON);
      #ifdef DEBUG
        Serial.printf("Boton: %s Factor de riego: %d \n", boton->desc,factorRiegos[zIndex]);
        Serial.printf("          boton.index: %d \n", bIndex);
        Serial.printf("          boton(%d).led: %d \n", bIndex, Boton[bIndex].led);
      #endif
      savedValue = value;
      value = factorRiegos[zIndex];
      display->print(value);
      delay(2000);
      value = savedValue;  // para que restaure reloj
      led(Boton[bIndex].led,OFF);
      StaticTimeUpdate();
    }
  }
}


void procesaEstadoConfigurando()
{
  //Deshabilitamos el hold de Pause
  Boton[bID_bIndex(bPAUSE)].flags.holddisabled = true;
  if (boton != NULL) {
    if (boton->flags.action) {
      int n_grupo;
      int bIndex = bID_bIndex(boton->id);
      int zIndex = bID_zIndex(boton->id);
      switch(boton->id) {
        case bMULTIRIEGO:
          if (configure->configuring()) return; //si ya estamos configurando algo salimos
          n_grupo = setMultibyId(getMultiStatus(), config);
          if (encoderSW) {  //encoderSW pulsado: actuamos segun posicion selector multirriego
            if (n_grupo == 1) {  // copiamos fichero parametros en fichero default
              if (copyConfigFile(parmFile, defaultFile)) {
                Serial.println(F("[ConF] salvado fichero de parametros actuales como DEFAULT"));
                infoDisplay("-dEF", DEFAULTBLINK, BIPOK, 5);
                display->print("ConF"); 
              }
            }
            #ifdef WEBSERVER
              if (n_grupo == 2) {  // activamos procesado webserver
                Serial.println(F("[ConF][WS] activado webserver para actualizaciones OTA de SW o filesystem"));
                webServerAct = true;
                infoDisplay("otA", DEFAULTBLINK, BIPOK, 5);
                //display->print("ConF"); 
              }
            #endif 
            if (n_grupo == 3) {  // activamos AP y portal de configuracion (bloqueante)
              Serial.println(F("[ConF] encoderSW + selector ABAJO: activamos AP y portal de configuracion"));
              ledConf(OFF);
              starConfigPortal(config);
              ledConf(ON);
              display->print("ConF");
            }
          }
          else {
            //Configuramos el grupo de multirriego activo
            configure->configureMulti(n_grupo);
            Serial.printf( "[ConF] configurando: GRUPO%d (%s) \n" , n_grupo, multi.desc);
            #ifdef DEBUG
              Serial.printf( "en configuracion de MULTIRRIEGO, setMultibyId devuelve: Grupo%d (%s) multi.size=%d \n" , n_grupo, multi.desc, *multi.size);
            #endif            
            displayGrupo(multi.serie, *multi.size);
            multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
            display->print("PUSH");
            led(Boton[bID_bIndex(*multi.id)].led,ON);
          } 
          break;
        case bPAUSE:
          if(!boton->estado) return; //no se procesa el release del PAUSE
          if(!configure->configuring()) { //si no estamos ya configurando algo
            configure->configureTime();   //configuramos tiempo por defecto
            Serial.println(F("[ConF] configurando tiempo riego por defecto"));
            delay(500);
            break;
          }
          if(configure->configuringTime()) {
            Serial.printf( "[ConF] Save DEFAULT TIME, minutes: %d  secons: %d \n", minutes, seconds);
            config.minutes = minutes;
            config.seconds = seconds;
            saveConfig = true;
            bipOK(3);
            configure->stop();
            break;
          }
          if(configure->configuringIdx()) {
            int bIndex = configure->getActualIdxIndex();
            int zIndex = bID_zIndex(Boton[bIndex].id);
            Boton[bIndex].idx = (uint16_t)value;
            config.botonConfig[zIndex].idx = (uint16_t)value;
            saveConfig = true;
            Serial.printf( "[ConF] Save Zona%d (%s) IDX value: %d \n", zIndex+1, Boton[bIndex].desc, value);
            value = savedValue;
            bipOK(3);
            led(Boton[bIndex].led,OFF);
            configure->stop();
            break;
          }
          if(configure->configuringMulti()) {
            // actualizamos config con las zonas introducidas
            if (multi.w_size) {  //solo si se ha pulsado alguna
              *multi.size = multi.w_size;
              int g = configure->getActualGrupo();
              for (int i=0; i<multi.w_size; ++i) {
                config.groupConfig[g-1].serie[i] = bID_zIndex(multi.serie[i])+1;
              }
              Serial.printf( "[ConF] SAVE PARM Multi : GRUPO%d  tamaño: %d (%s)\n", g , *multi.size , multi.desc );
              printMultiGroup(config, g-1);
              saveConfig = true;
              bipOK(3);
            }
            //value = savedValue;
            ultimosRiegos(HIDE);
            led(Boton[bID_bIndex(*multi.id)].led,OFF);
            configure->stop();
          }
          break;
        case bSTOP:
          if(!boton->estado) { //release STOP salvamos si procede y salimos de ConF
            configure->stop();
            if (saveConfig) {
              Serial.println(F("saveConfig=true  --> salvando parametros a fichero"));
              if (saveConfigFile(parmFile, config)) infoDisplay("SAUE", DEFAULTBLINK, BIPOK, 5);
              saveConfig = false;
            }
            ledConf(OFF);
            Estado.estado = STANDBY;
            if (webServerAct) Serial.println(F("[ConF][WS] desactivado webserver"));
            webServerAct = false; //al salir de modo ConF no procesaremos peticiones al webserver
            standbyTime = millis();
            if (savedValue>0) value = savedValue;  // para que restaure reloj aunque no salvemos con pause el valor configurado
            ultimosRiegos(HIDE);
            led(Boton[bID_bIndex(*multi.id)].led,OFF);
            StaticTimeUpdate();
          }
          break;
        default:  //procesamos boton de ZONAx
          if (configure->configuringMulti()) {  //Configuramos el multirriego seleccionado
            if (multi.w_size < 16) {  //max. 16 zonas por grupo
              multi.serie[multi.w_size] = boton->id;
              Serial.printf("[ConF] añadiendo ZONA%d (%s) \n",zIndex+1, boton->desc);
              multi.w_size = multi.w_size + 1;
              led(Boton[bIndex].led,ON);
            }
            else longbip(1);
          }
          if (!configure->configuring()) {   //si no estamos configurando nada, configuramos el idx
            //savedValue = value;
            Serial.printf("[ConF] configurando IDX boton: %s \n",boton->desc);
            configure->configureIdx(bIndex);
            value = boton->idx;
            led(Boton[bIndex].led,ON);
          }
      }
    }
  }
  else procesaEncoder();
};


void procesaEstadoError(void)
{
  if(boton == NULL) return;  // si no se ha pulsado ningun boton salimos
  //En estado error no se responde a botones, a menos que este sea:
  //   - PAUSE y pasamos a modo NONETWORK
  //   - STOP y en este caso reseteamos 
  if(boton->id == bPAUSE && boton->estado) {  //evita procesar el release del pause
  //Si estamos en error y pulsamos pausa, nos ponemos en estado NONETWORK para test
    Estado.estado = STANDBY;
    Estado.fase = CERO;
    NONETWORK = true;
    Serial.println(F("estado en ERROR y PAUSA pulsada pasamos a modo NONETWORK y reseteamos"));
    bip(2);
    led(LEDB,ON);
    //reseteos varios:
    tic_parpadeoLedZona.detach(); //detiene parpadeo led zona con error
    stopAllRiego(false);          //apaga leds activos
    tic_parpadeoLedON.detach();   //detiene parpadeo led ON por si estuviera activado
    led(LEDR,ON);                 // y lo deja fijo
    multiriego = false;
    multiSemaforo = false;
    errorOFF = false;
    displayOff = false;
    standbyTime = millis();
    StaticTimeUpdate();
  }
  if(boton->id == bSTOP) {
  //Si estamos en ERROR y pulsamos o liberamos STOP, reseteamos
    Serial.println(F("ERROR + STOP --> Reset....."));
    longbip(3);
    ESP.restart();  
  }
};


void procesaEstadoRegando(void)
{
  tiempoTerminado = T.Timer();
  if (T.TimeHasChanged()) refreshTime();
  if (tiempoTerminado == 0) {
    Estado.estado = TERMINANDO;
  }
};


void procesaEstadoTerminando(void)
{
  bip(5);
  stopRiego(ultimoBoton->id);
  if (Estado.estado == ERROR) return; //no continuamos si se ha producido error al parar el riego
  display->blink(DEFAULTBLINK);
  led(Boton[bID_bIndex(ultimoBoton->id)].led,OFF);
  StaticTimeUpdate();
  standbyTime = millis();
  Estado.estado = STANDBY;
  //Comprobamos si estamos en un multiriego
  if (multiriego) {
    multi.actual++;
    if (multi.actual < *multi.size) {
      //Simular la pulsacion del siguiente boton de la serie de multiriego
      boton = &Boton[bID_bIndex(multi.serie[multi.actual])];
      multiSemaforo = true;
    }
    else {
      bipOK(5);
      multiriego = false;
      multiSemaforo = false;
        Serial.printf("MULTIRRIEGO %s terminado \n", multi.desc);
      led(Boton[bID_bIndex(*multi.id)].led,OFF);
    }
  }
};


void procesaEstadoStandby(void)
{
  Boton[bID_bIndex(bPAUSE)].flags.holddisabled = true;
  //Apagamos el display si ha pasado el lapso
  if (reposo) standbyTime = millis();
  else {
    if (millis() > standbyTime + (1000 * STANDBYSECS)) {
      Serial.println(F("Entramos en reposo"));
      reposo = true;
      display->clearDisplay();
    }
  }
  procesaEncoder();
};


void procesaEstadoStop(void)
{
  //En stop activamos el comportamiento hold de pausa
  Boton[bID_bIndex(bPAUSE)].flags.holddisabled = false;
  //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
  if(reposo && !displayOff) {
    if (millis() > standbyTime + (4 * 1000 * STANDBYSECS) && reposo) {
      display->clearDisplay();
      displayOff = true;
    }
  }
};



/**---------------------------------------------------------------
 * Chequeo de perifericos
 */
void check(void)
{
  display->print("----");
  #ifndef DEBUG
  initLeds();
  display->check(1);
  #endif
}


/**---------------------------------------------------------------
 * Lee factores de riego del domoticz
 */
void initFactorRiegos()
{
  #ifdef TRACE
    Serial.println(F("TRACE: in initFactorRiegos"));
  #endif
  //inicializamos a valor 100 por defecto para caso de error
  for(uint i=0;i<NUMZONAS;i++) {
    factorRiegos[i]=100;
  }
  //leemos factores del Domoticz
  for(uint i=0;i<NUMZONAS;i++) 
  {
    int bIndex = bID_bIndex(ZONAS[i]);
    uint factorR = getFactor(Boton[bIndex].idx);
    if(factorR == 999) break; //en modo NONETWORK no continuamos iterando si no hay conexion
    if(Estado.estado == ERROR) {  //al primer error salimos
      if(Estado.fase == E3) {     // y señalamos zona que falla si no es error general de conexion
        ledID = Boton[bIndex].led;
        tic_parpadeoLedZona.attach(0.4,parpadeoLedZona);
      }
      break;
    factorRiegos[i] = factorR;
    }
    if (strlen(descDomoticz)) {
      //actualizamos en Boton la DESCRIPCION con la recibida del Domoticz (campo Name)
      if (xNAME) {
        strlcpy(Boton[bIndex].desc, descDomoticz, sizeof(Boton[bIndex].desc));
        Serial.printf("\tdescripcion ZONA%d actualizada en boton \n", i+1);
      }
      //printCharArray(config.botonConfig[i].desc, sizeof(config.botonConfig[i].desc));
      //si el parm desc estaba vacio actualizamos en todo caso (en config y Boton)
      if (config.botonConfig[i].desc[0] == 0) {
        strlcpy(config.botonConfig[i].desc, descDomoticz, sizeof(config.botonConfig[i].desc));
        strlcpy(Boton[bIndex].desc, descDomoticz, sizeof(Boton[bIndex].desc));
        Serial.printf("\tdescripcion ZONA%d incluida en config \n", i+1);
        // saveConfig = true;   TODO ¿deberiamos salvarlo?
      }  
    }
  }
  //printParms(config);
  #ifdef VERBOSE
    //Leemos los valores para comprobar que lo hizo bien
    Serial.print(F("Factores de riego "));
    factorRiegosOK ? Serial.println(F("leidos: ")) :  Serial.println(F("(simulados): "));
    for(uint i=0;i<NUMZONAS;i++) {
      Serial.printf("\tfactor ZONA%d: %d (%s) \n", i+1, factorRiegos[i], Boton[bID_bIndex(ZONAS[i])].desc);
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
     Serial.println(F("[ERROR] initClock: no se ha recibido time por NTP"));
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
      for(uint i=0;i<NUMZONAS;i++) {
        if(lastRiegos[i] > previousMidnight(t)) {
            led(Boton[bID_bIndex(ZONAS[i])].led,ON);
        }
      }
      display->printTime(hour(t),minute(t));
      break;
    case HIDE:
      StaticTimeUpdate();
      for(unsigned int i=0;i<NUMZONAS;i++) {
        led(Boton[bID_bIndex(ZONAS[i])].led,OFF);
      }
      break;
  }
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
       Serial.print(F("i"));
      #endif
      value -= Encoder->getValue();
      if (value > 1000) value = 1000;
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
  reposo = false;
  StaticTimeUpdate();
  standbyTime = millis();
}


void initLastRiegos()
{
  for(uint i=0;i<NUMZONAS;i++) {
   lastRiegos[i] = 0;
  }
}


//Inicia el riego correspondiente al idx del boton (id) pulsado
bool initRiego(uint16_t id)
{
  int bIndex = bID_bIndex(id);
  #ifdef DEBUG
    Serial.printf("Boton: %s boton.index: %d \n", boton->desc, bIndex);
  #endif
  int zIndex = bID_zIndex(id);
  time_t t;
  if(zIndex == 999) return false;
  Serial.printf( "Iniciando riego: %s \n", Boton[bIndex].desc);
  led(Boton[bIndex].led,ON);
  utc = timeClient.getEpochTime();
  t = CE.toLocal(utc,&tcr);
  lastRiegos[zIndex] = t;
  return domoticzSwitch(Boton[bIndex].idx,(char *)"On");
}


//Termina el riego correspondiente al idx del boton (id) pulsado
bool stopRiego(uint16_t id)
{
  int bIndex = bID_bIndex(id);
  ledID = Boton[bIndex].led;
  #ifdef DEBUG
  Serial.printf( "Terminando riego: %s \n", Boton[bIndex].desc);
  #endif
  domoticzSwitch(Boton[bIndex].idx,(char *)"Off");
  if (Estado.estado != ERROR && !simErrorOFF) Serial.printf( "Terminado OK riego: %s \n" , Boton[bIndex].desc );
  else {     //avisa de que no se ha podido terminar un riego
    if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
      if (simErrorOFF) statusError(E5,5); // simula error si simErrorOFF es true
      errorOFF = true;  // recordatorio error
      tic_parpadeoLedON.attach(0.2,parpadeoLedON);
      tic_parpadeoLedZona.attach(0.4,parpadeoLedZona);
    } 
    return false;
  }
  return true;
}


//Pone a off todos los leds de riegos y detiene riegos (solo si la llamamos con true)
bool stopAllRiego(bool stop)
{
  //Apago los leds de multiriego
  led(Boton[bID_bIndex(*multi.id)].led,OFF);
  //Apago los leds de riego
  for(unsigned int i=0;i<NUMZONAS;i++) {
    led(Boton[bID_bIndex(ZONAS[i])].led,OFF);
    if (stop) {  //paramos todas las zonas de riego
      if(!stopRiego(ZONAS[i])) { 
        return false; //al primer error salimos
      }  
    }
  }
  return true;
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


void bipOK(int veces)
{
    led(BUZZER,ON);
    delay(500);
    led(BUZZER,OFF);
    delay(100);
    bip(veces);
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


/**
 * @brief muestra en el display texto informativo y suenan bips de aviso
 * 
 * @param textDisplay = texto a mostrar en el display
 * @param dnum = veces que parpadea el texto en el display
 * @param btype = tipo de bip emitido
 * @param bnum = numero de bips emitidos
 */
void infoDisplay(const char *textDisplay, int dnum, int btype, int bnum) {
  display->print(textDisplay);
  if (btype == LONGBIP) longbip(bnum);
  if (btype == BIP) bip(bnum);
  if (btype == BIPOK) bipOK(bnum);
  display->blink(dnum);
}

/**---------------------------------------------------------------
 * Comunicacion con Domoticz usando httpGet
 */
String httpGetDomoticz(String message) 
{
  #ifdef TRACE
    Serial.println(F("TRACE: in httpGetDomoticz"));
  #endif
  String tmpStr = "http://" + String(config.domoticz_ip) + ":" + config.domoticz_port + String(message);
  #ifdef DEBUG
    Serial.print(F("TMPSTR: "));Serial.println(tmpStr);
  #endif
  httpclient.begin(client, tmpStr); // para v3.0.0 de platform esp8266
  String response = "{}";
  int httpCode = httpclient.GET();
  if(httpCode > 0) {
    if(httpCode == HTTP_CODE_OK) {
      response = httpclient.getString();
      #ifdef EXTRADEBUG
        Serial.print(F("httpGetDomoticz RESPONSE: "));Serial.println(response);
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
      Serial.println(F("[ERROR] httpGetDomoticz: SE HA DEVUELTO ERROR")); 
    #endif
    Estado.estado = ERROR;
    return "ErrX";
  }
  httpclient.end();
  return response;
}


/**---------------------------------------------------------------
 * lee factor de riego del Domoticz, almacenado en campo comentarios
 */
int getFactor(uint16_t idx)
{
  #ifdef TRACE
    Serial.println(F("TRACE: in getFactor"));
  #endif
  // si el IDX es 0 devolvemos 0 sin procesarlo (boton no asignado)
  if(idx == 0) return 0;
  factorRiegosOK = false;
  if(!checkWifi()) {
    if(NONETWORK) return 999; //si estamos en modo NONETWORK devolvemos 999 y no damos error
    else {
      statusError(E1,3);
      return 100;
    }
  }
  char JSONMSG[200]="/json.htm?type=devices&rid=%d";
  char message[250];
  sprintf(message,JSONMSG,idx);
  String response = httpGetDomoticz(message);
  //procesamos la respuesta para ver si se ha producido error:
  if (Estado.estado == ERROR && response.startsWith("Err")) {
    if (NONETWORK) {  //si estamos en modo NONETWORK devolvemos 999 y no damos error
      Estado.estado = STANDBY;
      return 999;
    }
    if(response == "ErrX") statusError(E3,3);
    else statusError(E2,3);
    #ifdef DEBUG
      Serial.printf("GETFACTOR IDX: %d [HTTP] GET... failed\n", idx);
    #endif
    return 100;
  }
  /* Teoricamente ya tenemos en response el JSON, lo procesamos
     Si el IDX no existe Domoticz no devuelve error, asi que hay que controlarlo
     Ante cualquier problema (no de error) devolvemos 100% para no factorizar ese riego,
     sin error si VERIFY=false o Err2 si VERIFY=true
  */
  // ojo el ArduinoJson Assistant recomienda usar char* en vez de String (gasta menos memoria)
  char* response_pointer = &response[0];
  DynamicJsonDocument jsondoc(2048);
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    Serial.print(F("[ERROR] getFactor: deserializeJson() failed: "));
    Serial.println(error.f_str());
    if(!VERIFY) return 100;
    else {
      statusError(E2,3);  //TODO ¿deberiamos devolver E3?
      return 100;
    }
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *factorstr = jsondoc["result"][0]["Description"];
  if(factorstr == NULL) {
    //El rid (idx) no esta definido en el Domoticz
    #ifdef VERBOSE
      Serial.printf("El idx %d no se ha podido leer del JSON\n",idx);
    #endif
    if(!VERIFY) return 100;
    else {
      statusError(E3,3);
      return 100;
    }
  }
  #ifdef xNAME
    //extraemos la DESCRIPCION para ese boton en Domoticz del json (campo Name)
    strlcpy(descDomoticz, jsondoc["result"][0]["Name"] | "", sizeof(descDomoticz));
  #endif  
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
    Serial.println(F("TRACE: in domoticzSwitch"));
  #endif
  if(NONETWORK || idx == 0) return true; //simulamos que ha ido OK
  if(!checkWifi()) {
    statusError(E1,3);
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
        if(strcmp(msg,"On") == 0 ) statusError(E4,3); //error al iniciar riego
        else statusError(E5,5); //error al parar riego
      }
      else statusError(E2,3); //otro error al comunicar con domoticz
    }
    Serial.printf("DOMOTICZSWITH IDX: %d fallo en %s\n", idx, msg);
    return false;
  }
  return true;
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
  #ifdef WEBSERVER
  if (Estado.estado == CONFIGURANDO && webServerAct) {
    server.handleClient();
    MDNS.update();
  }  
  #endif
  if (!flagV) return;      //si no activada por Ticker salimos sin hacer nada
  if (Estado.estado == STANDBY) Serial.print(F("."));
  if (errorOFF) bip(2);  //recordatorio error grave
  if (!NONETWORK && (Estado.estado == STANDBY || (Estado.estado == ERROR && !connected))) {
    if (checkWifi()) Estado.estado = STANDBY; //verificamos si seguimos connectados a la wifi
    if (connected && falloAP) {
      Serial.println(F("Wifi conectada despues Setup, leemos factor riegos"));
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
void statusError(uint8_t errorID, int n) 
{
  char errorText[7] = "Err";    
  Estado.estado = ERROR;
  Estado.fase = errorID;
  if (errorID == E0) strcpy(errorText, "Err0");
  else sprintf(errorText, "Err%d", errorID);
  Serial.printf("[statusError]: %s \n", errorText);
  display->print(errorText);
  longbip(n);
}


void parpadeoLedON()
{
  byte estado = ledStatusId(LEDR);
  led(LEDR,!estado);
}
void parpadeoLedZona()
{
  byte estado = ledStatusId(ledID);
  led(ledID,!estado);
}

void parpadeoLedConf()
{
  byte estado = ledStatusId(LEDR);
  led(LEDR,!estado);
  estado = ledStatusId(LEDG);
  led(LEDG,!estado);
}

//activa o desactiva el(los) led(s) indicadores de que estamos en modo configuracion
void ledConf(int estado)
{
  if(estado == ON) 
  {
    //parpadeo alterno leds ON/RED
    led(LEDB,OFF);
    led(LEDG,OFF);
    tic_parpadeoLedConf.attach(0.7,parpadeoLedConf);
    //led(Boton[bID_bIndex(bCONFIG)].led,ON); 
  }
  else 
  {
    tic_parpadeoLedConf.detach();   //detiene parpadeo alterno leds ON/RED
    led(LEDR,ON);                   // y los deja segun estado
    NONETWORK ? led(LEDB,ON) : led(LEDB,OFF);
    checkWifi();
    //led(Boton[bID_bIndex(bCONFIG)].led,OFF);
  }
}

void setupParm()
{
  #ifdef TRACE
    Serial.println(F("TRACE: in setupParm"));
  #endif
  if(clean_FS) cleanFS();
  #ifdef DEBUG
    filesInfo();
    Serial.printf( "initParm= %d \n", initFlags.initParm );
  #endif
  if( initFlags.initParm) {
    Serial.println(F(">>>>>>>>>>>>>>  cargando parametros por defecto  <<<<<<<<<<<<<<"));
    bool bRC = copyConfigFile(defaultFile, parmFile); // defaultFile --> parmFile
    if(bRC) {
      Serial.println(F("carga parametros por defecto OK"));
      //señala la carga parametros por defecto OK
      infoDisplay("dEF-", DEFAULTBLINK, BIPOK, 3);
    }  
    else    Serial.println(F("[ERROR] carga parametros por defecto"));
  }
  if (!setupConfig(parmFile, config)) {
    Serial.printf("[ERROR] Leyendo fichero parametros %s \n", parmFile);
    if (!setupConfig(defaultFile, config)) {
      Serial.printf("[ERROR] Leyendo fichero parametros %s \n", defaultFile);
    }
  }
  if (!config.initialized) zeroConfig(config);  //init config con zero-config
  #ifdef VERBOSE
    if (config.initialized) Serial.print(F("Parametros cargados, "));
    else Serial.print(F("Parametros zero-config, "));
    printParms(config);
  #endif
}


bool setupConfig(const char *p_filename, Config_parm &cfg) 
{
  Serial.printf("Leyendo fichero parametros %s \n", p_filename);
  bool loaded = loadConfigFile(p_filename, config);
  minutes = config.minutes;
  seconds = config.seconds;
  value = ((seconds==0)?minutes:seconds);
  if (loaded) {
    for(int i=0;i<NUMZONAS;i++) {
      int bIndex = bID_bIndex(ZONAS[i]);
      //NOTA: i (zIndex) y bIndex seran iguales si el orden de los botones de las zonas en ZONA[] y Boton[] 
      //es el mismo (seria mas facil si lo fueran).... @TODO ¿como detectarlo al compilar?
      if (bIndex != i) Serial.println(F("\t\t\t@@@@@@@@@@@@  bIndex != zIndex  @@@@@@@@@@"));
      //actualiza el IDX leido sobre estructura Boton:
      Boton[bIndex].idx = config.botonConfig[i].idx;
      //actualiza la descripcion leida sobre estructura Boton:
      strlcpy(Boton[bIndex].desc, config.botonConfig[i].desc, sizeof(Boton[bIndex].desc));
    }
    #ifdef DEBUG
      printFile(p_filename);
    #endif
    return true;
  }  
  Serial.println(F("[ERROR] parámetros de configuración no cargados"));
  return false;
}

#ifdef WEBSERVER
void setupWS()
{
  MDNS.begin(host);
  httpUpdater.setup(&server, update_path, update_username, update_password);
  server.begin();
  MDNS.addService("http", "tcp", 8080);
  Serial.println(F("[WS] HTTPUpdateServer ready!"));
  Serial.printf("[WS]    --> Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n\n", host, update_path, update_username, update_password);
}
#endif

// funciones solo usadas en DEVELOP
// (es igual, el compilador no las incluye si no son llamadas)
#ifdef DEBUG
  //imprime contenido actual de la estructura multi
  void printMulti()
  {
      Serial.println(F("TRACE: in printMulti"));
      Serial.printf("MULTI Boton_id x%x: size=%d (%s)\n", *multi.id, *multi.size, multi.desc);
      for(int j = 0; j < *multi.size; j++) {
        Serial.printf("  Zona  id: x%x \n", multi.serie[j]);
      }
    Serial.println();
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
          Serial.println(F("Teclee: "));
          Serial.println(F("   1 - simular error NTP"));
          Serial.println(F("   2 - simular error apagar riego"));
          Serial.println(F("   9 - anular simulacion errores"));
      }
      switch (inputNumber) {
            case 1:
                Serial.println(F("recibido:   1 - simular error NTP"));
                timeOK = false;
                break;
            case 2:
                Serial.println(F("recibido:   2 - simular error apagar riego"));
                simErrorOFF = true;
                break;
            case 9:
                Serial.println(F("recibido:   9 - anular simulacion errores"));
                simErrorOFF = false;
                timeOK = true;                         
      }
    }
  }

#endif  
