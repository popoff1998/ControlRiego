#define __MAIN__
#include "Control.h"

/* 
 0   ESP_RST_UNKNOWN,    //!< Reset reason can not be determined
 1   ESP_RST_POWERON,    //!< Reset due to power-on event
 2   ESP_RST_EXT,        //!< Reset by external pin (not applicable for ESP32)
 3   ESP_RST_SW,         //!< Software reset via esp_restart
 4   ESP_RST_PANIC,      //!< Software reset due to exception/panic
 5   ESP_RST_INT_WDT,    //!< Reset (software or hardware) due to interrupt watchdog
 6   ESP_RST_TASK_WDT,   //!< Reset due to task watchdog
 7   ESP_RST_WDT,        //!< Reset due to other watchdogs
 8   ESP_RST_DEEPSLEEP,  //!< Reset after exiting deep sleep mode
 9   ESP_RST_BROWNOUT,   //!< Brownout reset (software or hardware)
 10  ESP_RST_SDIO,       //!< Reset over SDIO

 */

// interrupt para el encoder:
void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}


/*----------------------------------------------*
 *               Setup inicial                  *
 *----------------------------------------------*/
void setup()
{
  #ifdef DEMO
                modoDEMO=true;
  #endif
  #ifdef NOWIFI
                modoDEMO=true;
                noWIFI=true;
  #endif

  Serial.begin(115200);
  #ifdef RELEASE
      // if (!serialDetect()) LOG_SET_LEVEL(DebugLogLevel::LVL_NONE); 
      if (!serialDetect()) LOG_SET_LEVEL(DebugLogLevel::LVL_ERROR); 
  #endif
  delay(500);

  PRINTLN("\n\n CONTROL RIEGO V" + String(VERSION) + "    Built on " __DATE__ " at " __TIME__  "\n");
  #ifndef DEBUGLOG_DISABLE_LOG
    PRINTLN("\n (current log level is", (int)LOG_GET_LEVEL(), ")");
  #endif
  LOG_DEBUG("Startup reason: ", esp_reset_reason());
  LOG_TRACE("TRACE: in setup");
  // init de los GPIOs y bus I2C
  initGPIOs();
  initWire();
  //led encendido/error
  ledPWM(LEDR,ON);
  LOG_TRACE("Inicializando MCPs");
  mcpOinit();
  mcpIinit();
  LOG_TRACE("Inicializando display");
  lcd.initLCD();
  LOG_TRACE("Inicializando Encoder");
  initEncoder();
  LOG_TRACE("Inicializando Configure");
  configure = new Configure(config);   // se pasa por referencia la estructura config al constructor de la clase
  //preparo indicadores de inicializaciones opcionales
  setupInit();
  //setup parametros configuracion
  setupParm();
  #ifdef EXTRADEBUG
   printFile(parmFile);
  #endif
  //Recuperamos lastRiegos y lastGrupos (registro fecha/hora y riego realizado)
  initLastRiegos();
  initLastGrupos();
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  delay(1000);
  setupRedWM(config, initFlags);
  if (saveConfig) {
    if (saveConfigFile(parmFile, config))  sonido.bipOK();
    else sonido.bipKO();
    saveConfig = false;
  }
  // delay(1000);
  //Obtenemos hora del servidor ntp y ajustamos hora del sistema y timezone
  setClock();
  //Cargamos factorRiegos
  initFactorRiegos();
  //Estado final en funcion de la conexion
  setupEstado();
  if(Estado.estado==STANDBY) sonido.bipOK();
  //Llamo a parseInputs CLEAR para eliminar prepulsaciones antes del bucle loop
  parseInputs(CLEAR);
  //lanzamos supervision periodica estado cada VERIFY_INTERVAL seg.
  tic_verificaciones.attach(VERIFY_INTERVAL, flagVerificaciones);
  standbyTime = millis();
  PRINTLN("   *** Setup finalizado *** \n\n");
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
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  //NOTA: el encoderSW esta en estado HIGH en reposo y en estado LOW cuando esta pulsado
  encoderSW = !digitalRead(ENCBOTON);
  if (!validaBoton()) return;
  //Procesamos el boton pulsado:
  switch (boton->bID) {
    //Primero procesamos los botones singulares, el resto van por default
    case bPAUSE:
      procesaBotonPause();
      break;
    case bSTOP:
      procesaBotonStop();
      break;
    case MULTIRRIEGO:
      procesaBotonMultiriego(); 
      break;
    default:
      procesaBotonZona();
  }
}

bool validaBoton() {
  //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multirriego
  if (multi.semaforo) multi.semaforo = false;  // si multisemaforo, no leemos botones: ya los pasa multirriego
  else  boton = parseInputs(READ);  // si no, vemos si algun boton ha cambiado de estado
  // si no se ha pulsado ningun boton salimos
  if(boton == NULL) return false;
  //Si estamos en reposo pulsar cualquier boton solo nos saca de ese estado (salvo STOP que si actua y se procesa)
  if (reposo && boton->bID != bSTOP) {
    reposoOFF();
    return false;
  }
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
    case STOP:          procesaEstadoStop(); break;
    case PAUSE:         procesaEstadoPause(); if(Estado.estado == PAUSE) blinkDisplay(); break;
  }
}  


/**---------------------------------------------------------------
 * Estado final al acabar Setup en funcion de la conexion a la red
 */
void setupEstado() 
{
  LOG_DEBUG("setupEstado entrada, Estado.error=", Estado.error, "recoverableError=", recoverableError, "modoDEMO=", modoDEMO);
  
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = true; //Deshabilitamos el hold de Pause
   
  if(!config.initialized) {  //no se ha podido cargar parámetros desde ficheros -> señalamos el error
    statusError(E0);  
    return;
  }

  // Si estamos en modoDEMO pasamos a STANDBY (o STOP si esta pulsado) aunque no exista conexión wifi o estemos en ERROR
  if (modoDEMO) {
    if (testButton(bSTOP,ON))  setEstado(STOP,1);
    else setEstado(STANDBY,2);
    LOG_DEBUG("setupEstado salida por modoDEMO=", modoDEMO);
    return;
  }
  
  if (Estado.estado == ERROR) {  // Si estado actual es ERROR seguimos así
    LOG_DEBUG("setupEstado salida por estado ERROR (", errorText, ") recoverableError=", recoverableError, "modoDEMO=", modoDEMO);
    return;
  }
  
  if (connected) {  //si estamos conectados a la red pasamos a STANDBY (o STOP si esta pulsado)
      if (testButton(bSTOP,ON))  setEstado(STOP,1);
      else setEstado(STANDBY,1);
  } else {  //si no estamos conectados a la red pasamos a estado ERROR
    statusError(E1, RECUPERABLE); //error de conexion wifi recuperable
    LOG_DEBUG("setupEstado salida por estado ERROR(E1)"); 
  }
}  //fin de setupEstado

