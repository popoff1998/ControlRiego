#define __MAIN__
#include "Control.h"

// interrupt para el encoder:
void IRAM_ATTR readEncoderISR() {rotaryEncoder.readEncoder_ISR();}
// variables persistentes al soft reset para control de reinicios:
RTC_NOINIT_ATTR int bootCount; // contador de reinicios del sistema (persistente en reinicios por SW y deep sleep) 
RTC_NOINIT_ATTR uint32_t magicNumber; // numero magico para detectar reinicios en frio
RTC_NOINIT_ATTR uint32_t lastUptime; // Segundos de vida del ciclo anterior
   


/*----------------------------------------------*
 *               Setup inicial                  *
 *----------------------------------------------*/
void setup()
{
  #ifdef DEMO
                Estado.modoDEMO=true;
  #endif
  #ifdef NOWIFI
                Estado.modoDEMO=true;
                Estado.noWIFI=true;
  #endif

  Serial.begin(115200);
  char versionFW[80];
  snprintf(versionFW, sizeof(versionFW), "CONTROL RIEGO V%s    Built on " __DATE__ " at " __TIME__, FW_VERSION);
  PRINTLN("\n\n", versionFW, "\n");
  #ifdef RELEASE
      if (!serialDetect()) LOG_SET_LEVEL(DebugLogLevel::LVL_ERROR);
  #endif
  String bootmessage = registrarArranqueSistema();
  #ifndef DEBUGLOG_DISABLE_LOG
      PRINTLN(" (current log level is", (int)LOG_GET_LEVEL(), ")\n");
      PRINTLN(bootmessage, "\n");
  #endif
  LOG_TRACE("TRACE: in setup");
  // init de GPIOs, bus I2C, Display, Encoder, Expansores MCP, LEDs, Buzzer
  initHardware(bootmessage == "BOOTLOOP");
  // inicializacion del sistema de ficheros
  initFS();
  logStatus(bootmessage.c_str());
  logStatus(versionFW);
  //preparo indicadores de inicializaciones opcionales
  setupInit();
  //setup parametros configuracion
  setupParm();
  #ifdef DEVELOP
   printFile(parmFile);
  #endif
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  setupRedWM(initFlags);
  //Obtenemos hora del servidor ntp y ajustamos hora del sistema y timezone
  setClock();
  //Si se ha modificado alguna opcion de configuracion en el portal AP, la guardamos
  if (saveConfigRequired) saveConfig();
  //Recuperamos lastRiegos y lastGrupos (registro fecha/hora y riego realizado)
  initLastRiegos();
  initLastGrupos();
  //Si parametros ok verificamos conexion y cargamos factores de riego desde el SCD
  if (config.initialized) checkAndInitFactorRiegos();
  //Estado final en funcion de la conexion
  setupEstadoFinal();
  #ifdef DEVELOP
    // filesInfo();
    // printFile(logErrorFile);
  #endif
  Estado.inSetup = false;
  PRINTLN("   *** Setup finalizado *** MS:", millis() , "\n\n");
}


/*----------------------------------------------*
 *            Bucle principal                   *
 *----------------------------------------------*/

void loop()
{
  procesaBotones();
  procesaEstados();
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
  // Si no hay boton a procesar salimos
  if (!validaBoton())   
      return;
  // procesaEstadoConfigurando procesa sus botones    
  if (Estado.estado == CONFIGURANDO)  
      return; 
  // Procesamos el boton pulsado:
  switch (boton->bID) {
    //Primero procesamos los botones singulares, el resto van por default
    case bPAUSE:       procesaBotonPause(); break;
    case bSTOP:        procesaBotonStop(); break;
    case MULTIRRIEGO:  procesaBotonMultirriego(); break;
    default:           procesaBotonZona();
  }
  //limpiamos el boton procesado (evitando borrar zona apuntada si multirriego) 
  if (!Estado.botonSemaforo)  boton = nullptr;
}

/**---------------------------------------------------------------
 * Determina si se ha pulsado un boton y si debe ser procesado
 */
bool validaBoton() {
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  leerEncoderSW();
  // si botonSemaforo, no leemos botones: ya lo ha apuntado multirriego u otro
  if (Estado.botonSemaforo) Estado.botonSemaforo = false;
  else  boton = parseInputs(READ);  // si no, vemos si algun boton ha cambiado de estado
  //En modo configuracion pulsar encoderSW equivale a pause (enter)
  if (Estado.estado == CONFIGURANDO && boton == nullptr && ENCSWASPAUSE) {
    simulaPauseIfEncoderSW(); }
  //Si no se ha pulsado ningun boton salimos
  if(boton == nullptr) return false;
  //Si estamos en reposo pulsar cualquier boton solo nos saca de ese estado (salvo STOP que si actua y se procesa)
  if (Estado.reposo && boton->bID != bSTOP) {
    reposoOFF();
    return false;
  }
  // Ha habido interaccion con el sistema: actualizamos tiempo de standby
  standbyTime = millis();
  //a partir de aqui procesamos solo los botones con flag ACTION
  if (!boton->flags.action) return false;
  return true;
}


/**---------------------------------------------------------------
 * Proceso en funcion del estado
 */
void procesaEstados()
{
  switch (Estado.estado) {
    case CONFIGURANDO:  procesaEstadoConfigurando(); break;
    case ERROR:         procesaEstadoError(); if(Estado.estado == ERROR) blinkDisplay(); break;
    case REGANDO:       procesaEstadoRegando(); break;
    case TERMINANDO:    procesaEstadoTerminando(); break;
    case STANDBY:       procesaEstadoStandby(); break;
    case DIFERIDO:      procesaEstadoDiferido(); if(Estado.tipo == WAITING) blinkDisplay();break;
    case STOP:          procesaEstadoStop(); break;
    case PAUSE:         procesaEstadoPause(); if(Estado.estado == PAUSE) blinkDisplay(); break;
  }
}  

/**---------------------------------------------------------------
 * Inicializacion de hardware: GPIOs, I2C, Display, Encoder, Expansores MCP
 * Si bootloopdetected es true, paramos el sistema tras la inicializacion de los GPIOs
 */
void initHardware(bool bootloopdetected) {
    LOG_DEBUG("-> Inicializando Hardware", bootloopdetected ? "(modo BOOTLOOP)" : "");
    initGPIOs();
    if (bootloopdetected) stopHW("!!!BOOTLOOP DETECTADO !!!"); // paramos el sistema
    initWire();
    // Expansores de I/O
    mcpOinit();
    mcpIinit();
    // Display y Encoder
    lcd.initLCD();
    initEncoder();
    LOG_TRACE("<- Hardware inicializado");
}


/**-------------------------------------------------------------------------------------------
 * Estado final al acabar Setup (o tras recuperar conexion) en funcion de la conexion a la red
 */
void setupEstadoFinal() 
{
  LOG_DEBUG("setupEstadoFinal entrada, Estado.error=", Estado.error, "Estado.recoverableError=",
     Estado.recoverableError, "modoDEMO=", Estado.modoDEMO, "connected=", Estado.connected);
  
  if (Estado.inSetup) {
    //Deshabilitamos el hold de Pause
    getBotonPointer(bPAUSE)->flags.holddisabled = true;
    //Llamo a parseInputs CLEAR para eliminar prepulsaciones antes del bucle loop
    parseInputs(CLEAR);
    //lanzamos supervision periodica estado cada VERIFY_INTERVAL seg.
    tic_verificaciones.attach(VERIFY_INTERVAL, flagVerificaciones);
  }
  // Si no se ha podido cargar parámetros desde ficheros -> señalamos el error 
  if(!config.initialized) {
    statusError(E0); 
    // Si STOP pulsado: modo DEMO y activamos webserver para cargar parámetros
    #ifdef WEBSERVER
    if (testButton(bSTOP,ON) && Estado.connected) {
        setWarnToFile(true); // activamos grabacion msg warning
        LOG_WARN("Err0 + STOP -> modo DEMO y activamos webserver");
        Estado.modoDEMO = true;
        delay(config.msgdisplaymillis);
        setEstado(CONFIGURANDO);
        setupWS();
        return;
    }
    #endif  
    // En caso contrario salimos con el Error 0
    LOG_ERROR("Salida por NO config.initialized"); 
    return;
  }
  // Si estamos en modoDEMO pasamos a STANDBY (o STOP si esta pulsado) aunque no exista conexión wifi o estemos en ERROR
  if (Estado.modoDEMO) {
    setEstado(STANDBY);
    LOG_DEBUG("Salida por modoDEMO");
    return;
  }
  // Si estado actual es ERROR seguimos así
  if (Estado.estado == ERROR) {
    if (Estado.inSetup) LOG_ERROR(">>>>   Setup ended with ERROR ", Estado.error, "(", errorToString(Estado.error), ") ");
    else LOG_DEBUG("Salida por ERROR", Estado.error, "(", errorToString(Estado.error), ") ");
    LOG_DEBUG("   Recuperable:", Estado.recoverableError, "modoDEMO:", Estado.modoDEMO);
    return;
  }
  // Si estamos conectados a la red pasamos a STANDBY (o STOP si esta pulsado)
  if (Estado.connected) {  
      setEstado(STANDBY);
      if (Estado.inSetup) {
          sonido.bipOK();
          logStatusF(" <<<<<  Setup ended OK  >>>> MS: %lu", millis());
      } else {
          if (config.tempRemote == -1) {
              config.tempRemote = 1; // restauramos temp remota si estaba asi configurada
              logStatus("Restored remote temperature sensor mode");
          }    
      }
  } else {  //si no estamos conectados a la red pasamos a estado ERROR
    statusError(E1, hayWifiSalvada); //error de conexion wifi recuperable si hay una red wifi salvada
    LOG_ERROR("setupEstadoFinal salida por estado ERROR(E1)"); 
  }
}  //fin de setupEstadoFinal

void setupBoton()
{
  // Trasladamos numero de zona de Zonas[] a zNumber de Boton[] para facilidad de acceso a todos los datos de la zona desde el puntero al boton correspondiente
  for(uint i=0;i<NUMZONAS;i++) {
    getBotonPointer(Zonas[i])->_zNumber = i+1;
  }
}

#ifdef GRP4
  /**---------------------------------------------------------------
   * Verificamos si STOP y encoderSW esta pulsado (estado OFF) en el arranque,
   * en ese caso se muestra pantalla de opciones.
   * Pulsando entonces:
   *    - boton Grupo1 --> borramos ficheros de parametros : reset a valores por defecto
   *    - boton Grupo3 --> borramos red wifi almacenada en el ESP32
   *    - liberando boton de STOP  --> salimos sin hacer nada y continua la inicializacion
   */
  void setupInit() {
    #ifdef TEMPLOCAL
      dht.begin();
    #endif

    setupBoton();
    if (!digitalRead(ENCBOTON) && testButton(bSTOP,ON)) {
      LOG_TRACE("en opciones setupInit");
      lcd.infoclear("       Pulse:");
      lcd.info("grupo1 >RESET parm",2);
      lcd.info("grupo3 >erase WIFI",3);
      lcd.info("EXIT -> release STOP",4);
      while (1) {
        boton = parseInputs(READ);
        if(boton == nullptr) continue;
        Serial.printf("parseImputs devuelve: boton->id %x  boton->estado %d \n", boton->bID ,boton->estado);
        if(boton->bID == bSTOP && !boton->estado ) break;
        if (boton->bID == bGRUPO1) 
        {
          if(initFlags.preinitParm)
          {
              initFlags.initParm = true;
              LOG_WARN("repulsado GRUPO1  --> flag de reset/erase PARM true");
              lcd.infoclear("RESET/ERASE PARM",1);
              deleteParmSignal(6);
              break;
          }    
          else
          {
              initFlags.preinitParm = true;
              LOG_WARN("pulsado GRUPO1  --> pedida confirmacion");
              lcd.infoclear(">>RESET/ERASE PARM<<",1);
              lcd.info(" confirme con GRUPO1",3);
              lcd.info(" CANCEL release STOP",4);
          }    
        }
        if (boton->bID == bGRUPO3) 
        {
          initFlags.initWifi = true;
          LOG_WARN("pulsado GRUPO3  --> flag de init WIFI true");
          lcd.infoclear("clear WIFI",1,BIPOK);
          wifiClearSignal(6);
          break;
        }
      }
    }
  };
#else // M3GRP
  /**---------------------------------------------------------------
   * verificamos si encoderSW esta pulsado (estado OFF) y selector de multirriego esta en:
   *    - Grupo1 (arriba) --> en ese caso cargamos los parametros del fichero de configuracion por defecto
   *    - Grupo3 (abajo) --> en ese caso borramos red wifi almacenada en el ESP32
   */
  void setupInit() {
    LOG_TRACE("");
    setupBoton();
    if (!digitalRead(ENCBOTON)) {
      if (testButton(bGRUPO1,ON)) {
        initFlags.initParm = true;
        LOG_WARN("encoderSW pulsado y multirriego en GRUPO1  --> flag de load BACKUP PARM true");
        deleteParmSignal(6);
      }
      if (testButton(bGRUPO3,ON)) {
        initFlags.initWifi = true;
        LOG_WARN("encoderSW pulsado y multirriego en GRUPO3  --> flag de init WIFI true");
        wifiClearSignal(6);
      }
    }
  };
  #endif // GRP4
  

       /*---------------------------------------------------------------*
        *                                                               *
        *                    Proceso de los BOTONES                     *
        *                                                               *
        *---------------------------------------------------------------*/


/*---------------------------------------------------------------*
 *                    Proceso boton PAUSE                        *
 *---------------------------------------------------------------*/

  void procesaBotonPause()
  {
    if(!boton->estado && Estado.estado != STOP) //No procesamos los release del boton salvo en STOP
       return;
    switch (Estado.estado) {
      case REGANDO:
        if(encoderSW) handleEncPauseInRegando();  //cancela riego zona en curso
        else handlePauseInRegando();              //pausa riego zona en curso
        break;
      case PAUSE:
        if(encoderSW) handleEncPauseInPause();    //cancela riego zona en curso
        else handlePauseInPause();                //reanuda riego zona en curso
        break;
      case STANDBY:
        if(encoderSW) handleEncPauseInStandby();  //conmuta modoDEMO
        else handlePauseInStandby();              //muestra ultimos riegos y hora
        break;
      case STOP:
        if(encoderSW) handleEncPauseInStop ();    //resetea el ESP32
        else handlePauseInStop();                 //pasa a CONFIGURANDO
        break;
      case ERROR:
        handlePauseInError();                     //pasa a modo DEMO y resetea error
        break;
    }
  }; //fin de procesaBotonPause
  
// Si pulsamos junto con encoderSW terminamos el riego (pasaria al siguiente en caso de multirriego)
void handleEncPauseInRegando() {
  setEstado(TERMINANDO);
  LOG_DEBUG("encoderSW+PAUSE terminamos riego de zona en curso");
  // si estamos en un multirriego y no es la ultima zona y no hemos salvado ya un riego en curso
  // --> salvamos el riego en curso en riegoSaved para poder continuarlo despues del multirriego
  if (multi.riegoON && (multi.actualIndex+1 < *multi.size) && !riegoSaved.znumber) {
    saveRiego(zonaEnCurso.znumber, zonaEnCurso.pBoton, timer.ShowMinutes(), timer.ShowSeconds());
    LOG_DEBUG("salvando riego de zona en curso en riegoSaved multi.actualIndex=", multi.actualIndex, " multi.size=", *multi.size);
  }
}

// Pausa el riego en curso
void handlePauseInRegando() {
  setEstado(PAUSE,1);
  if (stopRiego(zonaEnCurso.pBoton)) timer.PauseTimer();
  else { //error al parar riego (ya puesto por stopRiego)
    LOG_WARN("error al pausar riego ERROR(E",Estado.error, ") (", errorToString(Estado.error), ") zona :",zonaEnCurso.pBoton->desc );
  }  
}  

