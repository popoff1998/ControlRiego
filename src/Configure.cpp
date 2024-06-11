#include "Configure.h"

Configure::Configure()
{
  this->reset();
}

void Configure::menu()
{
  this->reset();
  _configuringMenu = true;
  _maxItems = this->showMenu(0);
  _currentItem = 0;
}

void Configure::reset()
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  _configuringMultiTemp = false;
  _configuringMenu = false;
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
  value = boton->idx;
  setEncoderTime();
  this->configureIdx_display();
}

// configuramos tiempo riego por defecto
void Configure::Time_process_start(struct Config_parm& config)
{
  this->reset();
  _configuringTime = true;
  minutes = config.minutes;
  seconds = config.seconds;
  setEncoderTime();
  this->configureTime_display(config);
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

void Configure::configureTime_display(struct Config_parm &config)    
{
      LOG_INFO("configurando tiempo riego por defecto");
      lcd.infoclear("Configurando");
      lcd.info("tiempo riego def.:",2);
      StaticTimeUpdate(REFRESH);
}              

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

void Configure::configureIdx_display(void)
{
      LOG_INFO("[ConF] configurando IDX boton:",boton->desc);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "IDX de:  %s", boton->desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, "actual %d", value);
      lcd.info(buff, 3);
      
      led(Boton[_actualIdxIndex].led,ON);
}

//  salvamos en config el nuevo tiempo por defecto
void Configure::Time_process_end(struct Config_parm &config)
{
      config.minutes = minutes;
      config.seconds = seconds;
      saveConfig = true;

      LOG_INFO("Save DEFAULT TIME, minutes:",minutes," secons:",seconds);
      lcd.info("DEFAULT TIME saved",2);
      bipOK();
      delay(MSGDISPLAYMILLIS);

      setEncoderMenu(_maxItems, _currentItem);
      this->menu();  // vuelve a mostrar menu de configuracion
}

//  salvamos en config y Boton el nuevo IDX de la zona
void Configure::Idx_process_end(struct Config_parm &config)
{
      int bIndex = _actualIdxIndex;
      int zIndex = bID2zIndex(Boton[bIndex].id);
      Boton[bIndex].idx = (uint16_t)value;
      config.botonConfig[zIndex].idx = (uint16_t)value;
      saveConfig = true;
      
      LOG_INFO("Save Zona",zIndex+1,"(",Boton[bIndex].desc,") IDX value:",value);
      lcd.info("guardado IDX",2);
      lcd.clear(BORRA2H);
      bipOK();
      delay(MSGDISPLAYMILLIS);  // para que se vea el msg
      led(Boton[bIndex].led,OFF);

      value = savedValue;  // restaura tiempo (en lugar del IDX)
      setEncoderMenu(_maxItems, _currentItem);
      this->menu();  // vuelve a mostrar menu de configuracion
}

//  se añade zona pulsada a grupo
void Configure::Multi_process_update(void)
{
      int bIndex = bID2bIndex(boton->id);
      int zIndex = bID2zIndex(boton->id);

      if (multi.w_size < ZONASXGRUPO) {  //max. zonas por grupo
        multi.serie[multi.w_size] = boton->id;
        multi.zserie[multi.w_size] = zIndex+1 ;
        multi.w_size = multi.w_size + 1;

        LOG_INFO("[ConF] añadiendo ZONA",zIndex+1,"(",boton->desc,") multi.w_size=",multi.w_size);
        led(Boton[bIndex].led,ON);
        displayLCDGrupo(multi.zserie, multi.w_size);
      }
      else bipKO();  
}

// actualizamos config con las zonas introducidas
void Configure::Multi_process_end(struct Config_parm &config)
{
      if (multi.w_size) {  //solo si se ha pulsado alguna
        *multi.size = multi.w_size;
        int g = _actualGrupo;
        for (int i=0; i<multi.w_size; ++i) {
          config.groupConfig[g-1].serie[i] = bID2zIndex(multi.serie[i])+1;
        }
        saveConfig = true;

        LOG_INFO("SAVE PARM Multi : GRUPO",g,"tamaño:",*multi.size,"(",multi.desc,")");
        printMultiGroup(config, g-1);
        bipOK();
        lcd.info("guardado GRUPO",2);
        lcd.clear(BORRA2H);
        delay(MSGDISPLAYMILLIS);
      }
      ultimosRiegos(HIDE);
      led(Boton[bID2bIndex(*multi.id)].led,OFF);
      setEncoderMenu(_maxItems, _currentItem);
      this->menu();  // vuelve a mostrar menu de configuracion
}

// actualizamos config con las zonas introducidas para grupo temporal
void Configure::MultiTemp_process_end(struct Config_parm &config)
{
      if (multi.w_size) {  //solo si se ha pulsado alguna
        *multi.size = multi.w_size;
        int g = _actualGrupo;
        for (int i=0; i<multi.w_size; ++i) {
          config.groupConfig[g-1].serie[i] = bID2zIndex(multi.serie[i])+1;
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


//  escritura de parametros a fichero si procede y salimos de ConF
void Configure::exit(struct Config_parm &config)
{
      this->reset();

      setEncoderTime();

      if (saveConfig) {
        LOG_INFO("saveConfig=true  --> salvando parametros a fichero");
        if (saveConfigFile(parmFile, config)) {
          lcd.infoclear("SAVED parameters", DEFAULTBLINK, BIPOK);
          delay(MSGDISPLAYMILLIS);
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
      "botones IDX/MULT.",  // 0 
      "TIEMPO riego",       // 1  
      "copy to DEFAULT",    // 2 
      "WIFI parm (AP)",     // 3 
      "WEBSERVER",          // 4 
      "MUTE on/off",        // 5 
      "load from DEFAULT"   // 6 
     /*-----------------*/ 
  };
  opcionesMenuConf[5] = (mute ?  "MUTE OFF" : "MUTE ON");

  const int MAXOPCIONES = sizeof(opcionesMenuConf)/sizeof(opcionesMenuConf[0]);
  _currentItem = opcion;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Menu Configuracion:");
  lcd.setCursor(0,1);
  lcd.print("->");
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
void Configure::procesaSelectMenu(struct Config_parm &config) 
{
          boton = NULL; //para que no se procese mas adelante  TODO ¿es necesario?

          switch(this->get_currentItem()) {  
            case 0 :      //configuramos boton de zona (IDX Domoticz asociado) o de grupo (zonas que lo componen)
                    lcd.infoclear("pulse ZONA o GRUPO",1);
                    lcd.info("a configurar...",2);   
                    break;
            case 1 :      //configuramos tiempo riego por defecto
                    this->Time_process_start(config);   
                    break;
            case 2 :  // copiamos fichero parametros en fichero default
                    if (copyConfigFile(parmFile, defaultFile)) {    // parmFile --> defaultFile
                      LOG_INFO("[ConF] salvado fichero de parametros actuales como DEFAULT");
                      lcd.infoclear("Save DEFAULT OK", DEFAULTBLINK, BIPOK);
                      delay(MSGDISPLAYMILLIS); 
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
                    setupWS();
                    break;
  #endif 
            case 5 :   // toggle MUTE
                    mute = !mute;
                    mute ? lcd.infoclear("     MUTE ON",2) : lcd.infoclear("     MUTE OFF",2);
                    bip(2);
                    delay(1000);
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
            default:         
                    LOG_DEBUG("salimos del CASE del MENU sin realizar accion");
          }
}

