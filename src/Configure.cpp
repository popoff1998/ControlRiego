#include "Control.h"

Configure::Configure()
{
     // Constructor
      this->reset();
      _currentItem = 0;
      _data_pos_valid = false;
      static_assert(IDX_MULT == 0, "IDX_MULT debe ser el primero en el ENUM");
      static_assert(__ENDLINE__ == NUM_ITEMS - 1, "__ENDLINE__ debe ser el último elemento del menu");
      LOG_DEBUG("Constructor Configure, NUM_ITEMS:", NUM_ITEMS);
}

void Configure::reset()
{
      all_configureflags = 0;
      _actualZona = 0;
      _actualGrupo = 0;
}


// Configuramos IDX asociado a la zona pasando el puntero al botón
void Configure::Idx_process_start()
{
      if (boton == nullptr) return; // Control preventivo
      this->reset();
      _configuringIdx = true;
      _actualZona = boton->zNumber();
      int zIndex = boton->zNumber() - 1; 
      tm.value = config.zona[zIndex].idx;
      setEncoderRange(0, 999, tm.value, 100);

      LOG_INFO("[ConF] configurando IDX boton:", config.zona[zIndex].desc);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "IDX de:  %s", config.zona[zIndex].desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, " actual %d", tm.value);
      lcd.info(buff, 3);
      led(boton->led, ON);
}

//  actualizamos en pantalla el nuevo IDX de la zona
void Configure::Idx_process_update()
{     
      snprintf(buff, MAXBUFF, " nuevo IDX ZONA%d %d", _actualZona, tm.value);
      lcd.info(buff, 4);
}  

//  salvamos en config el nuevo IDX de la zona
void Configure::Idx_process_end()
{
      config.zona[_actualZona-1].idx = (uint16_t)tm.value;
      saveConfigRequired = true;
      S_BOTON* pBoton = getBotonPointer(Zonas[_actualZona-1]);
      
      LOG_INFO("Save Zona",_actualZona,"(",pBoton->desc,") IDX :",tm.value);
      lcd.info(" << GUARDADO >>",3);
      sonido.bipOK();
      delay(config.msgdisplaymillis);  // para que se vea el msg
      led(pBoton->led,OFF);

      this->menu(0);  // vuelve a mostrar menu de configuracion, primera linea
}

// configuramos tiempo riego por defecto
void Configure::Time_process_start()
{
      this->reset();
      _configuringTime = true;
      tm.minutes = config.minutes;
      tm.seconds = config.seconds;
      setEncoderTime();

      lcd.setCursorBlink(_data_pos[_currentItem],1);
      sonido.bip(1);
}              

//  actualizamos en menu tiempo por defecto modificado
void Configure::Time_process_update()
{
      LOG_TRACE("DEFAULT TIME, minutes:",tm.minutes," secons:",tm.seconds);
      sprintf(buff, "%02d:%02d",tm.minutes,tm.seconds); 
      lcd.print(buff);
      lcd.setCursorBlink(_data_pos[_currentItem],1);
}

//  salvamos en config el nuevo tiempo por defecto
void Configure::Time_process_end()
{
      config.minutes = tm.minutes;
      config.seconds = tm.seconds;
      saveConfigRequired = true;

      LOG_INFO("Save DEFAULT TIME, minutes:",tm.minutes," secons:",tm.seconds);
      lcd.setCursor(0,0);  // solo para anular visibilidad y parpadeo del cursor
      sonido.bipOK();

      this->menu();  // vuelve a mostrar menu de configuracion
}

//  configuramos Range
void Configure::Range_process_start(int min, int max, int aceleracion, int rangefactor)
{
      this->reset();
      _configuringRange = true;
      _rangeFactor = rangefactor;
      tm.value = *configValuep;
      setEncoderRange(min, max, tm.value, aceleracion);

      LOG_DEBUG("[ConF] configurando Rango: '",_currenItemText,"' valor actual:", tm.value);
      LOG_TRACE("[ConF]   min:",min," max:",max," aceleracion:",aceleracion," rangefactor:",rangefactor, "_data_pos:",_data_pos[_currentItem]);
      lcd.setCursorBlink(_data_pos[_currentItem],1);
      sonido.bip(1);
}