// Si pulsamos junto con encoderSW terminamos el riego (pasaria al siguiente en caso de multirriego)
void handleEncPauseInPause() {
  cancelFromPause = true; // para que no actualize tiempo final riego en procesaEstadoTerminando
  handleEncPauseInRegando();
}

// Reanudamos riego que estaba parado
void handlePauseInPause() {
  if(simular.ErrorPause) statusError(E2); //simulamos error al salir del PAUSE
  else initRiego(RESUME);         
  if(Estado.estado == ERROR) { // caso de error al reanudar el riego seguimos en PAUSE y señalamos con blink rapido zona
    LOG_WARN("error al salir de PAUSE ERROR(E",Estado.error, ") (", errorToString(Estado.error), ") zona :",zonaEnCurso.pBoton->desc );
    lcd.displayON();
    delay(config.msgdisplaymillis);
    setEstado(PAUSE,1,LOCAL,RAPIDO);
  } else {
    timer.ResumeTimer(); // reanudamos el timer de cuenta atras
    setEstado(REGANDO,1);
  }
}  

// Si encoderSW+Pause --> conmutamos estado modoDEMO
void handleEncPauseInStandby() {
    // modoDEMO --> NORMAL ,leemos factores de riego y recuperamos tablas de ultimos riegos reales
    if (Estado.modoDEMO) {
      Estado.modoDEMO = false;
      Estado.noWIFI = false;
      LOG_INFO("encoderSW+PAUSE pasamos a modo NORMAL y leemos factor riegos");
      sonido.bip(2);
      ledPWM(LEDB,OFF);
      lcd.infoclear("Saliendo de DEMO");
      if (!checkWifi()) wifiReconnect();
      // verificamos conexion con Domoticz y cargamos factores de riego
      // Si carga OK y config.verify es true, paramos todos los riegos de las zonas.
      if (checkAndInitFactorRiegos() && config.verify) { // OJO: el orden del && es importante
          lcd.info("..y parando riegos",2);
          stopAllRiegos(); //verificamos operativa OFF para las zonas
        }    
      setupEstadoFinal();
      //recuperamos tablas de ultimos riegos reales
      initLastRiegos();
      initLastGrupos();
    }
    // NORMAL --> modoDEMO
    else {
      Estado.modoDEMO = true;
      LOG_INFO("encoderSW+PAUSE pasamos a modoDEMO (DEMO)");
      setUI(STANDBY,2);
    }
}
      
// Muestra hora y ultimos riegos
void handlePauseInStandby() {
    ultimosRiegos(SHOW);
    delay(config.msgdisplaymillis*3);
    ultimosRiegos(HIDE);
    setUI(STANDBY);  // para restaurar pantalla
}  
      
// En estado STOP si ENC + hold del boton pause reseteamos el ESP32  
void handleEncPauseInStop() {
    if(handleHoldPause()) {
      LOG_WARN("Stop + encoderSW + PAUSA --> Reset.....");
      resetESP32();
    }
}

// En estado STOP si hold del boton pause pasamos a modo Configuracion  
void handlePauseInStop() {
    if(handleHoldPause()) {
      LOG_INFO("Stop + hold PAUSA --> modo ConF()");
      setEstado(CONFIGURANDO,1);
      configure->menu(0);  // mostramos menu de configuracion, primera linea
    }
}

void handlePauseInError() {
    LOG_INFO("estado en ERROR y PAUSA pulsada pasamos a modoDEMO y reset del error");
    Estado.modoDEMO = true;
    setEstado(STANDBY,2);
}

// Detecta si se mantiene pulsado el boton PAUSE
bool handleHoldPause() {
    static unsigned long countHoldPause = 0;
    static bool holdPause = false; 
    bool RC = false;
    if(boton->estado) {
      if(!holdPause) {
        countHoldPause = millis();
        holdPause = true;
      }
      else {
        if((millis() - countHoldPause) > HOLDTIME) {
          RC = true;
          holdPause = false;
        } 
      }
    }
    else holdPause = false; //Si lo hemos soltado quitamos holdPause
    return RC;
}


/*---------------------------------------------------------------*
 *                    Proceso boton STOP                         *
 *---------------------------------------------------------------*/

void procesaBotonStop()
{
  if (boton->estado) {  //si hemos PULSADO STOP
    if (Estado.estado == REGANDO || Estado.estado == PAUSE || Estado.estado == TERMINANDO) {
      handleStopInRegandoPauseTerm();           //parar riegos
      return;
    }
    if (Estado.estado == STANDBY) { 
      if (encoderSW) handleEncStopInStandby();  // activa configuracion de grupo multirriego temporal
      else handleStopInStandby();               // seguro antinenes
      return;
    }
    if (Estado.estado == DIFERIDO) {
      handleStopInDiferido();                  // cancela proceso riego diferido y pasa a STOP
      return;
    }
    if (Estado.estado == ERROR) {
      handleStopInError();                      // resetea el ESP32 (o activa webserver si E0)
      return;
    }
  }
  //si hemos liberado STOP: salimos del estado stop
  if (Estado.estado == STOP && !boton->estado) {
    LOG_TRACE("[poniendo estado STANDBY]");
    setEstado(STANDBY);
  }
  //(en estado CONFIGURANDO dejamos el release del STOP para que actue el codigo de procesaEstadoConfigurando
} //fin de procesaBotonStop

// Paramos el riego en curso primero y todas las zonas de riego, y pasamos a estado STOP
void handleStopInRegandoPauseTerm() {
    if (!Estado.modoDEMO) lcd.infoclear("Parando riegos", NOBLINK, BIP, 6);
    timer.StopTimer();
    tic_CountDownTimer.detach(); //detiene actualizacion periodica del temporizador
    bool updateTimeFin = (Estado.estado == PAUSE ? false : true); // si estamos en PAUSE no actualizamos tiempo fin
    // paramos riego en curso primero y todas las zonas despues
    if (!stopRiego(zonaEnCurso.pBoton, updateTimeFin) || !stopAllRiegos()) {
      return;    //error al parar riegos
    }
    saveRiegosToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS);  //guardamos en fichero tabla de ultimos riegos de zonas
    if (!Estado.modoDEMO) lcd.infoclear("STOP riegos OK", BLINKDISPLAY, BIP, 0);
    else lcd.infoclear("STOP riegos SIMULADO", BLINKDISPLAY, BIP, 0);
    setEstado(STOP,1);
}

void handleStopInDiferido() {
    timer.StopTimer();
    tic_CountDownTimer.detach(); //detiene actualizacion periodica del temporizador
    tm = tm_saved; // restauramos tiempo de riego original (antes de la cuenta atras)
    setEstado(STOP,1);
}

// Paramos todas las zonas de riego y pasamos a estado STOP
void handleStopInStandby() {
    reposoOFF();
    if (!Estado.modoDEMO) lcd.infoclear("Parando riegos", NOBLINK, BIP, 6);
    if (!stopAllRiegos()) {   //error al parar riegos
      return; 
    }
    if (!Estado.modoDEMO) lcd.infoclear("STOP riegos OK", 1, BIP, 0);
    else lcd.infoclear("STOP riegos SIMULADO", 2, BIP, 0);
    setEstado(STOP,1);
}

// Iniciamos la configuracion de un multirriego temporal
void handleEncStopInStandby() {
    setMultiTemp(NEWMTEMP);  // apunta estructura multi a grupo temporal
    setEstado(CONFIGURANDO,1);
    configure->MultiTemp_process_start();
}

// Reseteamos el ESP32 para intentar recuperar de un error
void handleStopInError() {
  // si el error es no cargar parametros, lanzamos webserver para que el usuario pueda cargar unos nuevos  
  if (Estado.error == E0) scWebserver();
  else {
    LOG_WARN("ERROR + STOP --> Reset.....");
    resetESP32();
  }  
}


/*---------------------------------------------------------------*
 *            Proceso botones MULTIRRIEGO (GRUPOS)               *
 *---------------------------------------------------------------*/

void procesaBotonMultirriego()
{
  if (multi.riegoON)  //ya hay un multirriego en curso,, ignoramos boton
      return;
  if (Estado.estado == STANDBY) {
    int n_grupo = setGrupo(); //apunta estructura multi al grupo seleccionado
    LOG_DEBUG("en MULTIRRIEGO, encoderSW status  :", encoderSW, "grupo seleccionado:", n_grupo, "multi.desc:", multi.desc, "multi.size:", *multi.size);
    if (encoderSW) showInfoGrupo(n_grupo);  //muestra info del grupo
              else handleGrupoInStandby(n_grupo);     //inicia el multirriego
  }
  // Si estamos ajustando el retardo del riego, procedemos al proceso de cuenta atras para iniciarlo.
  if (Estado.estado == DIFERIDO && Estado.tipo == SETDEFER) startRiegoDiferido(config.group[getGroupIndex(boton->bID)].desc); 
  // atajos de teclas para STOP+ENC+GRUPOn
  if (encoderSW && Estado.estado == STOP && SHORTCUTSENABLED) handleEncGrupoInStop(getGroupIndex(boton->bID)+1);
} //fin de procesaBotonMultiriego

// Iniciamos el MULTIRRIEGO
void handleGrupoInStandby(int n_grupo) {
    /* Iniciamos el primer riego del grupo machacando la variable boton.
       Realmente estoy simulando la pulsacion del primer boton de riego de la serie
       que sera procesado en el siguiente paso del loop.
       Tambien grabamos el tiempo de inicio del riego de grupo  */
    if(startMultirriego()) inicioTimeLastRiego(lastGrupos[n_grupo-1], INICIO);
}

// Atajos combinacion STOP+ENC+GRUPOn
void handleEncGrupoInStop(int n_grupo) {
  switch (n_grupo) {
      case 1:                     //activa Webserver
          scWebserver();
          break;
      case 2:                     //togle display del nivel de señal wifi
          scWifiLevel();
          break;
      case 4:                     //easter egg
          scSorpresa();
          break;
  }    
}


/*---------------------------------------------------------------*
 *                    Proceso boton ZONA                         *
 *---------------------------------------------------------------*/

void procesaBotonZona()
{
  int zNumber = boton->zNumber();  // numero de zona pulsada (1..NUMZONAS)
  LOG_DEBUG("zona:", zNumber, "encoderSW:", encoderSW, "multi.riegoON:", multi.riegoON);
  if (Estado.estado == STANDBY) {
    if (!encoderSW || multi.riegoON) {  // (1)
        startZoneWatering();    //iniciamos el riego correspondiente al boton pulsado
    }
    else {  
        showInfoZona(zNumber);   // mostramos en el display info zona
    }
    return;
  }
  // Si estamos ajustando el retardo del riego, procedemos al proceso de cuenta atras para iniciarlo.
  if (Estado.estado == DIFERIDO && Estado.tipo == SETDEFER) {
      startRiegoDiferido(config.zona[zNumber-1].desc);
  }    
  /* Si config.dynamic=true se permite añadir/eliminar zonas durante un riego individual o de grupo. 
     Para ello el riego debe estar en PAUSE  */
  if ((Estado.estado==PAUSE) && config.dynamic) {
        handleDynamicZoneChange(zNumber);
  }
  /* (1) la comprobacion de multi.riegoON es necesaria para evitar que al cancelar el riego de una zona en multirriego
  salte a mostrar info de la siguiente al detectar el enc pulsado  */
} //fin de procesaBotonZona


/**---------------------------------------------------------------
 * Si config.dynamic = true , se puede una vez iniciado un riego (de zona, multirriego temporal o de grupo)
 * el poder añadir o eliminar zonas en la cola de pendientes de riego. 
 * Para ello se pasa este riego a multirriego temporal si no lo fuera ya.
 * En el caso de riego en curso de zona individual, este no se ha factorizado y no se factorizan los añadidos.
 * Si lo que se modifica dinamicamente es un multirriego temporal o de grupo lanzados al principio 
 * si se factorizaron las zonas iniciales y se factorizaran las añadidas.
 */
void handleDynamicZoneChange(int znumber) {
  // Si la zona pulsada coincide con la actualmente en riego, se ignora:
  if (zonaEnCurso.pBoton == boton)
    {sonido.bipKO(); LOG_DEBUG("zona pulsada:",znumber," es = a zona actual:",zonaEnCurso.znumber);}
  else // si la zona pulsada es distinta a la zona en curso: procesamos el cambio dinamico
  {
    // CASO 1: estamos en riego de zona individual -> pasamos a multirriego temporal con la zona en curso como unica zona del grupo
    if (!multi.riegoON) {
      multi.riegoON = true;
      multi.noFactorizado  = true;  // marcamos como no factorizado
      multi.actualIndex=0;
      setMultiTemp(NEWMTEMP);  // completa resto campos estructura multi como grupo temporal nuevo
      multi.zserie_pBoton[0] = zonaEnCurso.pBoton;  // apuntador de la zona actual como primera de la lista
      multi.w_zserie[0] = zonaEnCurso.znumber;  // numero de la zona actual como primera de la lista
      multi.w_size = 1; // indicamos que hay una zona en la lista 
    }
    // CASO 2: estamos en multirriego de grupo -> pasamos a multirriego temporal con las zonas del grupo como zonas del grupo temporal
    if (!multi.temporal) setMultiTemp();  // pasa estructura multi del grupo activo a grupo temporal 
    // CASO 3: llegados aqui ya estamos en multiriego temporal (veniamos de el o lo hemos generado en el caso 1 o 2)  
    if (procesaDynamic(znumber)) displayLCDGrupo(RESTO, 2); // Procesa cambio dinamico y reflejarlo en el display
    LOG_DEBUG("MULTI: noFactorizado?:",multi.noFactorizado,"actualIndex:",multi.actualIndex,"size:",multi.w_size,"zona:",znumber);
  }
}

/**------------------------------------------------------------------------------------------
 * Procesa el cambio dinamico de zonas pendientes de riego en un multirriego temporal.
 * Si la zona existe en la cola se elimina, si no está se añade al final.
 * NOTA: en un multirriego temporal *multi.size apunta a multi.w_size */
bool procesaDynamic(int znumber)
{
  LOG_DEBUG("[RECIBE] actualIndex:", multi.actualIndex, " size:", multi.w_size, " zona:", znumber);
  // 1. BUSQUEDA Y ELIMINACIÓN
  for (int n = multi.actualIndex; n < multi.w_size; n++) { 
      if (multi.w_zserie[n] == znumber) { 
          LOG_DEBUG("[vamos a ELIMINAR] zona:",znumber,"posicion",n+1,"size:",multi.w_size);
          // Desplazamos elementos hacia la izquierda. 
          for (int j = n; j < (multi.w_size - 1); j++) { // El límite es (multi.w_size - 1) para no leer j+1 fuera del array
              multi.w_zserie[j] = multi.w_zserie[j + 1];
              multi.zserie_pBoton[j] = multi.zserie_pBoton[j + 1];
          }
          multi.w_size--; // Decrementamos el tamaño
          LOG_INFO("DYNAMIC [ELIMINA] Zona:",znumber);
          LOG_DEBUG("\t posicion",n+1,"nuevo size:",multi.w_size);
          sonido.bip(2); 
          return true; // zona encontrada y eliminada, salimos
      }
  } 
  // 2. ADICIÓN AL FINAL
  if (multi.w_size < ZONASXGRUPO) {
      int index = multi.w_size; // El tamaño actual es el índice del siguiente hueco libre
      multi.zserie_pBoton[index] = boton;
      multi.w_zserie[index] = znumber;
      multi.w_size++; // Incrementamos tamaño
      LOG_INFO("DYNAMIC [AÑADE] Zona:", znumber);
      LOG_DEBUG("\t nuevo size:",multi.w_size);
      sonido.bip(1); 
      return true; // zona añadida, salimos
  }
  sonido.bipKO(); 
  return false; //no hay sitio --> zona ignorada   
}   //fin de procesaDynamic



     /*---------------------------------------------------------------*
      *                                                               *
      *                    Proceso de los ESTADOS                     *
      *                                                               *
      ----------------------------------------------------------------*/