#ifdef GRP4
  /**---------------------------------------------------------------
   * Verificamos si STOP y encoderSW esta pulsado (estado OFF) en el arranque,
   * en ese caso se muestra pantalla de opciones.
   * Pulsando entonces:
   *    - boton Grupo1 --> borramos ficheros de parametros : reset a valores por defecto
   *    - boton Grupo3 --> borramos red wifi almacenada en el ESP32
   *    - liberando boton de STOP  --> salimos sin hacer nada y continua la inicializacion
   */
  void setupInit(void) {
    #ifdef TEMPLOCAL
      dht.begin();
    #endif

    if (!digitalRead(ENCBOTON) && testButton(bSTOP,ON)) {
      LOG_TRACE("en opciones setupInit");
      lcd.infoclear("       Pulse:");
      lcd.info("grupo1 >RESET parm",2);
      lcd.info("grupo3 >erase WIFI",3);
      lcd.info("EXIT -> release STOP",4);
      while (1) {
        boton = parseInputs(READ);
        if(boton == NULL) continue;
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
  void setupInit(void) {
    LOG_TRACE("");
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
  
  void procesaBotonPause(void)
  {
    if(!boton->estado && Estado.estado != STOP) return; //No procesamos los release del boton salvo en STOP
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
  LOG_INFO("encoderSW+PAUSE terminamos riego de zona en curso");
  // si estamos en un multirriego y no es la ultima zona y todavia no hemos salvado el riego actual
  // --> salvamos el riego en curso en riegoSaved para poder continuarlo despues del multirriego
  if (multi.riegoON && (multi.actual < *multi.size) && !riegoSaved.zonevalid) {
    saveRiego(ultimoBotonZona->znumber, ultimoBotonZona->bID, T.ShowMinutes(), T.ShowSeconds());
    LOG_INFO("salvando riego de zona en curso en riegoSaved");
  }
}

// Pausa el riego en curso
void handlePauseInRegando() {
  setEstado(PAUSE,1);
  tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
  led(ultimoBotonZona->led,ON);// y lo deja fijo
  if (stopRiego(ultimoBotonZona->bID)) T.PauseTimer();
  else { //error al parar riego
    LOG_WARN("error al pausar riego de zona en curso errorText :",errorText,"zona :",ultimoBotonZona->desc );
  }  
}  

// Si pulsamos junto con encoderSW terminamos el riego (pasaria al siguiente en caso de multirriego)
void handleEncPauseInPause() {
  riegoFromPause = true; // para que no actualize tiempo final riego en procesaEstadoTerminando
  handleEncPauseInRegando();
}

// Reanudamos riego que estaba parado
void handlePauseInPause() {
  if(simular.ErrorPause) statusError(E2); //simulamos error al salir del PAUSE
  else initRiego(RESUME);         
  if(Estado.estado == ERROR) { // caso de error al reanudar el riego seguimos en PAUSE y señalamos con blink rapido zona
    ledID = ultimoBotonZona->led;
    tic_parpadeoLedZona.attach(RAPIDO, parpadeoLedZona, ledID);
    LOG_WARN("error al salir de PAUSE errorText :",errorText,"Estado.error :",Estado.error );
    lcd.displayON();
    delay(MSGDISPLAYMILLIS);
    lcd.clear(BORRA2H); //borra msgs de error
    refreshTime();
    setEstado(PAUSE,1);
  } else {
    sonido.bip(2);
    lcd.clear(BORRA2H); //por si hubiera msgs de error (caso error al salir del pause previo)
    T.ResumeTimer(); // reanudamos el timer de cuenta atras
    refreshTime(); //actualizamos tiempo de cuenta atras en pantalla
    tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
    led(ultimoBotonZona->led,ON);// y lo deja fijo
    setEstado(REGANDO);
  }
}  

// Si encoderSW+Pause --> conmutamos estado modoDEMO
void handleEncPauseInStandby() {
    if (modoDEMO) {
      modoDEMO = false;
      noWIFI = false;
      LOG_INFO("encoderSW+PAUSE pasamos a modo NORMAL y leemos factor riegos");
      sonido.bip(2);
      lcd.infoclear("Saliendo de DEMO");
      if (!checkWifi()) wifiReconnect();
      if (connected) { 
        initFactorRiegos();
        if(VERIFY && Estado.estado != ERROR) {
          lcd.info("..y parando riegos",2);
          stopAllRiego(); //verificamos operativa OFF para los IDX's
        }    
        ledPWM(LEDB,OFF);
      }    
      setupEstado();
      //recuperamos tablas de ultimos riegos reales
      initLastRiegos();
      initLastGrupos();
    }
    else {
      modoDEMO = true;
      LOG_INFO("encoderSW+PAUSE pasamos a modoDEMO (DEMO)");
      sonido.bip(2);
      ledPWM(LEDB,ON);
      displayDemo();
    }
}
      
// Muestra hora y ultimos riegos
void handlePauseInStandby() {
    ultimosRiegos(SHOW);
    delay(config.msgdisplaymillis*3);
    ultimosRiegos(HIDE);
    LOG_TRACE("[poniendo estado STANDBY]");
    setEstado(STANDBY);  // para restaurar pantalla
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
      configure->menu();
    }
}

void handlePauseInError() {
    LOG_INFO("estado en ERROR y PAUSA pulsada pasamos a modoDEMO y reset del error");
    modoDEMO = true;
    sonido.bip(2);
    resetFlags();   //reset flags de status
    resetLeds();    //reset leds
    if (Boton[bID2bIndex(bSTOP)].estado) setEstado(STOP,1);
    else setEstado(STANDBY);
}

// Detecta si se mantiene pulsado el boton PAUSE
bool handleHoldPause() {
    bool RC = false;
    if(boton->estado) {
      if(!holdPause) {
        countHoldPause = millis();
        holdPause = true;
      }
      else {
        if((millis() - countHoldPause) > HOLDTIME) RC = true;
      }
    }
    //Si lo hemos soltado quitamos holdPause
    else holdPause = false;
    return RC;
}

void procesaBotonStop(void)
{
  if (boton->estado) {  //si hemos PULSADO STOP
    if (Estado.estado == REGANDO || Estado.estado == PAUSE || Estado.estado == TERMINANDO) {
      handleStopInRegandoPauseTerm();           //parar riegos y pasar a estado STOP
      return;
    }
    if (Estado.estado == STANDBY) { 
      if (encoderSW) handleEncStopInStandby();  // activa configuracion de grupo multirriego temporal
      else handleStopInStandby();               // seguro antinenes
      return;
    }
    if (Estado.estado == ERROR) {
      handleStopInError();                      // resetea el ESP32
      return;
    }
  }
  //si hemos liberado STOP: salimos del estado stop
  //(dejamos el release del STOP en modo ConF para que actue el codigo de procesaEstadoConfigurando
  if (!boton->estado && Estado.estado == STOP) {
    LOG_TRACE("[poniendo estado STANDBY]");
    setEstado(STANDBY);
  }
} //fin de procesaBotonStop

void handleStopInRegandoPauseTerm() {
    lcd.infoclear("Parando riegos", 1, BIP, 6);
    T.StopTimer();
    tic_CountDownTimer.detach(); //detiene actualizacion periodica del temporizador
    // paramos riego en curso y todas las zonas
    bool updateTimeFin = (Estado.estado == PAUSE? false : true); // si estamos en PAUSE no actualizamos tiempo fin
    if (!stopRiego(ultimoBotonZona->bID, updateTimeFin) || !stopAllRiego()) {   //error al parar riegos
      return; 
    }
    saveTablaToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS);  //guardamos en fichero tabla de ultimos riegos de zonas
    lcd.infoclear("STOP riegos OK", DEFAULTBLINK, BIP, 0);
    resetFlags();
    setEstado(STOP,1);
}

void handleStopInStandby() {
    reposoOFF();
    lcd.infoclear("Parando riegos", 1, BIP, 6);
    if (!stopAllRiego()) {   //error al parar riegos
      return; 
    }
    lcd.infoclear("STOP riegos OK", 1, BIP, 0);
    setEstado(STOP,1);
    reposoON(LCDON); //pasamos directamente a reposo sin apagar pantalla
}

void handleEncStopInStandby() {
    setMultibyId(0, config);  // apunta estructura multi a grupo temporal en config (n+1) con id = 0
    setEstado(CONFIGURANDO,1);
    configure->MultiTemp_process_start();
}

void handleStopInError() {
    LOG_WARN("ERROR + STOP --> Reset.....");
    resetESP32();
}


void procesaBotonMultiriego(void)
{
  if (multi.riegoON) return; //ya hay un multirriego en curso,, ignoramos boton
  int n_grupo = setGrupo(config); //apunta estructura multi al grupo seleccionado
  if (n_grupo == 0) return; //error en setup de apuntadores
  LOG_DEBUG("en MULTIRRIEGO, encoderSW status  :", encoderSW );
  if (Estado.estado == STANDBY) {
    if (encoderSW) handleEncGrupoInStandby(n_grupo);  //muestra info del grupo
    else handleGrupoInStandby(n_grupo);               //inicia el multirriego
  }
  // En STOP si pulsamos junto con encoderSW tenemos atajos de teclas (si habilitados en config.shortcuts)
  else if (encoderSW && Estado.estado == STOP && config.shortcuts) handleEncGrupoInStop(n_grupo);  
} //fin de procesaBotonMultiriego

// Hacemos encendido de los leds del grupo y mostramos en el display info de este
void handleEncGrupoInStandby(int n_grupo) {
    LOG_DEBUG("en MULTIRRIEGO + encoderSW, display de grupo:", multi.desc,"tamaño:", *multi.size );
    snprintf(buff, MAXBUFF, "grupo: %s", multi.desc);
    lcd.infoclear(buff, 1);
    displayLCDGrupo(FULL, 2);
    showTimeLastRiego(lastGrupos[n_grupo-1], n_grupo-1, GRUPO);
    displayGrupo(multi.serie, *multi.size);
    delay(config.msgdisplaymillis*3);
    setEstado(STANDBY);   //para que restaure pantalla
}

// Iniciamos el MULTIRRIEGO
void handleGrupoInStandby(int n_grupo) {
    /* Iniciamos el primer riego del MULTIRRIEGO machacando la variable boton
       realmente estoy simulando la pulsacion del primer boton de riego de la serie
       tambien grabamos el tiempo de inicio del riego de grupo  */
    char grupoText[7];
    snprintf(grupoText, sizeof(grupoText), "GRUPO%d", n_grupo);
    if(setMultirriego(config)) inicioTimeLastRiego(lastGrupos[n_grupo-1], grupoText, INICIO);
}

// Atajos combinacion STOP+ENC+GRUPOn
void handleEncGrupoInStop(int n_grupo) {
  switch (n_grupo) {
      case 1:                     //activa Webserver
          scWebserver();
          break;
      case 4:                     //easter egg
          scSorpresa();
          break;
  }    
}

void procesaBotonZona(void)
{
  int zIndex = boton->znumber-1;
  if (zIndex < 0) return; //el boton no es de ZONA o error en la matriz Boton[]
  if (Estado.estado == STANDBY) {
    if (!encoderSW || multi.riegoON) {  // (1)
        startZoneWatering();    //iniciamos el riego correspondiente al boton seleccionado
    }
    else {  
        showInfoZona(zIndex);   // mostramos en el display info zona
    }
    return;
  }
  /* Si config.dynamic=true se permite añadir/eliminar zonas durante un riego individual o multirriego 
     temporal (no durante un multirriego de grupo normal). Para ello el riego debe estar en PAUSE  */
  if ((Estado.estado==PAUSE) && config.dynamic && (multi.riegoON == multi.temporal)) {
    handleDynamicZoneChange();
  }
  /* (1) la comprobacion de multi.riegoON es necesaria para evitar que al cancelar el riego de una zona en multirriego
  salte a mostrar info de la siguiente al detectar el enc pulsado  */
  } //fin de procesaBotonZona


/*
 La mecanica general de la maquina de estados en modo normal es que primero se procesa el boton pulsado, 
 pudiendo este cambiar el estado, y despues se procesa el estado.
 En modo configuracion es totalmente opuesto: los botones se procesan en procesaEstadoConfigurando.
 Esto se hace así para tener separada la lógica de modo normal de la de configuración, ya que las acciones 
 que realizan los botones en una y otra son totalmente distintas.
*/
void procesaEstadoConfigurando()
{
  if (boton != NULL) {
    if (boton->flags.action) {
      if (boton->bID != bSTOP && webServerAct) return; //si webserver esta activo solo procesamos boton STOP

      switch(boton->bID) {
        case bPAUSE:
            if(!boton->estado) break; //no se procesa el release del PAUSE
            if(configure->statusMenu()) {       //si estamos en el menu:
              configure->procesaSelectMenu();   // procesamos la seleccion
              LOG_DEBUG("[MENU] PAUSE pulsado recibido");
              break;
            }
            LOG_DEBUG("PAUSE pulsado recibido y estamos configurando algo");
            handleParameterConsolidation(); // si ya estamos configurando algo: PAUSE consolida lo configurado en config
            break;
        case bSTOP:
            if(!boton->estado) {    //release STOP
              if(configure->configuringMultiTemp()) handleStartMultiTemp(); //si configurando multirriego temporal: STOP lanza el riego
              VERIFY = config.verify;
              configure->exit();  // salvamos parametros a fichero si procede y salimos de ConF
            }
            break;
        case MULTIRRIEGO:
            if (configure->statusMenu() && configure->get_currentItem()==0) { //si no estamos configurando nada:
              handleGroupConfig();                                            // configuramos el grupo seleccionado
            }  
            break;
        default:  //procesamos boton de ZONAx
            if (configure->statusMenu() && configure->get_currentItem()==0) {   //si no estamos configurando nada :
              configure->Idx_process_start(bID2bIndex(boton->bID));             // configuramos el idx del boton
            }
            if (configure->configuringMulti() || configure->configuringMultiTemp()) { //si estamos configurando grupo multirriego:
              configure->Multi_process_update();                             //añadimos zona al multirriego que estamos definiendo
            }
      }
    }
  } else webServerAct ? procesaWebServer() : procesaEncoderConfig();
}; //fin de procesaEstadoConfigurando


void handleGroupConfig()
{
      int n_grupo = setGrupo(config); //apunta estructura multi al grupo seleccionado
      if (n_grupo == 0) return; //error en setup de apuntadores 
      //Configuramos el grupo de multirriego apuntado en multi
      rotaryEncoder.disable();
      configure->Multi_process_start(n_grupo);
}

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
        configure->Multi_process_end(encoderSW);  // actualizamos config con las zonas introducidas
      }
      if(configure->configuringMultiTemp()) {
        configure->MultiTemp_process_end();  // preparamos lanzamiento multirriego temporal
      }
}

void handleStartMultiTemp()
{
  if (multi.w_size && saveConfig) {  //solo si se ha guardado alguna zona iniciamos riego grupo temporal
      saveConfig = false;  
      multi.temporal = true;
      setMultirriego(config); 
  }    
}


