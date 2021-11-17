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
                NONETWORK=false;
                VERIFY=true; 
  #endif
  #ifdef DEMO
                NONETWORK=true;
                VERIFY=false;
  #endif

  Serial.begin(115200);
  Serial.print(F("\n\n"));
  Serial.println("CONTROL RIEGO V" + String(VERSION) + "    Built on " __DATE__ " at " __TIME__ );
  strncpy(version_n, VERSION, 10); //eliminamos "." y "-" para mostrar version en el display
  std::remove(std::begin(version_n),std::end(version_n),'.');
  std::remove(std::begin(version_n),std::end(version_n),'-');
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
  //setup parametros configuracion
  #ifdef EXTRADEBUG
    printFile(parmFile);
  #endif
  setupParm();
  //led encendido
  led(LEDR,ON);
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  setupRedWM(config);
  if (saveConfig) {
    if (saveConfigFile(parmFile, config))  bipOK(3);;
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
    //Serial.println(F("TRACE: in loop"));
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
    //Serial.println(F("TRACE: in procesaBotones"));
    Serial.print(F("B"));
  #endif
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  //NOTA: el encoderSW esta en estado HIGH en reposo y en estado LOW cuando esta pulsado
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
    Serial.println(F("Salimos de reposo -1-"));
    reposo = false;
    displayOFF = false;
    standbyTime = millis();
    if(Estado.estado == STOP) display->print("StoP");
    else StaticTimeUpdate();
    return;
  }
  //En estado error procesaremos el boton pulsado (si procede) en procesaEstados
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
    //Serial.println(F("TRACE: in procesaEstados"));
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
    case STANDBY:
      procesaEstadoStandby();
      break;
    case TERMINANDO:
      procesaEstadoTerminando();
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
    statusError("Err0", 3);  //no se ha podido cargar parámetros de ficheros -> señalamos el error
  return;
  }
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
      display->print("StoP");
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
 *    - Grupo1 (arriba) --> en ese caso cargamos los parametros del fichero de configuracion por defecto
 *    - Grupo3 (abajo) --> en ese caso borramos red wifi almacenada en el ESP8266
 */
void setupInit(void) {
  #ifdef TRACE
    Serial.println(F("TRACE: in setupInit"));
  #endif
  //S_initFlags initFlags = 0;
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
        if(Estado.estado != ERROR) Estado.estado = REGANDO; // para que no borre ERROR
        else boton = NULL; //para que procesaEstadoError no pase directamente a NONETWORK
        break;
      case CONFIGURANDO:
        //if(!configure->configuringIdx() && !configure->configuringTime() && !configure->configuringMulti()) {
        if(!configure->configuring()) { //si no estamos ya configurando algo
          configure->configureTime();
          Serial.println(F("[ConF] configurando tiempo riego por defecto"));
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
              if(value!=savedValue) Serial.printf("#01 savedValue: %d  value: %d \n",savedValue,value);
            #endif
            savedValue = value;
          }
          else {   //si esta pulsado encoderSW hacemos un soft reset
            Serial.println(F("ConF + encoderSW + PAUSA --> Reset....."));
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
    //De alguna manera esta regando y hay que parar
    if (Estado.estado == REGANDO || Estado.estado == MULTIREGANDO || Estado.estado == PAUSE) {
      display->print("StoP");
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
      display->print("StoP");
      bip(1);
      longbip(1);
      stopAllRiego(true);  // apagar leds y parar riegos ¿?
      standbyTime = millis();
      if (Estado.estado == ERROR) return; //error en stopAllRiego
      Estado.estado = STOP;
      //pasamos directamente a reposo
      //Serial.println(F("Stanby + Stop : Entramos en reposo"));
      reposo = true;
      displayOFF = false;
    }
  }
  //si hemos liberado STOP: salimos del estado stop o de configuracion
  if (!boton->estado && (Estado.estado == STOP || Estado.estado == CONFIGURANDO)) {
    if (savedValue>0) {
      #ifdef DEBUG
      if(value!=savedValue) Serial.printf("#02 savedValue: %d  value: %d \n",savedValue,value);
      #endif
      value = savedValue;  // para que restaure reloj aunque no salvemos con pause el valor configurado
    }
    StaticTimeUpdate();
    led(Boton[bId2bIndex(bCONFIG)].led,OFF);
    reposo = false; //por si salimos de stop antinenes
    displayOFF = false;
    //Estado.estado = STANDBY;
    //dejamos el procesado del release del STOP en modo ConF para que actue el codigo de procesaEstados
    if (Estado.estado == STOP) Estado.estado = STANDBY;
  }
  standbyTime = millis();
}


