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

  Serial.begin(115200);
  //if (!serialDetect()) {
  //  LOG_SET_LEVEL(DebugLogLevel::LVL_NONE); 
  //}
  delay(500);
  PRINTLN("\n\n CONTROL RIEGO V" + String(VERSION) + "    Built on " __DATE__ " at " __TIME__  "\n");
  PRINTLN("(current log level is", (int)LOG_GET_LEVEL(), ")");

  LOG_DEBUG("Startup reason: ", esp_reset_reason());
  LOG_TRACE("TRACE: in setup");
  // init de los GPIOs y bus I2C
  initGPIOs();
  initWire();
  //Para los MCPs
  LOG_TRACE("Inicializando MCPs");
  mcpOinit();
  mcpIinit();
  //Para el display
  LOG_TRACE("Inicializando displays");
  display = new Display(DISPCLK,DISPDIO);
  display->clearDisplay();
  lcd.initLCD();
  //Para el encoder
  LOG_TRACE("Inicializando Encoder");
  //Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  initEncoder();
  //Para el Configure le paso display porque lo usara.
  LOG_TRACE("Inicializando Configure");
  configure = new Configure(display);
  //preparo indicadores de inicializaciones opcionales
  setupInit();
  //led encendido/error
  ledPWM(LEDR,ON);
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
    if (saveConfigFile(parmFile, config))  bipOK();;
    saveConfig = false;
  }
  delay(2000);
  //Ponemos en hora
  timeClient.begin();
  delay(500);
  initClock();
  //Inicializamos lastRiegos y lastGrupos (registro fecha y riego realizado)
  initLastRiegos();
  initLastGrupos();
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
  //lanzamos supervision periodica estado cada VERIFY_INTERVAL seg.
  tic_verificaciones.attach(VERIFY_INTERVAL, flagVerificaciones);  //TODO ¿es correcto?
  //tic_verificaciones.attach_scheduled(VERIFY_INTERVAL, flagVerificaciones);
  standbyTime = millis();
  LOG_INFO("*** ending setup");
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
  #ifdef EXTRATRACE
    Serial.print(F("B"));
  #endif
  // almacenamos estado pulsador del encoder (para modificar comportamiento de otros botones)
  //NOTA: el encoderSW esta en estado HIGH en reposo y en estado LOW cuando esta pulsado
  encoderSW = !digitalRead(bENCODER);
  //(testButton(bENCODER, OFF)) ? encoderSW = true : encoderSW = false;
  //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multirriego
  if (multiSemaforo) multiSemaforo = false;
  else  boton = parseInputs(READ);  //vemos si algun boton ha cambiado de estado
  // si no se ha pulsado ningun boton salimos
  if(boton == NULL) return;
  //Si estamos en reposo pulsar cualquier boton solo nos saca de ese estado
  // (salvo STOP que si actua y se procesa mas adelante)
  if (reposo && boton->id != bSTOP) {
    reposoOFF();
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
      strcmp(errorText, "") == 0 ? blinkPause() : blinkPauseError();
      break;
  }
}


/**---------------------------------------------------------------
 * Estado final al acabar Setup en funcion de la conexion a la red
 */
void setupEstado() 
{
  LOG_TRACE("");
  //inicializamos apuntador estructura multi (posicion del selector multirriego):
  if(!setMultibyId(getMultiStatus(), config) || !config.initialized) {
    statusError(E0);  //no se ha podido cargar parámetros de ficheros -> señalamos el error
  return;
  }
  // Si estamos en modo NONETWORK pasamos a STANDBY (o STOP si esta pulsado) aunque no exista conexión wifi o estemos en ERROR
  if (NONETWORK) {
    if (Boton[bID_bIndex(bSTOP)].estado) {
      setEstado(STOP);
      infoDisplay("StoP", NOBLINK, LONGBIP, 1);
      lcd.infoclear("STOP", NOBLINK, LONGBIP, 1);
    }
    else setEstado(STANDBY);
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
      setEstado(STOP);
      infoDisplay("StoP", NOBLINK, LONGBIP, 1);
      lcd.infoclear("STOP", NOBLINK, LONGBIP, 1);
    }
    else setEstado(STANDBY);
    bip(1);
    return;
  }
  //si no estamos conectados a la red y no estamos en modo NONETWORK pasamos a estado ERROR
  statusError(E1);
}


/**---------------------------------------------------------------
 * verificamos si encoderSW esta pulsado (estado OFF) y selector de multirriego esta en:
 *    - Grupo1 (arriba) --> en ese caso cargamos los parametros del fichero de configuracion por defecto
 *    - Grupo3 (abajo) --> en ese caso borramos red wifi almacenada en el ESP8266
 */