// actualizamos rango en pantalla
void Configure::Range_process_update()
{
      LOG_DEBUG("[ConF] rangefactor:",_rangeFactor,"multiplicador:", _rangeFactor/100.0, "resultado=", tm.value*(_rangeFactor/100.0));
      snprintf(buff, MAXBUFF-_data_pos[_currentItem], "%g        ", tm.value*(_rangeFactor/100.0));
      lcd.print(buff);
      lcd.setCursorBlink(_data_pos[_currentItem],1);
}

//  salvamos en config la nueva Range
void Configure::Range_process_end()
{
      *configValuep = tm.value;
      saveConfigRequired = true;

      LOG_INFO("Save new value:", *configValuep);
      lcd.setCursor(0,0);  // solo para anular visibilidad y parpadeo del cursor
      ledYellow(ON);  // para actualizar ya brillo led status por si se hubiera cambiado
      this->configuringMelody() ? sonido.bipFIN() : sonido.bipOK(); // y la hacemos sonar en lugar de bipOK

      this->menu();  // vuelve a mostrar menu de configuracion
}

//  configuramos grupo multirriego
void Configure::Multi_process_start(int grupo)
{
      this->reset();
      _configuringMulti = true;
      _actualGrupo = grupo;
      this->configureMulti_display();
}

//  configuramos grupo multirriego temporal
void Configure::MultiTemp_process_start()
{
      // LOG_DEBUG("MultiTemp_process_start: multi.w_size=",multi.w_size);  
      this->reset();
      _configuringMultiTemp = true;
      _actualGrupo = 0;
      this->configureMulti_display();
}

// Muestra en pantalla mensaje para configurar grupo multirriego y enciende leds de las zonas actuales del grupo)
void Configure::configureMulti_display()    
{

      LOG_INFO("Configurando: GRUPO",_actualGrupo,"(",multi.desc,") multi.size=", *multi.size);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "grupo%d: %s",_actualGrupo, multi.desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, "pulse ZONAS (+PAUSE)");
      lcd.info(buff, 3);

      if(!_configuringMultiTemp) {    // no encendemos leds si grupo TEMPORAL
        displayLedsGrupo(); // mostramos leds de las zonas ya configuradas para el grupo
        led(multi.id->led, ON); // encendemos led del boton del grupo
      }  
}              

void Configure::Multi_process_update()
{
      // LOG_DEBUG("Multi_process_update: boton->bID=", boton->bID, "multi.w_size=",multi.w_size);  
      int zNumber = boton->zNumber();
      if (multi.w_size < ZONASXGRUPO) {  //max. zonas por grupo
        multi.zserie_pBoton[multi.w_size] = boton;  // apuntador de la zona en Boton[]
        multi.w_zserie[multi.w_size] = zNumber ;  // numero de la zona
        multi.w_size = multi.w_size + 1;

        LOG_INFO("[ConF] añadiendo ZONA",zNumber,"(",config.zona[zNumber-1].desc,") multi.w_size=",multi.w_size);
        led(boton->led,ON);
        displayLCDGrupo(WORKING, 4);
      }
      else sonido.bipKO();  
}

// actualizamos config con las zonas introducidas
void Configure::Multi_process_end()
{
      saveConfigRequired = true;
      char grupoText[21];
      if (multi.w_size) {  //solo si se ha pulsado alguna zona
        // actualizamos config con tamaño y zonas introducidas para el grupo
        *multi.size = multi.w_size;
        int g = _actualGrupo;
        for (int i=0; i<multi.w_size; ++i) {
          config.group[g-1].zNumber[i] = multi.w_zserie[i];
        }
        LOG_INFO("Config updated : GRUPO",g,"tamaño:",*multi.size,"(",multi.desc,")");
        #ifdef EXTRADEBUGMULTI
        printMultiGroup(g-1);
        #endif
        snprintf(grupoText, sizeof(grupoText), "Actualizado GRUPO%d", _actualGrupo);
      }
      else {   //se borra contenido del grupo
        *multi.size = 0;
        LOG_INFO("vaciado GRUPO",_actualGrupo,"tamaño:",*multi.size,"(",multi.desc,")");
        snprintf(grupoText, sizeof(grupoText), ">> Vaciado GRUPO%d <<", _actualGrupo);
      }
      
      lcd.info(grupoText,2);
      lcd.clear(BORRA2H);
      sonido.bipOK();
      delay(config.msgdisplaymillis);
      ultimosRiegos(HIDE);
      led(multi.id->led, OFF);
      this->menu(0);  // vuelve a mostrar menu de configuracion, primera linea
}

