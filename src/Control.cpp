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
  #ifdef noWIFI
                NONETWORK=true;
                NOWIFI=true;
  #endif

  Serial.begin(115200);
  #ifdef RELEASE
      if (!serialDetect()) LOG_SET_LEVEL(DebugLogLevel::LVL_NONE); 
  #endif
  delay(500);
  PRINTLN("\n\n CONTROL RIEGO V" + String(VERSION) + "    Built on " __DATE__ " at " __TIME__  "\n");
  #ifndef DEBUGLOG_DISABLE_LOG
    PRINTLN("(current log level is", (int)LOG_GET_LEVEL(), ")");
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
  configure = new Configure();
  //preparo indicadores de inicializaciones opcionales
  setupInit();
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
  //Llamo a parseInputs CLEAR para eliminar prepulsaciones antes del bucle loop
  parseInputs(CLEAR);
  //lanzamos supervision periodica estado cada VERIFY_INTERVAL seg.
  tic_verificaciones.attach(VERIFY_INTERVAL, flagVerificaciones);
  standbyTime = millis();
  LOG_INFO("*** ending setup");
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
  else  boton = parseInputs(READ);  //vemos si algun boton ha cambiado de estado
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
  //inicializamos apuntador estructura multi (posicion del selector multirriego):
  //if(!setMultibyId(getMultiStatus(), config) || !config.initialized) {
  // Verificamos que se han cargado parametros de configuracion correctamente  
  if(!config.initialized) {
    statusError(E0);  //no se ha podido cargar parámetros de ficheros -> señalamos el error
  return;
  }
  // Si estamos en modo NONETWORK pasamos a STANDBY (o STOP si esta pulsado) aunque no exista conexión wifi o estemos en ERROR
  if (NONETWORK) {
    //if (Boton[bID2bIndex(bSTOP)].estado) {
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
   *    - boton Grupo1 --> cargamos los parametros del fichero de configuracion por defecto
   *    - boton Grupo3 --> borramos red wifi almacenada en el ESP32
   *    - liberando boton de STOP  --> salimos sin hacer nada y continua la inicializacion
   */
  void setupInit(void) {

    if (!digitalRead(ENCBOTON) && testButton(bSTOP,ON)) {
      LOG_TRACE("en opciones setupInit");
      lcd.infoclear("       Pulse:");
      lcd.info("grupo1 >load DEFAULT",2);
      lcd.info("grupo3 >erase WIFI",3);
      lcd.info("EXIT -> release STOP",4);
      while (1) {
        boton = parseInputs(READ);
        if(boton == NULL) continue;
        Serial.printf("parseImputs devuelve: boton->id %x  boton->estado %d \n", boton->bID ,boton->estado);
        
        if(boton->bID == bSTOP && !boton->estado ) break;
        
        if (boton->bID == bGRUPO1) 
        {
          initFlags.initParm = true;
          LOG_WARN("pulsado GRUPO1  --> flag de load default PARM true");
          lcd.infoclear("load default PARM",1,BIPOK);
          loadDefaultSignal(6);
          break;
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
        LOG_WARN("encoderSW pulsado y multirriego en GRUPO1  --> flag de load default PARM true");
        loadDefaultSignal(6);
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
                if(VERIFY && Estado.estado != ERROR) stopAllRiego(); //verificamos operativa OFF para los IDX's 
            }
            else {
                NONETWORK = true;
                LOG_INFO("encoderSW+PAUSE pasamos a modo NONETWORK (DEMO)");
                bip(2);
                ledPWM(LEDB,ON);
            }
          }
          else {    // muestra hora y ultimos riegos
            ultimosRiegos(SHOW);
            delay(3000);
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
            setEncoderMenu(configure->get_maxItems());
            LOG_INFO("Stop + hold PAUSA --> modo ConF()");
          }
          else {   //si esta pulsado encoderSW hacemos un soft reset
            LOG_WARN("Stop + encoderSW + PAUSA --> Reset.....");
            lcd.infoclear(">>  REINICIANDO  <<", NOBLINK, LOWBIP, 1);
            delay(MSGDISPLAYMILLIS);
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
      setEstado(STOP);
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
          if (!stopAllRiego()) {   //error al parar riegos
            boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
            return; 
          }
          setEstado(STOP,1);
          reposo = true; //pasamos directamente a reposo
          dimmerLeds(ON);
      }    
    }
  }
  //si hemos liberado STOP: salimos del estado stop
  //(dejamos el release del STOP en modo ConF para que actue el codigo de procesaEstadoConfigurando
  if (!boton->estado && Estado.estado == STOP) {
    LOG_INFO("Salimos de reposo");
    LOG_TRACE("[poniendo estado STANDBY]");
    setEstado(STANDBY);
  }
  //standbyTime = millis();  ¿hace falta?
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
    for (int k=0; k < *multi.size; k++) LOG_DEBUG( "       multi.serie: x" , multi.serie[k]);
    LOG_DEBUG("en MULTIRRIEGO, encoderSW status  :", encoderSW );
    // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
    // y mostramos en el display las zonas que componen el grupo y fecha ultimo riego de este
    if (encoderSW) {
      LOG_DEBUG("en MULTIRRIEGO + encoderSW, display de grupo:", multi.desc,"tamaño:", *multi.size );
      snprintf(buff, MAXBUFF, "grupo: %s", multi.desc);
      lcd.infoclear(buff, 1);
      //displayLCDGrupo(*multi.znumber, *multi.size, 2);  //si usasemos pointer a config
      displayLCDGrupo(multi.zserie, *multi.size, 2);
      showTimeLastRiego(lastGrupos[n_grupo-1], n_grupo-1);
      displayGrupo(multi.serie, *multi.size);
      delay(MSGDISPLAYMILLIS*3);
      setEstado(STANDBY);   //para que restaure pantalla
    }  
    else {
      //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
      //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
      setMultirriego(n_grupo);
      inicioTimeLastRiego(lastGrupos[n_grupo-1], n_grupo-1);
    }
  }
}