void procesaEstadoError(void)
{
  if (flagV) {   // acciones cada VERIFY_INTERVAL en estado ERROR
    if(errorOFF) sonido.bip(2);  //recordatorio error grave al parar un riego
    //se intenta recuperar error si en el SETUP no hemos podido conectar con la wifi o con domoticz
    if(Estado.error == E1 && recoverableError) wifiVerifyRecovery(config);
    if(Estado.error == E2 && recoverableError && checkReconInterval) domoticzVerifyRecovery();
  }
}; //fin de procesaEstadoError


void procesaEstadoRegando(void)
{
  int tiempoTerminado = T.Timer();
  if (T.TimeHasChanged()) refreshTime();
  if (tiempoTerminado == 0) setEstado(TERMINANDO);
  // verificamos periodicamente que el riego sigue activo en Domoticz
  else if(flagV && VERIFY && (!modoDEMO || simular.all_simFlags)) { 
    ledID = ultimoBotonZona->led;
    if (Estado.tipo != REMOTO) {
      tic_parpadeoLedZona.detach(); //detiene posible parpadeo led zona
      led(ledID,ON); //y lo dejamos fijo
    }               
    if(queryStatus(config.zona[ultimoBotonZona->znumber-1].idx, (char *)"On")) return;
    else {
      if(!Estado.error) { //riego zona parado: entramos en PAUSE y blink lento zona pausada remotamente 
        T.PauseTimer();
        finalTimeLastRiego(lastRiegos[ultimoBotonZona->znumber-1]); //actualizamos tiempo de riego de la zona
        tic_parpadeoLedZona.attach(LENTO, parpadeoLedZona, ledID);
        LOG_WARN(">>>>>>>>>> procesaEstadoRegando zona:", config.zona[ultimoBotonZona->znumber-1].desc, "en PAUSA remota <<<<<<<<");
        setEstado(PAUSE,1,REMOTO); //pasamos a PAUSE remoto
      }
      else {  // si no hemos podido verificar estado, señalamos zona blink rapido y continuamos
        tic_parpadeoLedZona.attach(NORMAL, parpadeoLedZona, ledID);
        Estado.error = NOERROR; // si no hemos podido verificar estado, ignoramos el error
        sonido.bip(2);
        LOG_ERROR("** SE HA DEVUELTO ERROR al verificar estado riego");
      }  
    }
  }
}; //fin de procesaEstadoRegando


void procesaEstadoTerminando(void)
{
  sonido.bip(5);
  tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
  tic_CountDownTimer.detach(); //detiene actualizacion periodica del temporizador
  bool updateTimeFin = (riegoFromPause? false : true); // si veniamos de PAUSE no actualizamos tiempo fin
  stopRiego(ultimoBotonZona->bID, updateTimeFin); // paramos riego en curso
  riegoFromPause = false; //reiniciamos flag
  if (Estado.estado == ERROR) return; //no continuamos si se ha producido error al parar el riego
  lcd.blinkLCD(DEFAULTBLINK);
  led(ultimoBotonZona->led,OFF);  // apaga led zona
  //Comprobamos si estamos en un multirriego
  if (multi.riegoON) {
    //sumamos tiempo riego zona terminada al tiempo de riego del grupo
    if(!multi.temporal) finalTimeGrupo(lastGrupos[multi.ngrupo-1], lastRiegos[ultimoBotonZona->znumber-1].total); 
    multi.actual++;
    if (multi.actual < *multi.size) {  // pasamos a regar la siguiente zona del grupo
      //Simular la pulsacion del siguiente boton de la serie de multirriego
      boton = &Boton[bID2bIndex(multi.serie[multi.actual])];
      multi.semaforo = true;
      //muestra en pantalla las zonas que restan por regar del grupo (excluida la zona en curso):
      int posicion = displayLCDGrupo(RESTO, 2);  //  display zonas quedan por regar
      if (riegoSaved.zonevalid) {  // si hay zona salvada la mostramos con "+" a continuacion
        lcd.setCursor(posicion, 1);
        lcd.printf("+%d", riegoSaved.znumber);
      }
    }
    else {         // señalamos fin del multirriego y actualizamos timestamp de finalizacion
      if(!multi.temporal) finalTimeGrupo(lastGrupos[multi.ngrupo-1]);
      lcd.info("multirriego",1);
      int msgl = snprintf(buff, MAXBUFF, "%s finalizado", multi.desc);
      lcd.info(buff, 2, msgl);
      resetFlags();
      sonido.bipFIN();
      LOG_INFO("MULTIRRIEGO", multi.desc, "terminado");
      delay(config.msgdisplaymillis*3);
      led(Boton[bID2bIndex(*multi.id)].led,OFF);  // apaga led grupo
      saveTablaToFile(lastGruposFile, "lastGrupos", lastGrupos, NUMGRUPOS);  //guardamos en fichero tabla de ultimos riegos de grupos
      saveTablaToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS);  //guardamos en fichero tabla de ultimos riegos de zonas
    }
  }
  else saveTablaToFile(lastRiegosFile, "lastRiegos", lastRiegos, NUMZONAS);  //guardamos en fichero tabla de ultimos riegos de zonas
  // si hay riego salvado y no estamos en multirriego lo recuperamos en pausa
  if (riegoSaved.zonevalid && !multi.riegoON) restoreRiego();
  else setEstado(STANDBY);
}; //fin de procesaEstadoTerminando


void procesaEstadoStandby(void)
{
  //Apagamos el display si ha pasado el lapso STANDBYSECS sin actividad
  if (reposo) standbyTime = millis();
  else {
    if (millis() > standbyTime + (1000 * STANDBYSECS)) {
      LOG_TRACE("LLamando a reposoON");
      reposoON();
    }
  }
  if (reposo & encoderSW) reposoOFF(); // pulsar boton del encoder saca del reposo
  // leemos encoder
  procesaEncoderClock();
  // verificaciones en STANDBY cada VERIFY_INTERVAL segundos
  //  - verificacion de wifi y recuperacion si procede
  //  - actualiza y muestra temperatura ambiente
  //  - actualizacion de hora por NTP si no la tenemos actualizada
  if (flagV) { 
    LOG_TRACE(".");
    if (multi.riegoON) return; //no se hacen verificaciones/acciones con multirriego en curso
    if (wifiVerifyRecovery(config)) { //verificacion de wifi y recuperacion si procede
      lcd.info("STANDBY",1);  //restaura pantalla (en caso de msg de reconexion)
      showTemp();  // muestra temperatura ambiente en standby
      if (!timeOK && connected) setClock(); // si no hemos recibido time por NTP -> actualizamos time del sistema con el del servidor NTP
      }
  }   
}; //fin de procesaEstadoStandby


void procesaEstadoStop(void)
{
  //En stop activamos el comportamiento hold de pausa
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = false;
  if (reposo & encoderSW) reposoOFF(); // pulsar boton del encoder saca del reposo
  //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
  if(reposo && !backlightOff) {
    if (millis() > standbyTime + (4 * 1000 * STANDBYSECS)) {
      lcd.setBacklight(OFF);
      backlightOff = true;
    }
  }
};

void procesaEstadoPause(void) {
  if(flagV && VERIFY && (!modoDEMO || simular.all_simFlags)) {  // verificamos zona sigue OFF en Domoticz periodicamente
    if(queryStatus(config.zona[ultimoBotonZona->znumber-1].idx, (char *)"Off")) return;
    else {
      if(!Estado.error) { //riego zona activo: salimos del PAUSE y blink lento zona activada remotamente 
        sonido.bip(2);
        ledID = ultimoBotonZona->led;
        tic_parpadeoLedZona.attach(LENTO, parpadeoLedZona, ledID);
        LOG_WARN(">>>>>>>>>> procesaEstadoPause zona:", config.zona[ultimoBotonZona->znumber-1].desc,"activada REMOTAMENTE <<<<<<<");
        T.ResumeTimer();
        char zonaText[7];
        snprintf(zonaText, sizeof(zonaText), "ZONA%d", ultimoBotonZona->znumber);
        inicioTimeLastRiego(lastRiegos[ultimoBotonZona->znumber-1], zonaText, RESUME); //actualizamos tiempo de riego de la zona
        setEstado(REGANDO,1,REMOTO); //pasamos a REGANDO remoto
      }
      else Estado.error = NOERROR; // si no hemos podido verificar estado, ignoramos el error
    }
  }
} //fin de procesaEstadoPause


/**---------------------------------------------------------------
 * Si config.dynamic = true , se admite una vez iniciado un riego de zona individual 
 * o multirriego temporal el poder añadir/eliminar más zonas para regar. 
 * Para ello se pasa este riego a multirriego temporal si no lo fuera ya.
 * Si la zona no está en el grupo se añade al final, si existe se elimina de este.
 * En el caso de riego en curso de zona individual, este no se ha factorizado y no se factorizaran los añadidos.
 * Si lo que se modifica dinamicamente es un multirriego temporal lanzado al principio 
 * si se factorizan todas las zonas iniciales y añadidas.
 */
bool procesaDynamic(void)
{
  // NOTA: si llegamos aquí, la zona pulsada NO coincide con la actualmente en riego (zona actual)
  if (!multi.riegoON) { //si estamos en riego de zona individual -> la pasamos a multirriego temporal
    setMultibyId(0, config);  // apunta estructura multi a grupo temporal en config (n+1) con id = 0
    multi.riegoON = true;
    multi.temporal = true;
    multi.dynamic  = true;  // marcamos como dinamico para no factorizarlo
    multi.semaforo = false;
    multi.actual=0;
    multi.serie[0] = ultimoBotonZona->bID;  // bId de la zona actual como primera de la lista
    multi.zserie[0] = ultimoBotonZona->znumber;  // numero de la zona actual como primera de la lista
    *multi.size = 1;  
  }
  if(!multi.temporal) return false;  //estamos en multirriego de grupo --> zona ignorada
  LOG_DEBUG("[RECIBE] MULTI dynamic:",multi.dynamic,"actual:",multi.actual,"size:",*multi.size,"zona:",boton->znumber);
  // ya estamos en multirriego temporal (generado dinamico o lanzado directo)
  int n;
  for(n=multi.actual; n<*multi.size; n++) { // recorremos la lista de zonas a ver si existe ya
    if(multi.zserie[n] == boton->znumber) { // zona pulsada ya existe en la lista 
      LOG_DEBUG("[vamos a ELIMINAR] n=",n,"actual:",multi.actual,"zona:",boton->znumber,"size:",*multi.size);
      for(n; n<*multi.size; n++) {          //  --> la eliminamos de esta
        multi.zserie[n] = multi.zserie[n+1];
        multi.serie[n] = multi.serie[n+1];
      }
      *multi.size = n-1;
      LOG_DEBUG("[ELIMINA] n=",n,"actual:",multi.actual,"size:",*multi.size,"zona:",boton->znumber);
      LOG_INFO("DYNAMIC [ELIMINA] Zona:",boton->znumber);
      sonido.bip(2); return true; //zona eliminada
    }
  } 
  // la zona pulsada no existe en la lista --> añadirla al final
  multi.w_size = n; //indice zona de la lista donde añadir la nueva zona (1 a ZONASXGRUPO-1)
  if (multi.w_size < ZONASXGRUPO) {  //añadimos zona al final de la lista si hay sitio
    multi.serie[multi.w_size] = boton->bID;  // bId de la zona pulsada
    multi.zserie[multi.w_size] = boton->znumber;  // numero de la zona pulsada
    *multi.size = multi.w_size + 1;
    LOG_DEBUG("[AÑADE] n=",n,"actual:",multi.actual,"size:",*multi.size,"zona:",boton->znumber);
    LOG_INFO("DYNAMIC [AÑADE] Zona:",boton->znumber);
    sonido.bip(1); return true; //zona añadida
  }
  else return false; //no hay sitio --> zona ignorada   
}   //fin de procesaDynamic


