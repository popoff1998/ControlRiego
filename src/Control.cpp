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
  #ifdef RELEASE
                NONETWORK=false; 
  #endif
  #ifdef DEVELOP
                NONETWORK=true;
  #endif
  #ifdef noWIFI
                NONETWORK=true;
                NOWIFI=true;
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
  #ifdef EXTRADEBUG
   printFile(parmFile);
  #endif
  //setup parametros configuracion
  setupParm();
  //Chequeo de perifericos de salida (leds, display, buzzer)
  check();
  //Para la red
  delay(1000);
  setupRedWM(config, initFlags);
  if (saveConfig) {
    if (saveConfigFile(parmFile, config))  bipOK();
    else bipKO();
    saveConfig = false;
  }
  delay(2000);
  //Ponemos en hora
  timeClient.begin();
  delay(500);
  setClock();
  //Inicializamos lastRiegos y lastGrupos (registro fecha/hora y riego realizado)
  initLastRiegos();
  initLastGrupos();
  //Cargamos factorRiegos
  initFactorRiegos();
  //Estado final en funcion de la conexion
  setupEstado();
  if(Estado.estado==STANDBY) bipOK();
  //Llamo a parseInputs CLEAR para eliminar prepulsaciones antes del bucle loop
  parseInputs(CLEAR);
  //lanzamos supervision periodica estado cada VERIFY_INTERVAL seg.
  tic_verificaciones.attach(VERIFY_INTERVAL, flagVerificaciones);
  standbyTime = millis();
  PRINTLN("   *** Setup finalizado ***");
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
  //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multirriego
  if (multi.semaforo) multi.semaforo = false;  // si multisemaforo, no leemos botones: ya los pasa multirriego
  else  boton = parseInputs(READ);  // si no, vemos si algun boton ha cambiado de estado
  // si no se ha pulsado ningun boton salimos
  if(boton == NULL) return;
  //Si estamos en reposo pulsar cualquier boton solo nos saca de ese estado
  // (salvo STOP que si actua y se procesa mas adelante)
  if (reposo && boton->bID != bSTOP) {
    reposoOFF();
    return;
  }
  //En estado error no actuan los botones
  // (salvo PAUSE y STOP que se procesan en procesaEstadoError)
  if(Estado.estado == ERROR) return;
  //a partir de aqui procesamos solo los botones con flag ACTION
  if (!boton->flags.action) return;
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


/**---------------------------------------------------------------
 * Proceso en funcion del estado
 */
void procesaEstados()
{
  switch (Estado.estado) {
    case CONFIGURANDO:
      procesaEstadoConfigurando();
      break;
    case ERROR:
      procesaEstadoError();
      if(Estado.estado == ERROR) blinkPause();
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
      procesaEstadoPause();
      blinkPause();
      break;
  }
}


/**---------------------------------------------------------------
 * Estado final al acabar Setup en funcion de la conexion a la red
 */
