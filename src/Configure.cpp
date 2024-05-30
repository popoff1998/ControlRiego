#include "Configure.h"

Configure::Configure()
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  _configuringMultiTemp = false;
  _actualIdxIndex = 0;
  _actualGrupo = 0;
}

void Configure::menu()
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  _configuringMultiTemp = false;
  _maxItems = this->mostrar_menu(0);
  _currentItem = 0;
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
  LOG_DEBUG("_configuringMultiTemp devuelve  >>>>>", _configuringMultiTemp); 
  return _configuringMultiTemp;
}

bool Configure::configuring()
{
  return (_configuringIdx | _configuringTime | _configuringMulti | _configuringMultiTemp);
}

void Configure::configureIdx(int index)
{
  _configuringIdx = true;
  _configuringTime = false;
  _configuringMulti = false;
  _configuringMultiTemp = false;
  _actualIdxIndex = index;

  this->configureIdx_process();
}

void Configure::configureTime(struct Config_parm& config)
{
  _configuringTime = true;
  _configuringIdx = false;
  _configuringMulti = false;
  _configuringMultiTemp = false;

  this->configureTime_process(config);
}

//  Configuramos el grupo de multirriego activo
void Configure::configureMulti(int grupo)
{
  _configuringMulti = true;
  _configuringTime = false;
  _configuringIdx = false;
  _configuringMultiTemp = false;
  _actualGrupo = grupo;

  this->configureMulti_process();
}

//  Configuramos el grupo de multirriego temporal
void Configure::configureMultiTemp(void)
{
  _configuringMultiTemp = true;
  _configuringTime = false;
  _configuringIdx = false;
  _configuringMulti = false;
  _actualGrupo = _NUMGRUPOS+1;   // grupo temporal: n+1

  this->configureMulti_process();
}

int Configure::getActualIdxIndex(void)
{
  return _actualIdxIndex;
}

int Configure::getActualGrupo(void)
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

int Configure::mostrar_menu(int opcion)
{
  String opcionesCF[] = {
    "BOTONES IDX/MULT.",  // 0 
    "TIEMPO riego",       // 1  
    "save DEFAULT",       // 2 
    "WEBSERVER",          // 3 
    "WIFI AP",            // 4 
    "MUTE ON",            // 5 
    "MUTE OFF"            // 6 
  };
  const int MAXOPCIONES = sizeof(opcionesCF)/sizeof(opcionesCF[0]);
  _currentItem = opcion;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Menu CONFIGURACION:");
  lcd.setCursor(0,1);
  lcd.print("->");
  for (int l=1; l<4; l++) {
    Serial.printf("linea %d opcion %d \n", l, opcion);
    lcd.setCursor(3,l);
    lcd.print(opcionesCF[opcion]);
    opcion++;
    if(opcion >= MAXOPCIONES) break; 
  }
  return MAXOPCIONES;
}

// configuramos tiempo riego por defecto
void Configure::configureTime_process(struct Config_parm &config)    
{
      setEncoderTime();
      
      minutes = config.minutes;
      seconds = config.seconds;

      LOG_INFO("configurando tiempo riego por defecto");
      lcd.infoclear("Configurando");
      lcd.info("tiempo riego def.:",2);
      StaticTimeUpdate(REFRESH);
}              

//  configuramos grupo multirriego
void Configure::configureMulti_process(void)    
{
      multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo

      LOG_INFO("Configurando: GRUPO",_actualGrupo,"(",multi.desc,")");
      LOG_DEBUG("en configuracion de MULTIRRIEGO, setMultibyId devuelve: Grupo",_actualGrupo,"(",multi.desc,") multi.size=", *multi.size);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "grupo%d: %s",_actualGrupo, multi.desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, "pulse ZONAS");
      lcd.info(buff, 3);

      if(!_configuringMultiTemp) displayGrupo(multi.serie, *multi.size); // no encendemos leds si grupo TEMPORAL 
}              

//  configuramos IDX asociado al boton de zona
void Configure::configureIdx_process(void)
{
      setEncoderTime();
      value = boton->idx;

      LOG_INFO("[ConF] configurando IDX boton:",boton->desc);
      lcd.infoclear("Configurando");
      snprintf(buff, MAXBUFF, "IDX de:  %s", boton->desc);
      lcd.info(buff, 2);
      snprintf(buff, MAXBUFF, "actual %d", value);
      lcd.info(buff, 3);
      
      led(Boton[_actualIdxIndex].led,ON);
}

//  salvamos en config el nuevo tiempo por defecto
void Configure::configuringTime_process_end(struct Config_parm &config)
{
      config.minutes = minutes;
      config.seconds = seconds;
      saveConfig = true;

      LOG_INFO("Save DEFAULT TIME, minutes:",minutes," secons:",seconds);
      lcd.info("DEFAULT TIME saved",2);
      bipOK();
      delay(MSGDISPLAYMILLIS);

      this->menu();  // vuelve a mostrar menu de configuracion
      setEncoderMenu(_maxItems);
}

//  salvamos en config y Boton el nuevo IDX de la zona
void Configure::configuringIdx_process_end(struct Config_parm &config)
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
      this->menu();  // vuelve a mostrar menu de configuracion
      setEncoderMenu(_maxItems);
}

//  se añade zona pulsada a grupo
void Configure::configuringMulti_process_update(void)
{
      int bIndex = bID2bIndex(boton->id);
      int zIndex = bID2zIndex(boton->id);

      if (multi.w_size < 16) {  //max. 16 zonas por grupo
        multi.serie[multi.w_size] = boton->id;
        multi.zserie[multi.w_size] = zIndex+1 ;
        multi.w_size = multi.w_size + 1;

        LOG_INFO("[ConF] añadiendo ZONA",zIndex+1,"(",boton->desc,")");
        led(Boton[bIndex].led,ON);
        displayLCDGrupo(multi.zserie, multi.w_size);
      }
      else bipKO();  
}

// actualizamos config con las zonas introducidas
void Configure::configuringMulti_process_end(struct Config_parm &config)
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
      this->menu();  // vuelve a mostrar menu de configuracion
      setEncoderMenu(_maxItems);
}

// actualizamos config con las zonas introducidas para grupo temporal
void Configure::configuringMultiTemp_process_end(struct Config_parm &config)
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
      _configuringIdx = false;
      _configuringTime = false;
      _configuringMulti = false;
      _configuringMultiTemp = false;

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

