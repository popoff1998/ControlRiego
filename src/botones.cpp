

#include <Arduino.h>
#include <Control.h>

byte    switchVar1 = 72;
byte    switchVar2 = 159;

void initCD4021B() {
  pinMode(CD4021B_LATCH, OUTPUT);
  pinMode(CD4021B_CLOCK, OUTPUT);
  pinMode(CD4021B_DATA, INPUT);
}

byte shiftInCD4021B(int myDataPin, int myClockPin)
{
  int i;
  int temp=0;
  //int pinState;
  int myDataIn = 0;

  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);

  for (i=7;i>=0;i--)
  {
    digitalWrite(myClockPin,0);
    delayMicroseconds(0.2);
    temp = digitalRead(myDataPin);
    if(temp) {
      //pinState = 1;
      myDataIn = myDataIn | (1 << i);
    }
    //else pinState = 0;
    digitalWrite(myClockPin,1);
  }
  return myDataIn;
}

int bId2bIndex(uint16_t id)
{
  for (int i=0;i<16;i++) {
    if (Boton[i].id == id) return i;
  }
  return 99;
}

uint16_t getMultiStatus()
{
  if (Boton[bId2bIndex(bCESPED)].estado) return bCESPED;
  if (Boton[bId2bIndex(bGOTEOS)].estado) return bGOTEOS;
  return bCOMPLETO;
}

uint16_t readInputs()
{
  //Activamos el latch para leer
  digitalWrite(CD4021B_LATCH,1);
  delayMicroseconds(20);
  digitalWrite(CD4021B_LATCH,0);
  //Leemos
  switchVar1 = shiftInCD4021B(CD4021B_DATA, CD4021B_CLOCK);
  switchVar2 = shiftInCD4021B(CD4021B_DATA, CD4021B_CLOCK);
  #ifdef EXTRADEBUG
    Serial.print(switchVar1, BIN);
    Serial.print(" - ");
    Serial.println(switchVar2, BIN);
  #endif
  return switchVar2 | (switchVar1 << 8);
}

S_BOTON *parseInputs()
{
  int i;
  uint16_t inputs = readInputs();

  for (i=0;i<16;i++) {
    //Nos saltamos los DISABLED
    if (!Boton[i].flags.enabled) continue;
    Boton[i].estado = inputs & Boton[i].id;
    //Solo si el estado ha cambiado
    if (Boton[i].estado != Boton[i].ultimo_estado)
    {
      Boton[i].ultimo_estado = Boton[i].estado;
      if (Boton[i].estado || Boton[i].flags.dual) {
        Serial.println(Boton[i].desc);
        return &Boton[i];
      }
    }
  }
  return NULL;
}