void procesaEstadoError()
{
  // Si se ha recuperado la conexion wifi, iniciamos el recovery sin experar el VERIFY_INTERVAL
  if (Estado.showWifiOK) {
    LOG_DEBUG("showWiFiOK"); 
    showWifiOK(); //muestra en pantalla wifi recuperada
    VerifyRecoveryWifi(false); //inicia proceso de recuperacion
  }  
  // gestion del tamano del fichero de log de errores cada LONGINTERVAL minutos
  if (checkLogSize) {
    gestionarTamanoLog(); // Borra/rota fichero de log de errores si su tamano es excesivo
    checkLogSize = false;
  }
  if (flagV) {   // acciones cada VERIFY_INTERVAL en estado ERROR
    if(Estado.failedStopRiego) sonido.bip(2);  //recordatorio error grave al parar un riego
    //se intenta recuperar error si en el SETUP no hemos podido conectar con la wifi o con domoticz
    if(Estado.error == E1 && Estado.recoverableError) VerifyRecoveryWifi(checkRecon);
    if(Estado.error == E2 && Estado.recoverableError && checkRecon) VerifyRecoverySCD();
  }
}; //fin de procesaEstadoError


void procesaEstadoRegando() 
{
    // Actualiza el temporizador de cuenta atrás (devuelve false si llega a 0)
    bool timerActivo = timer.Timer();
    if (timer.TimeHasChanged()) refreshTime();
    if (!timerActivo) {
        setEstado(TERMINANDO);
        return; 
    }
    // Si NO se cumplen las condiciones de verificación, salimos ya.
    bool debeVerificar = flagV && config.verify && (!Estado.modoDEMO || simular.all_simFlags);
    if (!debeVerificar) {
        return;
    }
    // Verificación del estado del riego en Domoticz esta activo
    if (queryStatus(zonaEnCurso.znumber, "On")) {
        if (Estado.tipo != REMOTO) setLed(tic_LedZona, ENCIENDE, zonaEnCurso.pBoton->led); //si local: led zona encendido fijo
        else setParpadeo(tic_LedZona, LENTO, parpadeoLedZona, zonaEnCurso.pBoton->led); //si remoto: parpadeo lento led zona
        return;
    }
    // Escenario 1: queryStatus falló
    if (Estado.error) {
        // Si no hemos podido verificar estado, señalamos zona con parpadeo rapido e ignoramos el error
        setParpadeo(tic_LedZona, RAPIDO, parpadeoLedZona, zonaEnCurso.pBoton->led);
        sonido.bip(2);
        Estado.error = NOERROR; 
        LOG_WARN("** SE HA DEVUELTO ERROR al verificar estado riego");
        return;
    }
    // Escenario 2: El riego se ha parado remotamente,
    // paramos el temporizador y pasamos a estado PAUSE señalando con parpadeo lento led zona
    timer.PauseTimer();
    finalTimeLastRiego(lastRiegos[zonaEnCurso.zindex]);
    LOG_WARN(">>>>>>>>>> procesaEstadoRegando zona:", config.zona[zonaEnCurso.zindex].desc, "en PAUSA remota <<<<<<<<");
    setEstado(PAUSE, 1, REMOTO, LENTO);
}   //fin de procesaEstadoRegando


void procesaEstadoTerminando()
{
  sonido.bip(5);
  tic_CountDownTimer.detach(); //detiene actualizacion periodica del temporizador
  // si veniamos de PAUSE no actualizamos tiempo fin (ya se hizo al entrar en PAUSE)
  bool updateTimeFin = (cancelFromPause? false : true); // por si venimos de cancel desde PAUSE
  cancelFromPause = false;
  stopRiego(zonaEnCurso.pBoton, updateTimeFin); // paramos riego en curso
  // no continuamos si se ha producido error al parar el riego 
  if (Estado.estado == ERROR)
      return;
  lcd.blinkLCD(BLINKDISPLAY);
  // aseguramos led zona apagado si hemos podido parar riego 
  // (ya que si estamos en multirriego pasaremos por STANDBY sin cambiar la UI)
  setLed(tic_LedZona, APAGA, zonaEnCurso.pBoton->led);
  //Comprobamos si estamos en un multirriego
  if (multi.riegoON) {
    //sumamos tiempo riego zona terminada al tiempo de riego del grupo
    if(!multi.temporal) finalTimeGrupo(lastGrupos[multi.ngrupo-1], lastRiegos[zonaEnCurso.zindex].total); 
    multi.actualIndex++;
    // pasamos a regar la siguiente zona del grupo si quedan en cola:
    if (multi.actualIndex < *multi.size) {  
      //Simular la pulsacion del siguiente boton de la serie de multirriego
      boton = multi.zserie_pBoton[multi.actualIndex];
      Estado.botonSemaforo = true;
    }
    // no quedan zonas por regar: señalamos fin del multirriego y actualizamos timestamp de finalizacion
    else {         
      if(!multi.temporal) {
        finalTimeGrupo(lastGrupos[multi.ngrupo-1]);
        saveRiegosToFile(lastGruposFile, "lastGrupos", lastGrupos, NUMGRUPOS);  //guardamos en fichero tabla de ultimos riegos de grupos
      }
      // descomentar la siguiente linea si queremos mostrar ultimo riego previo del grupo
      // que se ha cambiado dinamicamente a temporal. En caso contrario se mostrara "SIN DATOS"
      // else loadRiegosFromFile(lastGruposFile, "lastGrupos", lastGrupos, NUMGRUPOS);  
      saveRiegosToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS);  //guardamos en fichero tabla de ultimos riegos de zonas
      lcd.info("multirriego",1);
      int msgl = snprintf(buff, MAXBUFF, "%s fin", multi.desc);
      lcd.info(buff, 2, msgl);
      LOG_INFO("MULTIRRIEGO", multi.desc, "terminado");
      sonido.bipFIN();
      delay(config.msgdisplaymillis*3);
      if(!multi.temporal) led(multi.id->led, OFF);  // apaga led grupo
      multi.riegoON = false;  // reseteamos flag de multirriego activo
    }
  }
  else saveRiegosToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS);  //guardamos en fichero tabla de ultimos riegos de zonas
  // Caso especial: Si hay riego salvado y NO estamos en multirriego, lo recuperamos y terminamos.
  if (!multi.riegoON && riegoSaved.znumber) {
      restoreRiego();
      return; 
  }
  // Si acabamos riego pero seguimos en multirriego, pasamos por STANDBY silencioso (sin cambios en la UI)
  // para seguir con la siguiente zona , si no ponemos STANDBY normal.
  multi.riegoON ? setStateMachine(STANDBY) : setEstado(STANDBY);
}; //fin de procesaEstadoTerminando


void procesaEstadoStandby()
{
  if (multi.riegoON)  //no se hacen verificaciones/acciones con multirriego en curso
      return;
  if (Estado.showWifiOK) showWifiOK(); //muestra en pantalla wifi recuperada    
  //Apagamos el display y atenuamos led status si ha pasado el lapso STANDBYSECS sin actividad
  if (!Estado.reposo && (millis() - standbyTime >= (1000UL * STANDBYSECS))) reposoON();
  // leemos encoder
  procesaEncoderTime();
  // Verificaciones en STANDBY cada VERIFY_INTERVAL segundos
  //   - verificacion de wifi y recuperacion si procede
  //   - actualiza y muestra temperatura ambiente
  if (flagV) { 
    VerifyRecoveryWifi(checkRecon); //verificacion de wifi, refresco nivel wifi y recuperacion si procede
    showTemp(); // actualiza y muestra temperatura ambiente
  }
  // Verificaciones en STANDBY cada LONGINTERVAL minutos   
  //   - gestion del tamano del fichero de log de errores
  //   - actualizacion de hora por NTP si no la tenemos actualizada
  if (checkLogSize) {
    gestionarTamanoLog(); // Borra/rota fichero de log de errores si su tamano es excesivo
    if (!timeOK && Estado.connected) setClock(); 
    checkLogSize = false;
  }
}; //fin de procesaEstadoStandby

void procesaEstadoDiferido() 
{
  // ajuste del tiempo de riego diferido con el encoder
  if (Estado.tipo == SETDEFER) procesaEncoderTime();
  // tratamiento de la cuenta atras del riego diferido
  else procesaCuentaAtras();
}

void procesaEstadoStop()
{
  // En stop activamos el comportamiento hold de pausa
  getBotonPointer(bPAUSE)->flags.holddisabled = false;
  // Si se muestra nivel wifi, lo actualizamos cada intervalo de verificaciones
  if (config.showwifilevel && flagV) showWifiLevel(checkWifi(true));
  // Apagamos el display y atenuamos led status pasado 4 x STANDBYSECS
  if (!Estado.reposo && (millis() - standbyTime >= (4 * 1000UL * STANDBYSECS))) reposoON();
};


// verificamos zona sigue OFF en Domoticz periodicamente
void procesaEstadoPause() {
  // Solo verificamos si toca, VERIFY ON y no estamos en modoDEMO (o estamos en modo simulacion)
  if(flagV && config.verify && (!Estado.modoDEMO || simular.all_simFlags)) {  
    // Verificamos que el riego sigue parado en Domoticz, si es así salimos sin hacer nada.
    if(queryStatus(zonaEnCurso.znumber, "Off")) 
      return;
    // Hemos detectado riego zona activo: salimos del PAUSE y blink lento zona activada remotamente  
    if(!Estado.error) {  
      sonido.bip(2);
      LOG_WARN(">>>>>>>>>> procesaEstadoPause zona:", config.zona[zonaEnCurso.zindex].desc,"activada REMOTAMENTE <<<<<<<");
      timer.ResumeTimer();
      inicioTimeLastRiego(lastRiegos[zonaEnCurso.zindex], RESUME); //actualizamos tiempo de riego de la zona
      setEstado(REGANDO,2,REMOTO,LENTO); //pasamos a REGANDO remoto
    }
    // Si no hemos podido verificar estado, ignoramos el error (posible pause  por mantenimiento de la wifi o del domoticz)
    else Estado.error = NOERROR; 
  }
} //fin de procesaEstadoPause


/*---------------------------------------------------------------*
 *                  Proceso estado CONFIGURANDO                  *
 *---------------------------------------------------------------*/

void procesaEstadoConfigurando()
{
  /*
   La mecanica general de la maquina de estados en modo normal es que primero se procesa el boton pulsado, 
   pudiendo este cambiar el estado, y despues se procesa el estado.
   En modo configuracion es totalmente opuesto: los botones se procesan en procesaEstadoConfigurando.
   Esto se hace así para tener separada la lógica de modo normal de la de configuración, ya que las acciones 
   que realizan los botones en una y otra son totalmente distintas.
  */
  // Si hay boton pulsado (con flag ACTION), lo procesamos segun el menu en el que estemos
  if (boton != nullptr) {
    if (boton->flags.action) {
      if (webServerAct && boton->bID != bSTOP)  //si webserver esta activo solo procesamos boton STOP
        return;
      switch(boton->bID) {
        case bPAUSE:
            if(!boton->estado) break; //no se procesa el release del PAUSE
            //si estamos en el menu: PAUSE procesa la seleccion
            if(configure->inMenu()) {
              LOG_DEBUG("[MENU] PAUSE pulsado recibido");
              configure->procesaSelectMenu();
              break;
            }
            // si ya estamos configurando algo: PAUSE consolida en config lo modificado
            LOG_DEBUG("PAUSE pulsado recibido y estamos configurando algo");
            handleParameterConsolidation(); 
            break;
        case bSTOP:
            if(!boton->estado) {    //release STOP
              if(configure->configuringMultiTemp() && configure->get_MultiTempReady()) // si se han definido zonas en el multirriego temporal
                  startMultirriego(); // prepara comienzo multirriego temporal en el siguiente paso del loop
              configure->exit();  // salvamos parametros a fichero si procede y salimos de ConF
            }
            break;
        case MULTIRRIEGO:
            if (configure->inMenu() && configure->get_currentItem()==0) { //si no estamos configurando nada:
              handleGroupConfig();                                        // configuramos el grupo seleccionado
            }  
            break;
        default:  //procesamos boton de ZONAx
            if (configure->inMenu() && configure->get_currentItem()==0) {   //si no estamos configurando nada :
              configure->Idx_process_start();                          // configuramos el idx del boton de zona pulsado
            }
            if (configure->configuringMulti() || configure->configuringMultiTemp()) { //si estamos configurando grupo multirriego:
              configure->Multi_process_update();                             //añadimos zona al multirriego que estamos definiendo
            }
      }
    }
    //limpiamos el boton procesado (evitando borrar zona apuntada caso de multirriego temporal) 
    if (!Estado.botonSemaforo)  boton = nullptr;
    // Si no se ha pulsado boton procesamos el webserver si esta activado o el encoder en caso contrario
  } else webServerAct ? procesaIfWebServer() : procesaEncoderConfig();
}; //fin de procesaEstadoConfigurando


void handleGroupConfig()
{
      int n_grupo = setGrupo(); //apunta estructura multi al grupo seleccionado
      // assert(n_grupo > 0 && n_grupo <= NUMGRUPOS); // seguridad en modo desarrollo: verificamos que el grupo seleccionado es correcto
      //Configuramos el grupo de multirriego apuntado en multi
      rotaryEncoder.disable();
      configure->Multi_process_start(n_grupo);
}

// Al pulsar PAUSE estando configurando algo se consolidan los cambios realizados 
// en la configuracion de tiempo, idx, rango o multirriego.
void handleParameterConsolidation()
{
      if(configure->configuringTime()) {
        configure->Time_process_end();  //  salvamos en config el nuevo tiempo por defecto
      }
      if(configure->configuringIdx()) {
        configure->Idx_process_end();  //  salvamos en config el nuevo IDX
      }
      if(configure->configuringRange()) {
        configure->Range_process_end();  //  salvamos en config nuevo valor del parametro
      }
      if(configure->configuringMulti()) {
        configure->Multi_process_end();  // actualizamos config con las zonas introducidas
      }
      // En el caso de configurar un multirriego temporal el primer PAUSE consolida, 
      // pero un segundo PAUSE reiniciaria el proceso de definir el multirriego temporal, 
      // para que el usuario pueda corregir lo que ha introducido antes de lanzarlo.
      if(configure->configuringMultiTemp()) {
        if (configure->get_MultiTempReady()) { // si ya se habia consolidado el multirriego temporal, 
           handleEncStopInStandby();           // un nuevo PAUSE reinicia este proceso para que el usuario pueda corregir errores
           LOG_DEBUG("reiniciando definicion de multirriego temporal por nuevo PAUSE");
        }
        else if (multi.w_size) configure->MultiTemp_process_end();  // primer PAUSE y hay zonas: preparamos lanzamiento multirriego temporal
      }
}


     /*---------------------------------------------------------------*
      *                                                               *
      *            Funciones de la Maquina de Estados                 *
      *                                                               *
      ----------------------------------------------------------------*/


/**--------------------------------------------------------------------------------------------------
 * Activa estado pasado en la maquina de estados y en la interfaz de usuario (leds, display, sonidos)
 * (salvo el caso de estado ERROR que se gestiona en statusError)
 */
void setEstado(m_estados estado, int bipcount, estado_tipos tipo, velocidad_parpadeo ledblink)
{
  LOG_DEBUG( "recibido estado", estado, "bipcount=", bipcount, " tipo=", tipo, " ledblink=", ledblink);
  // si pedimos STANDBY y el boton STOP esta pulsado, pasamos a STOP en su lugar
  if (estado == STANDBY && testButton(bSTOP,ON)) estado = STOP; 
  // 1.setup maquina de estados
  setStateMachine(estado, tipo);
  // 2.setup interfaz de usuario
  setUI(estado, bipcount, tipo, ledblink);
} //fin setEstado

/**----------------------------------------------------------------------------
 * Maquina de estados: Inicializa estado pasado y flags asociados
 */
