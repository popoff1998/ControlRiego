#include "Configure.h"
#include "Control.h"

Configure::Configure(class Display *disp)
{
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  display = disp;
}

void Configure::start()
{
  bip(1);
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  display->print("ConF");
}

void Configure::stop()
{
  bip(1);
  _configuringIdx = false;
  _configuringTime = false;
  _configuringMulti = false;
  display->print("ConF");
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
