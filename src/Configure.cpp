#include "Configure.h"
#include "Control.h"
#include <EEPROM.h>

Configure::Configure(class Display *disp)
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  display = disp;
}

void Configure::start()
{
  //Serial << "Conf start" << endl;
  bip(1);
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  display->print("ConF");
}

void Configure::stop()
{
  //Serial << "Conf stop" << endl;
  bip(1);
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  display->print("ConF");
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

bool Configure::configuringMulti()
{
  Serial << "configuringMulti: " << _configuringMulti << endl;
  return _configuringMulti;
}

void Configure::configureIdx(int index)
{
  //Serial << "configureIdx" << endl;
  _configuringIdx = true;
  _configuringTime = false;
  _configuringMulti = false;
  _actualIdxIndex = index;
}

void Configure::configureTime(void)
{
  //Serial << "configureTime" << endl;
  _configuringTime = true;
  _configuringIdx = false;
  _configuringMulti = false;
}

void Configure::configureMulti(void)
{
  Serial << "configureMulti" << endl;
  _configuringTime = false;
  _configuringIdx = false;
  _configuringMulti = true;
}

int Configure::getActualIdxIndex(void)
{
  return _actualIdxIndex;
}
