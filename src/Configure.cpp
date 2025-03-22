#include "Configure.h"

Configure::Configure(struct Config_parm &conf) : config(conf)
{
      this->reset();
      _currentItem = 0;
      _data_pos_valid = false;
}

void Configure::menu(int item)
{
      this->reset();
      _configuringMenu = true;
      if(item >= 0) _currentItem = item;
      _maxItems = this->showMenu(_currentItem);
      setEncoderMenu(_maxItems, _currentItem);
      LOG_DEBUG("_currentitem=",_currentItem);
}

void Configure::reset()
{
      all_configureflags = 0;
      _actualIdxIndex = 0;
      _actualGrupo = 0;
}

bool Configure::configuringTime(void)
{
  return _configuringTime;
}

bool Configure::configuringIdx()
{
  return _configuringIdx;
}

bool Configure::configuringRange()
{
  return _configuringRange;
}

bool Configure::configuringMulti()
{
  return _configuringMulti;
}

bool Configure::configuringMultiTemp()
{
  return _configuringMultiTemp;
}

bool Configure::configuringMelody()
{
  return _configuringMelody;
}

bool Configure::statusMenu()
{
  return _configuringMenu;
}

//  configuramos IDX asociado al boton de zona
void Configure::Idx_process_start(int index)
{
      this->reset();
      _configuringIdx = true;
      _actualIdxIndex = index;
      tm.value = config.zona[boton->znumber-1].idx;
      setEncoderRange(0, 99, tm.value, 100);

      LOG_INFO("[ConF] configurando IDX boton:",config.zona[boton->znumber-1].desc);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "IDX de:  %s", config.zona[boton->znumber-1].desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, " actual %d", tm.value);
      lcd.info(buff, 3);
      
      led(Boton[_actualIdxIndex].led,ON);
}

//  actualizamos en pantalla el nuevo IDX de la zona
void Configure::Idx_process_update()
{     
      int currentZona = Boton[_actualIdxIndex].znumber;
      snprintf(buff, MAXBUFF, " nuevo IDX ZONA%d %d", currentZona, tm.value);
      lcd.info(buff, 4);
}  

//  salvamos en config y Boton el nuevo IDX de la zona
void Configure::Idx_process_end()
{
      int zIndex = Boton[_actualIdxIndex].znumber-1;
      config.zona[zIndex].idx = (uint16_t)tm.value;
      saveConfig = true;
      
      LOG_INFO("Save Zona",zIndex+1,"(",Boton[_actualIdxIndex].desc,") IDX :",tm.value);
      lcd.info("guardado IDX",2);
      lcd.clear(BORRA2H);
      bipOK();
      delay(config.msgdisplaymillis);  // para que se vea el msg
      led(Boton[_actualIdxIndex].led,OFF);

      tmvalue();  // restaura tiempo (en lugar del IDX)
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
      bip(1);
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
      saveConfig = true;

      LOG_INFO("Save DEFAULT TIME, minutes:",tm.minutes," secons:",tm.seconds);
      lcd.setCursor(0,0);  // solo para anular visibilidad y parpadeo del cursor
      bipOK();

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

      LOG_DEBUG("[ConF] configurando RANGO Item:",_currentItem, "valor actual:",tm.value,"_data_pos:",_data_pos[_currentItem]);
      LOG_DEBUG("[ConF]                    rangefactor:",_rangeFactor);
      lcd.setCursorBlink(_data_pos[_currentItem],1);
      bip(1);
}

// actualizamos rango en pantalla
void Configure::Range_process_update()
{
      LOG_DEBUG("[ConF] rangefactor:",_rangeFactor,"multiplicador:",(float)_rangeFactor/100,"resultado=",tm.value*((float)_rangeFactor/100));
      snprintf(buff, MAXBUFF-_data_pos[_currentItem], "%g        ", (float)tm.value*((float)_rangeFactor/100));
      lcd.print(buff);
      lcd.setCursorBlink(_data_pos[_currentItem],1);
}