void setStateMachine(m_estados estado, estado_tipos tipo)
{
  if (estado == Estado.estado && tipo == Estado.tipo) 
      LOG_WARN("llamada SIN cambio de estado/tipo, actual:", Estado.estado, Estado.tipo, "pedido:", estado, tipo);
  // set state (FSM):
  Estado.estado = estado;
  Estado.error = NOERROR;
  Estado.tipo = tipo;
  Estado.failedStopRiego = false;
  Estado.recoverableError = false;
  Estado.errorInformado = false;
  Estado.showWifiOK = false;
  getBotonPointer(bPAUSE)->flags.holddisabled = true; //Deshabilitamos el hold de Pause
  rotaryEncoder.disable();  // para que no cuente pasos salvo que lo habilitemos
  if(Estado.reposo) reposoOFF();     //por si salimos de stop antinenes
  if (estado == STOP || (estado == STANDBY && !multi.riegoON && tipo != WAITING)) resetFlags(); //reset flags riegos en curso
  if (estado == CONFIGURANDO) simulaPauseIfEncoderSW(INITIALIZE); // para evitar que al entrar en configuracion se simule un pause por el encoderSW pulsado
  standbyTime = millis(); //reseteamos tiempo de inactividad    
}

/**----------------------------------------------------------------------------
 * Ajusta la interfaz de usuario UI al estado pasado
 * (display, leds y sonidos)
 */
void setUI(m_estados estado, int bipcount, estado_tipos tipo, velocidad_parpadeo ledblink)
{
    // literales para los estados en el display (Definición estática y vinculada por índice de Enum m_estados)
    static const char* const nEstado[] = {
        [INITIAL]      = "",
        [STANDBY]      = "STANDBY",
        [REGANDO]      = "REGANDO:",
        [CONFIGURANDO] = "CONFIGURANDO",
        [TERMINANDO]   = "TERMINANDO",
        [PAUSE]        = "PAUSA:",
        [STOP]         = "STOP",
        [ERROR]        = "ERROR",
        [DIFERIDO]     = "DIFERIDO"
    };
    const char* textoEstado = (estado >= 0 && estado < NUM_ESTADOS) ? nEstado[estado] : "UNKNOWN";
    // Verificación en tiempo de compilación de que la cantidad de estados definida en el enum y en el array coinciden
    static_assert(ELEMENTCOUNT(nEstado) == NUM_ESTADOS, "Desincronización en nEstado");    
    // Seguridad: no debe llamarse directamente para cambiar estado
    if (estado != Estado.estado) {
      LOG_ERROR("llamada directa CON cambio de estado, actual", Estado.estado, "nuevo", estado);
      return;
    }  
    // (PRE) setup elementos de interfaz (UI) comunes a la mayoria de los estados:
    resetLCD();  // enciende display
    if (estado == STANDBY || estado == STOP || estado == CONFIGURANDO || estado == DIFERIDO) resetLeds();
    else setLedStatus();

    // setup elementos de interfaz (visual y sonora) propios de cada estado
    switch (estado) {
        
        case REGANDO:
            lcd.clear();
            lcd.infoEstado(textoEstado, config.zona[zonaEnCurso.zindex].desc, bipcount);
            if (tipo == REMOTO) displayEstadoRemoto(textoEstado); // muestra en display tipo de estado (LOCAL/REMOTO)
            //muestra en pantalla las zonas que restan por regar del grupo (excluida la zona en curso) y tipo del grupo
            if (multi.riegoON) {
                displayLCDGrupo(RESTO, 2, riegoSaved.znumber);
                displayTipoGrupo();
            }
            refreshTime(); //actualizamos tiempo de cuenta atras en pantalla
            setLedsRiego(ledblink); //enciende los leds del riego en curso 
            break;
            
        case TERMINANDO:
            lcd.infoEstado(textoEstado, config.zona[zonaEnCurso.zindex].desc, bipcount);
            setLedsRiego(ledblink); //enciende los leds del riego en curso
            break;
            
        case PAUSE:
            lcd.infoEstado(textoEstado, config.zona[zonaEnCurso.zindex].desc, bipcount);
            if (tipo == REMOTO) displayEstadoRemoto(textoEstado); // muestra en display tipo de estado (LOCAL/REMOTO)
            lcd.clear(BORRA2H); //borra posible msgs de error
            if(multi.riegoON) displayTipoGrupo();
            refreshTime(); //actualizamos tiempo de cuenta atras en pantalla
            setLedsRiego(ledblink); //enciende los leds del riego en curso
            break;
            
        case STANDBY:
            lcd.infoclear("STANDBY",NOBLINK,BIP,bipcount);
            showTemp();
            StaticTimeUpdate(REFRESH);
            showWifiLevel(checkWifi(config.showwifilevel));
            setEncoderTime();
            break;
            
        case STOP:
            lcd.infoclear("STOP", NOBLINK, LOWBIP, bipcount);
            showWifiLevel(checkWifi(config.showwifilevel));
            break;
            
        case CONFIGURANDO:
            sonido.lowbip(bipcount);
            break;

        case DIFERIDO:
            lcd.infoclear("Ajuste HH:MM espera",NOBLINK,BIP,bipcount);
            lcd.info("para inicio DIFERIDO",2);
            delay(200);
            lcd.blinkLCD(2);
            StaticTimeUpdate(REFRESH);
            setEncoderTime();
            break;
    
    }
    // (POST) setup elementos de interfaz (UI) comunes a la mayoria de los estados:
    if ( Estado.modoDEMO && estado != CONFIGURANDO ) displayDemo();
}  

/**---------------------------------------------------------------
 * pasa FSM a estado ERROR
 */
void statusError(error_tipos errorID, bool recoverable, velocidad_parpadeo zonablinkvel, velocidad_parpadeo errorblinkvel) 
{
  if (Estado.estado != ERROR || Estado.error != errorID) LOG_ERROR("ERROR activado: ", errorToString(errorID), recoverable ? " [RECUPERABLE]" : "");
  else LOG_WARN("MISMO ERROR REITERADO: ", errorToString(errorID));  // por aqui no se deberia pasar nunca
  gestionarTamanoLog(); // gestionamos tamaño log tras escritura (previa) del nuevo error
  LOG_DEBUG( "recibido errorID:", errorID, "recuperable:", recoverable, "zonablinkvel:", zonablinkvel, "errorblinkvel:", errorblinkvel);
  // set state (FSM): 
      Estado.estado = ERROR;
      Estado.recoverableError = recoverable; //error recuperable o no
      Estado.error = errorID;
      Estado.tipo = LOCAL;
      resetFlags(); // reseteamos flags varios
      rotaryEncoder.disable();
  // set user interfase (UI):
      lcd.clear(BORRA2H);
      lcd.setCursor(2,2);
      lcd.print(">>>  Error");
      lcd.print(errorID == E0 ? 0 : (int)errorID); // Imprime 0 si es E0, o el ID
      lcd.print(recoverable ? " >>> R" : " <<<");
      lcd.setCursor(0,3);
      lcd.print(errorToString(errorID));  // mostramos explicacion del error en pantalla
      setLedStatus(); // led RGB rojo
      sonido.bipKO();
      if (zonablinkvel) {  // señalamos parpadeando zona que ha fallado (por stop o getfactor)
        setParpadeo(tic_LedZona, zonablinkvel, parpadeoLedZona, zonaEnCurso.pBoton->led);
      }
      if (errorblinkvel) {  // parpadeo del led RGB de error
        setParpadeo(tic_LedError, errorblinkvel, parpadeoLedPWM, LEDR);
        sonido.longbip(5); // resaltamos error al parar riego
      }
}  //fin statusError


     /*---------------------------------------------------------------*
      *                                                               *
      *                    Funciones auxiliares                       *
      *                                                               *
      ----------------------------------------------------------------*/


void procesaIfWebServer()
{
  #ifdef WEBSERVER
    procesaWebServer();
  #endif    
} 

/**---------------------------------------------------------------
 * Chequeo de perifericos
 */
void check()
{
  apagaLeds();
  #ifndef noCHECK
    initLeds();
  #endif
}

/**------------------------------------------------------------------------------------------------------------
 * Verifica si hay conexion con el SCD y en ese caso llama a loadFactorRiegos. 
 * Devuelve true si el SCD esta operativo y se han podido cargar los factores de riego, false en caso contrario.
 * Si el SCD no responde, se activa error E2 recuperable si no hay error previo
 */
bool checkAndInitFactorRiegos(bool signalError) {
  //inicializamos a valor 100 por defecto para caso de error
  for(uint i=0;i<NUMZONAS;i++) { factorRiegos[i]=100; }
  factorRiegosLeido = false;
  if (config.SCD_ip[0] == '\0') { //si no hay ip de domoticz configurada, no intentamos leer factores de riego
    LOG_ERROR("No hay IP de Domoticz configurada, no se cargarán factores de riego");
    if (signalError) statusError(E2); // activamos error de conexion con SCD no recuperable
    return false;
  }
  //si no tenemos wifi o noWIFI, ni lo intentamos
  if (!Estado.connected || Estado.noWIFI) return false;
  lcd.info("conectando Domoticz", 2);
  if (!checkSCD()) {
    if (signalError) statusError(E2, RECUPERABLE); // activamos error de conexion con SCD recuperable
    return false;
  }
  lcd.info("Domoticz OK", 2);
  lcd.clear(BORRA2H);
  // si hemos sido llamados por un error previo grabamos que ya se ha recuperado el error de conexion con SCD
  if (!signalError) logStatus("Conectado a Domoticz, leyendo factores de riego...");
  return loadFactorRiegos();
}


/**---------------------------------------------------------------
 * Lee factores de riego del domoticz
 */
bool loadFactorRiegos()
{
  LOG_DEBUG("entrada loadFactorRiegos Estado.error=", Estado.error, "Estado.recoverableError=", Estado.recoverableError, "noWIFI=", Estado.noWIFI);
  //leemos factores del Domoticz:
  for(uint i=0;i<NUMZONAS;i++) {
    uint factorR = getFactor(i+1, factorRiegosLeido);
    if(factorR == 999) break;     //en modoDEMO no continuamos iterando si no se ha podido leer por alguna causa
    if (Estado.estado == ERROR) break;   //al primer error salimos
    factorRiegos[i] = factorR;
    LOG_TRACE("zona",i+1,"factor asignado=",factorR);
    // si XNAME: true, leemos la descripcion de la zona del domoticz (si existe) y la guardamos en config
    if (config.xname) updateZoneDescription(i);
  }
  zonaEnCurso = S_zonaEnCurso{}; // reseteamos zonaEnCurso para evitar que quede apuntando a una zona erronea
  LOG_DEBUG("salida  loadFactorRiegos Estado.error=", Estado.error, "Estado.recoverableError=", Estado.recoverableError, "Estado.noWIFI=", Estado.noWIFI);
  if(Estado.error) return false;
  #ifdef VERBOSE
    printFactoresRiego();
  #endif
  return true;
}  //fin loadFactorRiegos

//Aqui convertimos minutes y seconds por el factorRiegos
void timeByFactor(int factor,uint8_t *fminutes, uint8_t *fseconds)
{
  uint tseconds = (60*tm.major) + tm.minor;
  //factorizamos
  tseconds = (tseconds*factor)/100;
  if (tseconds > 59*60) tseconds = 59*60; //limitamos tiempo maximo factorizado a 59 minutos
  //reconvertimos
  *fminutes = tseconds/60;
  *fseconds = tseconds%60;
}

// void cbSyncTime(struct timeval *tv)  { // callback function to show when NTP was synchronized
//   struct tm timeinfo;
//   getLocalTime(&timeinfo);
//   Serial.printf("<<<<   NTP time synched   >>>>   Local time: %s \n", asctime(&timeinfo));
// }

// set reloj del ESP32 y timezone con el time recibido por NTP (se actualizara automaticamente cada 3 horas (default))
void setClock()
{
  // sntp_set_time_sync_notification_cb(cbSyncTime);  // set a Callback function for time synchronization notification
  // sntp_set_sync_interval(60 * 60 * 1000UL); // 60 minutos (default ESP32 es 180 minutos - 3 horas)
  if (!Estado.connected) return; //si no tenemos wifi no intentamos sincronizar reloj
  if (Estado.inSetup || Estado.error) lcd.info("sincronizando clock", 2);
  LOG_DEBUG("Timezone: ", config.TZ, "   NTP server: ", config.ntpServer);
  configTzTime(config.TZ, config.ntpServer); 
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, NTP_TIMEOUT)) {
    timeOK = false;
    if (Estado.inSetup) LOG_ERROR(">>> NO TIME SET by NTP <<<");
    return;
  }
  timeOK = true;
  if (Estado.inSetup || Estado.error) {
    // Creamos el mensaje para el LCD (ej: "clock OK 14:30")
    strftime(buff, MAXBUFF, "clock OK  %H:%M", &timeinfo);
    lcd.info(buff, 2); // Mostramos la hora en la línea 2
    delay(config.msgdisplaymillis);
  }
  // Creamos el mensaje para el log
  char message[150];
  strftime(message, sizeof(message), ">>> TIME SET by NTP <<<   Local time: %A, %B %d %Y %H:%M:%S (zone %Z %z)", &timeinfo);
  PRINTLN("\n[setClock]", message);
  LOG_INFO("NTP update every ", sntp_get_sync_interval()/(1000*60), " minutos");
  if (!Estado.inSetup) logStatus(message); // registramos en log de errores que ya tenemos NTP time 
}


// devuelve time_t en hora local a partir del time_t del sistema (UTC)
time_t tLoc()
{
  if (!timeOK) return (millis() / 1000); //no tenemos time, devolvemos segundos transcurridos desde el arranque 
  time_t t = time(NULL); // time() devuelve el tiempo UTC actual (epoch time en segundos desde 00:00 1/1/1970) 
  struct tm tm_loc;
  localtime_r(&t, &tm_loc); 
  // _timezone en ESP32 guarda el desfase ESTÁNDAR (invierno) cambiado de signo.
  // Para España (UTC+1), _timezone vale -3600. Por eso usamos el signo menos (-_timezone).
  // Si tm_isdst > 0, significa que actualmente estamos en horario de verano (+1 hora extra = 3600s).
  long desfaseTotal = -_timezone + (tm_loc.tm_isdst > 0 ? 3600 : 0);
  return t + desfaseTotal;
}


// Devuelve el timestamp del último inicio de día (00:00:00) para un timestamp dado
time_t previousMidnight(time_t tLocal) {
    struct tm tm_s = getTimeStruct(tLocal);
    // Calculamos cuántos segundos han pasado desde las 00:00:00 de hoy
    uint32_t segundosHoy = (tm_s.tm_hour * 3600UL) + (tm_s.tm_min * 60UL) + tm_s.tm_sec;
    return tLocal - segundosHoy;
}  


void initEncoder() {
    LOG_TRACE("");
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    setEncoderTime();
}

void setEncoderTime() {
    LOG_TRACE("");
    rotaryEncoder.setBoundaries(-2147483648, 2147483647, false); // Usamos los límites máximos del tipo long para que sea virtualmente infinito
    rotaryEncoder.setEncoderValue(0);
    rotaryEncoder.setAcceleration(50); // set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
    rotaryEncoder.enable();
    // tm.value = ((tm.minor == 0) ? tm.major : tm.minor); //set valor tm.value para ajustar tiempo con procesaencoder
    }

void setEncoderRange(int min, int max, int current, int aceleracion) {
    LOG_DEBUG("min=",min,"max=",max,"current=",current);
    rotaryEncoder.setBoundaries(min, max, false); //minValue, maxValue, circleValues true|false 
    rotaryEncoder.setEncoderValue(current);
    rotaryEncoder.setAcceleration(aceleracion);
    rotaryEncoder.enable();
}

void setEncoderMenu(int menuitems, int currentitem) {
    LOG_DEBUG("currentitem=",currentitem, "menuitems=",menuitems);
    rotaryEncoder.setBoundaries(0, menuitems, true); //minValue, maxValue, circleValues true|false
    rotaryEncoder.setEncoderValue(currentitem);
    rotaryEncoder.disableAcceleration();
    rotaryEncoder.enable();
}

