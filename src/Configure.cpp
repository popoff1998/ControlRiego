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
  while(1)
  {
    clkvalue += encoder->getValue();
    if (clkvalue > 1000) clkvalue = 1000;
    if (clkvalue <  1) clkvalue = 1;
    display->print(clkvalue);
    //Serial.print("CLKVALUE: ");Serial.println(clkvalue);
    ClickEncoder::Button b = encoder->getButton();
    if(b != ClickEncoder::Open) {
      longbip(2);
      boton->idx = clkvalue;
      Serial.print("IDX: ");Serial.println(boton->idx);
      EEPROM.put(ADDRBASE_BOTON + (bId2bIndex(boton->id)*sizeof(boton->idx)),boton->idx);
      display->print("conf");
      return;
    }
    //Serial.print("CLKVALUE: ");Serial.println(clkvalue);
  }
}