//  salvamos en config la nueva Range
void Configure::Range_process_end()
{
      *configValuep = tm.value;
      saveConfig = true;

      LOG_INFO("Save new value:", *configValuep);
      lcd.setCursor(0,0);  // solo para anular visibilidad y parpadeo del cursor
      ledYellow(ON);  // para actualizar ya brillo led status por si se hubiera cambiado
      config_volume = config.volume;  // actualizamos volumen sonidos por si hubiera cambiado
      config_finMelody = config.finMelody;  // actualizamos melodia final riego grupo por si hubiera cambiado
      this->configuringMelody() ? bipFIN() : bipOK();

      this->menu();  // vuelve a mostrar menu de configuracion
}

//  configuramos grupo multirriego
void Configure::Multi_process_start(int grupo)
{
      this->reset();
      _configuringMulti = true;
      _actualGrupo = grupo;
      multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
      this->configureMulti_display();
}

//  configuramos grupo multirriego temporal
void Configure::MultiTemp_process_start(void)
{
      this->reset();
      _configuringMultiTemp = true;
      _actualGrupo = NUMGRUPOS+1;   // grupo temporal: n+1
      multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
      this->configureMulti_display();
}

//  se añade zona pulsada a grupo
void Configure::configureMulti_display(void)    
{

      LOG_INFO("Configurando: GRUPO",_actualGrupo,"(",multi.desc,")");
      LOG_DEBUG("en configuracion de MULTIRRIEGO, setMultibyId devuelve: Grupo",_actualGrupo,"(",multi.desc,") multi.size=", *multi.size);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "grupo%d: %s",_actualGrupo, multi.desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, "pulse ZONAS (+PAUSE)");
      lcd.info(buff, 3);

      if(!_configuringMultiTemp) displayGrupo(multi.serie, *multi.size); // no encendemos leds si grupo TEMPORAL 
}              

void Configure::Multi_process_update()
{
      int zNumber = boton->znumber;

      if (multi.w_size < ZONASXGRUPO) {  //max. zonas por grupo
        multi.serie[multi.w_size] = boton->bID;  // bId de la zona
        multi.zserie[multi.w_size] = zNumber ;  // numero de la zona
        multi.w_size = multi.w_size + 1;

        LOG_INFO("[ConF] añadiendo ZONA",zNumber,"(",config.zona[zNumber-1].desc,") multi.w_size=",multi.w_size);
        led(boton->led,ON);
        displayLCDGrupo(multi.zserie, multi.w_size,4,0);
      }
      else bipKO();  
}

// actualizamos config con las zonas introducidas
void Configure::Multi_process_end(bool clear)
{
      if (multi.w_size && !clear) {  //solo si se ha pulsado alguna
        *multi.size = multi.w_size;
        int g = _actualGrupo;
        for (int i=0; i<multi.w_size; ++i) {
          config.group[g-1].zNumber[i] = multi.zserie[i];
        }
        saveConfig = true;

        LOG_INFO("SAVE PARM Multi : GRUPO",g,"tamaño:",*multi.size,"(",multi.desc,")");
        printMultiGroup(config, g-1);
        bipOK();
        lcd.info("guardado GRUPO",2);
        lcd.clear(BORRA2H);
        delay(config.msgdisplaymillis);
      }
      else if(clear) {   //se borra contenido del grupo
        *multi.size = 0;
        saveConfig = true;

        LOG_INFO("borrado GRUPO",_actualGrupo,"tamaño:",*multi.size,"(",multi.desc,")");
        bipOK();
        lcd.info("vaciado GRUPO",2);
        lcd.clear(BORRA2H);
        delay(config.msgdisplaymillis);

      }
      ultimosRiegos(HIDE);
      led(Boton[bID2bIndex(*multi.id)].led,OFF);
      this->menu(0);  // vuelve a mostrar menu de configuracion, primera linea
}