void setupEstado() 
{
  LOG_TRACE("");
  Estado.tipo = LOCAL;
  //Deshabilitamos el hold de Pause
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = true;
  // Verificamos que se han cargado parametros de configuracion correctamente  
  if(!config.initialized) {
    statusError(E0);  //no se ha podido cargar parámetros desde ficheros -> señalamos el error
  return;
  }
  // Si estamos en modo NONETWORK pasamos a STANDBY (o STOP si esta pulsado) aunque no exista conexión wifi o estemos en ERROR
  if (NONETWORK) {
    if (testButton(bSTOP,ON))  setEstado(STOP,1);
    else setEstado(STANDBY,2);
    return;
  }
  // Si estado actual es ERROR seguimos así
  if (Estado.estado == ERROR) {
    return;
  }
  // Si estamos conectados pasamos a STANDBY o STOP (caso de estar pulsado este al inicio)
  if (checkWifi()) {
    if (testButton(bSTOP,ON))  setEstado(STOP,1);
    else setEstado(STANDBY,1);
    return;
  }
  //si no estamos conectados a la red y no estamos en modo NONETWORK pasamos a estado ERROR
  statusError(E1);
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
  if (Estado.estado != STOP) {
    //No procesamos los release del boton salvo en STOP
    if(!boton->estado) return;
    switch (Estado.estado) {
      case REGANDO:
        if(encoderSW) {  //si pulsamos junto con encoderSW terminamos el riego (pasaria al siguiente en caso de multirriego)
          setEstado(TERMINANDO);
          LOG_INFO("encoderSW+PAUSE terminamos riego de zona en curso");
        }
        else {    //pausa el riego en curso
          setEstado(PAUSE,1);
          tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
          led(ultimoBotonZona->led,ON);// y lo deja fijo
          stopRiego(ultimoBotonZona->bID);
          T.PauseTimer();
        }
        break;
      case PAUSE:
        if(simular.ErrorPause) statusError(E2); //simulamos error al salir del PAUSE
        else initRiego();        // reanudamos riego que estaba parado 
        if(Estado.estado == ERROR) { // caso de error al reanudar el riego seguimos en PAUSE y señalamos con blink rapido zona
          ledID = ultimoBotonZona->led;
          tic_parpadeoLedZona.attach(0.2, parpadeoLedZona, ledID);
          LOG_WARN("error al salir de PAUSE errorText :",errorText,"Estado.error :",Estado.error );
          refreshTime();
          setEstado(PAUSE,1);
          break;
        }
        bip(2);
        T.ResumeTimer();
        tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
        led(ultimoBotonZona->led,ON);// y lo deja fijo
        lcd.clear(BORRA2H); //por si hubiera msgs de error (caso error al salir del pause previo)
        setEstado(REGANDO);
        break;
      case STANDBY:
          boton = NULL; //lo borramos para que no sea tratado más adelante en procesaEstados
          if(encoderSW) {  //si encoderSW+Pause --> conmutamos estado NONETWORK
            if (NONETWORK) {
                NONETWORK = false;
                NOWIFI = false;
                LOG_INFO("encoderSW+PAUSE pasamos a modo NORMAL y leemos factor riegos");
                bip(2);
                ledPWM(LEDB,OFF);
                lcd.clear();
                if (!checkWifi()) wifiReconnect();
                initFactorRiegos();
                setupEstado();
                if(VERIFY && Estado.estado != ERROR) stopAllRiego(); //verificamos operativa OFF para los IDX's 
            }
            else {
                NONETWORK = true;
                LOG_INFO("encoderSW+PAUSE pasamos a modo NONETWORK (DEMO)");
                bip(2);
                ledPWM(LEDB,ON);
                displayDemo();
            }
          }
          else {    // muestra hora y ultimos riegos
            ultimosRiegos(SHOW);
            delay(config.msgdisplaymillis*3);
            ultimosRiegos(HIDE);
            LOG_TRACE("[poniendo estado STANDBY]");
            setEstado(STANDBY);  // para restaurar pantalla
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
            setEstado(CONFIGURANDO,1);
            configure->menu();
            LOG_INFO("Stop + hold PAUSA --> modo ConF()");
          }
          else {   //si esta pulsado encoderSW hacemos un soft reset
            LOG_WARN("Stop + encoderSW + PAUSA --> Reset.....");
            lcd.infoclear(">>  REINICIANDO  <<", NOBLINK, LOWBIP, 1);
            delay(config.msgdisplaymillis);
            ESP.restart();  // reset ESP32
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
    if (Estado.estado == REGANDO || Estado.estado == PAUSE) {
      //De alguna manera esta regando y hay que parar
      lcd.infoclear("Parando riegos", 1, BIP, 6);
      T.StopTimer();
      // paramos riego en curso y todas las zonas
      if (!stopRiego(ultimoBotonZona->bID) || !stopAllRiego()) {   //error al parar riegos
        boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
        return; 
      }
      lcd.infoclear("STOP riegos OK", DEFAULTBLINK, BIP, 0);
      setEstado(STOP,1);
      resetFlags();
    }
    if (Estado.estado == STANDBY) { //Lo hemos pulsado en standby
      if (encoderSW) {  // activar configuracion de grupo multirriego temporal
          setMultibyId(0, config);  // apunta estructura multi a grupo temporal en config (n+1) con id = 0
          setEstado(CONFIGURANDO,1);
          boton = NULL;
          configure->MultiTemp_process_start();
          return;
      }
      else {      // seguro antinenes
          // apagar leds y parar riegos (por si riego activado externamente)
          reposoOFF();
          lcd.infoclear("Parando riegos", 1, BIP, 6);
          if (!stopAllRiego()) {   //error al parar riegos
            boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
            return; 
          }
          lcd.infoclear("STOP riegos OK", DEFAULTBLINK, BIP, 0);
          setEstado(STOP,1);
          reposoON(LCDON); //pasamos directamente a reposo sin apagar pantalla
      }    
    }
  }
  //si hemos liberado STOP: salimos del estado stop
  //(dejamos el release del STOP en modo ConF para que actue el codigo de procesaEstadoConfigurando
  if (!boton->estado && Estado.estado == STOP) {
    LOG_TRACE("[poniendo estado STANDBY]");
    setEstado(STANDBY);
  }
}


void procesaBotonMultiriego(void)
{
  if (Estado.estado == STANDBY && !multi.riegoON) {
    int n_grupo;
    #ifdef GRP4
      n_grupo = setMultibyId(boton->bID, config);
    #endif
    #ifdef M3GRP
      n_grupo = setMultibyId(getMultiStatus(), config);
    #endif
    if (n_grupo == 0) return; //error en setup de apuntadores 
    LOG_DEBUG("en MULTIRRIEGO, setMultibyId devuelve: Grupo", n_grupo,"(",multi.desc,") multi.size=" , *multi.size);
    for (int k=0; k < *multi.size; k++) LOG_DEBUG( "       multi.serie: x" ,DebugLogBase::HEX, multi.serie[k]);
    for (int k=0; k < *multi.size; k++) LOG_DEBUG( "       multi.zserie: x" , multi.zserie[k]);
    LOG_DEBUG("en MULTIRRIEGO, encoderSW status  :", encoderSW );
    // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
    // y mostramos en el display las zonas que componen el grupo y fecha ultimo riego de este
    if (encoderSW) {
      LOG_DEBUG("en MULTIRRIEGO + encoderSW, display de grupo:", multi.desc,"tamaño:", *multi.size );
      snprintf(buff, MAXBUFF, "grupo: %s", multi.desc);
      lcd.infoclear(buff, 1);
      displayLCDGrupo(FULL, 2);
      showTimeLastRiego(lastGrupos[n_grupo-1], n_grupo-1);
      displayGrupo(multi.serie, *multi.size);
      delay(config.msgdisplaymillis*3);
      setEstado(STANDBY);   //para que restaure pantalla
    }  
    else {
      //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
      //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
      if(setMultirriego(config)) inicioTimeLastRiego(lastGrupos[n_grupo-1], n_grupo-1);
    }
  }
}


void procesaBotonZona(void)
{
  int zIndex = boton->znumber-1;
  if (zIndex < 0) return; //el boton no es de ZONA o error en la matriz Boton[]
  if (Estado.estado == STANDBY) {
    if (!encoderSW || multi.riegoON) {  //iniciamos el riego correspondiente al boton seleccionado
        bip(2);
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
        T.SetTimer(0,fminutes,fseconds);
        T.StartTimer();
        initRiego();
        if(Estado.estado != ERROR) setEstado(REGANDO); // para que no borre ERROR
    }
    else {  // mostramos en el display el factor de riego del boton pulsado y fecha ultimo riego
      led(boton->led,ON);
      #ifdef EXTRADEBUG
        Serial.printf("Boton: %s Factor de riego: %d \n", config.zona[boton->znumber-1].desc,factorRiegos[zIndex]);
        Serial.printf("          boton.led: %d \n",boton->led);
      #endif
      lcd.infoclear("factor riego de ");
      snprintf(buff, MAXBUFF, "%s :  %d", config.zona[boton->znumber-1].desc, factorRiegos[zIndex]);
      lcd.info(buff,2);
      showTimeLastRiego(lastRiegos[zIndex], zIndex);
      delay(config.msgdisplaymillis*4);
      led(boton->led,OFF);
      //value = savedValue;  // para que restaure reloj
      LOG_TRACE("[poniendo estado STANDBY]");
      setEstado(STANDBY);
    }
    return;
  }
  // Si config.dynamic=true se permite añadir/eliminar zonas durante un riego individual 
  // o multirriego temporal (no durante un multirriego de grupo normal).
  // TODO PREGUNTA: sería conveniente que solo se pudiese hacer una vez pausado?
  // TODO PREGUNTA: permitir eliminar (terminar riego) de la zona en curso solamente pulsando esa zona ?
  //                (ya se puede hacer de forma general con encoderSW+PAUSE) 
  if ((Estado.estado == REGANDO || Estado.estado==PAUSE) && config.dynamic && (multi.riegoON == multi.temporal)) {
    // NOTA: la zona pulsada no puede coincidir con la actualmente en riego, se ignora en ese caso
    if (ultimoBotonZona->bID != boton->bID) {
      // procesar cambio dinamico y reflejarlo en el display
      if (procesaDynamic()) displayLCDGrupo(RESTO, 2);
      LOG_DEBUG("MULTI dynamic:",multi.dynamic,"actual:",multi.actual,"size:",*multi.size,"zona:",boton->znumber);
    }
    else {bipKO(); LOG_DEBUG("[DYNAMIC] zona pulsada:",boton->znumber," es = a zona actual:",ultimoBotonZona->znumber);}
    // else { setEstado(TERMINANDO); LOG_INFO("DYNAMIC: terminamos riego de zona en curso"); }
    boton = NULL; // borrar boton pulsado
  }
}


void procesaEstadoConfigurando()
{
  if (boton != NULL) {
    if (boton->flags.action) {
      if (boton->bID != bSTOP && webServerAct) return; //si webserver esta activo solo procesamos boton STOP

      switch(boton->bID) {
        case MULTIRRIEGO:
            if (configure->statusMenu() && configure->get_currentItem()==0) { //si no estamos configurando nada
              int n_grupo;
              #ifdef GRP4
                n_grupo = setMultibyId(boton->bID, config);
              #endif
              #ifdef M3GRP
                n_grupo = setMultibyId(getMultiStatus(), config);
              #endif
              if (n_grupo == 0) return; //error en setup de apuntadores 
              //Configuramos el grupo de multirriego apuntado en multi
              rotaryEncoder.disable();
              configure->Multi_process_start(n_grupo);
            }  
            break;
        case bPAUSE:
            if(!boton->estado) break; //no se procesa el release del PAUSE
            LOG_DEBUG("[MENU] PAUSE pulsado recibido");
            if(configure->statusMenu()) { //estamos en el menu -> procesamos la seleccion
              configure->procesaSelectMenu(); 
              break;
            }
            LOG_DEBUG("PAUSE pulsado recibido y estamos configurando algo");
            // si ya estamos configurando algo: PAUSE consolida lo configurado en config
            if(configure->configuringTime()) {
              configure->Time_process_end();  //  salvamos en config el nuevo tiempo por defecto
              break;
            }
            if(configure->configuringIdx()) {
              configure->Idx_process_end();  //  salvamos en config el nuevo IDX
              break;
            }
            if(configure->configuringRange()) {
              configure->Range_process_end();  //  salvamos en config nuevo valor del parametro
              break;
            }
            if(configure->configuringMulti()) {
              configure->Multi_process_end(encoderSW);  // actualizamos config con las zonas introducidas
              break;
            }
            if(configure->configuringMultiTemp()) {
              configure->MultiTemp_process_end();  // preparamos lanzamiento multirriego temporal
              break;
            }
            break;
        case bSTOP:
            if(!boton->estado) {    //release STOP
                if(configure->configuringMultiTemp()) {     
                    if (multi.w_size && saveConfig) {  //solo si se ha guardado alguna zona iniciamos riego grupo temporal
                        saveConfig = false;  
                        multi.temporal = true;
                        setMultirriego(config); 
                    }    
                }
                VERIFY = config.verify;
                configure->exit();  // salvamos parametros a fichero si procede y salimos de ConF
            }
            break;
        default:  //procesamos boton de ZONAx
            if (configure->configuringMulti() || configure->configuringMultiTemp()) {  //añadimos zona al multirriego que estamos definiendo
              configure->Multi_process_update();
            }
            if (configure->statusMenu() && configure->get_currentItem()==0) {   //si no estamos configurando nada
              configure->Idx_process_start(bID2bIndex(boton->bID));             // configuramos el idx del boton
            }
      }
    }
  }
  else { 
      webServerAct ? procesaWebServer() : procesaEncoderConfig();
  }
};


void procesaEstadoError(void)
{
  if(boton == NULL) return;  // si no se ha pulsado ningun boton salimos
  //En estado error no se responde a botones, a menos que este sea:
  //   - PAUSE y pasamos a modo NONETWORK
  //   - STOP y en este caso reseteamos 
  if(boton->bID == bPAUSE && boton->estado) {  //evita procesar el release del pause
    LOG_INFO("estado en ERROR y PAUSA pulsada pasamos a modo NONETWORK y reset del error");
    NONETWORK = true;
    bip(2);
    if (Boton[bID2bIndex(bSTOP)].estado) setEstado(STOP,1);
    else setEstado(STANDBY);
    //reseteos varios:
    resetLeds();    //apaga leds activos y restablece leds ON y RED
    resetFlags();   //reset flags de status
  }
  if(boton->bID == bSTOP) {
  //Si estamos en ERROR y pulsamos o liberamos STOP, reseteamos
    lcd.infoclear("ERROR+STOP-> Reset..",3);
    LOG_WARN("ERROR + STOP --> Reset.....");
    lowbip(1);
    //if(checkWifi() && boton->estado) stopAllRiego(); //si es pulsado STOP intentamos parar riegos
    delay(3000);
    ESP.restart();  
  }
};


void procesaEstadoRegando(void)
{
  int tiempoTerminado = T.Timer();
  if (T.TimeHasChanged()) refreshTime();
  if (tiempoTerminado == 0) setEstado(TERMINANDO);
  // verificamos periodicamente que el riego sigue activo en Domoticz
  else if(flagV && VERIFY && (!NONETWORK || simular.all_simFlags)) { 
    ledID = ultimoBotonZona->led;
    tic_parpadeoLedZona.detach(); //detiene posible parpadeo led zona
    led(ledID,ON);                //y lo dejamos fijo
    if(queryStatus(config.zona[ultimoBotonZona->znumber-1].idx, (char *)"On")) return;
    else {
      if(Estado.error == NOERROR) { //riego zona parado: entramos en PAUSE y blink lento zona pausada remotamente 
        T.PauseTimer();
        tic_parpadeoLedZona.attach(0.8, parpadeoLedZona, ledID);
        LOG_WARN(">>>>>>>>>> procesaEstadoRegando zona:", config.zona[ultimoBotonZona->znumber-1].desc, "en PAUSA remota <<<<<<<<");
        Estado.tipo = REMOTO;
        setEstado(PAUSE,1);
      }
      else {  // si no hemos podido verificar estado, señalamos zona blink rapido y continuamos
        tic_parpadeoLedZona.attach(0.4, parpadeoLedZona, ledID);
        Estado.error = NOERROR; // si no hemos podido verificar estado, ignoramos el error
        bip(2);
        LOG_ERROR("** SE HA DEVUELTO ERROR al verificar estado riego");
      }  
    }
  }
};


void procesaEstadoTerminando(void)
{
  bip(5);
  tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
  stopRiego(ultimoBotonZona->bID);
  if (Estado.estado == ERROR) return; //no continuamos si se ha producido error al parar el riego
  lcd.blinkLCD(DEFAULTBLINK);
  led(ultimoBotonZona->led,OFF);  // apaga led zona
  //Comprobamos si estamos en un multirriego
  if (multi.riegoON) {
    multi.actual++;
    if (multi.actual < *multi.size) {  // pasamos a regar la siguiente zona del grupo
      //Simular la pulsacion del siguiente boton de la serie de multirriego
      boton = &Boton[bID2bIndex(multi.serie[multi.actual])];
      multi.semaforo = true;
      //muestra en pantalla las zonas que restan por regar del grupo (excluida la zona en curso):
      displayLCDGrupo(RESTO, 2);  //  display zonas quedan por regar
    }
    else {         // señalamos fin del multirriego y actualizamos timestamp de finalizacion
      if(!multi.temporal) finalTimeLastRiego(lastGrupos[multi.ngrupo-1], multi.ngrupo-1);
      lcd.info("multirriego",1);
      int msgl = snprintf(buff, MAXBUFF, "%s finalizado", multi.desc);
      lcd.info(buff, 2, msgl);
      resetFlags();
      bipFIN();
      LOG_INFO("MULTIRRIEGO", multi.desc, "terminado");
      delay(config.msgdisplaymillis*3);
      led(Boton[bID2bIndex(*multi.id)].led,OFF);  // apaga led grupo
    }
  }
  LOG_TRACE("[poniendo estado STANDBY]");
  setEstado(STANDBY);
};


void procesaEstadoStandby(void)
{
  //Apagamos el display si ha pasado el lapso
  if (reposo) standbyTime = millis();
  else {
    if (millis() > standbyTime + (1000 * STANDBYSECS)) {
      LOG_TRACE("LLamando a reposoON");
      reposoON();
    }
  }
  if (reposo & encoderSW) reposoOFF(); // pulsar boton del encoder saca del reposo
  procesaEncoderClock();
};


void procesaEstadoStop(void)
{
  //En stop activamos el comportamiento hold de pausa
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = false;
  //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
  if(reposo && !backlightOff) {
    if (millis() > standbyTime + (4 * 1000 * STANDBYSECS)) {
      lcd.setBacklight(OFF);
      backlightOff = true;
    }
  }
};

void procesaEstadoPause(void) {
  if(flagV && VERIFY && (!NONETWORK || simular.all_simFlags)) {  // verificamos zona sigue OFF en Domoticz periodicamente
    if(queryStatus(config.zona[ultimoBotonZona->znumber-1].idx, (char *)"Off")) return;
    else {
      if(Estado.error == NOERROR) { //riego zona activo: salimos del PAUSE y blink lento zona activada remotamente 
        bip(2);
        ledID = ultimoBotonZona->led;
        tic_parpadeoLedZona.attach(0.8, parpadeoLedZona, ledID);
        LOG_WARN(">>>>>>>>>> procesaEstadoPause zona:", config.zona[ultimoBotonZona->znumber-1].desc,"activada REMOTAMENTE <<<<<<<");
        T.ResumeTimer();
        Estado.tipo = REMOTO;
        setEstado(REGANDO);
      }
      else Estado.error = NOERROR; // si no hemos podido verificar estado, ignoramos el error
    }
  }
}


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
      bip(2); return true; //zona eliminada
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
    bip(1); return true; //zona añadida
  }
  else return false; //no hay sitio --> zona ignorada   
}  