/**---------------------------------------------------------------
 * Pone estado pasado y sus indicadores opcionales
 */
void setEstado(uint8_t estado, int bnum, int tipo)
{
  LOG_DEBUG( "recibido ", nEstado[estado], "bnum=", bnum, " tipo=", tipo);
  // setup y reseteos varios
  Estado.estado = estado;
  Estado.error = NOERROR;
  recoverableError = false;
  if(Estado.estado == !PAUSE) riegoFromPause = false; //reiniciamos flag  TODO ¿esto hay que revisarlo?
  strcpy(errorText, "");
  //Deshabilitamos el hold de Pause
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = true;
  if(reposo) reposoOFF();     //por si salimos de stop antinenes
  rotaryEncoder.disable();  // para que no cuente pasos salvo que lo habilitemos
  lcd.displayON();
  setledRGB();   // led RGB segun status wifi y modoDEMO
  lcd.setCursor(17, 1);
  if (tipo == REMOTO) {Estado.tipo = REMOTO; lcd.print("(R)");}
  else {Estado.tipo = LOCAL; lcd.print("   ");}
  LOG_DEBUG( ">>>> Estado.tipo =", Estado.tipo);
  if((estado==REGANDO || estado==TERMINANDO ) && ultimoBotonZona != NULL) {
    lcd.infoEstado(nEstado[estado], config.zona[ultimoBotonZona->znumber-1].desc);
    if(modoDEMO) displayDemo(); 
    if(multi.dynamic) displayNoFactorizado();
      else if(multi.temporal) displayMultiTemporal(); 
    return;
  }
  if(estado==PAUSE) {
    lcd.infoEstado(nEstado[estado], config.zona[ultimoBotonZona->znumber-1].desc); 
    if(bnum) sonido.bip(bnum);
    return;
  }
  if(estado == STANDBY) {
    // if(!multi.riegoON && !multi.temporal) {
    if(!multi.riegoON) {  //en los intervalos entre riegos no se muestra la pantalla de standby
      resetLeds();
      lcd.infoclear("STANDBY",NOBLINK,BIP,bnum);
      showTemp();
    }  
    StaticTimeUpdate(REFRESH);
    setEncoderTime();
    if(modoDEMO) displayDemo();
    standbyTime = millis();
    return;
  }
  if(estado == STOP) {
    lcd.infoclear("STOP", NOBLINK, LOWBIP, bnum);
    return;
  }
  if(estado == CONFIGURANDO) {
    lcd.infoclear("CONFIGURANDO", NOBLINK, LOWBIP, bnum);
    ledYellow(ON);
    boton = NULL; //borramos boton pulsado para que no sea tratado más adelante en procesaEstadoConfigurando
    holdPause = false;
    return;
  }
} //fin setEstado


/**---------------------------------------------------------------
 * Chequeo de perifericos
 */
void check(void)
{
  apagaLeds();
  #ifndef noCHECK
    initLeds();
  #endif
}


/**---------------------------------------------------------------
 * Lee factores de riego del domoticz
 */
void initFactorRiegos()
{
  LOG_DEBUG("entrada InitFactorRiegos Estado.error=", Estado.error, "recoverableError=", recoverableError, "noWIFI=", noWIFI);
  
  for(uint i=0;i<NUMZONAS;i++) {  //inicializamos a valor 100 por defecto para caso de error
    factorRiegos[i]=100;
  }
  factorRiegosLeido = false;

  if((!connected) || noWIFI) return; //si no tenemos wifi o noWIFI, ni lo intentamos
  lcd.info("conectando Domoticz", 2);
  lcd.clear(BORRA2H);
  
  for(uint i=0;i<NUMZONAS;i++) //leemos factores del Domoticz
  {
    uint factorR = getFactor(config.zona[i].idx);
    if(factorR == 999) break;     //en modoDEMO no continuamos iterando si no se ha podido leer por alguna causa
    if (checkErrorgetFactor(bID2bIndex(ZONAS[i]))) break;   //al primer error salimos
    factorRiegos[i] = factorR;
    LOG_TRACE("zona",i+1,"factor asignado=",factorR);
    
    // si XNAME: true, leemos la descripcion de la zona del domoticz (si existe) y la guardamos en config
    if (config.xname) updateZoneDescription(i);
  }
  LOG_DEBUG("salida  InitFactorRiegos Estado.error=", Estado.error, "recoverableError=", recoverableError, "noWIFI=", noWIFI);
  #ifdef VERBOSE
    printFactoresRiego();
  #endif
}  //fin initFactorRiegos

bool checkErrorgetFactor(int bIndex) {
  if(Estado.estado == ERROR) {
    if(Estado.error == E3) {    // señalamos zona que falla si no es error general de conexion
      ledID = Boton[bIndex].led;
      tic_parpadeoLedZona.attach(NORMAL, parpadeoLedZona, ledID);
    }
    return true;
  } else return false;
}

//Aqui convertimos minutes y seconds por el factorRiegos
void timeByFactor(int factor,uint8_t *fminutes, uint8_t *fseconds)
{
  uint tseconds = (60*tm.minutes) + tm.seconds;
  //factorizamos
  tseconds = (tseconds*factor)/100;
  if (tseconds > 59*60) tseconds = 59*60; //limitamos tiempo maximo factorizado a 59 minutos
  //reconvertimos
  *fminutes = tseconds/60;
  *fseconds = tseconds%60;
}

void cbSyncTime(struct timeval *tv)  { // callback function to show when NTP was synchronized
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  Serial.printf("<<<<   NTP time synched   >>>>   Local time: %s \n", asctime(&timeinfo));
}

// set reloj del ESP32 y timezone con el time recibido por NTP (se actualizara automaticamente cada 3 horas (default))
void setClock()
{
  // sntp_set_time_sync_notification_cb(cbSyncTime);  // set a Callback function for time synchronization notification
  // sntp_set_sync_interval(60 * 60 * 1000UL); // 60 minutos (default ESP32 es 180 minutos - 3 horas)

  LOG_INFO("Timezone: ", config.TZ, "   NTP server: ", config.ntpServer);
  configTzTime(config.TZ, config.ntpServer); 
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, NTP_TIMEOUT)) {
    timeOK = false;
    LOG_WARN(">>> NO TIME SET by NTP <<<");
    return;
  }
  timeOK = true;
  char message[150];
  strftime(message, sizeof(message), "\n>>> TIME SET by NTP <<<   Local time: %A, %B %d %Y %H:%M:%S (zone %Z %z)", &timeinfo);
  PRINTLN(message);
  PRINTLN("NTP update every ", sntp_get_sync_interval()/(1000*60), " minutos\n");
}

time_t tLoc()
{
  if (!timeOK) return 0; //no tenemos time, devolvemos 0
  time_t t = time(NULL); // time() devuelve el tiempo UTC actual (epoch time en segundos desde 00:00 1/1/1970) leyendolo del reloj del ESP32
  struct tm *tmd;
  tmd = localtime(&t); // localtime() convierte time_t a struct tm en la zona horaria local
  //copy tmd struct to tmElements_t struct
  tmElements_t tmElements;
  tmElements.Second = tmd->tm_sec;
  tmElements.Minute = tmd->tm_min;
  tmElements.Hour = tmd->tm_hour;
  tmElements.Day = tmd->tm_mday;
  tmElements.Month = tmd->tm_mon + 1; // tm_mon is 0-based
  tmElements.Year = tmd->tm_year - 70;    // tmd->tm_year is years since 1900 , tmElements.Year is years since 1970
  //calculate time_t from tmElements_t struct
  // makeTime() (from TimeLib) NO tiene en cuenta el timezone del sistema  --> devuelve time local en este caso
  // mktime() (from time.h) si lo tiene en cuenta --> devolveria time UTC
  time_t tLocal = makeTime(tmElements);
  return tLocal;
}


void initEncoder() {
    LOG_TRACE("");
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    setEncoderTime();
}

void setEncoderTime() {
    LOG_TRACE("");
    rotaryEncoder.setBoundaries(0, 1000, false); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
    rotaryEncoder.setEncoderValue(500);
    rotaryEncoder.setAcceleration(50); // set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
    rotaryEncoder.enable();
    tmvalue(); //set valor tm.value para ajustar tiempo con procesaencoder
    }

void setEncoderRange(int min, int max, int current, int aceleracion) {
    LOG_DEBUG("min=",min,"max=",max,"current=",current);
    rotaryEncoder.setBoundaries(min, max, false); //minValue, maxValue, circleValues true|false 
    // ver issue: https://github.com/igorantolic/ai-esp32-rotary-encoder/issues/78
    //   (rotaryEncoder.setEncoderValue() offset by one when value is negative #78):
    if (current < 0 && current > min) current = current - 1 ; //ÑAPA hasta que se arregle
    rotaryEncoder.setEncoderValue(current);
    rotaryEncoder.setAcceleration(aceleracion);
    rotaryEncoder.enable();
}

void setEncoderMenu(int menuitems, int currentitem) {
    LOG_DEBUG("currentitem=",currentitem);
    rotaryEncoder.setBoundaries(0, menuitems-1, true); //minValue, maxValue, circleValues true|false
    rotaryEncoder.setEncoderValue(currentitem);
    rotaryEncoder.disableAcceleration();
    rotaryEncoder.enable();
}

//muestra lo regado desde las 0h encendiendo sus leds y mostrando hora o apagandolos
void ultimosRiegos(int modo)
{
  LOG_TRACE("modo:",modo);
  switch(modo) {
    case SHOW:
    lcd.infoclear("Hora actual:");
    if (timeOK) {
        time_t t = tLoc();
        for(uint i=0;i<NUMZONAS;i++) { // enciende leds zonas regadas desde medianoche
          if(lastRiegos[i].inicio > previousMidnight(t)) {
              LOG_DEBUG("[ULTIMOSRIEGOS] zona:", i+1, "time:",lastRiegos[i].inicio);
              led(Boton[bID2bIndex(ZONAS[i])].led,ON);
          }
        }
        if (config.lastr24) { //activa parpadeo leds zonas regadas entre 24h y medianoche
          tic_parpadeoLedZonas24h.attach(RAPIDO, parpadeoLedZonas24h, t);
        }
        sprintf(buff, " %d", day(t));
        lcd.info(buff,3);
        lcd.info(MESES[month(t)-1],4);
        lcd.displayTime(hour(t),minute(t));
      } else {lcd.info("   <<< NO TIME >>>",3); sonido.bipKO();}
      break;
    case HIDE:
      tic_parpadeoLedZonas24h.detach();
      for(unsigned int i=0;i<NUMZONAS;i++) {
        led(Boton[bID2bIndex(ZONAS[i])].led,OFF);
      }
      break;
  }
}