// actualizamos config con las zonas introducidas para grupo temporal
void Configure::MultiTemp_process_end()
{
      if (multi.w_size) {  //solo si se ha pulsado alguna
        *multi.size = multi.w_size;
        //int g = _actualGrupo;
        // for (int i=0; i<multi.w_size; ++i) {
        //   config.group[g-1].zNumber[i] = multi.zserie[i];
        // }
        saveConfig = true;  //  solo para indicar que hemos salvado grupo temporal y tenemos que iniciarlo

        LOG_INFO("SAVE config grupo TEMPORAL : GRUPO",_actualGrupo,"tamaño:",*multi.size,"(",multi.desc,")");
        printMultiGroup(config, _actualGrupo-1);
        bipOK();
        lcd.info("  >> libere STOP <<",1);
        lcd.info("para comenzar riego",2);
        lcd.info("de las zonas:",3);
      }
}

int Configure::get_ActualIdxIndex(void)
{
  return _actualIdxIndex;
}

int Configure::get_ActualGrupo(void)
{
  return _actualGrupo;
}

int Configure::get_currentItem(void)
{
  return _currentItem;
}

int Configure::get_datapos(void)
{
  return _data_pos[_currentItem];
}

int Configure::get_maxItems(void)
{
  return _maxItems;
}


//  escritura de parametros a fichero si procede y salimos de ConF
void Configure::exit()
{
      this->reset();
      _currentItem = 0;
      setEncoderTime();
      if (saveConfig) {
        LOG_INFO("saveConfig=true  --> salvando parametros a fichero");
        if (saveConfigFile(parmFile, config)) {
          lcd.infoclear("SAVED parameters", DEFAULTBLINK, BIPOK);
          delay(config.msgdisplaymillis);
        }  
        saveConfig = false;
      }
      #ifdef WEBSERVER
        if (webServerAct) {
          endWS();           //al salir de modo ConF no procesaremos peticiones al webserver
          LOG_INFO("[ConF][WS] desactivado webserver");
        }
      #endif
      resetLeds();
      LOG_TRACE("[poniendo estado STANDBY]");
      setEstado(STANDBY);
}