void setupInit(void) {
  LOG_TRACE("");
  if (!digitalRead(bENCODER)) {
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
          bip(1);
          setEstado(PAUSE);
          tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
          led(ultimoBoton->led,ON);// y lo deja fijo
          stopRiego(ultimoBoton->id);
          T.PauseTimer();
        }
        break;
      case PAUSE:
        if(simular.ErrorPause) statusError(E2); //simulamos error al salir del PAUSE
        else initRiego(ultimoBoton->id);
        if(Estado.estado == ERROR) { // caso de error al reanudar el riego seguimos en PAUSE y señalamos con blink rapido zona
          ledID = ultimoBoton->led;
          tic_parpadeoLedZona.attach(0.2,parpadeoLedZona);
          LOG_WARN("error al salir de PAUSE errorText :",errorText,"Estado.fase :",Estado.fase );
          refreshTime();
          setEstado(PAUSE);
          break;
        }
        bip(2);
        lcd.clear(BORRA2H);
        T.ResumeTimer();
        tic_parpadeoLedZona.detach(); //detiene parpadeo led zona (por si estuviera activo)
        led(ultimoBoton->led,ON);// y lo deja fijo
        setEstado(REGANDO);
        break;
      case STANDBY:
          boton = NULL; //lo borramos para que no sea tratado más adelante en procesaEstados
          if(encoderSW) {  //si encoderSW+Pause --> conmutamos estado NONETWORK
            if (NONETWORK) {
                NONETWORK = false;
                LOG_INFO("encoderSW+PAUSE pasamos a modo NORMAL y leemos factor riegos");
                bip(2);
                ledPWM(LEDB,OFF);
                display->print("----");
                lcd.clear();
                if (!checkWifi()) {
                  WiFi.reconnect(); 
                  delay(2000);
                }
                initFactorRiegos();
                if(VERIFY && Estado.estado != ERROR) stopAllRiego(); //verificamos operativa OFF para los IDX's 
            }
            else {
                NONETWORK = true;
                LOG_INFO("encoderSW+PAUSE pasamos a modo NONETWORK (DEMO)");
                bip(2);
                ledPWM(LEDB,ON);
                lcd.clear();
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
            setEstado(CONFIGURANDO);
            configure->start();
            longbip(1);
            ledConf(ON);
            LOG_INFO("Stop + hold PAUSA --> modo ConF()");
            boton = NULL;
            holdPause = false;
            savedValue = value;
          }
          else {   //si esta pulsado encoderSW hacemos un soft reset
            LOG_WARN("Stop + encoderSW + PAUSA --> Reset.....");
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
    if (Estado.estado == REGANDO || Estado.estado == PAUSE) {
      //De alguna manera esta regando y hay que parar
      display->print("StoP");
      lcd.infoclear("parando riegos", 1, BIP, 6);
      T.StopTimer();
      if (!stopAllRiego()) {   //error en stopAllRiego
        boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
        return; 
      }
      lcd.infoclear("detenidos riegos", DEFAULTBLINK, BIP, 0);
      infoDisplay("StoP", DEFAULTBLINK, BIP, 6);
      setEstado(STOP);
      resetFlags();
    }
    else {
      //Lo hemos pulsado en standby - seguro antinenes
      infoDisplay("StoP", NOBLINK, BIP, 3);
      lcd.infoclear("STOP", NOBLINK, BIP, 3);
      // apagar leds y parar riegos (por si riego activado externamente)
      if (!stopAllRiego()) {   //error en stopAllRiego
        boton = NULL; //para que no se resetee inmediatamente en procesaEstadoError
        return; 
      }
      lcd.clear();
      setEstado(STOP);
      reposo = true; //pasamos directamente a reposo
      dimmerLeds(ON);
      displayOff = false;
      lcd.setBacklight(ON);
    }
  }
  //si hemos liberado STOP: salimos del estado stop
  //(dejamos el release del STOP en modo ConF para que actue el codigo de procesaEstadoConfigurando
  if (!boton->estado && Estado.estado == STOP) {
    StaticTimeUpdate(REFRESH);
    LOG_INFO("Salimos de reposo");
    reposo = false; //por si salimos de stop antinenes
    dimmerLeds(OFF);
    displayOff = false;
    setEstado(STANDBY);
  }
  standbyTime = millis();
}


bool procesaBotonMultiriego(void)
{
  if (Estado.estado == STANDBY && !multirriego) {
    int n_grupo = setMultibyId(getMultiStatus(), config);
    if (n_grupo == 0) {  //error en setup de apuntadores 
      statusError(E0); 
      return false;
    }  
    LOG_DEBUG("en MULTIRRIEGO, setMultibyId devuelve: Grupo", n_grupo,"(",multi.desc,") multi.size=" , *multi.size);
    for (int k=0; k < *multi.size; k++) LOG_DEBUG( "       multi.serie: x" , multi.serie[k]);
    LOG_DEBUG("en MULTIRRIEGO, encoderSW status  :", encoderSW );
    // si esta pulsado el boton del encoder --> solo hacemos encendido de los leds del grupo
    // y mostramos en el display la version del programa.
    if (encoderSW) {
      snprintf(buff, MAXBUFF, "grupo: %s", multi.desc);
      lcd.infoclear(buff, 1);
      displayLCDGrupo(multi.zserie, *multi.size, 2);
      lcd.info("ultimo:",3);
      time_t t1=lastGrupos[n_grupo].inicio;
      time_t t2=lastGrupos[n_grupo].final;
      LOG_DEBUG("Grupo:", n_grupo, "time", t1);
      if (t1) {
        snprintf(buff, MAXBUFF, "%d/%02d %d:%02d (%d:%02d)", day(t1), month(t1), hour(t1), minute(t1), hour(t2), minute(t2));
        lcd.info(buff,4);
      }
      else lcd.info("  > sin datos <",4);
      displayGrupo(multi.serie, *multi.size);
      LOG_DEBUG("en MULTIRRIEGO + encoderSW, display de grupo:", multi.desc,"tamaño:", *multi.size );
      delay(MSGDISPLAYMILLIS*3);
      lcd.infoclear(nEstado[Estado.estado]);
      StaticTimeUpdate(REFRESH);
      return false;    //para que procese el BREAK al volver a procesaBotones         
    }  
    else {
      //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
      //Realmente estoy simulando la pulsacion del primer boton de riego de la serie
      bip(4);
      multirriego = true;
      multi.actual = 0;
      LOG_INFO("MULTIRRIEGO iniciado: ", multi.desc);
      led(Boton[bID_bIndex(*multi.id)].led,ON);
      lcd.info(multi.desc, 2);
      boton = &Boton[bID_bIndex(multi.serie[multi.actual])];
      utc = timeClient.getEpochTime();
      time_t t = CE.toLocal(utc,&tcr);
      LOG_DEBUG("Grupo:", n_grupo, "time", t);
      lastGrupos[n_grupo].inicio = t;
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
    if (!encoderSW || multirriego) {  //iniciamos el riego correspondiente al boton seleccionado
        bip(2);
        //cambia minutes y seconds en funcion del factor de cada sector de riego
        uint8_t fminutes=0,fseconds=0;
        if(multirriego) {
          timeByFactor(factorRiegos[zIndex],&fminutes,&fseconds);
        }
        else {
          fminutes = minutes;
          fseconds = seconds;
        }
        LOG_DEBUG("Minutos:",minutes,"Segundos:",seconds,"FMinutos:",fminutes,"FSegundos:",fseconds);
        ultimoBoton = boton;
        // si tiempo factorizado de riego es 0 o IDX=0, nos saltamos este riego
        if ((fminutes == 0 && fseconds == 0) || boton->idx == 0) {
          setEstado(TERMINANDO);
          led(Boton[bIndex].led,ON); //para que se vea que zona es 
          display->print("-00-");
          lcd.clear(BORRA2H);
          lcd.info("IDX/factor:     -00-",4);
          return;
        }
        T.SetTimer(0,fminutes,fseconds);
        T.StartTimer();
        initRiego(boton->id);
        if(Estado.estado != ERROR) setEstado(REGANDO); // para que no borre ERROR
    }
    else {  // mostramos en el display el factor de riego del boton pulsado
      led(Boton[bIndex].led,ON);
      #ifdef EXTRADEBUG
        Serial.printf("Boton: %s Factor de riego: %d \n", boton->desc,factorRiegos[zIndex]);
        Serial.printf("          boton.index: %d \n", bIndex);
        Serial.printf("          boton(%d).led: %d \n", bIndex, Boton[bIndex].led);
      #endif
      savedValue = value;
      value = factorRiegos[zIndex];
      display->print(value);
      lcd.infoclear("factor riego de ");
      snprintf(buff, MAXBUFF, "%s :  %d", boton->desc, factorRiegos[zIndex]);
      lcd.info(buff,3);
      delay(2*MSGDISPLAYMILLIS);
      lcd.clear(BORRA2H);
      lcd.infoEstado("STANDBY");
      value = savedValue;  // para que restaure reloj
      led(Boton[bIndex].led,OFF);
      StaticTimeUpdate(REFRESH);
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
                lcd.infoclear("Saved DEFAULT", DEFAULTBLINK, BIPOK);
                display->print("ConF");
                delay(MSGDISPLAYMILLIS); 
                lcd.info("modo CONFIGURACION",1); 
              }
            }
            #ifdef WEBSERVER
              if (n_grupo == 2) {  // activamos webserver
                setupWS();
                Serial.println(F("[ConF][WS] activado webserver para actualizaciones OTA de SW o filesystem"));
                webServerAct = true;
                ledConf(OFF);
                infoDisplay("otA", DEFAULTBLINK, BIPOK, 5);
                lcd.infoclear("OTA Webserver act", DEFAULTBLINK, BIPOK);
                //snprintf(buff, MAXBUFF, "\"%s.local:%d\"", WiFi.getHostname(), wsport);
                snprintf(buff, MAXBUFF, "\"%s.local:8080\"", WiFi.getHostname());
                lcd.info(buff, 3);
                snprintf(buff, MAXBUFF, "%s:8080", WiFi.localIP().toString());
                lcd.info(buff,4);
                delay(MSGDISPLAYMILLIS);
              }
            #endif 
            if (n_grupo == 3) {  // activamos AP y portal de configuracion (bloqueante)
              Serial.println(F("[ConF] encoderSW + selector ABAJO: activamos AP y portal de configuracion"));
              ledConf(OFF);
              starConfigPortal(config);
              ledConf(ON);
              display->print("ConF");
              lcd.info("modo CONFIGURACION",1); 
            }
          }
          else {
            //Configuramos el grupo de multirriego activo
            configure->configureMulti(n_grupo);
            LOG_INFO("Configurando: GRUPO",n_grupo,"(",multi.desc,")");
            LOG_DEBUG("en configuracion de MULTIRRIEGO, setMultibyId devuelve: Grupo",n_grupo,"(",multi.desc,") multi.size=", *multi.size);
            displayGrupo(multi.serie, *multi.size);
            multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
            display->print("PUSH");
            led(Boton[bID_bIndex(*multi.id)].led,ON);
            snprintf(buff, MAXBUFF, "grupo%d: %s",n_grupo, multi.desc);
            lcd.info(buff, 2);
            snprintf(buff, MAXBUFF, "pulse ZONAS");
            lcd.info(buff, 3);
          } 
          break;
        case bPAUSE:
          if(!boton->estado) return; //no se procesa el release del PAUSE
          if(!configure->configuring()) { //si no estamos ya configurando algo
            configure->configureTime();   //configuramos tiempo por defecto
            LOG_INFO("configurando tiempo riego por defecto");
            lcd.info("tiempo riego def.:",2);
            delay(500);
            break;
          }
          if(configure->configuringTime()) {
            LOG_INFO("Save DEFAULT TIME, minutes:",minutes," secons:",seconds);
            lcd.info("DEFAULT TIME saved",2);
            config.minutes = minutes;
            config.seconds = seconds;
            saveConfig = true;
            bipOK();
            delay(MSGDISPLAYMILLIS);
            configure->stop();
            break;
          }
          if(configure->configuringIdx()) {
            int bIndex = configure->getActualIdxIndex();
            int zIndex = bID_zIndex(Boton[bIndex].id);
            Boton[bIndex].idx = (uint16_t)value;
            config.botonConfig[zIndex].idx = (uint16_t)value;
            saveConfig = true;
            LOG_INFO("Save Zona",zIndex+1,"(",Boton[bIndex].desc,") IDX value:",value);
            lcd.info("guardado IDX",2);
            lcd.clear(BORRA2H);
            value = savedValue;
            bipOK();
            led(Boton[bIndex].led,OFF);
            delay(MSGDISPLAYMILLIS);  // para que se vea el msg
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
              LOG_INFO("SAVE PARM Multi : GRUPO",g,"tamaño:",*multi.size,"(",multi.desc,")");
              printMultiGroup(config, g-1);
              saveConfig = true;
              bipOK();
              lcd.info("guardado GRUPO",2);
              lcd.clear(BORRA2H);
              delay(MSGDISPLAYMILLIS);
            }
            ultimosRiegos(HIDE);
            led(Boton[bID_bIndex(*multi.id)].led,OFF);
            configure->stop();
          }
          break;
        case bSTOP:
          if(!boton->estado) { //release STOP salvamos si procede y salimos de ConF
            configure->stop();
            if (saveConfig) {
              LOG_INFO("saveConfig=true  --> salvando parametros a fichero");
              if (saveConfigFile(parmFile, config)) {
                infoDisplay("SAUE", DEFAULTBLINK, BIPOK, 5);
                lcd.infoclear("SAVED parameters", DEFAULTBLINK, BIPOK);
                delay(MSGDISPLAYMILLIS);
              }  
              saveConfig = false;
            }
            #ifdef WEBSERVER
              if (webServerAct) {
                endWS();
                LOG_INFO("[ConF][WS] desactivado webserver");
                webServerAct = false; //al salir de modo ConF no procesaremos peticiones al webserver
              }
            #endif
            setEstado(STANDBY);
            resetLeds();
            standbyTime = millis();
            if (savedValue>0) value = savedValue;  // para que restaure reloj aunque no salvemos con pause el valor configurado
            StaticTimeUpdate(REFRESH);
          }
          break;
        default:  //procesamos boton de ZONAx
          if (configure->configuringMulti()) {  //Configuramos el multirriego seleccionado
            if (multi.w_size < 16) {  //max. 16 zonas por grupo
              multi.serie[multi.w_size] = boton->id;
              multi.zserie[multi.w_size] = zIndex+1 ;
              LOG_INFO("[ConF] añadiendo ZONA",zIndex+1,"(",boton->desc,")");
              led(Boton[bIndex].led,ON);
              multi.w_size = multi.w_size + 1;
              displayLCDGrupo(multi.zserie, multi.w_size);
            }  
            else bipKO();
          }
          if (!configure->configuring()) {   //si no estamos configurando nada, configuramos el idx
            LOG_INFO("[ConF] configurando IDX boton:",boton->desc);
            configure->configureIdx(bIndex);
            value = boton->idx;
            led(Boton[bIndex].led,ON);
            snprintf(buff, MAXBUFF, "IDX de:  %s", boton->desc);
            lcd.info(buff, 2);
            snprintf(buff, MAXBUFF, "actual %d", value);
            lcd.info(buff, 3);

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
    //Si estamos en error y pulsamos pausa, nos ponemos en modo NONETWORK para test
    //reset del display lcd:    
    resetLCD();
    if (Boton[bID_bIndex(bSTOP)].estado) {
      setEstado(STOP);
      infoDisplay("StoP", NOBLINK, LONGBIP, 1);
      lcd.infoclear("STOP", NOBLINK, LONGBIP, 1);
      displayOff = true;
    }
    else {
      setEstado(STANDBY);
      displayOff = false;
      standbyTime = millis();
      StaticTimeUpdate(REFRESH);
    } 
    NONETWORK = true;
    LOG_INFO("estado en ERROR y PAUSA pulsada pasamos a modo NONETWORK y reset del error");
    bip(2);
    //reseteos varios:
    resetLeds();    //apaga leds activos y restablece leds ON y RED
    resetFlags();   //reset flags de status
  }
  if(boton->id == bSTOP) {
  //Si estamos en ERROR y pulsamos o liberamos STOP, reseteamos, pero antes intentamos parar riegos
    setEstado(STANDBY);
    lcd.infoclear("ERROR+STOP-> Reset..",3);
    LOG_WARN("ERROR + STOP --> Reset.....");
    longbip(3);
    if(checkWifi()) stopAllRiego();
    delay(3000);
    ESP.restart();  
  }
};


