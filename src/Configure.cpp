#include "Configure.h"

Configure::Configure(struct Config_parm &conf) : config(conf)
{
      this->reset();
      _currentItem = 0;
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
      setEncoderTime();

      LOG_INFO("[ConF] configurando IDX boton:",config.zona[boton->znumber-1].desc);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "IDX de:  %s", config.zona[boton->znumber-1].desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, " actual %d", tm.value);
      lcd.info(buff, 3);
      
      led(Boton[_actualIdxIndex].led,ON);
}

//  salvamos en config y Boton el nuevo IDX de la zona
void Configure::Idx_process_end()
{
      int bIndex = _actualIdxIndex;
      int zIndex = Boton[bIndex].znumber-1;
      config.zona[zIndex].idx = (uint16_t)tm.value;
      saveConfig = true;
      
      LOG_INFO("Save Zona",zIndex+1,"(",Boton[bIndex].desc,") IDX :",tm.value);
      lcd.info("guardado IDX",2);
      lcd.clear(BORRA2H);
      bipOK();
      delay(config.msgdisplaymillis);  // para que se vea el msg
      led(Boton[bIndex].led,OFF);

      tm.value = tm.savedValue;  // restaura tiempo (en lugar del IDX)
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

      LOG_INFO("configurando tiempo riego por defecto");
      lcd.infoclear("Configurando");
      lcd.info("tiempo riego def.:",2);
      StaticTimeUpdate(REFRESH);
}              

//  salvamos en config el nuevo tiempo por defecto
void Configure::Time_process_end()
{
      config.minutes = tm.minutes;
      config.seconds = tm.seconds;
      saveConfig = true;

      LOG_INFO("Save DEFAULT TIME, minutes:",tm.minutes," secons:",tm.seconds);
      lcd.info("DEFAULT TIME saved",2);
      bipOK();
      delay(config.msgdisplaymillis);

      this->menu();  // vuelve a mostrar menu de configuracion
}

//  configuramos Range
void Configure::Range_process_start(int min, int max, int aceleracion)
{
      this->reset();
      _configuringRange = true;
      tm.value = *configValuep;
      setEncoderRange(min, max, tm.value, aceleracion);

      LOG_INFO("[ConF] configurando RANGO, valor actual:",tm.value);
      lcd.infoclear("Configurando");
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, " actual: %d", tm.value);
      lcd.info(buff, 3);
}

//  salvamos en config la nueva Range
void Configure::Range_process_end()
{
      *configValuep = tm.value;
      saveConfig = true;
      
      LOG_INFO("Save new value:", *configValuep);
      lcd.info("guardado nuevo valor",2);
      lcd.clear(BORRA2H);
      bipOK();
      delay(config.msgdisplaymillis);  // para que se vea el msg

      tm.value = tm.savedValue;  // restaura tiempo 
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
      _actualGrupo = _NUMGRUPOS+1;   // grupo temporal: n+1
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
      snprintf(buff, MAXBUFF, "pulse ZONAS");
      lcd.info(buff, 3);

      if(!_configuringMultiTemp) displayGrupo(multi.serie, *multi.size); // no encendemos leds si grupo TEMPORAL 
}              

void Configure::Multi_process_update()
{
      int bIndex = bID2bIndex(boton->bID);
      int zNumber = boton->znumber;

      if (multi.w_size < ZONASXGRUPO) {  //max. zonas por grupo
        multi.serie[multi.w_size] = boton->bID;  // bId de la zona
        multi.zserie[multi.w_size] = zNumber ;  // numero de la zona
        multi.w_size = multi.w_size + 1;

        LOG_INFO("[ConF] añadiendo ZONA",zNumber,"(",config.zona[boton->znumber-1].desc,") multi.w_size=",multi.w_size);
        led(Boton[bIndex].led,ON);
        displayLCDGrupo(multi.zserie, multi.w_size);
      }
      else bipKO();  
}

