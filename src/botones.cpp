#include <Control.h>

//Globales a este modulo
byte    switchVar1 = 72;
byte    switchVar2 = 159;
unsigned long lastMillis;
#define DEBOUNCEMILLIS 20
volatile uint16_t ledStatus = 0;

void apagaLeds()
{
  Serial.println("APAGANDO LEDS");
  digitalWrite(HC595_LATCH, LOW);
  shiftOut(HC595_DATA, HC595_CLOCK, MSBFIRST, 0);
  shiftOut(HC595_DATA, HC595_CLOCK, MSBFIRST, 0);
  digitalWrite(HC595_LATCH, HIGH);
  delay(500);
}

void initHC595()
{
  pinMode(HC595_CLOCK, OUTPUT);
  pinMode(HC595_DATA, OUTPUT);
  pinMode(HC595_LATCH, OUTPUT);
  apagaLeds();
}

void led(uint8_t id,int estado)
{
    //Por seguridad no hacemos nada si id=0
    Serial.print("ESTADO: ");Serial.print(estado);
    Serial.print(" ID: ");Serial.print(id);
    Serial.print(" LEDSTATUS: ");Serial.println(ledStatus);
    if(id==0) return;
    if(estado == ON) ledStatus |= (1 << (id-1));
    else ledStatus &= ~(1 << (id-1));
    //convertimos a la parte baja y alta
    uint8_t bajo = (uint8_t)((ledStatus & 0x00FF));
    uint8_t alto = (uint8_t)((ledStatus & 0xFF00) >> 8);
    digitalWrite(HC595_LATCH, LOW);
    shiftOut(HC595_DATA, HC595_CLOCK, MSBFIRST, alto);
    shiftOut(HC595_DATA, HC595_CLOCK, MSBFIRST, bajo);
    digitalWrite(HC595_LATCH, HIGH);
}

void initCD4021B()
{
  pinMode(CD4021B_LATCH, OUTPUT);
  pinMode(CD4021B_CLOCK, OUTPUT);
  pinMode(CD4021B_DATA, INPUT);
}

byte shiftInCD4021B(int myDataPin, int myClockPin)
{
  int i;
  int temp=0;
  int myDataIn = 0;

  for (i=7;i>=0;i--)
  {
    digitalWrite(myClockPin,0);
    delayMicroseconds(0.2);
    temp = digitalRead(myDataPin);
    if(temp) myDataIn = myDataIn | (1 << i);
    digitalWrite(myClockPin,1);
  }
  return myDataIn;
}

int bId2bIndex(uint16_t id)
{
  for (int i=0;i<16;i++) {
    if (Boton[i].id == id) return i;
  }
  return 999;
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
  return switchVar2 | (switchVar1 << 8);
}

S_BOTON *parseInputs()
{
  int i;
  //Para el debounce
  unsigned long currentMillis = millis();
  if(currentMillis < (lastMillis + DEBOUNCEMILLIS)) return NULL;
  else lastMillis = currentMillis;

  uint16_t inputs = readInputs();

  for (i=0;i<16;i++) {
    //Nos saltamos los DISABLED
    if (!Boton[i].flags.enabled) continue;
    Boton[i].estado = inputs & Boton[i].id;
    //Solo si el estado ha cambiado
    if ((Boton[i].estado != Boton[i].ultimo_estado) || (Boton[i].estado && Boton[i].flags.hold && !Boton[i].flags.holddisabled))
    {
      Boton[i].ultimo_estado = Boton[i].estado;
      if (Boton[i].estado || Boton[i].flags.dual) {
        return &Boton[i];
      }
    }
  }
  return NULL;
}