// Mostramos en pantalla mensaje para iniciarlo y activamos flag para que al salir de ConF se lance multirriego temporal
void Configure::MultiTemp_process_end()
{
      if (!multi.w_size) return;  // si no se ha definido ninguna zona no hacemos nada  
      _MultiTempReady = true;  // al salir de ConF se lanzará el multirriego temporal con las zonas configuradas

      LOG_INFO("process_end grupo TEMPORAL : GRUPO",_actualGrupo,"tamaño:",*multi.size);
      sonido.bipOK();
      lcd.info("  >> libere STOP <<",1);
      lcd.info("para comenzar riego",2);
      lcd.info("de las zonas:",3);

      #ifdef EXTRADEBUGMULTI
        printMulti();
      #endif
}

void Configure::toggle(bool &value)
{
      value = !value;
      LOG_DEBUG("item '", _currenItemText,"' -> New value:", value);
      lcd.setCursor(_data_pos[_currentItem],1);
      lcd.print(value ? "ON " : "OFF");
      sonido.bip(2);
      saveConfigRequired = true;
}

void Configure::menu(int item)
{
      this->reset();
      _configuringMenu = true;
      if(item >= 0 && item < __ENDLINE__) _currentItem = item;
      LOG_DEBUG("item recibido:", item, "menu item:",_currentItem);
      _maxItems = this->showMenu(_currentItem);
      setEncoderMenu(_maxItems, _currentItem);
      LOG_TRACE("_currentitem=",_currentItem);
}


int Configure::showMenu(int itemIndex)
{
  static const char* parteFija[NUM_ITEMS] = { nullptr };

  if (!_data_pos_valid) {
    //  OJO el orden en que se muestran en el menu no depende de su posicion aqui
    //  sino del orden en que se definen en el enum _menuItems.
    //  Texto parte fija del menu:
                          /*    <------17------->     maxima longitud */ 
    parteFija[IDX_MULT]      = "Botones IDX/MULT.";
    parteFija[DFLT_TIME]     = "Dflt TIME: ";
    parteFija[COPY_BACKUP]   = "Copy to BACKUP";
    parteFija[WIFI_PARM]     = "WIFI parm (AP)";
    #ifdef WEBSERVER 
    parteFija[WEBSERVER_ACT] = "WEBSERVER act.";
    #endif
    parteFija[LOAD_BACKUP]   = "Load from BACKUP";
    parteFija[ESP32_TEMP]    =  "ESP32 temp: xx/";
    parteFija[LED_DIMM_LVL]  = "Led DIMM lvl: ";
    parteFija[LED_MAX_LVL]   = "Led MAX lvl: ";
    parteFija[TEMP_ADJ]      = "Temp adj.: ";
    parteFija[TEMP_SOURCE]   = "TEMP:       ";
    parteFija[REM_TEMP_IDX]  = "Rem TEMP IDX: ";
    parteFija[MSG_TIME]      = "MSG time: ";
    parteFija[MUTE]          = "MUTE: ";
    parteFija[VOLUME]        = "VOLUME: ";
    parteFija[FIN_MELODY]    = "FinMELODY: ";
    parteFija[NIVEL_WIFI]    = "NIVEL wifi: ";
    parteFija[XNAME_ONOFF]   = "XNAME: ";
    parteFija[VERIFY_ONOFF]  = "VERIFY: ";
    parteFija[DYNAMIC]       = "DYNAMIC: ";
    parteFija[LASTRIEGOS24]  = "RIEGOS 24H: ";
    #ifdef LOGTOFILE 
    parteFija[WARNTOLOG]     = "DEBUG mode: ";
    #endif
    parteFija[__ENDLINE__]   = "-----------------";
                            /*  <------17------->     maxima longitud */ 

    for(int r = 0; r < NUM_ITEMS; r++) {
      if(parteFija[r]) _data_pos[r] = strlen(parteFija[r]) + 3;
      LOG_DEBUG("menuitem",r,"longitud",_data_pos[r]-3,"data_pos",_data_pos[r]);
    }
    _data_pos_valid = true;
  }

  _currentItem = itemIndex;
  _currenItemText = parteFija[_currentItem];
  LOG_DEBUG("item:",_currentItem,"'",_currenItemText,"'");

  // Muestra menu (4 lineas) en pantalla
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menu Configuracion:");
  lcd.setCursor(0,1);
  lcd.print("->");
  for (int lin = 1; lin < 4; lin++) {
    if (itemIndex > __ENDLINE__ || parteFija[itemIndex] == nullptr) break;
    lcd.setCursor(3, lin);
    String itemL = String(parteFija[itemIndex]);
    // Construye la linea del menu añadiendo la parte variable a la fija (solo se generan las lineas a mostrar)
    switch (itemIndex) {
      case DFLT_TIME: snprintf(buff, MAXBUFF, "%02d:%02d", config.minutes, config.seconds); itemL += buff; break;
      case ESP32_TEMP: itemL = "ESP32 temp: " + String((int)temperatureRead()) + "/" + String(config.warnESP32temp); break;
      case LED_DIMM_LVL: itemL += String(config.dimmlevel); break;
      case LED_MAX_LVL: itemL += String(config.maxledlevel); break;
      case TEMP_ADJ: snprintf(buff, MAXBUFF, "%+g", config.tempOffset*(TEMP_OFFSET_FACTOR/100.0));; itemL += buff; break;
      case TEMP_SOURCE: itemL = (config.tempRemote==0 ?  "TEMP: LOCAL " : "TEMP: REM.  ") + (readTemp()==999 ? "--" : String(readTemp())); break;
      case REM_TEMP_IDX: itemL += String(config.tempRemoteIdx); break;
      case MSG_TIME: itemL += String(config.msgdisplaymillis); break;
      case MUTE: itemL += (config.mute ? "ON" : "OFF"); break;
      case VOLUME: itemL += String(config.volume); break;
      case FIN_MELODY: itemL += String(config.finMelody); break;
      case NIVEL_WIFI: itemL += (config.showwifilevel ? "ON" : "OFF"); break;
      case XNAME_ONOFF: itemL += (config.xname ? "ON" : "OFF"); break;
      case VERIFY_ONOFF: itemL += (config.verify ? "ON" : "OFF"); break;
      case DYNAMIC: itemL += (config.dynamic ? "ON" : "OFF"); break;
      case LASTRIEGOS24: itemL += (config.lastr24 ? "ON" : "OFF"); break;
      #ifdef LOGTOFILE 
      case WARNTOLOG: itemL += (config.logWarnToFile ? "ON" : "OFF"); break;
      #endif
    }
    lcd.print(itemL);
    itemIndex++;
  }
  return __ENDLINE__ - 1; // maximo indice del menu (excluyendo __ENDLINE__ no seleccionable)
}      


