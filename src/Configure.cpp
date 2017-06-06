#include "Configure.h"
#include "Control.h"
#include <EEPROM.h>

Configure::Configure(class Display *disp)
{
  _configuringIdx = false;
  _configuringTime = false;
  display = disp;
}

void Configure::start()
{
  //Serial << "Conf start" << endl;
  bip(1);
  _configuringIdx = false;
  _configuringTime = false;
  display->print("conf");
}

void Configure::stop()
{
  //Serial << "Conf stop" << endl;
  bip(1);
  _configuringIdx = false;
  _configuringTime = false;
  display->print("conf");
}

bool Configure::configuringTime()
{
  //Serial << "configuringTime: " << _configuringTime << endl;
  return _configuringTime;
}

bool Configure::configuringIdx()
{
  //Serial << "configuringIdx: " << _configuringIdx << endl;
  return _configuringIdx;
}

void Configure::configureIdx(int index)
{
  //Serial << "configureIdx" << endl;
  _configuringIdx = true;
  _configuringTime = false;
  _actualIdxIndex = index;
}

void Configure::configureTime(void)
{
  //Serial << "configureTine" << endl;
  _configuringTime = true;
  _configuringIdx = false;
}

int Configure::getActualIdxIndex(void)
{
  return _actualIdxIndex;
}