void parpadeoLedZonas24h(time_t t)
{
  for(uint i=0;i<NUMZONAS;i++) { // enciende leds zonas regadas ultimas 24h hasta medianoche
    if(lastRiegos[i].inicio > (t-SECS_PER_DAY) && lastRiegos[i].inicio <= previousMidnight(t)) {
        LOG_TRACE("[ULTIMOSRIEGOS 24H] zona:", i+1, "time:",lastRiegos[i].inicio);
        int ledid = Boton[bID2bIndex(ZONAS[i])].led;
        byte estado = ledStatusId(ledid);
        led(ledid,!estado);
    }
  }
}

void inicioTimeLastRiego(S_timeRiego &timeRiego, const char* texto, bool resume) 
{
  time_t t = tLoc();
  if (resume)
  {
    // si estamos reanudando un riego, mantenemos el inicio del riego anterior
    LOG_DEBUG("actualizo lastriegos: reanudando riego ", texto, "timestamp:", t);
    timeRiego.reinicio = t;
  }
  else
  {
    // si estamos iniciando, actualizamos el inicio del riego
    LOG_DEBUG("actualizo lastriegos: iniciando riego ", texto, "timestamp:", t);
    timeRiego.inicio = t;
    timeRiego.final = 0;
    timeRiego.reinicio = t;
    timeRiego.total = 0;
  }
}  

void finalTimeLastRiego(S_timeRiego &timeRiego) 
{
  time_t t = tLoc();
  char zonaText[7];
  snprintf(zonaText, sizeof(zonaText), "ZONA%d", ultimoBotonZona->znumber);
  LOG_DEBUG("actualizo lastriegos fin ", zonaText, "timestamp:", t);
  timeRiego.final = t;
  timeRiego.total = timeRiego.total + (timeRiego.final - timeRiego.reinicio); //acumulado = acumulado + (intervalo regado)
  LOG_DEBUG("tiempo total regado hasta ahora ", zonaText, "total:", timeRiego.total / 60.0, "minutos");
}  

void finalTimeGrupo(S_timeRiego &timeRiego, time_t tZona) 
{
  char grupoText[7];
  snprintf(grupoText, sizeof(grupoText), "GRUPO%d", multi.ngrupo);
  // si tZona no es 0, es el tiempo de regado de una zona del grupo -> acumulamos el tiempo de la zona al total del grupo
  if (tZona) {
    timeRiego.total = timeRiego.total + tZona; //total = acumulado hasta ahora + tiempo ultima zona regada
    LOG_DEBUG("tiempo regado hasta ahora ", grupoText, "acumulado:", timeRiego.total / 60.0, "minutos");
  }
  // si tZona es 0, significa que estamos en fin de riego de grupo -> registramos el timestamp final del grupo
  else {
    time_t t = tLoc();
    LOG_DEBUG("actualizo lastriegos fin ", grupoText, "timestamp:", t);
    timeRiego.final = t;
    LOG_DEBUG("tiempo total riego ", grupoText, "total:", timeRiego.total / 60.0, "minutos");
  }
}  

void showTimeLastRiego(S_timeRiego &timeRiego, int index, int tipo) 
{
  time_t t1=timeRiego.inicio;
  time_t t2=timeRiego.final;
  LOG_DEBUG("Zona/Grupo:", index+1 , "time.inicio", t1, "time.final", t2);
  if (t1 && t2-t1 > 0) { // si tenemos inicio y finalizacion del riego
    snprintf(buff, MAXBUFF, "-ultimo riego:   %02dm", (timeRiego.total+20)/60);
    // if (tipo == ZONA) snprintf(buff, MAXBUFF, "-ultimo riego:   %02dm", (timeRiego.total+20)/60);
    // if (tipo == GRUPO) snprintf(buff, MAXBUFF, "-ultimo riego grupo:");
    lcd.info(buff,3);
    snprintf(buff, MAXBUFF, " %d/%02d %d:%02d (%d:%02d)", day(t1), month(t1), hour(t1), minute(t1), hour(t2), minute(t2));
    lcd.info(buff,4);
  } else {
      lcd.info("-ultimo riego:",3);
      lcd.info("   > sin datos <",4);
  }  
}

void startZoneWatering() {
    sonido.bip(2);
    //cambia minutes y seconds en funcion del factor de cada sector de riego
    uint8_t fminutes=0,fseconds=0;
    if(multi.riegoON && !multi.dynamic) {
      timeByFactor(factorRiegos[boton->znumber-1],&fminutes,&fseconds);
    }
    else {
      fminutes = tm.minutes;
      fseconds = tm.seconds;
    }
    LOG_DEBUG("Minutos:",tm.minutes,"Segundos:",tm.seconds,"FMinutos:",fminutes,"FSegundos:",fseconds);
    ultimoBotonZona = boton;
    // si tiempo factorizado de riego es 0 o IDX=0, nos saltamos este riego
    if ((fminutes == 0 && fseconds == 0) || config.zona[(boton->znumber)-1].idx == 0) {
      setEstado(TERMINANDO);
      led(boton->led,ON); //para que se vea que zona es 
      lcd.clear(BORRA2H);
      lcd.info("IDX/factor:     -00-",4);
      return;
    }
    if(initRiego(INICIO)) { //comenzamos el riego de la zona
      setEstado(REGANDO);
      //inicializamos el timer de cuenta atras
      T.SetTimer(0,fminutes,fseconds);
      T.StartTimer();
      tic_CountDownTimer.attach_ms(10, timerTick); // Llama a timerTick() cada 10 ms
    }  
}

// Muestra en el display info zona (idx, factor de riego, fecha y tiempo ultimo riego)
void showInfoZona(int zIndex) {
    led(boton->led,ON);
    #ifdef EXTRADEBUG
      Serial.printf("Boton: %s Factor de riego: %d \n", config.zona[boton->znumber-1].desc,factorRiegos[zIndex]);
      Serial.printf("          boton.led: %d \n",boton->led);
    #endif
    lcd.clear();
    lcd.infoCut(config.zona[boton->znumber-1].desc, 11);
    lcd.setCursor(12, 0);
    snprintf(buff, MAXBUFF, "idx(%d)", config.zona[boton->znumber-1].idx);
    lcd.print(buff);
    snprintf(buff, MAXBUFF, "-factor riego:  %d", factorRiegos[zIndex]);
    lcd.info(buff,2);
    showTimeLastRiego(lastRiegos[zIndex], zIndex, ZONA);
    delay(config.msgdisplaymillis*4);
    led(boton->led,OFF);
    setEstado(STANDBY);
}

// Procesar cambio dinamico y reflejarlo en el display
void handleDynamicZoneChange() {
    // NOTA: la zona pulsada no puede coincidir con la actualmente en riego, se ignora en ese caso
    if (ultimoBotonZona->bID != boton->bID) {
      if (procesaDynamic()) displayLCDGrupo(RESTO, 2);
      LOG_DEBUG("MULTI dynamic:",multi.dynamic,"actual:",multi.actual,"size:",*multi.size,"zona:",boton->znumber);
    }
    else {sonido.bipKO(); LOG_DEBUG("[DYNAMIC] zona pulsada:",boton->znumber," es = a zona actual:",ultimoBotonZona->znumber);}
}

void updateZoneDescription(int i) {
      String response = deviceInfo(config.zona[i].idx, (char *)"Name");
      if (response.startsWith("Err") || strlen(response.c_str()) == 0) {
        LOG_WARN("Sin descripcion de zona", i+1, "idx=", config.zona[i].idx, "response:", response.c_str());
        return; //error en la lectura de la descripcion, no actualizamos nada y pasamos al siguiente idx
      }
      LOG_INFO("\t descripcion ZONA", i+1, "actualizada en config");
      strlcpy(config.zona[i].desc, response.c_str(), sizeof(config.zona[i].desc));
}

void printFactoresRiego() {
    Serial.print(F("Factores de riego "));
    factorRiegosLeido ? Serial.println(F("leidos: ")) :  Serial.println(F("(simulados): "));
    for(uint i=0;i<NUMZONAS;i++) {
      Serial.printf("\tfactor ZONA%d: %d (%s) \n", i+1, factorRiegos[i], config.zona[i].desc);
    }
}

// ON/OFF atenuacion LEDG y LEDB
void dimmerLeds(bool status)
{
  if(status) {
    LOG_TRACE("leds atenuados ");
    if(connected) analogWrite(LEDG, config.dimmlevel);
    if(modoDEMO) analogWrite(LEDB, config.dimmlevel);
  }
  else {
    LOG_TRACE("leds brillo normal ");
    if(connected) analogWrite(LEDG, config.maxledlevel);
    if(modoDEMO) analogWrite(LEDB, config.maxledlevel);
  }  
}

void reposoOFF()
{
  LOG_INFO(" salimos de reposo");
  reposo = false;
  dimmerLeds(OFF);
  lcd.setBacklight(ON);
  backlightOff = false;
  standbyTime = millis();
}

void reposoON(bool lcdOFF)
{
  LOG_INFO(" entramos en reposo");
  reposo = true;
  dimmerLeds(ON);
  if(lcdOFF) lcd.setBacklight(OFF);
}