int Configure::showMenu(int opcion)
{

      String opcionesMenuConf[__ENDLINE__ + 1] = {};
      
      //  OJO el orden en que se muestran en el menu no depende de su posicion aqui
      //  sino del orden en que se definen en el enum _menuItems.
      //  Texto fijo:
                                    /*   <------17------->     maxima longitud */ 
      opcionesMenuConf[IDX_MULT]      = "botones IDX/MULT.";
      opcionesMenuConf[DFLT_TIME]     = "dflt TIME: ";
      opcionesMenuConf[COPY_BACKUP]   = "copy to BACKUP";
      opcionesMenuConf[WIFI_PARM]     = "WIFI parm (AP)";
      opcionesMenuConf[WEBSERVER_ACT] = "WEBSERVER";
      opcionesMenuConf[LOAD_BACKUP]   = "load from BACKUP";
      opcionesMenuConf[ESP32_TEMP]    =  "ESP32 temp: xx/";
      opcionesMenuConf[LED_DIMM_LVL]  = "led DIMM lvl: ";
      opcionesMenuConf[LED_MAX_LVL]   = "led MAX lvl: ";
      opcionesMenuConf[TEMP_ADJ]      = "TEMP adj.: ";
      opcionesMenuConf[TEMP_SOURCE]   = "TEMP:       ";
      opcionesMenuConf[REM_TEMP_IDX]  = "rem TEMP IDX: ";
      opcionesMenuConf[MSG_TIME]      = "MSG time: ";
      opcionesMenuConf[MUTE]          = "MUTE: ";
      opcionesMenuConf[VOLUME]        = "VOLUME: ";
      opcionesMenuConf[FIN_MELODY]    = "finMELODY: ";
      opcionesMenuConf[NIVEL_WIFI]    = "NIVEL wifi: ";
      opcionesMenuConf[XNAME_ONOFF]   = "XNAME: ";
      opcionesMenuConf[VERIFY_ONOFF]  = "VERIFY: ";
      opcionesMenuConf[DYNAMIC]       = "DYNAMIC: ";
      opcionesMenuConf[__ENDLINE__]   = "-----------------";
                                    /*   <------17------->     maxima longitud */ 

      const int MAXOPCIONES = ELEMENTCOUNT(opcionesMenuConf);
      LOG_DEBUG("sizeof Total",sizeof(opcionesMenuConf),"sizeof [0]",sizeof(opcionesMenuConf[0]));
      if(!_data_pos_valid) {
          for(int r=0; r<MAXOPCIONES; r++) {
            _data_pos[r] = opcionesMenuConf[r].length() + 3;
            //_data_pos[r] = strlen(opcionesMenuConf[r].c_str()) + 3; // otra opcion
            LOG_DEBUG("menuitem",r,"longitud",_data_pos[r]-3,"data_pos",_data_pos[r]);
            LOG_DEBUG("menuitem",r,"sizeof",sizeof(opcionesMenuConf[r]));
          }
          _data_pos_valid = true;
      }    

      //  Texto variable:
      sprintf(buff, "%02d:%02d",config.minutes,config.seconds);
      opcionesMenuConf[DFLT_TIME]  += buff;
      opcionesMenuConf[ESP32_TEMP]  =  "ESP32 temp: " + String((int)temperatureRead()) + "/" + String(config.warnESP32temp);
      opcionesMenuConf[LED_DIMM_LVL]  += String(config.dimmlevel);
      opcionesMenuConf[LED_MAX_LVL]  += String(config.maxledlevel);
      sprintf(buff, "%+g", (float)config.tempOffset/2); //elimina ceros decimales al final y pone + si positivo
      opcionesMenuConf[TEMP_ADJ]  += buff;
      opcionesMenuConf[TEMP_SOURCE] = (config.tempRemote ?  "TEMP: REM.  " : "TEMP: LOCAL ") + (readTemp()==999 ? "--" : String(readTemp()));
      opcionesMenuConf[REM_TEMP_IDX] += String(config.tempRemoteIdx);
      opcionesMenuConf[MSG_TIME] += String(config.msgdisplaymillis);
      opcionesMenuConf[MUTE] += (config.mute ? "ON" : "OFF");
      opcionesMenuConf[VOLUME] += String(config.volume);
      opcionesMenuConf[FIN_MELODY] += String(config.finMelody);
      opcionesMenuConf[NIVEL_WIFI] += (config.showwifilevel ? "ON" : "OFF");
      opcionesMenuConf[XNAME_ONOFF] += (config.xname ? "ON" : "OFF");
      opcionesMenuConf[VERIFY_ONOFF] += (config.verify ? "ON" : "OFF");
      opcionesMenuConf[DYNAMIC] += (config.dynamic ? "ON" : "OFF");


      LOG_DEBUG("opcion=",opcion,"_currentitem=",_currentItem,"MAXOPCIONES=",MAXOPCIONES);
      _currentItem = opcion;
      snprintf(_currenItemText,18,"%s",opcionesMenuConf[_currentItem].c_str());
      LOG_DEBUG("_currentitem=",_currentItem,"_currenItemText=",_currenItemText);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Menu Configuracion:");
      lcd.setCursor(0,1);
      lcd.print("->");
      //lcd.print("--" "\x7E");  //  "-->"
      for (int l=1; l<4; l++) {
        LOG_TRACE("linea ", l, " opcion ", opcion);
        lcd.setCursor(3,l);
        lcd.print(opcionesMenuConf[opcion]);
        opcion++;
        if(opcion >= MAXOPCIONES) break; 
      }
      return MAXOPCIONES;
}