bool procesaBotonMultiriego(void)
{
  if (Estado.estado == CONFIGURANDO) {
    if (configure->configuring()) return false; //si ya estamos configurando algo salimos
    #ifdef DEBUG
      if(value!=savedValue) Serial.printf("#03 savedValue: %d  value: %d \n",savedValue,value);
    #endif
    savedValue = value;
    int n_grupo = setMultibyId(getMultiStatus(), config);
    if (encoderSW) 
    {
      //encoderSW pulsado: actuamos segun posicion selector multirriego
      if (n_grupo == 1) {  // copiamos fichero parametros en fichero default
        if (copyConfigFile(parmFile, defaultFile)) {
          Serial.println(F("[ConF] salvado fichero de parametros actuales como DEFAULT"));
          display->print("-dEF");
          bipOK(5);
          delay(1000);
          display->print("ConF"); 
        }
      } 
      if (n_grupo == 3) {  // activamos AP y portal de configuracion (bloqueante)
        Serial.println(F("[ConF] encoderSW + selector ABAJO: activamos AP y portal de configuracion"));
        starConfigPortal(config);
      }
      return false;    //para que procese el BREAK al volver a procesaBotones  
    } 
    //Configuramos el grupo de multirriego activo
    configure->configureMulti(n_grupo);
    Serial.printf( "[ConF] configurando: GRUPO%d (%s) \n" , n_grupo, multi.desc);
    #ifdef DEBUG
      Serial.printf( "en configuracion de MULTIRRIEGO, setMultibyId devuelve: Grupo%d (%s) multi.size=%d \n" , n_grupo, multi.desc, *multi.size);
    #endif            
    displayGrupo(multi.serie, *multi.size);
    //*multi.size = 0 ; // borramos grupo actual
    multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
    display->print("PUSH");
    return false;    //para que procese el BREAK al volver a procesaBotones  
  }
  if (Estado.estado == STANDBY && !multiriego) {
    bip(4);
    multiriego = true;
    multi.actual = 0;
    #ifdef DEBUG
      int n_grupo = setMultibyId(getMultiStatus(), config);
      Serial.printf( "en MULTIRRIEGO, setMultibyId devuelve: Grupo%d (%s) multi.size=%d \n" , n_grupo, multi.desc, *multi.size);
      for (int k=0; k < *multi.size; k++) Serial.printf( "       multi.serie: x%x \n" , multi.serie[k]);
      Serial.printf( "en MULTIRRIEGO, encoderSW status  : %d \n", encoderSW );
    #endif
    // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
    // y mostramos en el display la version del programa.
    if (encoderSW) {
      display->print(version_n);
      displayGrupo(multi.serie, *multi.size);
      multiriego = false;
      #ifdef DEBUG
        Serial.printf( "en MULTIRRIEGO + encoderSW, display de grupo: %s tamaño: %d \n", multi.desc , *multi.size );
      #endif
      StaticTimeUpdate();
      return false;    //para que procese el BREAK al volver a procesaBotones         
    }  
    else {
      //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
      //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
      Serial.printf("MULTIRRIEGO iniciado: %s \n", multi.desc);
      led(Boton[bId2bIndex(*multi.id)].led,ON);
      boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
    }
  }
  return true;   //para que continue con el case DEFAULT al volver
}