//muestra dia/hora actual y lo regado desde las 0h/ultimas24h encendiendo sus leds, o apagandolos
void ultimosRiegos(int modo) {
  static const char* const MESES[] = {"Ene.", "Feb.", "Mar.", "Abr.", "May.", "Jun.", "Jul.", "Ago.", "Sep.", "Oct.", "Nov.", "Dic."};
  switch(modo) {
    case HIDE:
      setParpadeo(tic_LedZonas24h, PARAR);
      for(uint i=0; i<NUMZONAS; i++) led(getBotonPointer(Zonas[i])->led, OFF);
        break;
    case SHOW:
      lcd.infoclear("Hora actual:");
      // Si no tenemos hora, mostramos mensaje de error y salimos
      if (!timeOK) { lcd.info("   <<< NO TIME >>>", 3); sonido.bipKO(); return; }
      time_t t = tLoc();
      // Si la fecha actual es invalida (anterior al 1/1/2026) mensaje de error y salimos 
      if (t < UMBRAL_EPOCH) { lcd.info(" << INVALID DATE >>", 3); sonido.bipKO(); return; }
      struct tm tm_now = getTimeStruct(t);  // obtenemos estructura tm con la fecha y hora local
      sprintf(buff, " %d", tm_now.tm_mday);
      lcd.info(buff, 3);
      lcd.info(MESES[tm_now.tm_mon], 4); // tm_mon ya es 0-11, perfecto para el array
      lcd.displayTime(tm_now.tm_hour, tm_now.tm_min);
      // Enciende o hace parpadear los leds de las zonas que se han regado
      static S_ledsParpadeo ledsParpadeo; // static: no se borra al salir de la función para poder usarla en parpadeoLedZonas
      ledsParpadeo.cantidad = 0;
      time_t limit24h = t - SECS_PER_DAY;
      time_t midnight = previousMidnight(t); // obtenemos timestamp de la última medianoche (hora local)
        // Encendemos leds de las zonas que se han regado desde medianoche hasta ahora
        for(uint i=0; i<NUMZONAS; i++) {
          if(lastRiegos[i].inicio > midnight) {
              LOG_DEBUG("[ULTIMOSRIEGOS medianoche] zona:", i+1, "time:", lastRiegos[i].inicio);
              led(getBotonPointer(Zonas[i])->led, ON);
            }
          else if (config.lastr24 && lastRiegos[i].inicio > limit24h) {
              LOG_DEBUG("[ULTIMOSRIEGOS 24h] zona:", i+1, "time:", lastRiegos[i].inicio);
              ledsParpadeo.leds[ledsParpadeo.cantidad] = getBotonPointer(Zonas[i])->led;
              ledsParpadeo.cantidad++;
          }
        }
      // Activa parpadeo leds zonas regadas desde hace 24h y medianoche, pasando la estructura con los leds
      // por su dirección de memoria &ledsParpadeo (4 bytes, Ticker no admite pasar un valor de mas tamaño)
      if (ledsParpadeo.cantidad > 0) tic_LedZonas24h.attach(RAPIDO/10.0, parpadeoLedZonas, &ledsParpadeo);
      break;
    }  
}


void inicioTimeLastRiego(S_timeRiego &timeRiego, bool resume) 
{
  time_t t = tLoc();
  if (resume)
  {
    // si estamos reanudando un riego, mantenemos el inicio del riego anterior
    LOG_DEBUG("actualizo lastriegos: reanudando riego. Timestamp:", t);
    timeRiego.reinicio = t;
  }
  else
  {
    // si estamos iniciando, actualizamos el inicio del riego
    LOG_DEBUG("actualizo lastriegos: iniciando riego. Timestamp:", t);
    timeRiego.inicio = t;
    timeRiego.final = 0;
    timeRiego.reinicio = t;
    timeRiego.total = 0;
  }
}  

void finalTimeLastRiego(S_timeRiego &timeRiego) 
{
  time_t t = tLoc();
  LOG_DEBUG("actualizo lastriegos fin Zona", zonaEnCurso.znumber, "timestamp:", t);
  timeRiego.final = t;
  timeRiego.total = timeRiego.total + (timeRiego.final - timeRiego.reinicio); //acumulado = acumulado + (intervalo regado)
  LOG_DEBUG("regado hasta ahora Zona", zonaEnCurso.znumber, "tiempo",timeRiego.total / 60, "m :", timeRiego.total % 60, "s");
}  

void finalTimeGrupo(S_timeRiego &timeRiego, time_t tZona) 
{
  // si tZona no es 0, es el tiempo de regado de una zona del grupo -> acumulamos el tiempo de la zona al total del grupo
  if (tZona) {
    timeRiego.total = timeRiego.total + tZona; //total = acumulado hasta ahora + tiempo ultima zona regada
    LOG_DEBUG("regado hasta ahora Grupo", multi.ngrupo, "acumulado:", timeRiego.total / 60.0, "minutos");
  }
  // si tZona es 0, significa que estamos en fin de riego de grupo -> registramos el timestamp final del grupo
  else {
    time_t t = tLoc();
    LOG_DEBUG("actualizo lastriegos fin Grupo", multi.ngrupo, "timestamp:", t);
    timeRiego.final = t;
    LOG_DEBUG("total riego Grupo", multi.ngrupo, ":", timeRiego.total / 60.0, "minutos");
  }
}  

// Muestra en el display el tiempo del ultimo riego de la zona o grupo apuntado en timeRiego.
void showTimeLastRiego(S_timeRiego &timeRiego) 
{
  time_t t1=timeRiego.inicio;
  time_t t2=timeRiego.final;
  time_t tnow = tLoc(); // timestamp local actual en segundos desde 1/1/1970
  LOG_DEBUG("time.inicio", t1, "time.final", t2, "time.now", tnow, "time.total (seg)", timeRiego.total);
  // Caso 1: Si tenemos inicio y finalizacion del riego mostramos la duración del riego en minutos 
  if (t1 && t2-t1 > 0) {  // si t2>t1>0, tenemos un riego registrado con inicio y fin correcto (real o arranque)
    snprintf(buff, MAXBUFF, "-ultimo riego:  %02dm", (timeRiego.total+20)/60);
    lcd.info(buff,3);
    // Los timestamps pueden ser absolutos (tiempo real obtenido via NTP) o relativos 
    // (tiempo desde arranque o incluso un fake-hwclock obtenido del sistema del SCD), 
    // por lo que se contemplan varios subcasos:
    // CASO 1.A: El riego se grabó con fecha real, la mostramos (ej: " 15/09 18:30 (18:45)")
    if (t1 >= UMBRAL_EPOCH) {
      struct tm tm1 = getTimeStruct(t1);
      struct tm tm2 = getTimeStruct(t2);
      snprintf(buff, MAXBUFF, " %d/%02d %d:%02d (%d:%02d)",
             tm1.tm_mday, tm1.tm_mon + 1, 
             tm1.tm_hour, tm1.tm_min, 
             tm2.tm_hour, tm2.tm_min);
    } 
    // CASO 1.B: El riego se grabó con tiempo de arranque, solo podemos calcular y mostrar el tiempo
    //   transcurrido desde que se regó (ej: " hace: 0d 2h y 15m") si tnow SIGUE contando tiempo desde el arranque
    else if (tnow < UMBRAL_EPOCH) { 
      time_t diff = (tnow > t1) ? (tnow - t1) : 0;  // Misma era, la resta es segura
      snprintf(buff, MAXBUFF, " hace: %dd %dh y %02dm", 
        (int)(diff / SECS_PER_DAY), 
        (int)((diff % SECS_PER_DAY) / 3600), 
        (int)((diff % 3600) / 60));
    // CASO 1.C: Se regó con tiempo de arranque y ahora hay NTP (tnow >> t1)
    } else strlcpy(buff, "   < sin datos >", MAXBUFF);
  // Caso 2: Si no hay tiempo registrado ( no se cumple t2>t1>0), no mostramos información de tiempo de riego
  } else {
    lcd.info("-ultimo riego:",3);
    strlcpy(buff, "   > sin datos <", MAXBUFF);
  }  
  lcd.info(buff,4);
}

/*---------------------------------------------------------------
 * Prepara temporizadores y comienza riego de la zona pulsada
 */
void startZoneWatering() {
    setZonaEnCurso(boton);
    sonido.bip(2);
    // Si multirriego factorizado, cambia minutes y seconds en funcion del factor de cada zona
    uint8_t fminutes=0,fseconds=0;
    if(multi.riegoON && !multi.noFactorizado) {
      timeByFactor(factorRiegos[zonaEnCurso.zindex],&fminutes,&fseconds);
    }
    else {
      fminutes = tm.major;
      fseconds = tm.minor;
    }
    LOG_DEBUG("Minutos:",tm.major,"Segundos:",tm.minor,"FMinutos:",fminutes,"FSegundos:",fseconds);
    // si tiempo factorizado de riego es 0 o IDX=0, nos saltamos este riego
    // TODO: dependiente idx?
    if ((fminutes == 0 && fseconds == 0) || config.zona[zonaEnCurso.zindex].idx == 0) {
      setEstado(TERMINANDO);
      lcd.clear(BORRA2H);
      lcd.info("IDX/factor:     -00-",4);
      return;
    }
    lcd.clear(BORRA2H);
    lcd.infoEstado("Iniciando", config.zona[zonaEnCurso.zindex].desc);
    if(initRiego(INICIO)) { //comenzamos el riego de la zona
      //inicializamos el timer de cuenta atras
      timer.SetTimer(0,fminutes,fseconds);
      setEstado(REGANDO,1);
      delay(300); //esperamos a que se refresque el display antes de iniciar el timer
      timer.StartTimer();
      tic_CountDownTimer.attach_ms(10, timerTick); // Llama a timerTick() cada 10 ms
    }  
}

/*---------------------------------------------------------------------------------------
 * Prepara temporizadores y comienza cuenta atras para el riego de la zona o grupo pulsado
 */
void startRiegoDiferido(const char* desc) {
    botonDefer = boton; //guardamos boton pulsado para iniciar riego tras tiempo diferido
    int rhours = tm.major;
    int rminutes = tm.minor;
    //inicializamos el timer de cuenta atras
    timer.SetTimer(rhours,rminutes,0);
    LOG_INFO("Esperando: ", rhours, "h", rminutes, "minutos para iniciar riego de",botonDefer->desc,"(",desc,")");
    setStateMachine(DIFERIDO,WAITING);
    // UI setup
    led(botonDefer->led,ON);
    sonido.bip(2);
    lcd.clear();
    lcd.infoEstado("Esperando", desc);
    lcd.info("riego comienza en:",2);
    refreshTime(true);
    //esperamos a que se refresque el display antes de iniciar el timer
    delay(1000);
    timer.StartTimer();
    tic_CountDownTimer.attach_ms(10, timerTick);
}

// Muestra en el display info zona (idx, factor de riego, fecha y tiempo ultimo riego)
void showInfoZona(int zNumber) {
    int zIndex = zNumber - 1;
    led(boton->led,ON);
    LOG_DEBUG("display de zona ", config.zona[zIndex].desc, "idx:", config.zona[zIndex].idx, "factorRiego:", factorRiegos[zIndex]); 
    lcd.clear();
    lcd.infoCut(config.zona[zIndex].desc, 11);
    lcd.setCursor(12, 0);
    snprintf(buff, MAXBUFF, "idx(%d)", config.zona[zIndex].idx);
    lcd.print(buff);
    snprintf(buff, MAXBUFF, "-factor riego:  %d", factorRiegos[zIndex]);
    lcd.info(buff,2);
    showTimeLastRiego(lastRiegos[zIndex]);
    delay(config.msgdisplaymillis*4);
    setUI(STANDBY); // restaurar pantalla STANDBY
}

// Hacemos encendido de los leds del grupo y mostramos en el display info de este
void showInfoGrupo(int n_grupo) {
    LOG_DEBUG("display de grupo:", n_grupo, "(", multi.desc,") tamaño:", *multi.size );
    snprintf(buff, MAXBUFF, "grupo: %s", multi.desc);
    lcd.infoclear(buff, 1);
    displayLCDGrupo(FULL, 2);
    showTimeLastRiego(lastGrupos[n_grupo-1]);
    displayLedsGrupo();
    delay(config.msgdisplaymillis*3);
    setUI(STANDBY); //restaurar pantalla STANDBY
}

void printFactoresRiego() {
    Serial.print(F("Factores de riego "));
    factorRiegosLeido ? Serial.println(F("leidos: ")) :  Serial.println(F("(simulados): "));
    for(uint i=0;i<NUMZONAS;i++) {
      Serial.printf("\tfactor ZONA%d: %d (%s) \n", i+1, factorRiegos[i], config.zona[i].desc);
    }
}

void setReposo(bool status) {
  Estado.reposo = status;
  dimmerLeds(status);
  lcd.setBacklight(!status);
}

void reposoOFF()
{
  // setCpuFrequencyMhz(240); // volvemos frecuencia CPU a 240Mhz
  WiFi.setSleep(WIFI_PS_NONE); // desactivamos modo ahorro energia de la radio wifi
  LOG_INFO(" salimos de reposo");
  setReposo(false);
  standbyTime = millis();
}  

void reposoON()
{
  LOG_INFO(" entramos en reposo");
  setReposo(true);
  WiFi.setSleep(WIFI_PS_MIN_MODEM); // ponemos wifi en modo ahorro energia minimo
  // setCpuFrequencyMhz(80); // bajamos frecuencia CPU a 80Mhz para ahorrar energia
}  


//lee encoder para actualizar parametro configuracion
void procesaEncoderConfig()
{

  if(configure->inMenu()) {  //encoder selecciona item menu
      int menuOption = rotaryEncoder.readEncoder();  //devuelve valor actual del encoder (se haya movido o no)
      if(menuOption == configure->get_currentItem()) return;  //no ha cambiado
      LOG_DEBUG("rotaryEncoder.readEncoder() devuelve menuOption =", menuOption, "currentItem =", configure->get_currentItem());
      configure->showMenu(menuOption);
      return;
  }

  if(configure->configuringRange()) {  //encoder ajusta valor entre un rango
      int readvalue = rotaryEncoder.readEncoder();  //devuelve valor actual del encoder (se haya movido o no)
      if(readvalue == tm.value) return;
      LOG_DEBUG("rotaryEncoder.readEncoder() devuelve readvalue =", readvalue, "tm.value=", tm.value);
      tm.value = readvalue;
      configure->Range_process_update();
      return;
  }

  if(configure->configuringIdx()) {  //encoder ajusta numero IDX
      int readvalue = rotaryEncoder.readEncoder();  //devuelve valor actual del encoder (se haya movido o no)
      if(readvalue == tm.value) return;
      LOG_DEBUG("rotaryEncoder.readEncoder() devuelve readvalue =", readvalue, "tm.value=", tm.value);
      tm.value = readvalue;
      configure->Idx_process_update();
      return;
  }

  if (configure->configuringTime()) procesaEncoderTime(); //encoder ajusta tiempo de riego por defecto
}
  
//lee encoder para actualizar el tiempo de riego (minutos:segundos) o el tiempo de retardo (horas:minutos) 
void procesaEncoderTime()
{
  int encvalue = rotaryEncoder.encoderChanged();
  if (!encvalue) return;
  if (Estado.reposo) { reposoOFF(); return; }
  bool tiempoCambiado = false;
  // Girar el encoder manteniendolo pulsado activa el estado DIFERIDO para definir tiempo retardo
  if (encoderSW && Estado.estado == STANDBY) { setDiferido(); return; }
  LOG_DEBUG("ENTRADA: encvalue=", encvalue, "tm.major=", tm.major, "tm.minor=", tm.minor);
  tiempoCambiado = (Estado.tipo == SETDEFER ?
                    ajustaTiempoProgresivo(encvalue, MAXHOURS) :
                    ajustaDuplaTiempo(encvalue, MAXMINUTES, MINSECONDS) );
  LOG_DEBUG("SALIDA: \t\t tm.major=", tm.major, "tm.minor=", tm.minor);
  if (tiempoCambiado) {
    configure->configuringTime() ? configure->Time_process_update() : StaticTimeUpdate(UPDATE);
    standbyTime = millis();
  }
}