void procesaEstadoRegando(void)
{
  tiempoTerminado = T.Timer();
  if (T.TimeHasChanged()) refreshTime();
  if (tiempoTerminado == 0) setEstado(TERMINANDO);
  else if(flagV && VERIFY && !NONETWORK) { // verificamos periodicamente que el riego sigue activo en Domoticz
    if(queryStatus(ultimoBoton->idx, (char *)"On")) return;
    else {
      ledID = ultimoBoton->led;
      if(Estado.fase == CERO) { //riego zona parado: entramos en PAUSE y blink lento zona pausada remotamente 
        bip(1);
        T.PauseTimer();
        tic_parpadeoLedZona.attach(0.8,parpadeoLedZona);
        Serial.printf(">>>>>>>>>> procesaEstadoRegando zona: %s en PAUSA remota <<<<<<<<\n", ultimoBoton->desc);
        setEstado(PAUSE);
      }
      else {
        statusError(Estado.fase); // si no hemos podido verificar estado, señalamos el error
        tic_parpadeoLedError.attach(0.2,parpadeoLedError);
        tic_parpadeoLedZona.attach(0.4,parpadeoLedZona);
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
  stopRiego(ultimoBoton->id);
  if (Estado.estado == ERROR) return; //no continuamos si se ha producido error al parar el riego
  #ifdef displayLED
    display->blink(DEFAULTBLINK);
  #endif
  lcd.blinkLCD(DEFAULTBLINK);
  led(Boton[bID_bIndex(ultimoBoton->id)].led,OFF);
  StaticTimeUpdate(REFRESH);
  standbyTime = millis();
  setEstado(STANDBY);
  //Comprobamos si estamos en un multirriego
  if (multirriego) {
    multi.actual++;
    if (multi.actual < *multi.size) {
      //Simular la pulsacion del siguiente boton de la serie de multirriego
      boton = &Boton[bID_bIndex(multi.serie[multi.actual])];
      multiSemaforo = true;
    }
    else {
      utc = timeClient.getEpochTime();
      time_t t = CE.toLocal(utc,&tcr);
      int n_grupo = setMultibyId(getMultiStatus(), config);
      LOG_DEBUG("Grupo:", n_grupo, "time", t);
      lastGrupos[n_grupo].final = t;
      int msgl = snprintf(buff, MAXBUFF, "%s finalizado", multi.desc);
      lcd.info(buff, 2, msgl);
      longbip(3);
      resetFlags();
      Serial.printf("MULTIRRIEGO %s terminado \n", multi.desc);
      delay(3000);
      lcd.info("", 2);
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
      LOG_INFO("Entramos en reposo");
      reposo = true;
      dimmerLeds(ON);
      display->clearDisplay();
      lcd.setBacklight(OFF);
    }
  }
  if (reposo & encoderSW) reposoOFF(); // pulsar boton del encoder saca del reposo
  procesaEncoder();
};


void procesaEstadoStop(void)
{
  //En stop activamos el comportamiento hold de pausa
  Boton[bID_bIndex(bPAUSE)].flags.holddisabled = false;
  //si estamos en Stop antinenes, apagamos el display pasado 4 x STANDBYSECS
  if(reposo && !displayOff) {
    if (millis() > standbyTime + (4 * 1000 * STANDBYSECS)) {
      display->clearDisplay();
      lcd.setBacklight(OFF);
      displayOff = true;
    }
  }
};

void procesaEstadoPause(void) {
  if(flagV && VERIFY && !NONETWORK) {  // verificamos zona sigue OFF en Domoticz periodicamente
    if(queryStatus(ultimoBoton->idx, (char *)"Off")) return;
    else {
      if(Estado.fase == CERO) { //riego zona activo: salimos del PAUSE y blink lento zona activada remotamente 
        bip(2);
        ledID = ultimoBoton->led;
        Serial.printf("\tactivado blink %s (boton id= %d) \n", ultimoBoton->desc, ultimoBoton->id);
        tic_parpadeoLedZona.attach(0.8,parpadeoLedZona);
        Serial.printf(">>>>>>>>>> procesaEstadoPause zona: %s activada REMOTAMENTE <<<<<<<\n", ultimoBoton->desc);
        T.ResumeTimer();
        setEstado(REGANDO);
      }
      else Estado.fase = CERO; // si no hemos podido verificar estado, ignoramos el error
    }
  }
}


/**---------------------------------------------------------------
 * Pone estado pasado y sus indicadores opcionales
 */
void setEstado(uint8_t estado)
{
  Estado.estado = estado;
  Estado.fase = CERO;
  strcpy(errorText, "");
  LOG_DEBUG( "Cambiado estado a: ", nEstado[estado]);
  if((estado==REGANDO || estado==TERMINANDO || estado==PAUSE) && ultimoBoton != NULL) {
    lcd.infoEstado(nEstado[estado], ultimoBoton->desc); }
  else lcd.infoEstado(nEstado[estado]);
}


/**---------------------------------------------------------------
 * Chequeo de perifericos
 */
void check(void)
{
  display->print("----");
  #ifndef noCHECK
    LOG_TRACE("TRACE: in initLeds + display check");
    initLeds();
    //display->check(1);
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
  if(Estado.estado == ERROR) return; // si estabamos en error (no wifi) ni lo intentamos
  lcd.info("conectando Domoticz", 2);
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
    }
    factorRiegos[i] = factorR;
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
    time_t t = CE.toLocal(now(),&tcr);
    LOG_INFO("time recibido OK  (UTC) --> ", timeClient.getFormattedTime(),"  local --> ",hour(t),":",minute(t),":",second(t));
  }  
   else {
     LOG_WARN(" ** no se ha recibido time por NTP");
     timeOK = false;
    }
}

void initEncoder() {
    LOG_TRACE("");
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    //set boundaries and if values should cycle or not
    bool circleValues = false;
    rotaryEncoder.setBoundaries(0, 1000, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
    rotaryEncoder.setEncoderValue(500);
    //rotaryEncoder.correctionOffset = 0;
    //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
    rotaryEncoder.setAcceleration(50); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
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
      lcd.infoclear("Hora actual:");
      if (timeOK) lcd.displayTime(hour(t),minute(t));
      else lcd.info("   <<< NO TIME >>>",3);
      break;
    case HIDE:
      StaticTimeUpdate(REFRESH);
      lcd.infoEstado("STANDBY");
      for(unsigned int i=0;i<NUMZONAS;i++) {
        led(Boton[bID_bIndex(ZONAS[i])].led,OFF);
      }
      break;
  }
}


//atenua LEDR, LEDG y LEDB
void dimmerLeds(bool status)
{
  if(status) {
    LOG_TRACE("leds atenuados ");
    //analogWrite(LEDR, DIMMLEVEL);
    if(connected) analogWrite(LEDG, DIMMLEVEL);
    if(NONETWORK) analogWrite(LEDB, DIMMLEVEL);
  }
  else {
    LOG_TRACE("leds brillo normal ");
    //analogWrite(LEDR, MAXledLEVEL);
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
  if(Estado.estado == STOP) display->print("StoP");
  else StaticTimeUpdate(REFRESH);
}


//lee encoder para actualizar el display
void procesaEncoder()
{
  //Encoder->service();
  //encvalue = Encoder->getValue();
  encvalue = rotaryEncoder.encoderChanged();
  if(!encvalue) return; 
  else value = value - encvalue;

  if(Estado.estado == CONFIGURANDO && configure->configuringIdx()) {
      #ifdef EXTRATRACE
       Serial.print(F("i"));
      #endif
      //value -= Encoder->getValue();
      if (value > 1000) value = 1000;
      if (value <  1) value = 0; //permitimos IDX=0 para desactivar ese boton
      display->print(value);
      int bIndex = configure->getActualIdxIndex();
      int currentZona = bID_zIndex(Boton[bIndex].id)+1;
      snprintf(buff, MAXBUFF, "nuevo IDX ZONA%d %d", currentZona, value);
      lcd.info(buff, 4);

      //savedIDX = value;
      return;
  }

  if (Estado.estado == CONFIGURANDO && !configure->configuringTime()) return;

  if(!reposo) StaticTimeUpdate(UPDATE);

  //value -= Encoder->getValue();
  
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
  if(reposo) {
    LOG_INFO("Salimos de reposo");
    reposo = false;
    dimmerLeds(OFF);
    lcd.setBacklight(ON);
  }
  StaticTimeUpdate(UPDATE);
  standbyTime = millis();
}


void initLastRiegos()
{
  for(uint i=0;i<NUMZONAS;i++) {
   lastRiegos[i] = 0;
  }
}

void initLastGrupos()
{
  for(uint i=0;i<NUMGRUPOS;i++) {
   lastGrupos[i].inicio = 0;
   lastGrupos[i].final = 0;
  }
}


//Inicia el riego correspondiente al idx del boton (id) pulsado
bool initRiego(uint16_t id)
{
  int bIndex = bID_bIndex(id);
  LOG_DEBUG("Boton:",boton->desc,"boton.index:",bIndex);
  int zIndex = bID_zIndex(id);
  if(zIndex == 999) return false;
  LOG_INFO( "Iniciando riego: ", Boton[bIndex].desc);
  led(Boton[bIndex].led,ON);
  utc = timeClient.getEpochTime();
  time_t t = CE.toLocal(utc,&tcr);
  lastRiegos[zIndex] = t;
  return domoticzSwitch(Boton[bIndex].idx, (char *)"On", DEFAULT_SWITCH_RETRIES);
}


//Termina el riego correspondiente al idx del boton (id) pulsado
bool stopRiego(uint16_t id)
{
  int bIndex = bID_bIndex(id);
  ledID = Boton[bIndex].led;
  LOG_DEBUG( "Terminando riego: ", Boton[bIndex].desc);
  domoticzSwitch(Boton[bIndex].idx, (char *)"Off", DEFAULT_SWITCH_RETRIES);
  if (Estado.estado != ERROR) LOG_INFO( "Terminado OK riego: " , Boton[bIndex].desc );
  else {     //avisa de que no se ha podido terminar un riego
    if (!errorOFF) { //para no repetir bips en caso de stopAllRiego
      errorOFF = true;  // recordatorio error
      tic_parpadeoLedError.attach(0.2,parpadeoLedError);
      tic_parpadeoLedZona.attach(0.4,parpadeoLedZona);
    } 
    return false;
  }
  return true;
}


//Pone a off todos los leds de riegos y grupos y restablece estado led RGB
void resetLeds()
{
  //Apago los leds de multirriego
  for(unsigned int j=0;j<NUMGRUPOS;j++) {
    led(Boton[bID_bIndex(GRUPOS[j])].led,OFF);
  }
  //Apago los leds de riego y posible parpadeo
  tic_parpadeoLedZona.detach();
  for(unsigned int i=0;i<NUMZONAS;i++) {
    led(Boton[bID_bIndex(ZONAS[i])].led,OFF);
  }
  //restablece led RGB
  tic_parpadeoLedError.detach(); //por si estuviera parpadeando
  ledConf(OFF);
}


//Pone a false diversos flags de estado
void resetFlags()
{
  multirriego = false;
  multiSemaforo = false;
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

//Pone a off todos los leds de riegos y detiene riegos (solo si la llamamos con true)
bool stopAllRiego()
{
  LOG_TRACE("");
  //Apago los leds de multirriego
  led(Boton[bID_bIndex(*multi.id)].led,OFF);
  //Apago los leds de riego y posible parpadeo
  tic_parpadeoLedZona.detach();
  for(unsigned int i=0;i<NUMZONAS;i++) { //paramos todas las zonas de riego
    led(Boton[bID_bIndex(ZONAS[i])].led,OFF);
    if(!stopRiego(ZONAS[i])) return false; //al primer error salimos
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
  for (int i=0; i<veces;i++) {
    mitone(BUZZER, NOTE_A6, 50);
    mitone(BUZZER, 0, 50);
  }
}

void longbip(int veces)
{
  LOG_TRACE("longbip llamado");
  for (int i=0; i<veces;i++) {
    mitone(BUZZER, NOTE_A6, 750);
    mitone(BUZZER, 0, 100);
  }
}

void beep(int note, int duration){
  //mitone(BUZZER, note, duration);
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
  if (!displayOff) {
    if (millis() > lastBlinkPause + 1.5*DEFAULTBLINKMILLIS) {  // *1.5 para compensar inercia LCD
      displayOff = true;
      lastBlinkPause = millis();
      display->clearDisplay();
      //LOG_TRACE("blinkPause displayOFF");
      lcd.displayOFF();
    }
  }
  else {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      displayOff = false;
      lastBlinkPause = millis();
      refreshDisplay();
      lcd.displayON();
    }
  }
}

void blinkPauseError()
{
  if (!displayOff) {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      display->print(errorText);
      displayOff = true;
      lastBlinkPause = millis();
    }
  }
  else {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      refreshTime();
      displayOff = false;
      lastBlinkPause = millis();
    }
  }
}


void StaticTimeUpdate(bool refresh)
{
  if (Estado.estado == ERROR) return; //para que en caso de estado ERROR no borre el código de error del display
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;
  //solo se actualiza si cambia hora o REFRESH
  if (refresh) lcd.clear(BORRA2H);
  if(prevseconds != seconds || prevminutes != minutes || refresh) {
    display->printTime(minutes, seconds);
    lcd.displayTime(minutes, seconds); 
    prevseconds = seconds;
    prevminutes = minutes;
  }
}


void refreshDisplay()
{
  display->refreshDisplay();
}


void refreshTime()
{
  unsigned long curMinutes = T.ShowMinutes();
  unsigned long curSeconds = T.ShowSeconds();
  display->printTime(curMinutes,curSeconds);
  if(prevseconds != curSeconds) lcd.displayTime(curMinutes, curSeconds); //LCD solo se actualiza si cambia
  prevseconds = curSeconds;  //LCD

}


/**
 * @brief muestra en el display texto informativo y suenan bips de aviso
 * 
 * @param info = texto a mostrar en el display
 * @param dnum = veces que parpadea el texto en el display
 * @param btype = tipo de bip emitido
 * @param bnum = numero de bips emitidos
 */
void infoDisplay(const char *info, int dnum, int btype, int bnum) {
  LOG_DEBUG("[infoDisplay] Recibido: '",info, "'   (blink=",dnum, ") btype=",btype,"bnum=",bnum);
  #ifdef displayLED
    display->print(info);
    if (btype == LONGBIP) longbip(bnum);
    if (btype == BIP) bip(bnum);
    if (btype == BIPOK) bipOK();
    if (btype == BIPKO) bipKO();
    display->blink(dnum);
  #endif
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
  //if (Estado.estado == ERROR && response.startsWith("Err")) {
  if (response.startsWith("Err")) {
    if (NONETWORK) {  //si estamos en modo NONETWORK devolvemos 999 y no damos error
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
  LOG_TRACE("");
  if(!checkWifi()) {
    if(NONETWORK) return true; //si estamos en modo NONETWORK devolvemos true y no damos error
    else {
      Estado.fase=E1;
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
    if(response == "ErrX") Estado.fase=E3;
    else Estado.fase=E2;
    LOG_ERROR(" ** [ERROR] IDX: ", idx, " [HTTP] GET... failed");
    return false;
  }
  // ya tenemos en response el JSON, lo procesamos
  char* response_pointer = &response[0];
  DynamicJsonDocument jsondoc(2048);
  DeserializationError error = deserializeJson(jsondoc, response_pointer);
  if (error) {
    LOG_ERROR(" **  [ERROR] deserializeJson() failed: ", error.f_str());
    Estado.fase=E2;  
    return false;
  }
  //Tenemos que controlar para que no resetee en caso de no haber leido por un rid malo
  const char *actual_status = jsondoc["result"][0]["Status"];
  if(actual_status == NULL) {
    LOG_ERROR(" **  [ERROR] deserializeJson() failed: Status not found");
    Estado.fase=E2;  
    return false;
  }
  #ifdef EXTRADEBUG
    Serial.printf( "queryStatus verificando, status=%s / actual=%s \n" , status, actual_status);
    Serial.printf( "                status_size=%d / actual_size=%d \n" , strlen(status), strlen(actual_status));
  #endif
  if(simular.ErrorVerifyON) {   // simulamos EV no esta ON en Domoticz
    if(strcmp(status, "On")==0) return false; else return true; 
  } 
  if(simular.ErrorVerifyOFF) {   // simulamos EV no esta OFF en Domoticz
    if(strcmp(status, "Off")==0) return false; else return true; 
  } 
  if(strcmp(actual_status,status) == 0) return true;
  else{
    if(NONETWORK) return true; //siempre devolvemos ok en modo simulacion
    LOG_DEBUG("queryStatus devuelve FALSE, status / actual =",status,"/",actual_status);
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
       LOG_WARN("DOMOTICZSWITH IDX: %d fallo en %s (intento %d de %d)\n", idx, msg, i+1, retries);
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
    LOG_ERROR("DOMOTICZSWITH IDX: %d fallo en %s\n", idx, msg);
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
  #ifdef DEVELOP
    leeSerial();  // para ver si simulamos algun tipo de error
  #endif
  #ifdef DEBUGloops
    if (numloops < 100) {
      ++numloops;
      currentMillisLoop = millis();
    }
    else {
      currentMillisLoop = currentMillisLoop - lastMillisLoop;
      Serial.printf( "[CRONO] %d loops en milisegundos: %d \n" , numloops, currentMillisLoop);
      numloops = 0;
      lastMillisLoop = millis();
    }
    //  Serial.printf( "[CRONO] %d loops en milisegundos: %d \n" , numloops+1, currentMillisLoop);
  #endif
  #ifdef WEBSERVER
  if (webServerAct) {
    procesaWebServer();
    return;
  }
  #endif
  if (!flagV) return;      //si no activada por Ticker salimos sin hacer nada
  if (Estado.estado == STANDBY) Serial.print(F("."));
  if (errorOFF) bip(2);  //recordatorio error grave al parar un riego
  //si estamos en Standby o en Error por falta de conexion verificamos estado actual de la wifi (no en modo NONETWORK)
  if (!NONETWORK && (Estado.estado == STANDBY || (Estado.estado == ERROR && !connected))) {
    if (checkWifi() && Estado.estado!=STANDBY) setEstado(STANDBY); 
    if(!connected) {   //si no estamos conectados a la wifi, intentamos reconexion
      LOG_WARN("INTENTANDO RECONEXION WIFI");
      WiFi.reconnect();
      delay(2000);
      if(checkWifi()) {
        LOG_INFO("¡WIFI reconectada!");
        setEstado(STANDBY);
      } 
    }  
    if (connected && falloAP) {
      LOG_INFO("Wifi conectada despues Setup, leemos factor riegos");
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
void statusError(uint8_t errorID) 
{
  strcpy(errorText, "Err");    
  Estado.estado = ERROR;
  Estado.fase = errorID;
  if (errorID == E0) strcpy(errorText, "Err0");
  else sprintf(errorText, "Err%d", errorID);
  LOG_ERROR("SET ERROR: ", errorText);
  display->print(errorText);
  snprintf(buff, MAXBUFF, ">>> %s <<<", errorText);
  lcd.clear(BORRA2H);
  lcd.setCursor(4,2);
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


void parpadeoLedError()
{
  sLEDR = !sLEDR;
  ledPWM(LEDR,sLEDR);
}

void parpadeoLedZona()
{
  byte estado = ledStatusId(ledID);
  led(ledID,!estado);
}

void parpadeoLedConf()
{
  parpadeoLedError();
  parpadeoLedWifi();
}

//activa o desactiva el(los) led(s) indicadores de que estamos en modo configuracion
void ledConf(int estado)
{
  if(estado == ON) 
  {
    //parpadeo alterno leds ERROR/RED
    ledPWM(LEDB,OFF);
    ledPWM(LEDG,OFF);
    tic_parpadeoLedConf.attach(0.7,parpadeoLedConf);
  }
  else 
  {
    tic_parpadeoLedConf.detach();   //detiene parpadeo alterno leds ERROR/RED
    ledPWM(LEDR,OFF);                   // y los deja segun estado
    NONETWORK ? ledPWM(LEDB,ON) : ledPWM(LEDB,OFF);
    checkWifi();
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
      infoDisplay("dEF-", DEFAULTBLINK, BIPOK, 3);
      lcd.infoclear("load DEFAULT", DEFAULTBLINK, BIPOK);
      delay(MSGDISPLAYMILLIS);
    }  
    else LOG_ERROR(" **  [ERROR] cargando parametros por defecto");
  }
  if (!setupConfig(parmFile, config)) {
    LOG_ERROR(" ** [ERROR] Leyendo fichero parametros " , parmFile);
    if (!setupConfig(defaultFile, config)) {
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


bool setupConfig(const char *p_filename, Config_parm &cfg) 
{
  LOG_INFO("Leyendo fichero parametros", p_filename);
  bool loaded = loadConfigFile(p_filename, config);
  minutes = config.minutes;
  seconds = config.seconds;
  value = ((seconds==0)?minutes:seconds);
  if (loaded) {
    for(int i=0;i<NUMZONAS;i++) {
      int bIndex = bID_bIndex(ZONAS[i]);
      //NOTA: i (zIndex) y bIndex seran iguales si el orden de los botones de las zonas en ZONA[] y Boton[] 
      //es el mismo (seria mas facil si lo fueran).... @TODO ¿como detectarlo al compilar?
      if (bIndex != i) LOG_WARN("            @@@@@@@@@@@@  bIndex != zIndex  @@@@@@@@@@");
      //actualiza el IDX leido sobre estructura Boton:
      Boton[bIndex].idx = config.botonConfig[i].idx;
      //actualiza la descripcion leida sobre estructura Boton:
      strlcpy(Boton[bIndex].desc, config.botonConfig[i].desc, sizeof(Boton[bIndex].desc));
    }
    #ifdef EXTRADEBUG
      printFile(p_filename);
    #endif
    return true;
  }  
  LOG_ERROR(" ** [ERROR] parámetros de configuración no cargados");
  return false;
}


// funciones solo usadas en DEVELOP
// (es igual, el compilador no las incluye si no son llamadas)
#ifdef DEVELOP
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

bool serialDetect() {
  delay(100);
  Serial.println();
  Serial1.begin(0, SERIAL_8N1, -1, -1, true, 11000UL);  // Passing 0 for baudrate to detect it, the last parameter is a timeout in ms
  unsigned long detectedBaudRate = Serial1.baudRate();
  if(detectedBaudRate) {
    Serial.printf("Detected baudrate is %lu\n", detectedBaudRate);
    return(true);
  } else {
    Serial.println("No baudrate detected, Serial1 will not work!");
    return(false);
  }
}

#endif  