// ejecutamos opcion seleccionada del menu
void Configure::procesaSelectMenu() 
{
    switch(_currentItem) {  
        case IDX_MULT :      //configuramos boton de zona (IDX Domoticz asociado) o de grupo (zonas que lo componen)
                lcd.infoclear("pulse ZONA o GRUPO",1);
                lcd.info("a configurar...",2);   
                break;
        case DFLT_TIME :      //configuramos tiempo riego por defecto
                this->Time_process_start();   
                break;
        case COPY_BACKUP :  // copiamos fichero parametros en fichero backup
                if (copyFile(parmFile, backupParmFile)) {    // parmFile --> backupParmFile
                  LOG_INFO("[ConF] salvado fichero de parametros actuales como BACKUP");
                  lcd.infoclear("Save to BACKUP OK", BLINKDISPLAY, BIPOK);
                  delay(config.msgdisplaymillis); 
                }
                else BIPKO;  
                this->menu();  // vuelve a mostrar menu de configuracion 
                break;
        case WIFI_PARM :   // activamos AP y portal de configuracion (bloqueante)
                LOG_INFO("[ConF]  activamos AP y portal de configuracion");
                ledYellow(OFF);
                startConfigPortal();
                ledYellow(ON);
                this->menu();  // vuelve a mostrar menu de configuracion
                break; 
        #ifdef WEBSERVER
        case WEBSERVER_ACT :  // activamos webserver (no bloqueante, pero no respodemos a botones)
                Estado.connected ? setupWS() : sonido.bipKO();
                break;
        #endif 
        case LOAD_BACKUP :   // carga parametros de backup y reinicia
                if (copyFile(backupParmFile, parmFile)) {    // backupParmFile --> parmFile
                  lcd.infoclear("load BACKUP OK", BLINKDISPLAY, BIPOK);
                  lcd.info(">> RESET en 2 seg <<",3);
                  delay(config.msgdisplaymillis);
                  LOG_WARN("carga parametros de backup OK, RESET ESP32");
                  resetESP32();  // reset ESP32
                }
                else sonido.bipKO();  
                this->menu();  // vuelve a mostrar menu de configuracion
                break;
        case ESP32_TEMP :      //configuramos temperatura aviso ESP32
                configValuep = &config.warnESP32temp;  
                this->Range_process_start(40, 99);   
                break;
        case LED_DIMM_LVL :      //configuramos nivel atenuacion led STATUS (RGB)
                configValuep = &config.dimmlevel;  
                this->Range_process_start(10, config.maxledlevel);   
                break;
        case LED_MAX_LVL :      //configuramos nivel maximo brillo led STATUS (RGB)
                configValuep = &config.maxledlevel;  
                this->Range_process_start(config.dimmlevel, 255);   
                break;
        case TEMP_ADJ :     //configuramos correccion temperatura mostrada
                configValuep = &config.tempOffset;  
                this->Range_process_start(-5, 5, 100, TEMP_OFFSET_FACTOR);   
                break;
        case TEMP_SOURCE :   // toggle temperatura mostrada (sensor local o remoto)
                config.tempRemote = !config.tempRemote;
                if(readTemp()==999) { // si no esta disponible no dejamos cambiar
                  config.tempRemote = !config.tempRemote;
                  sonido.bipKO();
                }
                else {
                  sonido.bip(2);
                  saveConfigRequired = true;
                }
                this->menu();  // vuelve a mostrar menu de configuracion
                break; 
        case REM_TEMP_IDX :     //configuramos IDX del sensor de temperatura remoto en el Domotiz
                configValuep = &config.tempRemoteIdx;  
                this->Range_process_start(0, 999, 100); // 0 = no definido  
                break;
        case MSG_TIME :     //configuramos tiempo que se muestran los mensajes (en milisegundos)
                configValuep = &config.msgdisplaymillis;  
                this->Range_process_start(1000, 4000, 500);   
                break;
        case MUTE :   // toggle MUTE
                this->toggle(config.mute);
                break;
        case VOLUME :   //configuramos volumen de la melodia
                configValuep = &config.volume;  
                this->Range_process_start(1, 10);   
                break;
        case FIN_MELODY :   //configuramos melodia final riego grupo
                configValuep = &config.finMelody;  
                this->Range_process_start(1, 4);   
                _configuringMelody = true;
                break; 
        case NIVEL_WIFI :   // toggle display nivel señal wifi
                this->toggle(config.showwifilevel);
                break; 
        case XNAME_ONOFF :   // toggle actualizar nombres zonas con los del Domoticz
                this->toggle(config.xname);
                break;
        case VERIFY_ONOFF :   // toggle verificar estado dispositivo en el Domoticz
                this->toggle(config.verify);
                break;
        case DYNAMIC :   // toggle añadido/borrado dinamico de zonas durante el riego
                this->toggle(config.dynamic);
                break;
        case LASTRIEGOS24 :   // toggle ultimos riegos desde 0:00h o ultimas 24h
                this->toggle(config.lastr24);
                break;
        #ifdef LOGTOFILE 
        case WARNTOLOG :   // toggle log de mensajes WARN en fichero de log
                this->toggle(config.logWarnToFile);
                setLogToFile();
                break;
        #endif
        default:         
                LOG_DEBUG("salimos del CASE del MENU sin realizar accion");
      }
}

//  escritura de parametros a fichero si procede y salimos de ConF
void Configure::exit()
{
      if (saveConfigRequired) saveConfig();  
      #ifdef WEBSERVER
        if (webServerAct) {
          endWS();           //al salir de modo ConF no procesaremos peticiones al webserver
          LOG_INFO("[ConF][WS] desactivado webserver");
        }
      #endif
      if (Estado.estado == ERROR) return;
      // Si salimos de modo ConF para comenzar multirriego temporal, ponemos STANDBY silencioso
      // (sin cambios en la UI) , si no ponemos STANDBY normal.
      _MultiTempReady ? setStateMachine(STANDBY) : setEstado(STANDBY);
      this->reset();
      _currentItem = 0;
      setEncoderTime();
}