/* Ajuste en estado STANDBY o CONFIGURANDO tiempo de riego por defecto,
*  encoder ajusta tiempo (MM:SS) de MINSECONDS a MAXMINUTES.
*  En segundos hasta 59 y a partir de ahí en minutos enteros.
*  Por lo tanto uno de los dos (mm o ss) debe ser 0.
*  NOTA tecnica: este metodo de ajuste (basado en estados: o configurando minutos o segundos)
*  procesa bien los saltos de bloque aunque el valor absoluto de encvalue sea mayor que 1
*  (evento que se da por la aceleracion que definimos en el encoder).
*/
bool ajustaDuplaTiempo(int encvalue, uint8_t maxMayor, uint8_t minMenor)
{
  uint8_t minAnterior = tm.major;
  uint8_t segAnterior = tm.minor;
  int pasos = abs(encvalue); // para claridad de las operaciones: pasos siempre sera un valor positivo
  // --- RAMA CRECIENTE: encvalue POSITIVO ---
  if (encvalue > 0) {
    if (tm.minor == 0 && tm.major > 0) { // Rango de minutos enteros
      tm.major += pasos;
    } 
    else { // Rango de segundos (tm.major = 0)
      tm.minor += pasos;
      if (tm.minor >= 60) { // La aceleración nos hace saltar a minutos enteros
        tm.major = tm.minor - 59; // contamos los pasos restantes como minutos
        tm.minor = 0;
      }
    }
    if (tm.major > maxMayor) tm.major = maxMayor; // ajuste limite superior tm.major
  } 
  // --- RAMA DECRECIENTE: encvalue es NEGATIVO --- (pasos positivos)
  else { 
    if (tm.minor == 0 && tm.major > 0) { // Rango de minutos enteros
      if (tm.major > pasos) tm.major -= pasos; 
      else { // La aceleración consume todos los minutos. Restamos los pasos restantes como segundos
        tm.major = 0;
        int minorCalculado = 59 + minAnterior - pasos;
        tm.minor = (minorCalculado > minMenor) ? minorCalculado : minMenor;
      }
    } 
    else  // Rango de segundos (tm.major = 0)
      tm.minor = (tm.minor > pasos + minMenor) ? (tm.minor - pasos) : minMenor;
  }
  return (tm.major != minAnterior || tm.minor != segAnterior); // True si ha cambiado
}


/* Ajusta el tiempo HH:MM con incrementos progresivos según el valor actual y el sentido de giro:
*  - Hasta 10 min: pasos de 1 min.
*  - De 10 min a 1 hora: pasos de 10 min.
*  - A partir de 1 hora: pasos de 30 min.
*  NOTA tecnica: este metodo de ajuste (basado en ajustar el Tiempo Lineal del elemento menor)
*  no procesa bien los saltos de bloque cuando el valor absoluto de encvalue sea mayor que 1.
*  Esto obliga a procesar los pasos recibidos uno a uno en el while.
*  Sin embargo si se adapta mejor a multiples niveles de salto de bloque como es el caso.
*/
bool ajustaTiempoProgresivo(int encvalue, uint8_t maxHours)
{
  const uint16_t MAX_MINUTOS = maxHours * 60;
  const uint16_t MIN_MINUTOS = 1;
  // Calculamos los minutos totales actuales
  uint16_t minutosTotales = (tm.major * 60) + tm.minor;
  uint16_t minutosAnteriores = minutosTotales;
  // Procesamos el movimiento paso a paso virtual (con aceleración encvalue puede ser diferente a +/-1)
  int direccion = (encvalue > 0) ? 1 : -1;
  int pasosRestantes = abs(encvalue);
  while (pasosRestantes > 0) {
    uint8_t pasoMinutos = 1; // Por defecto rango < 10
    // Evaluamos el rango actual en este sub-paso
    if (direccion > 0) {
      // Lógica creciente paso a paso
      if (minutosTotales >= 60)      pasoMinutos = 30;
      else if (minutosTotales >= 10) pasoMinutos = 10;
    } 
    else {
      // Lógica decreciente paso a paso
      if (minutosTotales > 60)                              pasoMinutos = 30;
      else if (minutosTotales == 60 || minutosTotales > 10) pasoMinutos = 10;
    }
    LOG_DEBUG("encvalue", encvalue, "minutosTotales", minutosTotales, " pasoMinutos=", pasoMinutos);
    // Aplicamos la variación de este paso individual
    int16_t cambioUnitario = direccion * pasoMinutos;
    // Control de límites por cada iteración
    if (cambioUnitario > 0) {
      if (minutosTotales + cambioUnitario > MAX_MINUTOS) {
        minutosTotales = MAX_MINUTOS; break; } // Alcanzado el máximo absoluto, paramos el bucle
      minutosTotales += cambioUnitario;
    } 
    else {
      if ((minutosTotales + cambioUnitario) < MIN_MINUTOS) {
        minutosTotales = MIN_MINUTOS; break; } // Alcanzado el mínimo absoluto, paramos el bucle
      minutosTotales += cambioUnitario;
    }
    LOG_DEBUG("minutosTotales ajustados", minutosTotales, "minutosAnteriores", minutosAnteriores, "cambio", cambioUnitario);
    pasosRestantes--;
  }
  // Descomponemos nuevo tiempo ajustado a la estructura global tm
  tm.major = minutosTotales / 60;
  tm.minor = minutosTotales % 60;
  LOG_DEBUG("Tiempo ajustado: ", tm.major, "h", tm.minor, "minutos");
  return (minutosTotales != minutosAnteriores); // True si ha cambiado
}



// Carga la estructura de ultimos riegos de zonas desde el archivo correspondiente o inicializa a ceros
void initLastRiegos()
{
  static_assert(ELEMENTCOUNT(lastRiegos) == NUMZONAS, "El tamaño del array lastRiegos debe ser igual a NUMZONAS");
  memset(lastRiegos, 0, sizeof(lastRiegos)); // Inicializamos a ceros por defecto para el caso de que no se pueda cargar o no exista el archivo
  if (loadRiegosFromFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS)) {
    LOG_INFO("Ultimos riegos de zonas leidos de ", lastRiegosFile);
  } else {
    LOG_WARN("No se han podido cargar los ultimos riegos desde ", lastRiegosFile, ", inicializados a ceros");
    saveRiegosToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS, true); // intentamos crear el archivo con la estructura inicializada a ceros para futuros usos
    }
}

// Carga la estructura de ultimos riegos de grupos desde el archivo correspondiente o inicializa a ceros
void initLastGrupos()
{
  static_assert(ELEMENTCOUNT(lastGrupos) == NUMGRUPOS, "El tamaño del array lastGrupos debe ser igual a NUMGRUPOS");
  memset(lastGrupos, 0, sizeof(lastGrupos)); // Inicializamos a ceros por defecto para el caso de que no se pueda cargar o no exista el archivo
  if (loadRiegosFromFile(lastGruposFile, "lastGrupos", lastGrupos, NUMGRUPOS)) {
    LOG_INFO("Ultimos riegos de grupos leidos de ",lastGruposFile);
  } else {
    LOG_WARN("No se han podido cargar los ultimos riegos desde ", lastGruposFile, ", inicializados a ceros");
    saveRiegosToFile(lastGruposFile, "lastGrupos", lastGrupos, NUMGRUPOS, true); // intentamos crear el archivo con la estructura inicializada a ceros para futuros usos
    }
}

// Inicia/reanuda el riego correspondiente al boton de zona pulsado ultimo
bool initRiego(bool resume)
{
    int zIndex = zonaEnCurso.zindex;
    if (zIndex < 0) return false;
    led(zonaEnCurso.pBoton->led,ON);
    if (resume) LOG_INFO( "Continuando riego: ", config.zona[zIndex].desc);
    else LOG_INFO( "Iniciando riego: ", config.zona[zIndex].desc);
    if (deviceSwitch(zIndex+1, "On", SWITCH_RETRIES)) { 
        inicioTimeLastRiego(lastRiegos[zIndex], resume);
        #ifdef EXTRADEBUG
          for(uint i=0;i<NUMZONAS;i++) { LOG_DEBUG("[ULTIMOSRIEGOS] inicio zona:", i+1, "time:",lastRiegos[i].inicio); }
        #endif
        return true; 
    } else {
        // Error al iniciar: generamos la alerta con el código recibido de deviceSwitch y el mensaje de error correspondiente
        statusError(Estado.error);
        LOG_ERROR( "Error al iniciar riego de: ", config.zona[zIndex].desc );
        return false; // error al iniciar el riego   
    }
}

// Termina/interrumpe el riego correspondiente al boton de zona apuntado por pBoton
//  update: Si TRUE, actualiza hora de fin de riego (por defecto). 
//             FALSE en llamadas desde stopAllRiegos o al cancelar riego en pausa).
//  alertIfFails: Si TRUE y falla, dispara la alerta (por defecto). 
//                   FALSE en llamada desde stopAllRiegos).
bool stopRiego(const S_BOTON* pBoton, bool update, bool alertIfFails, int retries)
{
    if (pBoton == nullptr) return false; // Control preventivo
    int zIndex = pBoton->zNumber() - 1;
    LOG_DEBUG( "Terminando riego: ", config.zona[zIndex].desc, "updateTimeFin:", update, "alertIfFails:", alertIfFails);
    if (deviceSwitch(zIndex+1, "Off", retries)) {
        // solo actualizamos hora de fin si no hemos sido llamado desde stopAllRiegos a desde pausa
        if(update) finalTimeLastRiego(lastRiegos[zIndex]);
        const char* msgOk = "Terminado OK riego:";
        int totalSegundos = lastRiegos[zIndex].total;int min = totalSegundos / 60;int seg = totalSegundos % 60;
        if (Estado.estado == PAUSE && update) LOG_INFO( "(PAUSA)" , msgOk, config.zona[zIndex].desc );
          else if(update || alertIfFails) LOG_INFO(msgOk, config.zona[zIndex].desc, " tiempo regado: ", min, "m :", seg, "s");
                else LOG_INFO( msgOk, config.zona[zIndex].desc );
        #ifdef EXTRADEBUG
            for(uint i=0;i<NUMZONAS;i++) {
                  LOG_DEBUG("[ULTIMOSRIEGOS] fin zona:", i+1, "time:",lastRiegos[i].final);
              }
        #endif
        return true;
    } else { 
        // Error al apagar la EV
        if (alertIfFails) {  // Recordatorio de EV no cerrada.
          LOG_ERROR( "Error al detener riego de: ", config.zona[zIndex].desc );
          statusError(Estado.error,NORECUPERABLE,RAPIDO,RAPIDO); //disparamos alerta con el error ya establecido
          Estado.failedStopRiego = true; // El riego NO se detuvo, activar el recordatorio de error 
        } else {
            LOG_WARN( "Error al detener riego de: ", config.zona[zIndex].desc);
            statusError(Estado.error);
        }
        return false;
    }
}

//Pone a off todos los leds de riegos e intenta detener todas las EV de riegos
bool stopAllRiegos()
{
    LOG_TRACE("");
    bool allRiegoOK = true;
    int retries = SWITCH_RETRIES;
    // Apago los leds de multirriego y zonas y sus tickers de parpadeos
    resetLeds();
    // Paramos todas las zonas de riego (sin actualizar hora fin de riego)
    // Si falla no se activa la alerta de pendiente de parar riego de zona
    for(unsigned int i=0;i<NUMZONAS;i++) {
        const S_BOTON* pBoton = getBotonPointer(Zonas[i]); 
        if(!stopRiego(pBoton, false, false, retries)) { 
            allRiegoOK = false; // Marcamos que el lote falló
            if (Estado.error == E1) return false; // Si no hay WiFi, abortamos del todo inmediatamente
            // Si es un error de comunicación (E2 o E3), asumimos que el SCD está comprometido.
            // No abortamos para intentar cerrar el resto, pero reducimos los intentos a 1 
            if (Estado.error == E2 || Estado.error == E3) retries = 1; 
            // Si E3/4/5 decrementamos el número de intentos para las siguientes zonas
            else retries = (retries > 1) ? retries - 1 : 1;
            lcd.info(config.zona[i].desc,2); // Mostramos en pantalla la zona que ha fallado
        }
    }
    return allRiegoOK; // Retornamos el resultado del lote
}

//Guarda el estado del riego en curso para una posible reanudacion
void saveRiego(int znumber, S_BOTON* boton, int minutes, int seconds)
{
  if (znumber) LOG_INFO("salvando estado riego zona :",znumber," tiempo restante: ", minutes, "m :", seconds, "s");
  else LOG_DEBUG("reset estado riego salvado");
  riegoSaved.znumber = znumber;
  riegoSaved.pBoton = boton;
  riegoSaved.minutes = minutes;
  riegoSaved.seconds = seconds;
}

//Recupera el estado del riego salvado dejandolo en PAUSE para que el usuario confirme el reinicio
void restoreRiego()
{
    if (riegoSaved.znumber == 0)  //no hay riego salvado
        return;
    LOG_INFO("recuperando riego salvado de zona:", riegoSaved.znumber);
    setZonaEnCurso(riegoSaved.pBoton); //recuperamos zona en curso
    // led(zonaEnCurso.pBoton->led,ON); //encendemos led de la zona
    timer.SetTimer(0,riegoSaved.minutes,riegoSaved.seconds);  //inicializamos el timer de cuenta atras
    lcd.displayTime(timer.ShowMinutes(), timer.ShowSeconds());
    riegoSaved = S_Riego_estado{}; // reseteamos estado de riego salvado, ya no es valido
    setEstado(PAUSE); //ponemos en PAUSE para que el usuario confirme el inicio del riego salvado
}    

//Pone a off todos los leds de zonas y grupos y restablece estado led RGB
void resetLeds()
{
  //Apago los leds de multirriego
  for(unsigned int j=0;j<NUMGRUPOS;j++) {
    led(getBotonPointer(Grupos[j])->led,OFF);
  }
  //Apago los leds de riego y posible parpadeo
  setParpadeo(tic_LedZona, PARAR);
  setParpadeo(tic_LedZonas24h, PARAR);
  setParpadeo(tic_LedWhite, PARAR);
  for(unsigned int i=0;i<NUMZONAS;i++) {
    led(getBotonPointer(Zonas[i])->led,OFF);
  }
  //restablece led RGB
  setLedStatus();  //restablece led RGB a estado actual  
}

//Reset diversos flags de estado a valores por defecto
void resetFlags()
{
  LOG_TRACE("");
  multi = S_MULTI{}; // reset estado de multirriego
  riegoSaved = S_Riego_estado{}; // reset estado de riego salvado
  zonaEnCurso = S_zonaEnCurso{}; // reset zona en curso
  Estado.botonSemaforo = false; 
  cancelFromPause = false;
  webServerAct = false;
  simular.all_simFlags = false;
}


//reset estado display LCD
void resetLCD()
{
  LOG_TRACE("LCD reseteado");
  lcd.setBacklight(ON);
  lcd.displayON();
}

// Parpadeo del display LCD cada BLINKMILLIS ms (en el loop)
// y del led amarillo en PAUSE
void blinkDisplay()
{
  static unsigned long lastBlinkPause = 0; // Se inicializa solo en la primera llamada
  if (!lcd.get__displayOff()) {
    if (millis() - lastBlinkPause >= 1.5*BLINKMILLIS) {  // *1.5 para compensar inercia LCD
      lastBlinkPause = millis();
      lcd.displayOFF();
      if(Estado.estado == PAUSE) ledYellow(OFF);
      if(Estado.estado == DIFERIDO) ledWhite(OFF);
    }
  }
  else {
    if (millis() - lastBlinkPause >= BLINKMILLIS) {
      lastBlinkPause = millis();
      lcd.displayON();
      if(Estado.estado == PAUSE) ledYellow(ON);
      if(Estado.estado == DIFERIDO) ledWhite(ON);
    }
  }
}