/**---------------------------------------------------------------
 * Pone estado pasado y sus indicadores opcionales
 */
void setEstado(uint8_t estado, int bnum)
{
  LOG_DEBUG( "recibido ", nEstado[estado], "bnum=", bnum);
  // setup y reseteos varios
  Estado.estado = estado;
  Estado.error = NOERROR;
  strcpy(errorText, "");
  //Deshabilitamos el hold de Pause
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = true;
  setledRGB();   // led RGB segun status wifi y nonetwork
  if(reposo) reposoOFF();     //por si salimos de stop antinenes
  rotaryEncoder.disable();  // para que no cuente pasos salvo que lo habilitemos
  lcd.displayON();
  LOG_DEBUG( "Estado.tipo =", Estado.tipo);
  lcd.setCursor(17, 1);
  if (Estado.tipo==REMOTO) lcd.print("(R)");
  else lcd.print("   ");
  Estado.tipo = LOCAL;
  if((estado==REGANDO || estado==TERMINANDO ) && ultimoBotonZona != NULL) {
    lcd.infoEstado(nEstado[estado], config.zona[ultimoBotonZona->znumber-1].desc);
    if(NONETWORK) displayDemo(); 
    if(multi.dynamic) displayNoFactorizado();
      else if(multi.temporal) displayMultiTemporal(); 
    return;
  }
  if(estado==PAUSE) {
    lcd.infoEstado(nEstado[estado], config.zona[ultimoBotonZona->znumber-1].desc); 
    if(bnum) bip(bnum);
    return;
  }
  if(estado == STANDBY) {
    if(!multi.riegoON && !multi.temporal) {
      lcd.infoclear("STANDBY",NOBLINK,BIP,bnum);
      showTemp();
    }  
    StaticTimeUpdate(REFRESH);
    setEncoderTime();
    if(NONETWORK) displayDemo();
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
    boton = NULL;
    holdPause = false;
    return;
  }
}


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
  LOG_TRACE("TRACE: in initFactorRiegos");
  //inicializamos a valor 100 por defecto para caso de error
  for(uint i=0;i<NUMZONAS;i++) {
    factorRiegos[i]=100;
  }
  if(Estado.estado == ERROR || NOWIFI) return; // si estabamos en error (o no wifi) ni lo intentamos
  lcd.info("conectando Domoticz", 2);
  lcd.clear(BORRA2H);
  //leemos factores del Domoticz
  for(uint i=0;i<NUMZONAS;i++) 
  {
    int bIndex = bID2bIndex(ZONAS[i]);
    uint factorR = getFactor(config.zona[i].idx);
    if(factorR == 999) break;     //en modo NONETWORK no continuamos iterando si no hay conexion
    if(Estado.estado == ERROR) {  //al primer error salimos
      if(Estado.error == E3) {    // y señalamos zona que falla si no es error general de conexion
        ledID = Boton[bIndex].led;
        tic_parpadeoLedZona.attach(0.4, parpadeoLedZona, ledID);
      }
      break;
    }
    factorRiegos[i] = factorR;
    LOG_TRACE("zona",i+1,"factor asignado=",factorR);
    if (strlen(descDomoticz)) {
      // si xNAME true, actualizamos en config la DESCRIPCION con la recibida del Domoticz (campo Name)
      if (config.xname) {
        strlcpy(config.zona[i].desc, descDomoticz, sizeof(config.zona[i].desc));
        LOG_INFO("\t descripcion ZONA", i+1, "actualizada en config");
      }
      //si el parm desc estaba vacio actualizamos en todo caso (en config, no en Boton)
      if (config.zona[i].desc[0] == 0) {
        strlcpy(config.zona[i].desc, descDomoticz, sizeof(config.zona[i].desc));
        LOG_INFO("\t descripcion ZONA", i+1, "incluida en config");
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
      Serial.printf("\tfactor ZONA%d: %d (%s) \n", i+1, factorRiegos[i], config.zona[i].desc);
    }
  #endif
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


void setClock()
{
  LOG_TRACE("");
  if (timeClient.update()) {
    NTPlastUpdate = millis();
    setTime(timeClient.getEpochTime());  // set reloj del sistema con el time recibido por NTP
    timeOK = true;
    time_t t = CE.toLocal(now(),&tcr);
    LOG_INFO("\n NTP time recibido OK  (UTC) --> ",timeClient.getFormattedTime(),"  local --> ",TS2Hour(t));
  }  
   else {  // si se recibio al menos una vez anteriormente consideramos valida la hora del sistema
     if (timeOK) LOG_WARN("no se ha recibido time por NTP desde",(millis()-NTPlastUpdate)/60000,"minutos");
     else LOG_WARN(">>> NO TIME SET by NTP <<<");
   }
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
      time_t t;
      utc = timeClient.getEpochTime();
      t = CE.toLocal(utc,&tcr);
      for(uint i=0;i<NUMZONAS;i++) {
        if(lastRiegos[i].inicio > previousMidnight(t)) {
            LOG_DEBUG("[ULTIMOSRIEGOS] zona:", i+1, "time:",lastRiegos[i].inicio);
            led(Boton[bID2bIndex(ZONAS[i])].led,ON);
        }
      }
      lcd.infoclear("Hora actual:");
      if (timeOK) {
        sprintf(buff, " %d", day(t));
        lcd.info(buff,3);
        lcd.info(MESES[month(t)-1],4);
        lcd.displayTime(hour(t),minute(t));
      }
      else lcd.info("   <<< NO TIME >>>",3);
      break;
    case HIDE:
      for(unsigned int i=0;i<NUMZONAS;i++) {
        led(Boton[bID2bIndex(ZONAS[i])].led,OFF);
      }
      break;
  }
}

void inicioTimeLastRiego(S_timeRiego &timeRiego, int index) 
{
  utc = timeClient.getEpochTime();
  time_t t = CE.toLocal(utc,&tcr);
  LOG_DEBUG("actualizo lastriegos inicio zona/grupo ", index+1);
  timeRiego.inicio = t;
  timeRiego.final = 0;
}  

void finalTimeLastRiego(S_timeRiego &timeRiego, int index) 
{
  utc = timeClient.getEpochTime();
  time_t t = CE.toLocal(utc,&tcr);
  LOG_DEBUG("actualizo lastriegos fin zona/grupo ", index+1);
  timeRiego.final = t;
}  

void showTimeLastRiego(S_timeRiego &timeRiego, int index) 
{
  lcd.info("ultimo riego:",3);
  time_t t1=timeRiego.inicio;
  time_t t2=timeRiego.final;
  LOG_DEBUG("Zona/Grupo:", index+1 , "time.inicio", t1, "time.final", t2);
  if (t1) {
    snprintf(buff, MAXBUFF, "%d/%02d %d:%02d (%d:%02d)", day(t1), month(t1), hour(t1), minute(t1), hour(t2), minute(t2));
    lcd.info(buff,4);
  }
  else lcd.info("   > sin datos <",4);
}


// ON/OFF atenuacion LEDG y LEDB
void dimmerLeds(bool status)
{
  if(status) {
    LOG_TRACE("leds atenuados ");
    if(connected) analogWrite(LEDG, config.dimmlevel);
    if(NONETWORK) analogWrite(LEDB, config.dimmlevel);
  }
  else {
    LOG_TRACE("leds brillo normal ");
    if(connected) analogWrite(LEDG, config.maxledlevel);
    if(NONETWORK) analogWrite(LEDB, config.maxledlevel);
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
  for(uint i=0;i<NUMZONAS;i++) {
   lastRiegos[i].inicio = 0;
   lastRiegos[i].final = 0;
  }
}

void initLastGrupos()
{
  for(uint i=0;i<NUMGRUPOS;i++) {
   lastGrupos[i].inicio = 0;
   lastGrupos[i].final = 0;
  }
}

//Inicia/reanuda el riego correspondiente al idx del boton de zona pulsado ultimo
bool initRiego()
{
  int zIndex = ultimoBotonZona->znumber-1;
  if (zIndex < 0) return false; //el boton no es de ZONA o error en la matriz Boton[]
  LOG_DEBUG("Boton:",config.zona[zIndex].desc,"zona:",ultimoBotonZona->znumber,"IDX:",config.zona[zIndex].idx);
  LOG_INFO( "Iniciando riego: ", config.zona[zIndex].desc);
  led(ultimoBotonZona->led,ON);
  inicioTimeLastRiego(lastRiegos[zIndex], zIndex);
      #ifdef EXTRADEBUG
          for(uint i=0;i<NUMZONAS;i++) {
                LOG_DEBUG("[ULTIMOSRIEGOS] inicio zona:", i+1, "time:",lastRiegos[i].inicio);
            }
      #endif
  return domoticzSwitch(config.zona[zIndex].idx, (char *)"On", DEFAULT_SWITCH_RETRIES);
}


//Termina el riego correspondiente al idx del boton (id) pasado
bool stopRiego(uint16_t id, bool update)
{
  int bIndex = bID2bIndex(id);
  int zIndex = Boton[bIndex].znumber-1;
  ledID = Boton[bIndex].led;
  LOG_DEBUG( "Terminando riego: ", config.zona[zIndex].desc);
  domoticzSwitch(config.zona[zIndex].idx, (char *)"Off", DEFAULT_SWITCH_RETRIES);
  if (Estado.estado != ERROR) {
    LOG_INFO( "Terminado OK riego: " , config.zona[zIndex].desc );
    // solo actualizamos hora de fin si no hemos sido llamado desde stopAllRiego :
    if(update) finalTimeLastRiego(lastRiegos[zIndex], zIndex);
        #ifdef EXTRADEBUG
            for(uint i=0;i<NUMZONAS;i++) {
                  LOG_DEBUG("[ULTIMOSRIEGOS] fin zona:", i+1, "time:",lastRiegos[i].final);
              }
        #endif
  }
  else {     //avisa de que no se ha podido terminar un riego
    if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
      errorOFF = true;  // recordatorio error
      tic_parpadeoLedError.attach(0.2,parpadeoLedError);
      tic_parpadeoLedZona.attach(0.4, parpadeoLedZona, ledID);
    } 
    return false;
  }
  return true;
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
  falloAP  = false;
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

void blinkPause()
{
  //LOG_TRACE("");
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

void refreshTime()   // Actualiza la cuenta atrás en pantalla, solo si ha cambiado
{
  unsigned long curMinutes = T.ShowMinutes();
  unsigned long curSeconds = T.ShowSeconds();
  if(prevseconds != curSeconds) lcd.displayTime(curMinutes, curSeconds);
  prevseconds = curSeconds;

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
      LOG_ERROR("ERROR comunicando con Domoticz: ", httpclient.errorToString(httpCode).c_str()); 
    }
    return "Err2";
  }
  //vemos si la respuesta indica status error
  int pos = response.indexOf("\"status\" : \"ERR");
  if(pos != -1) {
    LOG_ERROR(" ** SE HA DEVUELTO ERROR"); 
    return "ErrX";
  }
  httpclient.end();
  return response;
}

/**---------------------------------------------------------------
 * devuelve json con informacion del dispositivo con el idx pasado
 */
String deviceInfo(int idx)
{
  char JSONMSG[200]="/json.htm?type=command&param=getdevices&rid=%d";
  char message[250];
  sprintf(message,JSONMSG,idx);
  return httpGetDomoticz(message);
}

/**---------------------------------------------------------------
 * lee factor de riego del Domoticz, almacenado en campo comentarios
 */
int getFactor(uint16_t idx)
{
  LOG_TRACE("");
  if(idx == 0) return 100; //si el IDX es 0 devolvemos 100 sin procesarlo (boton no asignado)
  factorRiegosOK = false;
  strcpy(descDomoticz, "");
  if(!checkWifi()) {
    if(NONETWORK) return 999; //si estamos en modo NONETWORK devolvemos 999 y no damos error
    else {
      statusError(E1);
      return 100;
    }
  }
  String response = deviceInfo(idx);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    if (NONETWORK) {  //si estamos en modo NONETWORK devolvemos 999 y no damos error
      LOG_TRACE("[poniendo estado STANDBY]");
      setEstado(STANDBY);
      return 999;
    }
    if(response == "ErrX") statusError(E3);
    else statusError(E2);
    LOG_WARN("GETFACTOR IDX: ", idx, " [HTTP] GET... failed\n");
    return 100;
  }
  /* Teoricamente ya tenemos en response el JSON, lo procesamos
     Si el IDX no existe Domoticz no devuelve error, asi que hay que controlarlo
     Ante cualquier problema (no de error) devolvemos 100% para no factorizar ese riego,
     sin error si VERIFY=false o Err2 si VERIFY=true
  */
  char* response_pointer = &response[0];
  JsonDocument jsondoc;
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    LOG_ERROR(" ** [ERROR] deserializeJson() failed: ", error.c_str());
    if(!VERIFY) return 100;
    else {
      statusError(E2);  //TODO ¿deberiamos devolver E3?
      return 100;
    }
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *factorstr = jsondoc["result"][0]["Description"];
  if(factorstr == NULL) {
    //El rid (idx) no esta definido en el Domoticz
    LOG_WARN("El idx", idx, " no se ha podido leer del JSON");
    if(!VERIFY) return 100;
    else {
      statusError(E3);
      return 100;
    }
  }
  //extraemos la DESCRIPCION para ese boton en Domoticz del json (campo Name)
  strlcpy(descDomoticz, jsondoc["result"][0]["Name"] | "", sizeof(descDomoticz));
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
 * lee datos de temperatura y humedad del sensor definido en Domoticz
 */
float getTemperatureDomoticz(uint16_t idx)
{
  LOG_TRACE("sensor temp IDX: ", idx);
  // si el IDX es 0 devolvemos 999 sin procesarlo (sensor no asignado)
  if(idx == 0) return 999;
  if(!checkWifi()) return 999; //si no hay conexion devolvemos 999 y no damos error
  String response = deviceInfo(idx);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    LOG_WARN("getTemperatureDomoticz IDX: ", idx, " [HTTP] GET... failed\n");
    return 999;
  }
  /* Teoricamente ya tenemos en response el JSON, lo procesamos
     Si el IDX no existe Domoticz no devuelve error, asi que hay que controlarlo
  */
  char* response_pointer = &response[0];
  JsonDocument jsondoc;
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    LOG_ERROR(" ** [ERROR] deserializeJson() failed: ", error.c_str());
    return 999;
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *datastr = jsondoc["result"][0]["Data"];
  if(datastr == NULL) {
    //El rid (idx) no esta definido en el Domoticz
    LOG_WARN("El idx", idx, " no se ha podido leer del JSON");
    return 999;
  }
  else LOG_INFO(datastr);
  //devolvemos la temperatura del sensor en Domoticz del json (campo Temp)
  float temp = jsondoc["result"][0]["Temp"] | 999.00; // si no existe devuel 999 (float)
  LOG_INFO("devuelve temperatura =",temp);
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

  if(!checkWifi()) {
    if(NONETWORK) return true; //si estamos en modo NONETWORK devolvemos true y no damos error
    else {
      Estado.error=E1;
      return false;
    }
  }
  String response = deviceInfo(idx);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    if (NONETWORK) return true;  //si estamos en modo NONETWORK devolvemos true y no damos error
    if(response == "ErrX") Estado.error=E3;
    else Estado.error=E2;
    LOG_ERROR(" ** [ERROR] IDX: ", idx, " [HTTP] GET... failed");
    return false;
  }
  // ya tenemos en response el JSON, lo procesamos
  char* response_pointer = &response[0];
  JsonDocument jsondoc;
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    LOG_ERROR(" **  [ERROR] deserializeJson() failed: ", error.c_str());
    Estado.error=E2;  
    return false;
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *actual_status = jsondoc["result"][0]["Status"];
  if(actual_status == NULL) {
    LOG_ERROR(" **  [ERROR] deserializeJson() failed: Status not found");
    Estado.error=E2;  
    return false;
  }
  #ifdef EXTRADEBUG
    Serial.printf( "queryStatus verificando, status=%s / actual=%s \n" , status, actual_status);
    Serial.printf( "                status_size=%d / actual_size=%d \n" , strlen(status), strlen(actual_status));
  #endif
  if(strcmp(actual_status,status) == 0) return true;
  else{
    if(NONETWORK) return true; //siempre devolvemos ok en modo simulacion
    LOG_WARN("queryStatus devuelve FALSE, status / actual =",status,"/",actual_status);
    return false;
  }  
}