void procesaBotonZona(void)
{
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
          return;
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
        if(value!=savedValue) Serial.printf("#04 savedValue: %d  value: %d \n",savedValue,value);
      #endif
      savedValue = value;
      value = factorRiegos[idarrayRiego(boton->id)];
      display->print(value);
      delay(2000);
      #ifdef DEBUG
        if(value!=savedValue) Serial.printf("#05 savedValue: %d  value: %d \n",savedValue,value);
      #endif
      value = savedValue;  // para que restaure reloj
      led(Boton[index].led,OFF);
      StaticTimeUpdate();
    }
  }
  if (Estado.estado == CONFIGURANDO ) {
    if (configure->configuringMulti()) {  //Configuramos el multirriego seleccionado
      if (multi.w_size < 16) {
        #ifdef DEBUG
          if(value!=savedValue) Serial.printf("#07 savedValue: %d  value: %d \n",savedValue,value);
        #endif
        savedValue = value;
        multi.serie[multi.w_size] = boton->id;
        Serial.printf("[ConF] añadiendo ZONA%d (%s) \n",bId2bIndex(boton->id)+1, boton->desc);
        multi.w_size = multi.w_size + 1;
        //Serial.printf("[ConF] configurando multigrupo numero elementos (despues): %d \n",multi.w_size);
        led(Boton[bId2bIndex(boton->id)].led,ON);
      }
    }
    if (!configure->configuring()) {   //si no estamos configurando nada, configuramos el idx
      #ifdef DEBUG
        if(value!=savedValue) Serial.printf("#06 savedValue: %d  value: %d \n",savedValue,value);
      #endif
      savedValue = value;
      Serial.printf("[ConF] configurando IDX boton: %s \n",boton->desc);
      configure->configureIdx(bId2bIndex(boton->id));
      value = boton->idx;
    }
  }
}


void procesaEstadoConfigurando()
{
  //Deshabilitamos el hold de Pause
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
  if (boton != NULL) {
    if (boton->flags.action) {
      //Serial.println(F( "En estado CONFIGURANDO pulsado ACTION" );
      switch(boton->id) {
        case bMULTIRIEGO:
          break;
        case bPAUSE:
          if(!boton->estado) return; //no se procesa el release del PAUSE
          if(configure->configuringTime()) {
            Serial.printf( "[ConF] Save DEFAULT TIME, minutes: %d  secons: %d \n", minutes, seconds);
            config.minutes = minutes;
            config.seconds = seconds;
            saveConfig = true;
            bipOK(3);
            configure->stop();
          }
          if(configure->configuringIdx()) {
            int zonaN = configure->getActualIdxIndex();
            Boton[zonaN].idx = (uint16_t)value;
            config.botonConfig[zonaN].idx = (uint16_t)value;
            saveConfig = true;
            Serial.printf( "[ConF] Save Zona%d (%s) IDX value: %d \n", zonaN+1, Boton[zonaN].desc, value);
            #ifdef DEBUG
              if(value!=savedValue) Serial.printf("#08 savedValue: %d  value: %d \n",savedValue,value);
            #endif
            value = savedValue;
            bipOK(3);
            configure->stop();
          }
          if(configure->configuringMulti()) {
            // actualizamos config con las zonas introducidas
            if (multi.w_size) {  //solo si se ha pulsado alguna
              *multi.size = multi.w_size;
              int g = configure->getActualGrupo();
              for (int i=0; i<multi.w_size; ++i) {
                config.groupConfig[g-1].serie[i] = bId2bIndex(multi.serie[i])+1;
              }
              Serial.printf( "[ConF] SAVE PARM Multi : GRUPO%d  tamaño: %d (%s)\n", g+1 , *multi.size , multi.desc );
              printMultiGroup(config, g-1);
              saveConfig = true;
              bipOK(3);
            }
            #ifdef DEBUG
              if(value!=savedValue) Serial.printf("#09 savedValue: %d  value: %d \n",savedValue,value);
            #endif
            value = savedValue;
            ultimosRiegos(HIDE);
            configure->stop();
          }
          break;
        case bSTOP:
          if(!boton->estado) {
            configure->stop();
            //Serial.println(F( "release de STOP en modo ConF" );
            if (saveConfig) {
              Serial.println(F("saveConfig=true  --> salvando parametros a fichero"));
              if (saveConfigFile(parmFile, config)) bipOK(3);
              saveConfig = false;
            }
            ultimosRiegos(HIDE);  
            Estado.estado = STANDBY;
            standbyTime = millis();
          }
          break;
      }
    }
  }
  else procesaEncoder();
};