// ejecutamos opcion seleccionada del menu
void Configure::procesaSelectMenu() 
{
    boton = NULL; //para que no se procese mas adelante  TODO ¿es necesario?

    switch(_currentItem) {  
        case IDX_MULT :      //configuramos boton de zona (IDX Domoticz asociado) o de grupo (zonas que lo componen)
                lcd.infoclear("pulse ZONA o GRUPO",1);
                lcd.info("a configurar...",2);   
                break;
        case DFLT_TIME :      //configuramos tiempo riego por defecto
                this->Time_process_start();   
                break;
        case COPY_BACKUP :  // copiamos fichero parametros en fichero backup
                if (copyConfigFile(parmFile, backupParmFile)) {    // parmFile --> backupParmFile
                  LOG_INFO("[ConF] salvado fichero de parametros actuales como BACKUP");
                  lcd.infoclear("Save to BACKUP OK", DEFAULTBLINK, BIPOK);
                  delay(config.msgdisplaymillis); 
                }
                else BIPKO;  
                this->menu();  // vuelve a mostrar menu de configuracion 
                break;
        case WIFI_PARM :   // activamos AP y portal de configuracion (bloqueante)
                LOG_INFO("[ConF]  activamos AP y portal de configuracion");
                ledYellow(OFF);
                starConfigPortal(config);
                ledYellow(ON);
                this->menu();  // vuelve a mostrar menu de configuracion
                break; 
  #ifdef WEBSERVER
        case WEBSERVER_ACT :  // activamos webserver (no bloqueante, pero no respodemos a botones)
                connected ? setupWS(config) : bipKO();
                break;
  #endif 
        case LOAD_BACKUP :   // carga parametros por defecto y reinicia
                if (copyConfigFile(backupParmFile, parmFile)) {    // backupParmFile --> parmFile
                  LOG_WARN("carga parametros por defecto OK");
                  //señala la carga parametros por defecto OK
                  lcd.infoclear("load BACKUP OK", DEFAULTBLINK, BIPOK);
                  lcd.info(">> RESET en 2 seg <<",3);
                  delay(2000);
                  ESP.restart();  // reset ESP32
                }
                else BIPKO;  
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
                this->Range_process_start(-5, 5, 100, 50);   
                break;
        case TEMP_SOURCE :   // toggle temperatura mostrada (sensor local o remoto)
                config.tempRemote = !config.tempRemote;
                if(readTemp()==999) { // si no esta disponible no dejamos cambiar
                  config.tempRemote = !config.tempRemote;
                  bipKO();
                }
                else {
                  bip(2);
                  saveConfig = true;
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
                config.mute = !config.mute;
                config_mute = config.mute;  // actualizamos variable global (para sonidos)
                bip(2);
                saveConfig = true;
                this->menu();  // vuelve a mostrar menu de configuracion
                break;
        case VOLUME :   //configuramos volumen de la melodia
                configValuep = &config.volume;  
                this->Range_process_start(1, 10);   
                break;
        case FIN_MELODY :   //configuramos melodia final riego grupo
                configValuep = &config.finMelody;  
                this->Range_process_start(1, 3);   
                _configuringMelody = true;
                break; 
        case NIVEL_WIFI :   // toggle display nivel señal wifi
                config.showwifilevel = !config.showwifilevel;
                bip(2);
                saveConfig = true;
                this->menu();  // vuelve a mostrar menu de configuracion
                break; 
        case XNAME_ONOFF :   // toggle actualizar nombres zonas con los del Domoticz
                config.xname = !config.xname;
                bip(2);
                saveConfig = true;
                this->menu();  // vuelve a mostrar menu de configuracion
                break;
        case VERIFY_ONOFF :   // toggle verificar estado dispositivo en el Domoticz
                config.verify = !config.verify;
                bip(2);
                saveConfig = true;
                this->menu();  // vuelve a mostrar menu de configuracion
                break;
        case DYNAMIC :   // toggle añadido/borrado dinamico de zonas durante el riego
                config.dynamic = !config.dynamic;
                bip(2);
                saveConfig = true;
                this->menu();  // vuelve a mostrar menu de configuracion
                break;
        default:         
                LOG_DEBUG("salimos del CASE del MENU sin realizar accion");
      }
}