//lee encoder para actualizar parametro configuracion
void procesaEncoderConfig()
{

  if(configure->statusMenu()) {  //encoder selecciona item menu
      int menuOption = rotaryEncoder.readEncoder();  //devuelve valor actual del encoder (se haya movido o no)
      if(menuOption == configure->get_currentItem()) return;
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

  if (configure->configuringTime()) procesaEncoderClock(); //encoder ajusta tiempo de riego por defecto
}
  
//lee encoder para actualizar el clock
void procesaEncoderClock()
{
  //* Ajuste de tiempo de riego en STANDBY o CONFIGURANDO tiempo de riego por defecto
  // encoder ajusta tiempo (mm:ss) de MINSECONDS a MAXMINUTES
  // en segundos hasta 59 y a partir de ahí en minutos enteros
  // por lo tanto uno de los dos (mm o ss) debe ser 0.
  // tm.value recoge el valor del campo distinto de 00 que se ajusta  
  int encvalue = rotaryEncoder.encoderChanged();  //devuelve cuanto y en que sentido se ha movido el encoder
  if(!encvalue) return; 
  LOG_DEBUG("rotaryEncoder.encoderChanged() devuelve encvalue =", encvalue);
  
  tm.value = tm.value + encvalue;

  if(tm.seconds == 0 && tm.value>0) {   //Estamos en el rango de minutos
    if (tm.value > MAXMINUTES) tm.value = MAXMINUTES;
    if (tm.value != tm.minutes) {
      tm.minutes = tm.value;
    } else return;
  } else {    //o bien estamos en el rango de segundos o acabamos de entrar en el
      if(tm.value<60 && tm.value>=MINSECONDS) {
        if (tm.value != tm.seconds) {
          tm.seconds = tm.value;
        } else return;
      } else if (tm.value >=60) {
          tm.value = tm.minutes = 1;
          tm.seconds = 0;
        } else if(tm.minutes == 1) {
            tm.value = tm.seconds = 59;
            tm.minutes = 0;
          } else {
              tm.value = tm.seconds = MINSECONDS;
              tm.minutes = 0;
            }
    }
  if(reposo) reposoOFF();
  configure->configuringTime() ? configure->Time_process_update() : StaticTimeUpdate(UPDATE);
  standbyTime = millis();
}


void initLastRiegos()
{
  if (loadTablaFromFile("/lastRiegos.json", "lastRiegos", lastRiegos, NUMZONAS)) {
    Serial.println("Ultimos riegos de zonas leidos de " + String(lastRiegosFile));
    return;
  }
  for(uint i=0;i<NUMZONAS;i++) {
   lastRiegos[i].inicio = 0;
   lastRiegos[i].final = 0;
   lastRiegos[i].total = 0;
  }
}

void initLastGrupos()
{
  if (loadTablaFromFile(lastGruposFile, "lastGrupos", lastGrupos, NUMGRUPOS)) {
    Serial.println("Ultimos riegos de grupos leidos de " + String(lastGruposFile) + "\n");
    return;
  }
  for(uint i=0;i<NUMGRUPOS;i++) {
   lastGrupos[i].inicio = 0;
   lastGrupos[i].final = 0;
   lastGrupos[i].total = 0;
  }
}

//Inicia/reanuda el riego correspondiente al idx del boton de zona pulsado ultimo
bool initRiego(bool resume)
{
  int zIndex = ultimoBotonZona->znumber-1;
  if (zIndex < 0) return false; //el boton no es de ZONA o error en la matriz Boton[]
  led(ultimoBotonZona->led,ON);
  LOG_DEBUG("Boton:",config.zona[zIndex].desc,"zona:",ultimoBotonZona->znumber,"IDX:",config.zona[zIndex].idx);
  if (resume) LOG_INFO( "Continuando riego: ", config.zona[zIndex].desc);
  else LOG_INFO( "Iniciando riego: ", config.zona[zIndex].desc);
  if (domoticzSwitch(config.zona[zIndex].idx, (char *)"On", DEFAULT_SWITCH_RETRIES)) {
    char zonaText[7];
    snprintf(zonaText, sizeof(zonaText), "ZONA%d", zIndex+1);
    inicioTimeLastRiego(lastRiegos[zIndex], zonaText, resume);
      #ifdef EXTRADEBUG
          for(uint i=0;i<NUMZONAS;i++) {
                LOG_DEBUG("[ULTIMOSRIEGOS] inicio zona:", i+1, "time:",lastRiegos[i].inicio);
            }
      #endif
    return true; 
  } else return false; //error al iniciar el riego   
}


//Termina el riego correspondiente al idx del boton (id) pasado
bool stopRiego(uint16_t id, bool update)
{
  int bIndex = bID2bIndex(id);
  int zIndex = Boton[bIndex].znumber-1;
  ledID = Boton[bIndex].led;
  LOG_DEBUG( "Terminando riego: ", config.zona[zIndex].desc);
  if (domoticzSwitch(config.zona[zIndex].idx, (char *)"Off", DEFAULT_SWITCH_RETRIES)) {
    LOG_INFO( "Terminado OK riego: " , config.zona[zIndex].desc );
    // solo actualizamos hora de fin si no hemos sido llamado desde stopAllRiego :
    if(update) finalTimeLastRiego(lastRiegos[zIndex]);
        #ifdef EXTRADEBUG
            for(uint i=0;i<NUMZONAS;i++) {
                  LOG_DEBUG("[ULTIMOSRIEGOS] fin zona:", i+1, "time:",lastRiegos[i].final);
              }
        #endif
    return true;
  } else {    //avisa de que no se ha podido terminar un riego
      if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
        errorOFF = true;  // recordatorio error
        tic_parpadeoLedError.attach(RAPIDO,parpadeoLedError);
        tic_parpadeoLedZona.attach(RAPIDO, parpadeoLedZona, ledID);
      } 
      return false;
    }  
}


//Guarda el estado del riego en curso para una posible reanudacion
void saveRiego(int znumber, int bID, int minutes, int seconds)
{
  LOG_INFO("salvando estado riego zona :",znumber," tiempo restante: ", minutes, ":", seconds);
  riegoSaved.zonevalid = true;
  riegoSaved.znumber = znumber;
  riegoSaved.bID = bID;
  riegoSaved.minutes = minutes;
  riegoSaved.seconds = seconds;
}

//Recupera el estado del riego salvado dejandolo en PAUSE para que el usuario confirme el reinicio
void restoreRiego(void)
{
    LOG_INFO("recuperando riego salvado de zona:", riegoSaved.znumber);
    riegoSaved.zonevalid = false; //ya no es valida    
    ultimoBotonZona = &Boton[bID2bIndex(riegoSaved.bID)];
    led(ultimoBotonZona->led,ON); //encendemos led de la zona
    T.SetTimer(0,riegoSaved.minutes,riegoSaved.seconds);  //inicializamos el timer de cuenta atras
    lcd.displayTime(T.ShowMinutes(), T.ShowSeconds());
    setEstado(PAUSE); //ponemos en PAUSE para que el usuario confirme el inicio del riego salvado
    // if(initRiego(RESUME)) setEstado(REGANDO); //ponemos en REGANDO directamente
}    



//Pone a off todos los leds de zonas y grupos y restablece estado led RGB
void resetLeds()
{
  //Apago los leds de multirriego
  for(unsigned int j=0;j<NUMGRUPOS;j++) {
    led(Boton[bID2bIndex(GRUPOS[j])].led,OFF);
  }
  //Apago los leds de riego y posible parpadeo
  tic_parpadeoLedZona.detach();
  tic_parpadeoLedZonas24h.detach();
  for(unsigned int i=0;i<NUMZONAS;i++) {
    led(Boton[bID2bIndex(ZONAS[i])].led,OFF);
  }
  //restablece led RGB
  tic_parpadeoLedError.detach(); //por si estuviera parpadeando
  setledRGB();
}

int  ledlevel()
{
  return (reposo ? config.dimmlevel : config.maxledlevel);
}


//Pone a false diversos flags de estado
void resetFlags()
{
  LOG_TRACE("");
  multi.riegoON  = false;
  multi.temporal = false;
  multi.dynamic  = false;
  multi.semaforo = false;
  errorOFF = false;
  recoverableError = false;
  webServerAct = false;
  simular.all_simFlags = false;
}


//reset estado display LCD
void resetLCD()
{
  LOG_TRACE("LCD reseteado");
  lcd.clear();
  lcd.setBacklight(ON);
  lcd.displayON();
}

//Pone a off todos los leds de riegos y detiene riegos
bool stopAllRiego()
{
  LOG_TRACE("");
  //Apago los leds de multirriego
  for(unsigned int i=0;i<NUMGRUPOS;i++) { 
    led(Boton[bID2bIndex(GRUPOS[i])].led,OFF);
  }
  //Apago los leds de riego y posible parpadeo
  tic_parpadeoLedZona.detach();
  for(unsigned int i=0;i<NUMZONAS;i++) { //paramos todas las zonas de riego
    led(Boton[bID2bIndex(ZONAS[i])].led,OFF);
    if(!stopRiego(ZONAS[i], false)) return false; //al primer error salimos
  }
  return true;
}

void blinkDisplay()
{
  if (!lcd.get__displayOff()) {
    if (millis() > lastBlinkPause + 1.5*DEFAULTBLINKMILLIS) {  // *1.5 para compensar inercia LCD
      lastBlinkPause = millis();
      lcd.displayOFF();
      if(Estado.estado == PAUSE) ledYellow(OFF);
    }
  }
  else {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      lastBlinkPause = millis();
      lcd.displayON();
      if(Estado.estado == PAUSE) ledYellow(ON);
    }
  }
}


/// @brief Actualiza el tiempo en pantalla
/// @param refresh si true actualiza incondicionalmente, en caso contrario solo si ha cambiado
void StaticTimeUpdate(bool refresh)
{
  if (refresh) lcd.clear(BORRA2H);
  if(prevseconds != tm.seconds || prevminutes != tm.minutes || refresh) {
    lcd.displayTime(tm.minutes, tm.seconds); 
    prevseconds = tm.seconds;
    prevminutes = tm.minutes;
  }
}

void refreshTime()   // Actualiza la cuenta atrás en pantalla
{
  unsigned long curMinutes = T.ShowMinutes();
  unsigned long curSeconds = T.ShowSeconds();
  lcd.displayTime(curMinutes, curSeconds);
  // if(prevseconds != curSeconds) lcd.displayTime(curMinutes, curSeconds);
  // prevseconds = curSeconds;

}

// Para actualizar el temporizador de cuenta atrás
void timerTick() {
    T.Timer();
}

int tmvalue()
{
  tm.value = ((tm.seconds==0)?tm.minutes:tm.seconds);
  return tm.value;
}

/**---------------------------------------------------------------
 * Comunicacion con Domoticz usando httpGet
 */
String httpGetDomoticz(String message) 
{
  LOG_TRACE("");
  String tmpStr = "http://" + String(config.domoticz_ip) + ":" + config.domoticz_port + String(message);
  LOG_DEBUG("TMPSTR:", tmpStr);
  httpclient.begin(client, tmpStr);
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
      LOG_ERROR("ERROR comunicando con Domoticz: ", httpclient.errorToString(httpCode).c_str()); 
    }
    return "Err2";
  }
  //vemos si la respuesta indica status error
  int pos = response.indexOf("\"status\" : \"ERR");
  if(pos != -1) {
    LOG_ERROR(" ** Domoticz a devuelto error: ", response.c_str()); 
    return "ErrX";
  }
  httpclient.end();
  return response;
}

/**------------------------------------------------------------------------------------
 * Envia mandato al SCD (Sistema de Control Domotico) y devuelve json con la respuesta
 */
String cmdtoSCD(String mandato)
{
  LOG_DEBUG(" comando: ", mandato);
  String message = COMMANDPRF + mandato;
  return httpGetDomoticz(message);
}

/**---------------------------------------------------------------
 * devuelve campo con informacion del dispositivo con el idx pasado
 */