void procesaBotonZona(void)
{
  int zIndex = boton->znumber-1;
  if (zIndex < 0) return; //el boton no es de ZONA o error en la matriz Boton[]
  int bIndex = bID2bIndex(boton->bID); 
  if (Estado.estado == STANDBY) {
    if (!encoderSW || multi.riegoON) {  //iniciamos el riego correspondiente al boton seleccionado
        bip(2);
        //cambia minutes y seconds en funcion del factor de cada sector de riego
        uint8_t fminutes=0,fseconds=0;
        if(multi.riegoON) {
          timeByFactor(factorRiegos[boton->znumber-1],&fminutes,&fseconds);
        }
        else {
          fminutes = minutes;
          fseconds = seconds;
        }
        LOG_DEBUG("Minutos:",minutes,"Segundos:",seconds,"FMinutos:",fminutes,"FSegundos:",fseconds);
        ultimoBotonZona = boton;
        // si tiempo factorizado de riego es 0 o IDX=0, nos saltamos este riego
        if ((fminutes == 0 && fseconds == 0) || config.zona[(boton->znumber)-1].idx == 0) {
          setEstado(TERMINANDO);
          led(Boton[bIndex].led,ON); //para que se vea que zona es 
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
      led(Boton[bIndex].led,ON);
      #ifdef EXTRADEBUG
        Serial.printf("Boton: %s Factor de riego: %d \n", boton->desc,factorRiegos[zIndex]);
        Serial.printf("          boton.index: %d \n", bIndex);
        Serial.printf("          boton(%d).led: %d \n", bIndex, Boton[bIndex].led);
      #endif
      savedValue = value;
      value = factorRiegos[zIndex];
      lcd.infoclear("factor riego de ");
      snprintf(buff, MAXBUFF, "%s :  %d", config.zona[boton->znumber-1].desc, factorRiegos[zIndex]);
      lcd.info(buff,2);
      showTimeLastRiego(lastRiegos[zIndex], zIndex);
      delay(2*MSGDISPLAYMILLIS);
      led(Boton[bIndex].led,OFF);
      //value = savedValue;  // para que restaure reloj
      LOG_TRACE("[poniendo estado STANDBY]");
      setEstado(STANDBY);
    }
  }
}


void procesaEstadoConfigurando()
{
  if (boton != NULL) {
    if (boton->flags.action) {
      if (boton->bID != bSTOP && webServerAct) return; //si webserver esta activo solo procesamos boton STOP

      switch(boton->bID) {
        case MULTIRRIEGO:
            if (configure->statusMenu()) { //si no estamos configurando nada, configuramos el grupo multirriego
              int n_grupo;
              #ifdef GRP4
                n_grupo = setMultibyId(boton->bID, config);
              #endif
              #ifdef M3GRP
                n_grupo = setMultibyId(getMultiStatus(), config);
              #endif
              if (n_grupo == 0) return; //error en setup de apuntadores 
              //Configuramos el grupo de multirriego activo
              rotaryEncoder.disable();
              configure->Multi_process_start(n_grupo);
            }  
            break;
        case bPAUSE:
            if(!boton->estado) break; //no se procesa el release del PAUSE
            LOG_DEBUG("[MENU] PAUSE pulsado recibido");
            if(configure->statusMenu()) { //estamos en el menu -> procesamos la seleccion
              configure->procesaSelectMenu(config); 
              break;
            }
            LOG_DEBUG("PAUSE pulsado recibido y estamos configurando algo");
            // si ya estamos configurando algo: PAUSE consolida lo configurado en config
            if(configure->configuringTime()) {
              configure->Time_process_end(config);  //  salvamos en config el nuevo tiempo por defecto
              break;
            }
            if(configure->configuringIdx()) {
              configure->Idx_process_end(config);  //  salvamos en config el nuevo IDX
              break;
            }
            if(configure->configuringMulti()) {
              configure->Multi_process_end(config);  // actualizamos config con las zonas introducidas
              break;
            }
            if(configure->configuringMultiTemp()) {
              configure->MultiTemp_process_end(config);  // actualizamos config con las zonas introducidas
            }
            break;
        case bSTOP:
            if(!boton->estado) {    //release STOP
                if(configure->configuringMultiTemp()) {     
                    if (multi.w_size && saveConfig) {  //solo si se ha guardado alguna zona iniciamos riego grupo temporal
                        saveConfig = false;  
                        multi.temporal = true;
                        setMultirriego(NUMGRUPOS+1);  // grupo temporal n+1
                    }    
                }
                configure->exit(config);  // salvamos parm a fichero si procede y salimos de ConF
            }
            break;
        default:  //procesamos boton de ZONAx
            if (configure->configuringMulti() || configure->configuringMultiTemp()) {  //añadimos zona al multirriego que estamos definiendo
              configure->Multi_process_update();
            }
            if (configure->statusMenu()) {   //si no estamos configurando nada, configuramos el idx del boton
              configure->Idx_process_start(config, bID2bIndex(boton->bID));
            }
      }
    }
  }
  else { 
      webServerAct ? procesaWebServer() : procesaEncoder();
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
    if (Boton[bID2bIndex(bSTOP)].estado) {
      setEstado(STOP,1);
      displayOff = true;
    }
    else {
      setEstado(STANDBY);
    } 
    //reseteos varios:
    resetLeds();    //apaga leds activos y restablece leds ON y RED
    resetFlags();   //reset flags de status
  }
  if(boton->bID == bSTOP) {
  //Si estamos en ERROR y pulsamos o liberamos STOP, reseteamos
    lcd.infoclear("ERROR+STOP-> Reset..",3);
    LOG_WARN("ERROR + STOP --> Reset.....");
    lowbip(1);
    if(checkWifi() && boton->estado) stopAllRiego(); //si es pulsado STOP intentamos parar riegos
    delay(3000);
    ESP.restart();  
  }
};


void procesaEstadoRegando(void)
{
  int tiempoTerminado = T.Timer();
  if (T.TimeHasChanged()) refreshTime();
  if (tiempoTerminado == 0) setEstado(TERMINANDO);
  else if(flagV && VERIFY && (!NONETWORK || simular.all_simFlags)) { // verificamos periodicamente que el riego sigue activo en Domoticz
    if(queryStatus(config.zona[ultimoBotonZona->znumber-1].idx, (char *)"On")) return;
    else {
      ledID = ultimoBotonZona->led;
      if(Estado.error == NOERROR) { //riego zona parado: entramos en PAUSE y blink lento zona pausada remotamente 
        T.PauseTimer();
        tic_parpadeoLedZona.attach(0.8, parpadeoLedZona, ledID);
        LOG_WARN(">>>>>>>>>> procesaEstadoRegando zona:", config.zona[ultimoBotonZona->znumber-1].desc, "en PAUSA remota <<<<<<<<");
        Estado.tipo = REMOTO;
        setEstado(PAUSE,1);
      }
      else {
        statusError(Estado.error); // si no hemos podido verificar estado, señalamos el error
        tic_parpadeoLedError.attach(0.2,parpadeoLedError);
        tic_parpadeoLedZona.attach(0.4, parpadeoLedZona, ledID);
        errorOFF = true;  // recordatorio error
        LOG_ERROR("** SE HA DEVUELTO ERROR");
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
  led(Boton[bID2bIndex(ultimoBotonZona->bID)].led,OFF);
  //Comprobamos si estamos en un multirriego
  if (multi.riegoON) {
    multi.actual++;
    if (multi.actual < *multi.size) {
      //Simular la pulsacion del siguiente boton de la serie de multirriego
      boton = &Boton[bID2bIndex(multi.serie[multi.actual])];
      multi.semaforo = true;
    }
    else {
      int n_grupo;
      #ifdef GRP4
        n_grupo = setMultibyId(*multi.id, config);   // ultimo boton de multirriego pulsado
      #endif
      #ifdef M3GRP
        n_grupo = setMultibyId(getMultiStatus(), config);  // posicion del selector multirriego
      #endif
      if (n_grupo == 0) return; //error en setup de apuntadores 
      if(!multi.temporal) finalTimeLastRiego(lastGrupos[n_grupo-1], n_grupo-1);
      int msgl = snprintf(buff, MAXBUFF, "%s finalizado", multi.desc);
      lcd.info("multirriego",1);
      lcd.info(buff, 2, msgl);
      longbip(3);
      resetFlags();
      LOG_INFO("MULTIRRIEGO", multi.desc, "terminado");
      delay(MSGDISPLAYMILLIS*5);
      lcd.info("", 2);
      led(Boton[bID2bIndex(*multi.id)].led,OFF);
    }
  }
  LOG_TRACE("[poniendo estado STANDBY]");
  setEstado(STANDBY);
};


void procesaEstadoStandby(void)
{
  //Boton[bID2bIndex(bPAUSE)].flags.holddisabled = true;
  //Apagamos el display si ha pasado el lapso
  if (reposo) standbyTime = millis();
  else {
    if (millis() > standbyTime + (1000 * STANDBYSECS)) {
      LOG_INFO("Entramos en reposo");
      reposo = true;
      dimmerLeds(ON);
      lcd.setBacklight(OFF);
    }
  }
  if (reposo & encoderSW) reposoOFF(); // pulsar boton del encoder saca del reposo
  procesaEncoder();
};


void procesaEstadoStop(void)
{
  //En stop activamos el comportamiento hold de pausa
  Boton[bID2bIndex(bPAUSE)].flags.holddisabled = false;
  //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
  if(reposo && !displayOff) {
    if (millis() > standbyTime + (4 * 1000 * STANDBYSECS)) {
      lcd.setBacklight(OFF);
      displayOff = true;
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
    return;
  }
  if(estado==PAUSE) {
    lcd.infoEstado(nEstado[estado], config.zona[ultimoBotonZona->znumber-1].desc); 
    if(bnum) bip(bnum);
    return;
  }
  if(estado == STANDBY) {
    setEncoderTime();
    if(!multi.riegoON && !multi.temporal) lcd.infoclear("STANDBY",NOBLINK,BIP,bnum);
    if (savedValue) value = savedValue;  // para que restaure reloj
    StaticTimeUpdate(REFRESH);
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
    savedValue = value;  // salvamos tiempo riego en display
    return;
  }
}


/**---------------------------------------------------------------
 * Chequeo de perifericos
 */
void check(void)
{
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
    //uint factorR = getFactor(Boton[bIndex].idx);
    if(factorR == 999) break;     //en modo NONETWORK no continuamos iterando si no hay conexion
    if(Estado.estado == ERROR) {  //al primer error salimos
      if(Estado.error == E3) {    // y señalamos zona que falla si no es error general de conexion
        ledID = Boton[bIndex].led;
        tic_parpadeoLedZona.attach(0.4, parpadeoLedZona, ledID);
      }
      break;
    }
    factorRiegos[i] = factorR;
    if (strlen(descDomoticz)) {
      // si xNAME true, actualizamos en config la DESCRIPCION con la recibida del Domoticz (campo Name)
      if (xNAME) {
        strlcpy(config.zona[i].desc, descDomoticz, sizeof(config.zona[i].desc));
        //strlcpy(Boton[bIndex].desc, descDomoticz, sizeof(Boton[bIndex].desc));
        LOG_INFO("\t descripcion ZONA", i+1, "actualizada en config");
      }
      //si el parm desc estaba vacio actualizamos en todo caso (en config, no en Boton)
      if (config.zona[i].desc[0] == 0) {
        strlcpy(config.zona[i].desc, descDomoticz, sizeof(config.zona[i].desc));
        //strlcpy(Boton[bIndex].desc, descDomoticz, sizeof(Boton[bIndex].desc));
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
  uint tseconds = (60*minutes) + seconds;
  //factorizamos
  tseconds = (tseconds*factor)/100;
  //reconvertimos
  *fminutes = tseconds/60;
  *fseconds = tseconds%60;
}


void setClock()
{
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
     // timeOK = false; 
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
}

void setEncoderMenu(int menuitems, int currentitem) {
    LOG_TRACE("");
    rotaryEncoder.setBoundaries(0, menuitems-1, true); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
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


//atenua LEDG y LEDB
void dimmerLeds(bool status)
{
  if(status) {
    LOG_TRACE("leds atenuados ");
    if(connected) analogWrite(LEDG, DIMMLEVEL);
    if(NONETWORK) analogWrite(LEDB, DIMMLEVEL);
  }
  else {
    LOG_TRACE("leds brillo normal ");
    if(connected) analogWrite(LEDG, MAXledLEVEL);
    if(NONETWORK) analogWrite(LEDB, MAXledLEVEL);
  }  
}

void reposoOFF()
{
  LOG_INFO(" salimos de reposo");
  reposo = false;
  dimmerLeds(OFF);
  displayOff = false;
  standbyTime = millis();
  lcd.setBacklight(ON);
}


//lee encoder para actualizar el display
void procesaEncoder()
{
  if(Estado.estado == CONFIGURANDO && configure->statusMenu()) {  //encoder selecciona item menu
      int menuOption = rotaryEncoder.readEncoder();
      if(menuOption == configure->get_currentItem()) return;
      LOG_DEBUG("rotaryEncoder.readEncoder() devuelve menuOption =", menuOption, "currentItem =", configure->get_currentItem());
      configure->showMenu(menuOption);
      return;
  }

  int encvalue = rotaryEncoder.encoderChanged();
  if(!encvalue) return; 
  value = value + encvalue;
  LOG_DEBUG("rotaryEncoder.encoderChanged() devuelve encvalue =", encvalue);

  if(Estado.estado == CONFIGURANDO && configure->configuringIdx()) {  //encoder ajusta numero IDX
      if (value > 1000) value = 1000;
      if (value <  0) value = 0; //permitimos IDX=0 para desactivar ese boton
      int bIndex = configure->get_ActualIdxIndex();
      int currentZona = Boton[bIndex].znumber;
      snprintf(buff, MAXBUFF, "nuevo IDX ZONA%d %d", currentZona, value);
      lcd.info(buff, 4);
      return;
  }

  if (Estado.estado == CONFIGURANDO && !configure->configuringTime()) return;
  //encoder ajusta tiempo:
  if(seconds == 0 && value>0) {   //Estamos en el rango de minutos
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
  if(reposo) reposoOFF();
  StaticTimeUpdate(UPDATE);
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
  LOG_DEBUG( "Terminando riego: ", Boton[bIndex].desc);
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


//Pone a false diversos flags de estado
void resetFlags()
{
  LOG_TRACE("");
  multi.riegoON = false;
  multi.temporal = false;
  multi.semaforo = false;
  errorOFF = false;
  falloAP = false;
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

//override funcion tone de Tone.cpp para que no suenen bips en caso de MUTE
void mitone(uint8_t pin, unsigned int frequency, unsigned long duration) {
  if (mute) return;
  tone(pin, frequency, duration);
}

void bip(int veces)
{
  LOG_TRACE("BIP ", veces);
  for (int i=0; i<veces;i++) {
    mitone(BUZZER, NOTE_A6, 50);
    mitone(BUZZER, 0, 50);
  }
}

void longbip(int veces)
{
  LOG_TRACE("LONGBIP ", veces);
  for (int i=0; i<veces;i++) {
    mitone(BUZZER, NOTE_A6, 750);
    mitone(BUZZER, 0, 100);
  }
}

void lowbip(int veces)
{
  LOG_TRACE("LOWBIP ", veces);
  for (int i=0; i<veces;i++) {
    mitone(BUZZER, NOTE_A4, 500);
    mitone(BUZZER, 0, 100);
  }
}

void beep(int note, int duration){
  // we only play the note for 90% of the duration, leaving 10% as a pause
  mitone(BUZZER, note, duration*0.9);
  delay(duration); // espera a que acabe la nota antes de enviar la siguiente
}

void bipOK() {
  for (int thisNote = 0; thisNote < bipOK_num; thisNote++) {
    beep(bipOK_melody[thisNote], bipOK_duration);
  }
}

void bipKO() {
  for (int thisNote = 0; thisNote < bipKO_num; thisNote++) {
    beep(bipKO_melody[thisNote], bipKO_duration);
  }
}

void blinkPause()
{
  //LOG_TRACE("");
  if (!displayOff) {
    if (millis() > lastBlinkPause + 1.5*DEFAULTBLINKMILLIS) {  // *1.5 para compensar inercia LCD
      displayOff = true;
      lastBlinkPause = millis();
      lcd.displayOFF();
      if(Estado.estado == PAUSE) ledYellow(OFF);
    }
  }
  else {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      displayOff = false;
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
  if (Estado.estado == ERROR) return; //para que en caso de estado ERROR no borre el código de error del display
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;
  //solo se actualiza si cambia hora o REFRESH
  if (refresh) lcd.clear(BORRA2H);
  if(prevseconds != seconds || prevminutes != minutes || refresh) {
    lcd.displayTime(minutes, seconds); 
    prevseconds = seconds;
    prevminutes = minutes;
  }
}

void refreshTime()
{
  unsigned long curMinutes = T.ShowMinutes();
  unsigned long curSeconds = T.ShowSeconds();
  if(prevseconds != curSeconds) lcd.displayTime(curMinutes, curSeconds); //LCD solo se actualiza si cambia
  prevseconds = curSeconds;

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
 * lee factor de riego del Domoticz, almacenado en campo comentarios
 */
int getFactor(uint16_t idx)
{
  LOG_TRACE("");
  // si el IDX es 0 devolvemos 0 sin procesarlo (boton no asignado)
  if(idx == 0) return 0;
  factorRiegosOK = false;
  if(!checkWifi()) {
    if(NONETWORK) return 999; //si estamos en modo NONETWORK devolvemos 999 y no damos error
    else {
      statusError(E1);
      return 100;
    }
  }
  char JSONMSG[200]="/json.htm?type=devices&rid=%d";
  char message[250];
  sprintf(message,JSONMSG,idx);
  String response = httpGetDomoticz(message);
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
  // ojo el ArduinoJson Assistant recomienda usar char* en vez de String (gasta menos memoria)
  char* response_pointer = &response[0];
  DynamicJsonDocument jsondoc(2048);
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    LOG_ERROR(" ** [ERROR] deserializeJson() failed: ", error.f_str());
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
  char JSONMSG[200]="/json.htm?type=devices&rid=%d";
  char message[250];
  sprintf(message,JSONMSG,idx);
  String response = httpGetDomoticz(message);
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
  DynamicJsonDocument jsondoc(2048);
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    LOG_ERROR(" **  [ERROR] deserializeJson() failed: ", error.f_str());
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
  #endif
  if (!flagV || webServerAct) return;      //si no activada por Ticker salimos sin hacer nada
  if (Estado.estado == STANDBY) LOG_TRACE(".");
  if (errorOFF) bip(2);  //recordatorio error grave al parar un riego
  //si estamos en Standby o en Error por falta de conexion verificamos estado actual de la wifi (no en modo NONETWORK)
  if (!NONETWORK && (Estado.estado == STANDBY || (Estado.estado == ERROR && !connected))) {
    if (checkWifi() && Estado.estado!=STANDBY) setEstado(STANDBY,1); 
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
    }
  }
  // si tenemos conexion y no hemos recibido time por NTP o han pasado NTPUPDATEINTERVAL minutos
  // desde la ultima sincronizacion -> actualizamos time del sistema con el del servidor NTP
  if (connected && (!timeOK || millis() > NTPlastUpdate + NTPUPDATEINTERVAL * 60000 )) {
    setClock();
    // en cualquier caso, si timeOK, no intentaremos resincronizar hasta que haya pasado otro NTPUPDATEINTERVAL 
    NTPlastUpdate = millis(); 
  }  
  flagV = OFF;
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
    filesInfo();
    Serial.printf( "initParm= %d \n", initFlags.initParm );
  #endif
  if( initFlags.initParm) {
    LOG_WARN(">>>>>>>>>>>>>>  cargando parametros por defecto  <<<<<<<<<<<<<<");
    bool bRC = copyConfigFile(defaultFile, parmFile); // defaultFile --> parmFile
    if(bRC) {
      LOG_WARN("carga parametros por defecto OK");
      //señala la carga parametros por defecto OK
      lcd.infoclear("load DEFAULT parm OK", DEFAULTBLINK, BIPOK);
      delay(MSGDISPLAYMILLIS);
    }  
    else LOG_ERROR(" **  [ERROR] cargando parametros por defecto");
  }
  if (!setupConfig(parmFile)) {
    LOG_ERROR(" ** [ERROR] Leyendo fichero parametros " , parmFile);
    if (!setupConfig(defaultFile)) {
      LOG_ERROR(" ** [ERROR] Leyendo fichero parametros ", defaultFile);
    }
  }
  if (!config.initialized) zeroConfig(config);  //init config con zero-config
  #ifdef VERBOSE
    if (config.initialized) Serial.print(F("Parametros cargados, "));
    else Serial.print(F("Parametros zero-config, "));
    printParms(config);
  #endif
}


bool setupConfig(const char *p_filename) 
{
  //init grupo temporal n+1  
  LOG_INFO("Init grupo temporal (GRUPO", _NUMGRUPOS+1,")");
  config.group[_NUMGRUPOS].bID = 0;     // id del boton de grupo ficticio
  config.group[_NUMGRUPOS].size = 1;
  config.group[_NUMGRUPOS].zNumber[0]=1; // numero de zona ficticia ???
  sprintf(config.group[_NUMGRUPOS].desc, "TEMPORAL"); 

  LOG_INFO("Leyendo fichero parametros", p_filename);
  bool loaded = loadConfigFile(p_filename, config);
  minutes = config.minutes;
  seconds = config.seconds;
  value = ((seconds==0)?minutes:seconds);
  if (loaded) {
    for(int i=0;i<NUMZONAS;i++) {
      //si en config campo desc de la zona esta vacio se copia el de la estructura Boton:
      if(strlen(config.zona[i].desc) == 0) {
        strlcpy(config.zona[i].desc, Boton[zNumber2bIndex(i+1)].desc, sizeof(config.zona[i].desc));
      }  
    }
        #ifdef EXTRADEBUG
          printFile(p_filename);
        #endif
    return true;
  }  
  LOG_ERROR(" ** [ERROR] parámetros de configuración no cargados");
  return false;
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
      if ((!inputNumber || inputNumber>6) && inputNumber != 9) {
          Serial.println(F("Teclee: "));
          Serial.println(F("   1 - simular error NTP"));
          Serial.println(F("   2 - simular error apagar riego"));
          Serial.println(F("   3 - simular error encender riego"));
          Serial.println(F("   4 - simular EV no esta ON en Domoticz"));
          Serial.println(F("   5 - simular EV no esta OFF en Domoticz"));
          Serial.println(F("   6 - simular error al salir del PAUSE"));
          Serial.println(F("   7 - MUTE ON"));
          Serial.println(F("   8 - MUTE OFF"));
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
            case 7:
                Serial.println(F("recibido:   7 - mute ON"));
                mute = ON;
                break;
            case 8:
                Serial.println(F("recibido:   8 - mute OFF"));
                mute = OFF;
                break;
            case 9:
                Serial.println(F("recibido:   9 - anular simulacion errores"));
                timeOK = true;                         
                simular.all_simFlags = false;
      }
    }
  }

#endif  