// actualizamos config con las zonas introducidas
void Configure::Multi_process_end()
{
      if (multi.w_size) {  //solo si se ha pulsado alguna
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
      ultimosRiegos(HIDE);
      led(Boton[bID2bIndex(*multi.id)].led,OFF);
      this->menu(0);  // vuelve a mostrar menu de configuracion, primera linea
}

// actualizamos config con las zonas introducidas para grupo temporal
void Configure::MultiTemp_process_end()
{
      if (multi.w_size) {  //solo si se ha pulsado alguna
        *multi.size = multi.w_size;
        int g = _actualGrupo;
        for (int i=0; i<multi.w_size; ++i) {
          config.group[g-1].zNumber[i] = multi.zserie[i];
        }
        saveConfig = true;  //  solo para indicar que hemos salvado grupo temporal y tenemos que iniciarlo

        LOG_INFO("SAVE config grupo TEMPORAL : GRUPO",g,"tamaño:",*multi.size,"(",multi.desc,")");
        printMultiGroup(config, g-1);
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
      String opcionesMenuConf[] = {
        /*-----------------*/ 
          "botones IDX/MULT.",  // 0  (fijo)
          "TIEMPO riego",       // 1  
          "copy to DEFAULT",    // 2 
          "WIFI parm (AP)",     // 3 
          "WEBSERVER",          // 4 
          "MUTE on/off",        // 5 
          "load from DEFAULT",  // 6 
          "ESP32 temp: cc/mm",  // 7 
          "LED atenuacion",     // 8 
          "LED maximo brillo",  // 9 
          "correccion TEMP.",   // 10 
          "tiempo MENSAJES",    // 11
          "-----------------"   // - 
        /*-----------------*/ 
      };
      opcionesMenuConf[5] = (config.mute ?  "MUTE ON->OFF" : "MUTE OFF->ON");
      opcionesMenuConf[7] =  "ESP32 temp: " + String((int)temperatureRead()) + "/" + config.warnESP32temp;


      const int MAXOPCIONES = sizeof(opcionesMenuConf)/sizeof(opcionesMenuConf[0]);
      LOG_DEBUG("opcion=",opcion,"_currentitem=",_currentItem,"MAXOPCIONES=",MAXOPCIONES);
      _currentItem = opcion;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Menu Configuracion:");
      lcd.setCursor(0,1);
      lcd.print("->");
      //lcd.print("--" "\x7E");  //  "-->"
      for (int l=1; l<4; l++) {
        LOG_DEBUG("linea ", l, " opcion ", opcion);
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
        case 0 :      //configuramos boton de zona (IDX Domoticz asociado) o de grupo (zonas que lo componen)
                lcd.infoclear("pulse ZONA o GRUPO",1);
                lcd.info("a configurar...",2);   
                break;
        case 1 :      //configuramos tiempo riego por defecto
                this->Time_process_start();   
                break;
        case 2 :  // copiamos fichero parametros en fichero default
                if (copyConfigFile(parmFile, defaultFile)) {    // parmFile --> defaultFile
                  LOG_INFO("[ConF] salvado fichero de parametros actuales como DEFAULT");
                  lcd.infoclear("Save to DEFAULT OK", DEFAULTBLINK, BIPOK);
                  delay(config.msgdisplaymillis); 
                }
                else BIPKO;  
                this->menu();  // vuelve a mostrar menu de configuracion 
                break;
        case 3 :   // activamos AP y portal de configuracion (bloqueante)
                LOG_INFO("[ConF]  activamos AP y portal de configuracion");
                ledYellow(OFF);
                starConfigPortal(config);
                ledYellow(ON);
                this->menu();  // vuelve a mostrar menu de configuracion
                break; 
  #ifdef WEBSERVER
        case 4 :  // activamos webserver (no bloqueante, pero no respodemos a botones)
                connected ? setupWS(config) : bipKO();
                break;
  #endif 
        case 5 :   // toggle MUTE
                config.mute = !config.mute;
                config.mute ? lcd.infoclear("     MUTE ON",2) : lcd.infoclear("     MUTE OFF",2);
                bip(2);
                delay(config.msgdisplaymillis);
                saveConfig = true;
                this->menu();  // vuelve a mostrar menu de configuracion
                break; 
        case 6 :   // carga parametros por defecto y reinicia
                if (copyConfigFile(defaultFile, parmFile)) {    // defaultFile --> parmFile
                  LOG_WARN("carga parametros por defecto OK");
                  //señala la carga parametros por defecto OK
                  lcd.infoclear("load DEFAULT OK", DEFAULTBLINK, BIPOK);
                  lcd.info(">> RESET en 2 seg <<",3);
                  delay(2000);
                  ESP.restart();  // reset ESP32
                }
                else BIPKO;  
                this->menu();  // vuelve a mostrar menu de configuracion
                break;
        case 7 :      //configuramos temperatura aviso ESP32
                snprintf(buff, MAXBUFF, "max. temp. ESP32");
                configValuep = &config.warnESP32temp;  
                this->Range_process_start(40, 99);   
                break;
        case 8 :      //configuramos nivel atenuacion led STATUS (RGB)
                snprintf(buff, MAXBUFF, "atenuacion LED");
                configValuep = &config.dimmlevel;  
                this->Range_process_start(10, config.maxledlevel);   
                break;
        case 9 :      //configuramos nivel maximo brillo led STATUS (RGB)
                snprintf(buff, MAXBUFF, "brillo maximo LED");
                configValuep = &config.maxledlevel;  
                this->Range_process_start(config.dimmlevel, 255);   
                break;
        case 10 :     //configuramos correccion temperatura mostrada
                snprintf(buff, MAXBUFF, "OFFSET temperatura");
                configValuep = &config.tempOffset;  
                this->Range_process_start(-5, 5);   
                break;
        case 11 :     //configuramos tiempo que se muestran los mensajes (en milisegundos)
                snprintf(buff, MAXBUFF, "TIEMPO mensajes (ms)");
                configValuep = &config.msgdisplaymillis;  
                this->Range_process_start(1000, 4000, 500);   
                break;
        default:         
                LOG_DEBUG("salimos del CASE del MENU sin realizar accion");
      }
}