String deviceInfo(int idx, char *campo)
{
  char message[150];
  sprintf(message,COMMANDPRF QUERYDEVICE,idx);
  String response = httpGetDomoticz(message);
    //procesamos la respuesta para ver si se ha producido error:
    if (response.startsWith("Err")) {
      LOG_ERROR(" ** [ERROR] IDX: ", idx, " [HTTP] GET... failed");
      return response; //devolvemos el error recibido
    }
  /* Teoricamente ya tenemos en response el JSON, lo procesamos
     Si el IDX no existe Domoticz no devuelve error, asi que hay que controlarlo
  */
 char* response_pointer = &response[0];
    JsonDocument jsondoc;
    DeserializationError error = deserializeJson(jsondoc, response_pointer);
    if (error) {
      LOG_ERROR(" **  [ERROR] deserializeJson() failed: ", error.c_str());
      return "Err3"; //error de deserializacion
    }
    //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
    /* "ArduinoJson implements the Null Object Pattern, it is always safe to read the object:
     if the key doesn’t exist, it returns an empty value." */
    const char *contenido_campo = jsondoc["result"][0][campo];
    if(contenido_campo == NULL) {
      LOG_ERROR(" **  [ERROR] deserializeJson() return: IDX ", idx, " o ", campo, " not found");
      LOG_ERROR(" **  [ERROR] respuesta: ", response.c_str());
      return "Err3"; //campo no encontrado en la respuesta o idx no existe
    }
    return contenido_campo; //devolvemos el campo solicitado
}  //fin deviceInfo

/**---------------------------------------------------------------
 * lee factor de riego del Domoticz, almacenado en campo Description
 */
int getFactor(uint16_t idx)
{
  LOG_TRACE("");
  if(idx == 0) return 100; //si el IDX es 0 devolvemos 100 sin procesarlo (boton no asignado)
  factorRiegosLeido = false;

  String response = deviceInfo(idx, (char *)"Description");

  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
      if (modoDEMO) return 999;  //si estamos en modoDEMO devolvemos 999 y no damos error
      if(response != "Err2") {
        if (VERIFY) statusError(E3); //error de deserializacion, posible IDX inexistente
      } else statusError(E2, RECUPERABLE); //error de conexion con Domoticz recuperable
      LOG_WARN("GETFACTOR IDX: ", idx, " respuesta recibida: ", response.c_str());
      return 100;
  }
    
  //si hemos leido correctamente campo Description (numero, campo vacio o solo con comentarios)
  //el IDX existe, consideramos leido OK el factor riego. 
  //En los dos ultimos casos se devuelve valor por defecto 100.
  factorRiegosLeido = true;
  LOG_DEBUG("GETFACTOR IDX: ", idx, " respuesta recibida: ", response.c_str());
  char* factorstr = &response[0];
  long int factor = strtol(factorstr,NULL,10);
  //controlamos devolver 0 solo si se ha puesto explicitamente
  if (factor == 0) {
    if (strlen(factorstr) == 0) return 100;    //campo comentarios vacio -> por defecto 100
    if (!isdigit(factorstr[0])) return 100;    //comentarios no comienzan por 0
  }
  return (int)factor;
} //fin getFactor

// Obtiene del Domoticz el log de la zona en formato JSON
// (ultimos 15 dias, es un parametro ajustable en el Domoticz -> log historico de luces/interruptores) 
String readLogFile(int zona)
{
  int idx = config.zona[zona-1].idx;
  LOG_DEBUG("zona:", zona, "idx:", idx);
  if(idx == 0) return "No asignado";
  char message[150];
  sprintf(message,COMMANDPRF GETSWITCHLOG,idx);
  return httpGetDomoticz(message);

}

bool checkDomoticz()
{
  LOG_TRACE("");
  tic_parpadeoLedRecon.attach(RAPIDO, parpadeoLedAP);
  LOG_INFO("----  VERIFICANDO RECONEXION DOMOTICZ  ----");
  bool DomoticzOK = getDiaNoche(); //enviamos mandato a Domoticz para comprobar que hay conexion
  tic_parpadeoLedRecon.detach();
  ledPWM(LEDB,OFF);
  if(!DomoticzOK) { LOG_ERROR(" ** sin conexion con Domoticz"); return false; }
  Estado.estado = STANDBY; //borramos estado ERROR
  Estado.error = NOERROR; //reseteamos error
  recoverableError = false; //reseteamos error recuperable
  return true;
}

bool getDiaNoche()
{
  String response = cmdtoSCD(GETSUNHOURS); //lee info amanecer/anochecer del Domoticz
  LOG_DEBUG("Respuesta recibida del Domoticz: ", response.c_str());
  if (response.startsWith("Err")) return false;
  JsonDocument jsondoc;
  DeserializationError error = deserializeJson(jsondoc, response);
  if (error) return false; //error de deserializacion
  strlcpy(amanecer, jsondoc["CivTwilightStart"] | "NO TIME", sizeof(amanecer));
  strlcpy(anochecer, jsondoc["CivTwilightEnd"] | "NO TIME", sizeof(anochecer));
  LOG_DEBUG("amanece ", amanecer, "anochece ", anochecer);
  return true;
}


void domoticzVerifyRecovery()
{  //verificamos que hay wifi y el Domoticz esta conectado, solo en este caso reintentamos leer factores de riego
  LOG_TRACE("");
  lcd.displayON(); //por si estuviera parpadeando(apagado) por error en pantalla
  if(connected) {
    if (checkDomoticz()) {
      LOG_INFO("Domoticz conectado OK");
      initFactorRiegos(); //en caso de producirse error con esta funcion ya dejara este activado
      setupEstado();
    }
  } else {
    lcd.clear(BORRA1H);
    statusError(E1, RECUPERABLE); //error de conexion recuperable
    }
  if(recoverableError) LOG_INFO("reintento en ",RECONNECTINTERVAL," minutos \n");
}

/**---------------------------------------------------------------
 * lee datos de temperatura y humedad del sensor definido en Domoticz
 */
float getTemperatureDomoticz(uint16_t idx)
{
  LOG_TRACE("sensor temp IDX: ", idx);
  // si el IDX es 0 devolvemos 999 sin procesarlo (sensor no asignado)
  if(idx == 0) return 999;
  if(!checkWifi()) return 999; //si no hay conexion devolvemos 999 y no damos error
  String response = deviceInfo(idx, (char *)"Data");  //campo Data devuelve temperatura como caracteres (ej. "9.4 C")
  //String response = deviceInfo(idx, (char *)"Temp");  //campo Temp devuelve temperatura como numero (ej. 9.4)
  LOG_INFO("Temperatura recibida del Domoticz: ", response);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    LOG_WARN("getTemperatureDomoticz IDX: ", idx, " respuesta recibida: ", response.c_str());
    return 999;  //devolvemos 999 para indicar temperatura no valida
  }
  //devolvemos la temperatura del sensor en Domoticz del json (campo Data)
  float temp = strtof(response.c_str(), NULL); //strtof convierte a float (ej. 9.4 C -> 9.4)
  LOG_INFO("devuelve temperatura = ",temp);
  return temp;
}

/**---------------------------------------------------------------
 * verifica status de la zona coincide con el pasado, devolviendo true en ese caso
 */
bool queryStatus(uint16_t idx, char *status)
{
  LOG_DEBUG("idx:", idx, "status:", status, "allSimFlags:", simular.all_simFlags);

  if(simular.ErrorVerifyON) {   // simulamos EV no esta ON en Domoticz
    if(strcmp(status, "On") == 0) return false; else return true; 
  } 
  if(simular.ErrorVerifyOFF) {   // simulamos EV no esta OFF en Domoticz
    if(strcmp(status, "Off") == 0) return false; else return true; 
  } 
  if(!connected) {
    if(modoDEMO) return true; //si estamos en modoDEMO devolvemos true y no damos error
    else {
      Estado.error=E1;
      return false;
    }
  }
  String response = deviceInfo(idx, (char *)"Status");
  LOG_DEBUG("response:", response);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    if (modoDEMO) return true;  //si estamos en modoDEMO devolvemos true y no damos error
    if(response == "Err2") Estado.error=E2;
    else Estado.error=E3;
    LOG_WARN("queryStatus devuelve FALSE, error ",response.c_str());
    return false;
  }
  #ifdef EXTRADEBUG
    Serial.printf( "queryStatus verificando, status=%s / actual=%s \n" , status, response);
    Serial.printf( "                status_size=%d / actual_size=%d \n" , strlen(status), response.length());
  #endif
  if(strcmp(response.c_str(), status) == 0) return true; //si coinciden devolvemos true
  else{
    if(modoDEMO) return true; //siempre devolvemos ok en modo simulacion
    LOG_WARN("queryStatus devuelve FALSE, status / actual =",status,"/",response.c_str());
    return false;
  }  
} //fin queryStatus

/**---------------------------------------------------------------
 * Envia a domoticz orden de on/off del idx correspondiente
 */
bool domoticzSwitch(int idx, char *msg, int retries)
{
  LOG_TRACE("idx:", idx, " ", msg, "(", retries, "intentos)");
  if(idx == 0) return true; //simulamos que ha ido OK
  if(!connected && !modoDEMO) {
    statusError(E1);
    return false;
  }
  char message[150];
  sprintf(message,COMMANDPRF SWITCHDEVICE,idx,msg);
  String response;
  for(int i=0; i<retries; i++) {
     if ((simular.ErrorON && strcmp(msg,"On")==0) || (simular.ErrorOFF && strcmp(msg,"Off")==0)) response = "ErrX"; // simulamos el error
     else if(!modoDEMO) response = httpGetDomoticz(message); // enviamos orden al Domoticz
     if(response == "ErrX") { // solo reintentamos si Domoticz informa del estado de la zona
       sonido.bip(1);
       LOG_WARN("DOMOTICZSWITH IDX:", idx, "fallo en", msg, "(intento", i+1, "de", retries, ")");
       delay(DELAYRETRY);
     }
     else break;
  }   
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
      if(response == "ErrX") {
        if(strcmp(msg,"On") == 0 ) statusError(E4); //error al iniciar riego
        else statusError(E5); //error al parar riego
      }
      else statusError(E2); //otro error al comunicar con domoticz
    }
    LOG_ERROR("DOMOTICZSWITH IDX:", idx, "fallo en", msg);
    return false;
  }
  return true;
} //fin domoticzSwitch


void flagVerificaciones() 
{
  flagVtimer = ON; //aqui solo activamos flagVtimer para no usar llamadas a funciones bloqueantes en Ticker
}