// @brief Actualiza el tiempo estatico en pantalla
// @param refresh si true actualiza incondicionalmente, en caso contrario solo si ha cambiado
void StaticTimeUpdate(bool refresh)
{
  static uint8_t prev_minor = 255; // Inicialización de seguridad
  static uint8_t prev_major = 255; 
  if (refresh) lcd.clear(BORRA2H);
  if (prev_minor != tm.minor || prev_major != tm.major || refresh) {
    lcd.displayTime(tm.major, tm.minor); 
    prev_minor = tm.minor;
    prev_major = tm.major;
  }
}

void refreshTime(bool hour)   // Actualiza la cuenta atrás en pantalla
{
  if(hour) lcd.displayTime(timer.ShowHours(), timer.ShowMinutes(), timer.ShowSeconds(), 0, LCDBIGROW);
  else lcd.displayTime(timer.ShowMinutes(), timer.ShowSeconds());
}

// Para actualizar el temporizador de cuenta atrás
void timerTick() {
    timer.Timer();
}

// Actualiza UI: alertas sonoras y modo reposo según el tiempo restante de la cuenta atrás
void actualizarAlertasYReposo()
{ 
  uint8_t minRestantes = timer.ShowMinutes();
  uint8_t segRestantes = timer.ShowSeconds();
  // Alerta visual y sonora: al llegar a COUNTDOWNBIP segundos fija pantalla y parpadeo rapido led white
  if (!minRestantes && segRestantes == COUNTDOWNBIP+2) {
    Estado.tipo = IMMED;
    lcd.displayON();
    tic_LedWhite.attach(RAPIDO/10.0, parpadeoLedWhite);
    sonido.longbip(2);
    LOG_DEBUG(minRestantes,":",segRestantes,"restantes, Estado.tipo",Estado.tipo);
  }
  // Alerta sonora: ultimos COUNTDOWNBIP segundos
  if (!minRestantes && segRestantes <= COUNTDOWNBIP) {
    sonido.bip(1);
  }
  // Gestión del modo reposo (Entrar en reposo si se cumple el tiempo de inactividad)
  if (!Estado.reposo && (minRestantes >= COUNTDOWNSHOW) && (millis() - standbyTime >= (1000UL * STANDBYSECS))) {
    LOG_DEBUG("Entrando en reposo tras ", STANDBYSECS, " segundos de inactividad");
    reposoON();
  } 
  // Salir de reposo si queda poco tiempo de cuenta atrás
  else if (Estado.reposo && (minRestantes < COUNTDOWNSHOW)) {
    LOG_DEBUG("Saliendo de reposo por cuenta atras menor que ", COUNTDOWNSHOW, " minutos (minRestantes=", minRestantes, ")");
    reposoOFF();
    sonido.longbip(1);
  }
}

void procesaCuentaAtras() 
{
  // Cuenta atras activa: actualiza display y alertas si ha cambiado el tiempo
  bool timerActivo = timer.Timer();
  if (timer.TimeHasChanged()) {
    refreshTime(true); // muestra HH:MM:SS en pantalla
    actualizarAlertasYReposo();
  }  
  // Fin de la cuenta atrás: preparar el inicio del riego
  if (!timerActivo) {
    tm = tm_saved;  // restauramos el tiempo de riego original
    boton = botonDefer;  // restauramos el apuntador al boton de la zona o grupo que se habia pulsado
    Estado.botonSemaforo = true;  // set semaforo para que se procese el boton en el siguiente paso del loop
    setParpadeo(tic_LedWhite, PARAR);
    setStateMachine(STANDBY,WAITING);  // pasamos a STANDBY para que se procese el boton y se inicie el riego
    lcd.clear();
  }
}

// Verifica la conexion con SCD (Domoticz) devuelve TRUE si OK, FALSE  sin conexion
bool checkSCD()
{
  if (!Estado.inSetup) {setParpadeo(tic_LedRecon, RAPIDO, parpadeoLedPWM, LEDB); sonido.bip(1);} //parpadeo led recon y bip de aviso
  LOG_INFO("----  VERIFICANDO CONEXION DOMOTICZ  ----");
  bool SCD_OK = getDiaNoche(amanecer, anochecer); //enviamos mandato a Domoticz para comprobar que hay conexion
  setLed(tic_LedRecon, APAGA, LEDB); //paramos parpadeo led recon y lo dejamos apagado
  if (SCD_OK && Estado.estado == ERROR) setStateMachine(STANDBY); // pasa a STANDBY sin mostrar mensajes en LCD para borrar estado previo de error de conexion
  return SCD_OK;
}

//verificamos si el Domoticz esta conectado, solo en este caso reintentamos leer factores de riego
void VerifyRecoverySCD()
{  
  LOG_TRACE("");
  lcd.displayON(); //por si estuviera parpadeando(apagado) por error en pantalla
  checkAndInitFactorRiegos(NOSIGNALERROR);
  setupEstadoFinal();
  if(Estado.recoverableError) LOG_INFO("reintento en ",RECONNECTINTERVAL," minutos \n");
}


void flagVerificaciones() 
{
  flagVtimer = ON; //aqui solo activamos flagVtimer para no usar llamadas a funciones bloqueantes en Ticker
}


/**---------------------------------------------------------------
 * Si se ha cumplido el timer de verificaciones periodicas,
 * activacion del flagV para comprobaciones de estado (wifi, hora correcta, ...) 
 * Además activa los flags checkRecon y checkLogSize si se han cumplido sus respectivos intervalos.
 */
void Verificaciones() 
{   
  #ifdef DEVELOP
    leeSerial();  // para ver si simulamos algun tipo de error
  #endif
  #ifdef DEBUGloops
    debugloops();
  #endif
  
  static unsigned long lastmillisReconnect = 0;
  static unsigned long lastmillisLargo = 0;
  // Reiniciamos flags de verificaciones
  flagV = OFF;
  checkRecon = false;
  // Si no activada por Ticker salimos sin hacer nada mas
  if (!flagVtimer) 
      return;  
  if (Estado.error) LOG_TRACE("-------flagVtimer ON----   Estado.recoverableError: ", Estado.recoverableError, "Estado.error: ", Estado.error);
  flagVtimer = OFF;
  // Activamos flagV para que se realicen las verificaciones en las funciones de estado correspondientes
  flagV = ON;
  // Cada RECONNECTINTERVAL minutos activamos checkRecon para intentar reconectar si no hay conexion wifi o con Domoticz  
  if (millis() - lastmillisReconnect >= RECONNECTINTERVAL * 60000UL) {   
    lastmillisReconnect = millis();
    checkRecon = true;
  }
  // Cada LONGINTERVAL minutos activamos checkLogSize para verificar tamaño log y rotacion si procede
  if (millis() - lastmillisLargo >= LONGINTERVAL * 60000UL) {
      lastmillisLargo = millis();
      checkLogSize = true; 
  }
  // Actualiza el "latido" en la RAM RTC
  lastUptime = millis() / 1000;
  // Si llevamos 1 dia "vivos", limpiamos el contador de rearranques
  if (millis() > 24*60*60*1000UL && bootCount > 0) bootCount = 0; 
  /*
    Con flagV activado, se realizan las siguientes verificaciones periodicas:
      - estado de la wifi y recuperacion de la conexion si no la hay (en procesaEstadoStandby y procesaEstadoError)
      - actualiza y muestra nivel señal wifi si procede (en procesaEstadoStandby y procesaEstadoStop)
      - actualizacion de hora por NTP si no se hubiera hecho ya (en procesaEstadoStandby)
      - actualiza y muestra temperatura ambiente (en procesaEstadoStandby)
      - recordatorio error grave al parar un riego (en procesaEstadoError)
      - si config.verify=true, verifica que el estado de la zona en RIEGO coincide con el de Domoticz (en procesaEstadoRegando)
      - si config.verify=true, verifica que el estado de la zona en PAUSA coincide con el de Domoticz (en procesaEstadoPause)
    Con checkRecon activado, se realizan las siguientes verificaciones periodicas:
       - intento de recuperacion de la conexion wifi si no la hay (en procesaEstadoError)
       - intento de recuperacion de la conexion con Domoticz (en procesaEstadoError)
    Con checkLogSize activado, se realiza la verificacion del tamaño del log y rotacion si procede
       (en procesaEstadoStandby y procesaEstadoError)   
  */
}

// Lee la temperatura ambiente (sensor local o remoto segun config). Devuelve 999 si error de lectura.
// Otras posibles lecturas (no usadas): 
//    float humedad = dht.readHumidity();
//    float temp_sense = dht.computeHeatIndex(false); // false para calculo en grados centigrados
float readTemp() {
    float temperatura = 999;
    if(config.tempRemote>0) {
      temperatura = getRemoteTemperature();
    }
    else {
      #ifdef TEMPLOCAL   // temperatura ambiente del sensor local
        temperatura = dht.readTemperature();
        if(isnan(temperatura)) temperatura = 999;
      #endif
    }
    return temperatura;  
}

/* Calcula (aplicando offset de calibracion y redondeo) y muestra la temperatura ambiente en el display
 * Gestiona el timeout de lectura de temperatura remota y cambio a sensor local si procede */
void showTemp() {
    static float prev_temp = -1000.0; // valor inicial imposible para forzar la primera actualizacion
    static unsigned long lastValidTempMillis = millis();
    float temperatura = 999;
    bool tiempoExcedido = (millis() - lastValidTempMillis > LONGINTERVAL * 60000UL);
    if (tiempoExcedido) {
        if (config.tempRemote>0) {
          config.tempRemote = -1; // si fallo lectura temp remota, pasamos a temp local temporal
          Estado.errorInformado = false; // reseteamos flag para informar si error en temp local
          LOG_WARN("Tiempo excedido sin lectura valida de temperatura, cambiando a sensor local");
        }   
        lastValidTempMillis = millis();
    } 
    temperatura = readTemp();
    LOG_TRACE("temperatura=",temperatura,"prev_temp=",prev_temp,"errinfo=",Estado.errorInformado);
    if(temperatura != 999) {
      temperatura = temperatura + ((float)config.tempOffset*(TEMP_OFFSET_FACTOR/100.0)); // offset correccion
      LOG_TRACE("temp OFFSET=",config.tempOffset,"TEMP_OFFSET_FACTOR %=",TEMP_OFFSET_FACTOR,"temperatura corregida=",temperatura);
      temperatura = (temperatura < 0 ? (temperatura - 0.5) : (temperatura + 0.5)); //redondeo al entero mas cercano
      if (prev_temp == 999) { 
        Estado.errorInformado = false; // si antes habia error de temperatura, reseteamos flag
        logStatusF("Temperature sensor OK (%s)", config.tempRemote>0? "remote" : "local");
      }
      lastValidTempMillis = millis(); // actualizamos tiempo ultima lectura valida
    }  
    else {
      if (!Estado.errorInformado) {
        LOG_WARN("Read", config.tempRemote>0? "remote" : "local", "temperature sensor failed");
        Estado.errorInformado = true; // para no repetir el mensaje hasta que se recupere
      }
    }
    lcd.displayTemp((int)temperatura);
    prev_temp = temperatura;
}

void displayDemo() {
    ledPWM(LEDB,ON);
    lcd.setCursor(0,2);
    lcd.print("(DEMO)"); 
}

// muestra en esquina inferior izquierda del LCD el tipo de multirriego (sin factorizar, temporal o de grupos)
void displayTipoGrupo() {
    lcd.setCursor(0,3);
    if(multi.noFactorizado) lcd.print(" -NF-");
      else if(multi.temporal) lcd.print("*Mtemp");
        else {lcd.print("G");lcd.print(multi.ngrupo);} 
}

void displayEstadoRemoto(const char* estado_texto) {
    lcd.setCursor(strlen(estado_texto)-1, 0); // escribe sobre los ":"" finales
    lcd.print("(R)");
}

/**
 * Devuelve texto asociado a un codigo de error
 */
static const char* errorToString(error_tipos tipoerror)
{
  static const ErrorEntry tablaErrores[] = {
      { E0, "error en parametros" },
      { E1, "sin conexion wifi" },
      { E2, "sin conex. domoticz" },
      { E3, "en factores riego" },
      { E4, "al iniciar riego" },
      { E5, "al parar riego" }
  };
  for (size_t i = 0; i < ELEMENTCOUNT(tablaErrores); i++) {
      if (tablaErrores[i].id == tipoerror) return tablaErrores[i].descripcion;
  }
  return "[unknown error]"; 
}

// Carga parametros de configuracion desde fichero o inicializa a zero-config
void setupParm()
{
  LOG_TRACE("");
  if (!fsOK) {
    LOG_ERROR(" ** [ERROR] Fallo montando LittleFS");
    lcd.infoclear("ERROR en FileSystem",1,BIPKO);
    delay(config.msgdisplaymillis*3);
    return;
  }
  #ifdef DEVELOP
    Serial.printf( "\n initParm= %d \n", initFlags.initParm );
  #endif
  // Vacia directorio /datos si se ha solicitado borrado de este al arrancar
  if( initFlags.initParm) {
    LOG_WARN(">>>>>>>>>>>>>>  borrando ficheros de datos  <<<<<<<<<<<<<<");
    bool bRC = deleteDatos();
    if(bRC) {
      LOG_WARN("borrado ficheros de /datos OK");
      lcd.infoclear("RESET/ERASE parm OK",1,BIPOK); //señala el borrado ficheros de parámetros OK
      delay(config.msgdisplaymillis);
    }  
    else LOG_ERROR(" **  [ERROR] en borrado ficheros de datos");
  }
  // Intenta leer fichero de parametros (principal o de backup si falla el principal)
  if (!loadConfigFromFile(parmFile)) {
    LOG_ERROR(" ** [ERROR] Leyendo fichero parametros " , parmFile);
    config = Config_parm(); //reset estructura config a valores por defecto
    if (loadConfigFromFile(backupParmFile)) {lcd.infoclear("BACKUP parm loaded", BLINKDISPLAY, BIPKO);delay(config.msgdisplaymillis*3);}
    else LOG_ERROR(" ** [ERROR] Leyendo fichero parametros backup ", backupParmFile);
  }
  // Si no se ha podido leer ningun fichero de parametros, inicializa con zero-config
  if (!config.initialized) zeroConfig();
  setLogToFile(); // ya con los parametros cargados, establecemos si los warning se guardan en el log o no segun config
  // una vez cargados parametros, completa campos de config y boton
  setupConfig();
  #ifdef VERBOSE
    if (config.initialized) Serial.print(F("\nParametros cargados, "));
    else Serial.print(F("Parametros zero-config, "));
    printParms();
  #endif
} //fin setupParm


//Completa campos de config y boton
void setupConfig() 
{
  //si en config campo desc de la zona esta vacio se copia el de por defecto de la estructura Boton:
  for(int i=0;i<NUMZONAS;i++) {
    if(strlen(config.zona[i].desc) == 0)
      strlcpy(config.zona[i].desc, getBotonPointer(Zonas[i])->desc, sizeof(config.zona[i].desc));
  }
  // si en config campo desc del grupo esta vacio se copia el de por defecto de la estructura Boton:
  for(int i=0; i<NUMGRUPOS; i++) {
    if(strlen(config.group[i].desc) == 0)
      strlcpy(config.group[i].desc, getBotonPointer(Grupos[i])->desc, sizeof(config.group[i].desc));
  }
  //por si la ip del SCD Domoticz incluyera user y pass, los extraemos y los guardamos en los campos correspondientes de config
  // if (config.SCD_user[0] == '\0') parseSCDuri(config.SCD_ip);
  if (config.SCD_ip[0] == '\0') LOG_WARN("SCD IP vacia, no se podra conectar a Domoticz");
  //inicializamos factores de riego a valor 100 por defecto
  for(uint i=0;i<NUMZONAS;i++) {
    factorRiegos[i]=100;
  }
  #ifdef MUTESOUND
    config.mute = true;   // arranque con sonidos silenciados
  #endif
  tm.major = config.minutes;
  tm.minor = config.seconds;
  LOG_TRACE("Inicializando clase Configure");
  configure = new Configure();
} //fin setupConfig