/**---------------------------------------------------------------
 * Envia a domoticz orden de on/off del idx correspondiente
 */
bool domoticzSwitch(int idx, char *msg, int retries)
{
  LOG_TRACE("idx:", idx, " ", msg, "(", retries, "intentos)");
  if(idx == 0) return true; //simulamos que ha ido OK
  if(!checkWifi() && !NONETWORK) {
    statusError(E1);
    return false;
  }
  char JSONMSG[200]="/json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s";
  char message[250];
  sprintf(message,JSONMSG,idx,msg);
  String response;
  for(int i=0; i<retries; i++) {
     if ((simular.ErrorON && strcmp(msg,"On")==0) || (simular.ErrorOFF && strcmp(msg,"Off")==0)) response = "ErrX"; // simulamos el error
     else if(!NONETWORK) response = httpGetDomoticz(message); // enviamos orden al Domoticz
     if(response == "ErrX") { // solo reintentamos si Domoticz informa del estado de la zona
       bip(1);
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
  static unsigned long lastmillisReconnect = 0;
  #ifdef DEVELOP
    leeSerial();  // para ver si simulamos algun tipo de error
  #endif
  #ifdef DEBUGloops
    debugloops();
  #endif
  if (!flagV || webServerAct) return;      //si no activada por Ticker salimos sin hacer nada
  if (Estado.estado == STANDBY && !reposo) {
     LOG_TRACE(".");
      // si tenemos conexion y no hemos recibido time por NTP o han pasado NTPUPDATEINTERVAL minutos
      // desde la ultima sincronizacion -> actualizamos time del sistema con el del servidor NTP
      if (connected && (!timeOK || millis() > NTPlastUpdate + NTPUPDATEINTERVAL * 60000 )) {
        setClock();
        // en cualquier caso, si timeOK, no intentaremos volver a resincronizar hasta que haya pasado otro NTPUPDATEINTERVAL 
        NTPlastUpdate = millis(); 
      }
      showTemp();  // muestra temperatura ambiente en standby
  }   
  if (errorOFF) bip(2);  //recordatorio error grave al parar un riego
  //si estamos en Standby o en Error por falta de conexion verificamos estado actual de la wifi (no en modo NONETWORK sin conexion)
  if ((!NONETWORK || connected) && (Estado.estado == STANDBY || (Estado.estado == ERROR && !connected))) {
    int wifilevel = checkWifi(config.showwifilevel); // conectado a wifi?
    if(wifilevel) {
      if (Estado.estado!=STANDBY) setEstado(STANDBY,1); 
      if (config.showwifilevel) {
         LOG_DEBUG("showwifilevel=",config.showwifilevel,"wifilevel=",wifilevel);
         if(wifilevel==100) wifilevel=99; 
         lcd.setCursor(0,3);
         snprintf(buff,MAXBUFF,"%02d%%",wifilevel);
         lcd.print(buff); 
      }
    }
    if(!connected && millis() > lastmillisReconnect + RECONNECTINTERVAL * 60000) {   //si no estamos conectados a la wifi, intentamos reconexion
      LOG_WARN("INTENTANDO RECONEXION WIFI");
      //WiFi.reconnect();
      WiFi.disconnect();
      delay(500);
      WiFi.begin();
      lastmillisReconnect = millis();
    }  
    if (connected && falloAP) {
      LOG_INFO("Wifi conectada despues Setup, leemos factor riegos");
      falloAP = false;
      initFactorRiegos(); //esta funcion ya dejara el estado correspondiente
      setupEstado();
    }
  }
  flagV = OFF;
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
      temperatura = temperatura + ((float)config.tempOffset/2); // offset correccion de medio en medio grado
      LOG_TRACE("temp OFFSET=",config.tempOffset,"temperatura corregida=",temperatura);
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
void statusError(uint8_t errorID) 
{
  Estado.estado = ERROR;
  Estado.error = errorID;
  rotaryEncoder.disable();
  sprintf(errorText, "Error%d", errorID);
  LOG_ERROR("SET ERROR: ", errorText);
  snprintf(buff, MAXBUFF, ">>>  %s  <<<", errorText);
  lcd.clear(BORRA2H);
  lcd.setCursor(2,2);
  lcd.print(buff);
  lcd.setCursor(0,3);
  lcd.print(errorToString(errorID));
  actLedError();
  bipKO();
  if (errorID == E5) longbip(5); // resaltamos error al parar riego
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
  #ifdef DEVELOP
    Serial.printf( "\n initParm= %d \n", initFlags.initParm );
    filesInfo();
  #endif
  if( initFlags.initParm) {
    LOG_WARN(">>>>>>>>>>>>>>  borrando ficheros de parámetros  <<<<<<<<<<<<<<");
    bool bRC = deleteParmFiles();
    if(bRC) {
      LOG_WARN("borrado ficheros de parámetros OK");
      //señala el borrado ficheros de parámetros OK (parm y backup)
      lcd.infoclear("RESET/ERASE parm OK",1,BIPOK);
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
  config_finMelody = config.finMelody;
  config_volume = config.volume;
  config_mute = config.mute; 

  setupConfig(); //una vez cargados parametros, completa campos de config y boton

  #ifdef VERBOSE
    if (config.initialized) Serial.print(F("Parametros cargados, "));
    else Serial.print(F("Parametros zero-config, "));
    printParms(config);
  #endif
}

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
    //si en config campo desc de la zona esta vacio se copia el de la estructura Boton:
    if(strlen(config.zona[i].desc) == 0) {
      strlcpy(config.zona[i].desc, Boton[zNumber2bIndex(i+1)].desc, sizeof(config.zona[i].desc));
    }  
  }
  for(int i=0;i<NUMGRUPOS;i++) {
    //si en config campo desc del grupo esta vacio se copia el de la estructura Boton:
    if(strlen(config.group[i].desc) == 0) {
      strlcpy(config.group[i].desc, Boton[bID2bIndex(GRUPOS[i])].desc, sizeof(config.group[i].desc));
    }  
  }
  #ifdef MUTE
    config.mute = true;   // arranque con sonidos silenciados
  #endif
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
    //Wait for four seconds or till data is available on serial, 
    //whichever occurs first.
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

// funciones solo usadas en DEVELOP
// (es igual, el compilador no las incluye si no son llamadas)
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