void procesaEstadoError(void)
{
  if(boton == NULL) return;  // si no se ha pulsado ningun boton salimos
  //En estado error salimos sin procesar el boton que se haya pulsado, a menos que este sea:
  //   - PAUSE y pasamos a modo NONETWORK
  //   - STOP y en este caso reseteamos 
  if(boton->id == bPAUSE && boton->estado) {  //evita procesar el release del pause
  //Si estamos en error y pulsamos pausa, nos ponemos en estado NONETWORK para test
    Estado.estado = STANDBY;
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
    displayOFF = false;
    standbyTime = millis();
    StaticTimeUpdate();
  }
  if(boton->id == bSTOP) {
  //Si estamos en ERROR y pulsamos STOP, reseteamos
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

void procesaEstadoStandby(void)
{
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = true;
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

void procesaEstadoTerminando(void)
{
  bip(5);
  stopRiego(ultimoBoton->id);
  if (Estado.estado == ERROR) return; //no continuamos si se ha producido error al parar el riego
  display->blink(DEFAULTBLINK);
  led(Boton[bId2bIndex(ultimoBoton->id)].led,OFF);
  StaticTimeUpdate();
  standbyTime = millis();
  Estado.estado = STANDBY;
  //Comprobamos si estamos en un multiriego
  if (multiriego) {
    multi.actual++;
    if (multi.actual < *multi.size) {
      //Simular la pulsacion del siguiente boton de la serie de multiriego
      boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
      multiSemaforo = true;
    }
    else {
      bipOK(5);
      multiriego = false;
      multiSemaforo = false;
        Serial.printf("MULTIRRIEGO %s terminado \n", multi.desc);
      led(Boton[bId2bIndex(*multi.id)].led,OFF);
    }
  }
};

void procesaEstadoStop(void)
{
  //En stop activamos el comportamiento hold de pausa
  Boton[bId2bIndex(bPAUSE)].flags.holddisabled = false;
  //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
  if(reposo && !displayOFF) {
    if (millis() > standbyTime + (4 * 1000 * STANDBYSECS) && reposo) {
      display->clearDisplay();
      displayOFF = true;
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

uint idarrayRiego(uint16_t id)
{
  for (uint i=0;i<NUMZONAS;i++) {
    if(ZONAS[i] == id) return i;
  }
  return 999;
}

void timerIsr()
{
  Encoder->service();
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
    int zonaN = bId2bIndex(ZONAS[i]);
    factorRiegos[i]=getFactor(Boton[zonaN].idx);
    if(Estado.estado == ERROR) {  //al primer error salimos y señalamos zona que falla
      if(connected) { //solo señalamos zona si no es error general de conexion
        ledID = Boton[zonaN].led;
        tic_parpadeoLedZona.attach(0.4,parpadeoLedZona);
      }
      break;
    }
    if (sizeof(descFR)) {
      //actualizamos en Boton la DESCRIPCION con la recibida del Domoticz (campo Name)
      if (xNAME) {
        strlcpy(Boton[zonaN].desc, descFR, sizeof(Boton[zonaN].desc));
        Serial.printf("\tdescripcion ZONA%d actualizada en boton \n", i+1);
      }
      //printCharArray(config.botonConfig[zonaN].desc, sizeof(config.botonConfig[zonaN].desc));
      //si el parm desc estaba vacio actualizamos en todo caso (en config y Boton)
      if (config.botonConfig[zonaN].desc[0] == 0) {
        strlcpy(config.botonConfig[zonaN].desc, descFR, sizeof(config.botonConfig[zonaN].desc));
        strlcpy(Boton[zonaN].desc, descFR, sizeof(Boton[zonaN].desc));
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
      Serial.printf("\tfactor ZONA%d: %d (%s) \n", i+1, factorRiegos[i], Boton[bId2bIndex(ZONAS[i])].desc);
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
            led(Boton[bId2bIndex(ZONAS[i])].led,ON);
        }
      }
      display->printTime(hour(t),minute(t));
      break;
    case HIDE:
      StaticTimeUpdate();
      //for(unsigned int i=0;i<sizeof(ZONAS)/2;i++) {
      for(unsigned int i=0;i<NUMZONAS;i++) {
        led(Boton[bId2bIndex(ZONAS[i])].led,OFF);
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
       //Serial.println(F("TRACE: en procesaencoder idx"));
       Serial.print(F("i"));
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
  //Serial.println(F("Salimos de reposo -4-"));
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
  ledID = Boton[index].led;
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
      tic_parpadeoLedZona.attach(0.4,parpadeoLedZona);
    } 
    return false;
  }
  return true;
}

//Pone a off todos los leds de riegos y detiene riegos (solo si la llamamos con true)
void stopAllRiego(bool stop)
{
  //Apago los leds de multiriego
  led(Boton[bId2bIndex(*multi.id)].led,OFF);
  //Apago los leds de riego
  for(unsigned int i=0;i<NUMZONAS;i++) {
    led(Boton[bId2bIndex(ZONAS[i])].led,OFF);
    if (stop) {  //paramos todas las zonas de riego
      if(!stopRiego(ZONAS[i])) { 
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
  //Serial.println(F("TRACE: in httpGetDomoticz **return"));
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
  factorRiegosOK = false;
  if(!checkWifi()) {
    if(NONETWORK) return 100; //si estamos en modo NONETWORK devolvemos 100 y no damos error
    else {
      statusError("Err1",3);
      return 100;
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
      statusError("Err2",3);
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
      statusError("Err2",3);
      return 100;
    }
  }
  #ifdef xNAME
    //extraemos la DESCRIPCION para ese boton en Domoticz del json (campo Name)
    strlcpy(descFR, jsondoc["result"][0]["Name"] | "", sizeof(descFR));
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
        if(strcmp(msg,"On") == 0 ) statusError("Err4",3); //error al iniciar riego
        else statusError("Err5",5); //error al parar riego
      }
      else statusError(response,3); //otro error
    }
    Serial.printf("DOMOTICZSWITH IDX: %d fallo en %s\n", idx, msg);
    return false;
  }
  //if(Estado.estado != ERROR && strcmp(msg,"On")) == 0) Estado.estado = REGANDO;
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
void parpadeoLedZona()
{
  byte estado = ledStatusId(ledID);
  led(ledID,!estado);
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
    if(bRC) Serial.println(F("carga parametros por defecto OK"));
    else    Serial.println(F("[ERROR] carga parametros por defecto"));
    //señala la carga parametros por defecto OK
    if(bRC) bipOK(3);
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

bool setupConfig(const char *p_filename, Config_parm &cfg) {
  Serial.printf("Leyendo fichero parametros %s \n", p_filename);
  bool loaded = loadConfigFile(p_filename, config);
  minutes = config.minutes;
  seconds = config.seconds;
  value = ((seconds==0)?minutes:seconds);
  if (loaded) {
    for(int i=0;i<NUMZONAS;i++) {
      //actualiza el IDX leido sobre estructura Boton:
      Boton[i].idx = config.botonConfig[i].idx;
      //actualiza la descripcion leida sobre estructura Boton:
      strlcpy(Boton[i].desc, config.botonConfig[i].desc, sizeof(Boton[i].desc));
    }
    #ifdef DEBUG
      printFile(p_filename);
    #endif
    return true;
  }  
  Serial.println(F("[ERROR] parámetros de configuración no cargados"));
  return false;
}

// funciones solo usadas en DEVELOP
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
                Serial.println(F("   1 - simular error NTP"));
                timeOK = false;
                break;
            case 2:
                Serial.println(F("   2 - simular error apagar riego"));
                simErrorOFF = true;
                break;
            case 9:
                Serial.println(F("   9 - anular simulacion errores"));
                simErrorOFF = false;
                timeOK = true;                         
      }
    }
  }

#endif  