void resetESP32() {
    sonido.lowbip(1);
    #ifndef NODISPLAY
    lcd.infoclear(">>  REINICIANDO  <<", 3);
    delay(config.msgdisplaymillis);
    #endif
    ESP.restart();  // reset ESP32
}


/* On the computer side, everytime you want to start the debugging mode, 
simply send a byte over the serial connection during the setup phase and sit back.*/
bool serialDetect() {
  //Wait for four seconds or till data is available on serial, whichever occurs first.
  while(Serial.available()==0 && millis()<4000);
  //On timeout or availability of data, we come here.
  if(Serial.available()>0)
  {
    //If data is available, we enter here.
    Serial.print("\n \t SERIAL available"); //Give feedback indicating mode
    return true;
  }
  return false;
}    

// Devuelve timestamp actual en formato "YYYY-MM-DD HH:MM:SS" si hay hora NTP
// o bien "MS: xxxxxxx" con milisegundos desde arranque si no la hay
const char* getTimestamp() {
    static char buffer[25];
    if (timeOK) {
        struct tm *timeinfo;
        time_t t = time(NULL); // time() devuelve el tiempo UTC actual (epoch time en segundos desde 00:00 1/1/1970) leyendolo del reloj del ESP32
        timeinfo = localtime(&t); // localtime() convierte time_t a struct tm en la zona horaria local
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return buffer;
    }
    // Fallback: Si no hay hora NTP, devolvemos milisegundos
    unsigned long ms = millis();
    strcpy(buffer, "MS: ");
    ultoa(ms, buffer + 4, 10);        
    return buffer;
}    

// Gestion del fichero de log de errores: rotación y limpieza
void gestionarTamanoLog() {
  #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
  static const size_t totalFS = LittleFS.totalBytes();
  size_t freeSpace = totalFS - LittleFS.usedBytes();
  static const size_t maxLogRaw = (totalFS * 4) / 100; // maximo tamaño del log en bytes antes de rotar (4% del total FS)
  static const size_t limiteBloque4K = (maxLogRaw / 4096) * 4096; // redondeo por defecto al limite  múltiplo de 4096 bytes
  static const size_t MAXLOGFILESIZE = limiteBloque4K - 256; // aseguramos espacio para el msg de rotacion
  static const size_t MINFSSPACE = (totalFS * 8) / 100; // espacio libre minimo en LittleFS en bytes (8% del total FS)
  LOG_DEBUG("Espacio libre FS:", freeSpace, "bytes");
    // 1. Limpieza por espacio crítico: borramos log_prev si el espacio libre es inferior al 8%
    if (freeSpace < MINFSSPACE) {
        if (LittleFS.exists(logErrorFilePrev)) {
            LittleFS.remove(logErrorFilePrev);
            logStatus("--- LOG_PREV ELIMINADO PARA LIBERAR ESPACIO (Limite <8% alcanzado) ---");
            LOG_INFO("Espacio libre (%u bytes). log_prev eliminado.\n", freeSpace);
        }
    }
    // 2. Rotación por tamaño: rotamos el log actual si supera el tamaño máximo definido (4% del total FS)
    if (LittleFS.exists(logErrorFile)) {
        File f = LittleFS.open(logErrorFile, "r");
        if (f) {
            size_t currentSize = f.size();
            f.close();
            if (currentSize > MAXLOGFILESIZE) {
                logStatus("--- Fin de este segmento (rotando) ---");
                delay(100); // asegurar que el mensaje se graba (flush/close) antes de renombrar
                if (LittleFS.exists(logErrorFilePrev)) LittleFS.remove(logErrorFilePrev);
                LOG_FILE_CLOSE(); // cerrar log antes de renombrar
                if (LittleFS.rename(logErrorFile, logErrorFilePrev)) {
                    LOG_ATTACH_FS_AUTO(LittleFS, logErrorFile, FILE_APPEND); // Reabre el log
                    logStatus(" --- Log Rotated: Previous file saved as _prev ---"); // record log rotation message to logfile
                } else {
                    LOG_ATTACH_FS_AUTO(LittleFS, logErrorFile, FILE_APPEND); // Reabre el log
                    LOG_ERROR(" ** [ERROR] Renaming log file for rotation failed");
                }
            }    
        }    
    }
    #endif
}

// Configura el nivel de grabacion en fichero segun parametro config.logWarnToFile
void setLogToFile() {
    #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
    if (config.logWarnToFile) setWarnToFile(true); 
    else setWarnToFile(false);
    #endif
}

// Configura el nivel de grabacion en fichero de msg WARNING segun true/false del parametro pasado
void setWarnToFile(bool activar) {
    #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
    if (activar) {
        LOG_FILE_SET_LEVEL(DebugLogLevel::LVL_WARN);
        LOG_WARN("LOG_WARN messages will also be logged to file as per configuration");
    } else {
        LOG_FILE_SET_LEVEL(DebugLogLevel::LVL_ERROR);
        logStatus("Only error type messages will be logged to file");
    }
    #endif
}

// Fuerza el refresco del fichero de log
void refreshLogFile() {
      #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
      // logStatus("-----------  log  refresh  ----------");
      LOG_FILE_CLOSE(); // Fuerza el volcado y cierre del log
      LOG_ATTACH_FS_AUTO(LittleFS, logErrorFile, FILE_APPEND); // Reabre el log
      #endif

}

// Formatea y graba un mensaje formateado en el fichero log
void logStatusF(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    logStatus(buffer); 
}

// Graba un mensaje en el puerto serie y en fichero log si procede
void logStatus(const char* mensaje) {
  LOG_INFO("[SYSTEM]", mensaje);
  #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
    PRINTLN_FILE("[SYSTEM] [", getTimestamp(), "]", mensaje);
    // en setup, abre y cierra para grabar fecha correcta de lastwrite (borrada en el primer open sin ntp)
    if (Estado.inSetup && timeOK) { 
        LOG_FILE_CLOSE();
        LOG_ATTACH_FS_AUTO(LittleFS, logErrorFile, FILE_APPEND);
    }
  #endif
}

// Gestiona el registro de arranques y deteccion de bootloop
// Devuelve un String con el mensaje de arranque o "BOOTLOOP" si se detecta bootloop
String registrarArranqueSistema() {
    uint32_t uptimePrevio;
    char msgArranque[160];
    // Verificar si el sistema ha despertado de un deep sleep por boton manual
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
      // Reseteamos la variable de la RTC RAM (tenga un bootCount valido -lightsleep- o erroneo -deepsleep-)
      bootCount = 0; 
      return String("Despertado manualmente. Limpiando contador de errores...");
    }
    // Si el número mágico no coincide, es un arranque en frío (power-on o hard reset que borra la RAM RTC)
    if (magicNumber != 0xCAFEBABE) {
        magicNumber = 0xCAFEBABE;
        bootCount = 1;
        uptimePrevio = 0; // arranque por hard reset, no hay uptime previo 
    } else {
        bootCount++;
        uptimePrevio = lastUptime; // Guardamos el uptime del ciclo anterior para el mensaje
    }
    // Detecta bootloop: más de 5 arranques por SW con uptime previo menor de 30 segundos
    if (bootCount > 5 && uptimePrevio < 30) {
        return String("BOOTLOOP"); 
    }
    esp_reset_reason_t reason = esp_reset_reason();
    const char* razonTexto;
    switch (reason) {
        case ESP_RST_POWERON:  razonTexto = "Power-on / Hard Reset"; break;
        case ESP_RST_EXT:      razonTexto = "External Pin Reset"; break;
        case ESP_RST_SW:       razonTexto = "Software Reset (ESP.restart)"; break;
        case ESP_RST_PANIC:    razonTexto = "Exception / Crash"; break;
        case ESP_RST_INT_WDT:  razonTexto = "Interrupt Watchdog"; break;
        case ESP_RST_TASK_WDT: razonTexto = "Task Watchdog"; break;
        case ESP_RST_BROWNOUT: razonTexto = "Voltage Dip (Brownout)"; break;
        default:               razonTexto = "Other / Unknown"; break;
    }
    lastUptime = 0; // Reseteamos para el ciclo actual
    snprintf(msgArranque, sizeof(msgArranque), 
             "\t\t\t <<<<<<    CCR STARTED    >>>>>>       (Boot #%d | Reason: %s (%d) | Last Uptime: %lu s)", 
             bootCount, razonTexto, reason, uptimePrevio);    
    return String(msgArranque);
}

// Inicializa el sistema de ficheros LittleFS y el logger a fichero si procede
void initFS() {
  if (fsOK)  // ya inicializado
      return;
  PRINTLN("[setup] Inicializando LittleFS...");
  if(clean_FS) cleanFS();
  fsOK = LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED);
  if(!fsOK) {
    PRINTLN("[ERROR] [initFS] An Error has occurred while mounting LittleFS");
    return;
  }
  // Verificamos si existe el directorio /datos para crearlo en caso contrario
  bool dirCreated = false;
  if (!LittleFS.exists("/datos")) {
      PRINTLN("[initFS] El directorio /datos no existe. Creándolo...");
      if (!LittleFS.mkdir("/datos")) PRINTLN("[ERROR] No se pudo crear el directorio /datos");
      else {
         PRINTLN("[OK] Directorio /datos creado correctamente");
         dirCreated = true;
      }   
  }  
  #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
    LOG_ATTACH_FS_AUTO(LittleFS, logErrorFile, FILE_APPEND); // open/close automatico, se añaden mensajes al final del fichero
    LOG_FILE_SET_LEVEL(DebugLogLevel::LVL_WARN); // hasta leer config para ajustar a LVL_ERROR si config.logWarnToFile es false
    gestionarTamanoLog(); // Borra/rota fichero de log de errores si su tamano es excesivo
    if (dirCreated) logStatus("[WARN] --- Directorio /datos creado ---");
  #endif
}

// Detiene el sistema en modo deep sleep, despierta al pulsar el boton del ENC.
void stopHW(const char* mensaje) {
    initFS();
    if (mensaje != nullptr) LOG_ERROR(mensaje);
    if (!Estado.inSetup) {
      lcd.infoclear("PARANDO SISTEMA", 1, BIPKO);
      lcd.info(" - BUG de SW o HW -", 2);
      lcd.info("Pulsa ENC para", 3);
      lcd.info("despertar", 4);
      delay(config.msgdisplaymillis);
    }
    esp_sleep_enable_ext0_wakeup(ENCBOTON, 0); // Configura wakeup por boton ENC a LOW
    esp_deep_sleep_start();  // entra en deep sleep indefinidamente
}

// **************************************************************************
// Atajos Stop+Enc+Grupo_n
// **************************************************************************

// Shortcut Webserver: activa servidor web
void scWebserver() {
  #ifdef WEBSERVER
    if(Estado.connected) {
      setEstado(CONFIGURANDO);
      setupWS();
    }  
    else BIPKO; //no es posible
  #endif  
}

// Shortcut WifiLevel: togle display del nivel de señal wifi
void scWifiLevel() {
    config.showwifilevel = !config.showwifilevel;
    showWifiLevel(checkWifi(config.showwifilevel));
}

// Shortcut Eastern Egg
void scSorpresa() {
    if (getDiaNoche(amanecer, anochecer)) {
        lcd.infoclear("Hoy amanece a las..", 1);
        lcd.setCursor(7, 1); lcd.print(amanecer);
        lcd.info("..y anochece a las", 3);
        lcd.setCursor(7, 3); lcd.print(anochecer);
    } else lcd.infoclear("    EASTER EGG!", 2);
    enciendeLeds();
    bool premio =false;
    int probabilidad = esp_random() % 100; 
    LOG_DEBUG("Probabilidad premio (<15): ", probabilidad); 
    if (probabilidad < 15) {  // 15% probabilidad PREMIO
      lcd.infoclear("    !!!PREMIO!!!", 2);
      lcd.info("  SUPER MARIO BROS", 3);
      premio = true;
      sonido.bipMario(premio);
    }    
    // 85% de probabilidad tema individual o bien segundo tema con luces
    int temaAleatorio = esp_random() % 4;
    LOG_DEBUG("Tema aleatorio elegido (0 a 3): ", temaAleatorio); 
    switch(temaAleatorio) {
        case 0: sonido.temaPiratas(premio); break;
        case 1: sonido.temaStarWars(premio); break;
        case 2: sonido.temaIndianaJones(premio); break;
        case 3: sonido.temaHarry2(premio); break;
    }
    delay(config.msgdisplaymillis);
    apagaLeds();
    setEstado(STOP);
}

void setDiferido() {
    // si estamos en Standby, pasamos a set del tiempo diferido y mostramos mensaje en LCD
    if (Estado.estado == STANDBY) {
        tm_saved = tm; // guardamos tiempo de riego actual
        tm.major = 0;
        tm.minor = 30; // inicializamos a 30 minutos
        setEstado(DIFERIDO, 3, SETDEFER);
    };
}


// **************************************************************************
// funciones solo usadas en DEVELOP
// (es igual, el compilador no las incluye si no son llamadas)
// **************************************************************************
#ifdef DEVELOP

  /**---------------------------------------------------------------
   * lectura del puerto serie para debug
   */
  void leeSerial() 
  {
    if (Serial.available() > 0) {
      // lee cadena de entrada
      String inputSerial = Serial.readString();
      int inputNumber = inputSerial.toInt();
      if (!inputNumber) {
          Serial.println(F("Teclee: "));
          Serial.println(F("   0 o intro - anular simulacion errores"));
          Serial.println(F("   1 - simular error NTP"));
          Serial.println(F("   2 - simular error apagar riego"));
          Serial.println(F("   3 - simular error encender riego"));
          Serial.println(F("   4 - simular EV no esta ON en Domoticz"));
          Serial.println(F("   5 - simular EV no esta OFF en Domoticz"));
          Serial.println(F("   6 - simular error al salir del PAUSE"));
          Serial.println(F("   7 - simular fecha erronea de NTP"));
          Serial.println(F("   9 - simular crash de sw"));
      }
      switch (inputNumber) {
            case 1:
                Serial.println(F("recibido:   1 - simular error NTP"));
                timeOK = false;
                break;
            case 2:
                Serial.println(F("recibido:   2 - simular error apagar riego"));
                simular.ErrorOFF = true;
                break;
            case 3:
                Serial.println(F("recibido:   3 - simular error encender riego"));
                simular.ErrorON = true;
                break;
            case 4:
                Serial.println(F("recibido:   4 - simular EV no esta ON en Domoticz"));
                simular.ErrorVerifyON = true;
                break;
            case 5:
                Serial.println(F("recibido:   5 - simular EV no esta OFF en Domoticz"));
                simular.ErrorVerifyOFF = true;
                break;
            case 6:
                Serial.println(F("recibido:   6 - simular error al salir del PAUSE"));
                simular.ErrorPause = true;
                break;
            case 7:
                Serial.println(F("recibido:   7 - simular fecha erronea de NTP"));
                tv.tv_sec = 0; // fecha 1/1/1970
                settimeofday(&tv, NULL);
                timeOK = true;
                break;
            case 9:
                Serial.println(F("recibido:   9 - simular crash de sw"));
                *((int*)0) = 42; // access violation para simular crash
                break;
            case 0:
                Serial.println(F("recibido:   0 - anular simulacion errores"));
                timeOK = true;                         
                simular.all_simFlags = false;
      }
    }
  }

  void debugloops()
  {
    static unsigned long currentMillisLoop = 0;
    static unsigned long lastMillisLoop = 0;
    if (numloops < 10000) {
      ++numloops;
      currentMillisLoop = millis();
    }
    else {
      currentMillisLoop = currentMillisLoop - lastMillisLoop;
      Serial.printf( "[CRONO] %d loops en milisegundos: %d \n" , numloops, currentMillisLoop);
      numloops = 0;
      lastMillisLoop = millis();
    }
  }

#endif