/**---------------------------------------------------------------
 * activacion del flagV para verificaciones periodicas de estado (wifi, hora correcta, ...) si se ha cumplido el timer 
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
  flagV = OFF;
  checkReconInterval = false;
  if (!flagVtimer) return;  //si no activada por Ticker salimos sin hacer nada
  if (Estado.error) LOG_TRACE("-------flagVtimer ON----   recoverableError: ", recoverableError, "Estado.error: ", Estado.error);
  flagVtimer = OFF;
  flagV = ON;  //activamos flagV para que se realicen las verificaciones en las funciones de estado correspondientes
  if(millis() > lastmillisReconnect + RECONNECTINTERVAL * 60000) {   
    lastmillisReconnect = millis();
    checkReconInterval = true; //activamos el flag para que se realicen las verificaciones de reconexion
  }    

  /*
     Con flagV activado, se realizan las siguientes verificaciones periodicas:
      - estado de la wifi y recuperacion de la conexion si no la hay (en procesaEstadoStandby y procesaEstadoError)
      - actualiza y muestra nivel señal wifi si procede (en procesaEstadoStandby)
      - actualizacion de hora por NTP si no se hubiera hecho ya (en procesaEstadoStandby)
      - actualiza y muestra temperatura ambiente (en procesaEstadoStandby)
      - recordatorio error grave al parar un riego (en procesaEstadoError)
      - si VERIFY=true, verifica que el estado de la zona en RIEGO coincide con el de Domoticz (en procesaEstadoRegando)
      - si VERIFY=true, verifica que el estado de la zona en PAUSA coincide con el de Domoticz (en procesaEstadoPause)
      Con checkReconInterval activado, se realizan las siguientes verificaciones periodicas:
       - intento de recuperacion de la conexion wifi si no la hay (en procesaEstadoError)
       - intento de recuperacion de la conexion con Domoticz (en procesaEstadoError)
  */
}

float readTemp() {
    float temperatura;
    float humedad;
    tempOK=false;
    if(config.tempRemote) {
      temperatura = getTemperatureDomoticz(config.tempRemoteIdx);
    }
    else {
      #ifdef TEMPLOCAL   // temperatura ambiente del sensor local
        temperatura = dht.readTemperature();
        //humedad = dht.readHumidity();
        //float temp_sense = dht.computeHeatIndex(false); // false para calculo en grados centigrados
        if(isnan(temperatura)) temperatura = 999;
      #endif
    }
    temperatura == 999 ? tempOK=false : tempOK=true;
    return temperatura;  
}

void showTemp() {
    float temperatura = readTemp();
    LOG_TRACE("tempOK=",tempOK,"temperatura=",temperatura);
    if(tempOK) {
      temperatura = temperatura + ((float)config.tempOffset*(TEMP_OFFSET_FACTOR/100.0)); // offset correccion
      LOG_TRACE("temp OFFSET=",config.tempOffset,"TEMP_OFFSET_FACTOR %=",TEMP_OFFSET_FACTOR,"temperatura corregida=",temperatura);
      int temp_round = (temperatura < 0 ? (temperatura - 0.5) : (temperatura + 0.5)); //redondeo al entero mas cercano
      lcd.displayTemp(temp_round, config.warnESP32temp);
    }  
    else {
      LOG_ERROR("Read temperature sensor failed");
      lcd.displayTemp(999, config.warnESP32temp);  // borra temperatura del display 
    }
}

void displayDemo() {
    lcd.setCursor(0,2);
    lcd.print("(DEMO)"); 
}

void displayNoFactorizado() {
    lcd.setCursor(0,3);
    lcd.print(" -NF-"); 
}

void displayMultiTemporal() {
    lcd.setCursor(0,3);
    lcd.print("*Mtemp"); 
}

/**---------------------------------------------------------------
 * pasa a estado ERROR
 */
void statusError(uint8_t errorID, bool recoverable) 
{
  recoverableError = recoverable; //error recuperable o no
  Estado.estado = ERROR;
  Estado.error = errorID;
  Estado.tipo = LOCAL;
  rotaryEncoder.disable();
  boton = NULL; //borramos boton pulsado para que no sea tratado más adelante en procesaEstadoError

  if (errorID == E0) sprintf(errorText, "Error0");
  else sprintf(errorText, "Error%d", errorID);
  LOG_ERROR("SET ERROR: ", errorText);
  if (recoverableError) snprintf(buff, MAXBUFF, ">>>  %s  >>> R", errorText);
  else snprintf(buff, MAXBUFF, ">>>  %s  <<<", errorText);
  lcd.clear(BORRA2H);
  lcd.setCursor(2,2);
  lcd.print(buff);
  lcd.setCursor(0,3);
  lcd.print(errorToString(errorID));  // mostramos explicacion del error en pantalla
  actLedError();
  sonido.bipKO();
  if (errorID == E5) sonido.longbip(5); // resaltamos error al parar riego
}

/**
   * @brief Converts a level (enum value) to its string.
   * @param level Valid enum level element
   * @return error long text as a string
   */
  static const char* errorToString(uint8_t fase)
  {
      switch (fase)
      {
          case E0:      return "error en parametros";
          case E1:      return "sin conex. wifi";
          case E2:      return "sin conex. domoticz";
          case E3:      return "en factores riego";
          case E4:      return "al iniciar riego";
          case E5:      return "al parar riego";
          default:      return "[unknown error]";
      }
  }

void setupParm()
{
  LOG_TRACE("");
  if(clean_FS) cleanFS();
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    LOG_ERROR("An Error has occurred while mounting LittleFS");
    lcd.infoclear("No se ha podido montar el sistema de ficheros",1,BIPKO);
    delay(config.msgdisplaymillis*3);
    return;
  }
  #ifdef DEVELOP
    Serial.printf( "\n initParm= %d \n", initFlags.initParm );
    filesInfo();
  #endif
  if( initFlags.initParm) {
    LOG_WARN(">>>>>>>>>>>>>>  borrando ficheros de parámetros y riegos  <<<<<<<<<<<<<<");
    bool bRC = deleteParmFiles();
    if(bRC) {
      LOG_WARN("borrado ficheros de parámetros y riegos OK");
      lcd.infoclear("RESET/ERASE parm OK",1,BIPOK); //señala el borrado ficheros de parámetros OK
      delay(config.msgdisplaymillis);
    }  
    else LOG_ERROR(" **  [ERROR] en borrado ficheros de parámetros");
  }
  if (!loadConfigFile(parmFile, config)) {
    LOG_ERROR(" ** [ERROR] Leyendo fichero parametros " , parmFile);
    if (loadConfigFile(backupParmFile, config)) {lcd.infoclear("BACKUP parm loaded");delay(MSGDISPLAYMILLIS*3);}
    else LOG_ERROR(" ** [ERROR] Leyendo fichero parametros backup ", backupParmFile);
  }
  if (!config.initialized) zeroConfig(config);  //init config con zero-config
  else VERIFY = config.verify;

  setupConfig(); //una vez cargados parametros, completa campos de config y boton

  #ifdef VERBOSE
    if (config.initialized) Serial.print(F("\nParametros cargados, "));
    else Serial.print(F("Parametros zero-config, "));
    printParms(config);
  #endif
} //fin setupParm

//Completa campos de config y boton
void setupConfig() 
{
  //init grupo temporal n+1  
  config.group[NUMGRUPOS].bID = 0;     // id del boton de grupo ficticio
  config.group[NUMGRUPOS].size = 0;
  sprintf(config.group[NUMGRUPOS].desc, "TEMPORAL"); 
  LOG_TRACE("Init grupo temporal (GRUPO", NUMGRUPOS+1,")");

  //init campo zNumber de Boton[]
  setzNumber();
  //init campo bID de grupos en config
  setbIDgrupos(config);
  
  tm.minutes = config.minutes;
  tm.seconds = config.seconds;
  tmvalue();
  
  for(int i=0;i<NUMZONAS;i++) {
    //si en config campo desc de la zona esta vacio se copia el de por defecto de la estructura Boton:
    if(strlen(config.zona[i].desc) == 0) {
      strlcpy(config.zona[i].desc, Boton[zNumber2bIndex(i+1)].desc, sizeof(config.zona[i].desc));
    }  
  }
  for(int i=0;i<NUMGRUPOS;i++) {
    //si en config campo desc del grupo esta vacio se copia el de por defectode la estructura Boton:
    if(strlen(config.group[i].desc) == 0) {
      strlcpy(config.group[i].desc, Boton[bID2bIndex(GRUPOS[i])].desc, sizeof(config.group[i].desc));
    }  
  }
  #ifdef MUTE
    config.mute = true;   // arranque con sonidos silenciados
  #endif
} //fin setupConfig

void resetESP32() {
    LOG_WARN("REINICIANDO ESP32...");
    sonido.lowbip(1);
    lcd.infoclear(">>  REINICIANDO  <<", 3);
    delay(config.msgdisplaymillis);
    ESP.restart();  // reset ESP32
}

// convierte timestamp a fecha hora
String TS2Date(time_t t)
{
char buff[32];
sprintf(buff, "%02d-%02d-%02d %02d:%02d:%02d", day(t), month(t), year(t), hour(t), minute(t), second(t));
return buff;
}

// convierte timestamp a hora
String TS2Hour(time_t t)
{
char buff[32];
sprintf(buff, "%02d:%02d:%02d", hour(t), minute(t), second(t));
return buff;
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
    Serial.println("\n \t SERIAL available"); //Give feedback indicating mode
    return true;
  }
  return false;
}    

// **************************************************************************
// Atajos Stop+Enc+Grupo_n
// **************************************************************************

// Shortcut Webserver
void scWebserver() {
    if(connected) {
      setEstado(CONFIGURANDO);
      setupWS(config);
    }  
    else BIPKO; //no es posible
}

// Shortcut Eastern Egg
void scSorpresa() {
    if (getDiaNoche()) {
        lcd.infoclear("Hoy amanece a las..", 1);
        lcd.setCursor(7, 1);lcd.print(amanecer);
        lcd.info("..y anochece a las", 3);
        lcd.setCursor(7, 3);lcd.print(anochecer);
    }
    else lcd.infoclear("    EASTER EGG!", 2);
    enciendeLeds();
    sonido.bipTarari();
    delay(config.msgdisplaymillis);
    apagaLeds();
    setEstado(STOP);
}

// **************************************************************************
// funciones solo usadas en DEVELOP
// (es igual, el compilador no las incluye si no son llamadas)
// **************************************************************************
#ifdef DEVELOP
  //imprime contenido actual de la estructura multi
  void printMulti()
  {
      Serial.println(F("TRACE: in printMulti"));
      if(multi.id == NULL) return;  // evita guru meditation si no se ha apuntado a ningun grupo
      Serial.printf("MULTI Boton_id x%04x: size=%d (%s)\n", *multi.id, *multi.size, multi.desc);
      for(int j = 0; j < *multi.size; j++) {
        Serial.printf("  Zona  id: x%04x \n", multi.serie[j]);
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
      if (!inputNumber) {
          Serial.println(F("Teclee: "));
          Serial.println(F("   1 - simular error NTP"));
          Serial.println(F("   2 - simular error apagar riego"));
          Serial.println(F("   3 - simular error encender riego"));
          Serial.println(F("   4 - simular EV no esta ON en Domoticz"));
          Serial.println(F("   5 - simular EV no esta OFF en Domoticz"));
          Serial.println(F("   6 - simular error al salir del PAUSE"));
          Serial.println(F("   9 - anular simulacion errores"));
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
            case 9:
                Serial.println(F("recibido:   9 - anular simulacion errores"));
                timeOK = true;                         
                simular.all_simFlags = false;
      }
    }
  }

  void debugloops()
  {
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
