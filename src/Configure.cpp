#include "Configure.h"
#include "Control.h"

Configure::Configure(class ClickEncoder *enc, class Display *disp) {
  display = disp;
  encoder = enc;
}

void Configure::start() {
  display->print("conf");
}

void Configure::idx(struct S_BOTON *boton) {
  //Procesamos el encoder
  clkvalue = boton->idx;
  confBoton = NULL;
  while(1)
  {
    clkvalue += encoder->getValue();
    if (clkvalue > 1000) clkvalue = 1000;
    if (clkvalue <  1) clkvalue = 1;
    display->print(clkvalue);
    //Serial.print("CLKVALUE: ");Serial.println(clkvalue);
    confBoton = parseInputs();
    if(confBoton->id == bPAUSE) {
      longbip(2);
      boton->idx = clkvalue;
      //Serial.print("IDX: ");Serial.println(boton->idx);
      eeprom_data.botonIdx[bId2bIndex(boton->id)] = boton->idx;
      display->print("conf");
      return;
    }
    //Serial.print("CLKVALUE: ");Serial.println(clkvalue);
  }
}

void Configure::defaultTime(void)
{
  delay(500);
  while(1)
  {
    procesaEncoder();
    confBoton = parseInputs();
    if (!confBoton->estado) continue;
    if(confBoton->id == bPAUSE) {
      longbip(2);
      eeprom_data.minutes = minutes;
      eeprom_data.seconds = seconds;
      display->print("conf");
      return;
    }
  }
}
