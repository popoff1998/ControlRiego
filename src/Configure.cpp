#include "Control.h"



Configure::Configure()
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  _actualIdxIndex = 0;
  _actualGrupo = 0;
}

void Configure::start()
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  //lcd.infoclear("modo CONFIGURACION",1);
  _maxItems = mostrar_menu(0);
  _currentItem = 0;
}

void Configure::stop()
{
  Configure::start();
}

bool Configure::configuringTime()
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

bool Configure::configuring()
{
  return (_configuringIdx | _configuringTime | _configuringMulti);
}

void Configure::configureIdx(int index)
{
  _configuringIdx = true;
  _configuringTime = false;
  _configuringMulti = false;
  _actualIdxIndex = index;
}

void Configure::configureTime(void)
{
  _configuringTime = true;
  _configuringIdx = false;
  _configuringMulti = false;
}

void Configure::configureMulti(int grupo)
{
  _configuringTime = false;
  _configuringIdx = false;
  _configuringMulti = true;
  _actualGrupo = grupo;
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

