#include "Display.h"

static char CharTab[] = { '0','1','2','3',
                          '4','5','6','7',
                          '8','9','A','b',
                          'C','d','E','F',
                          '-',' ','*','r',
                          'h','n','t','+','u',
                          's','t','o','p'};
// 0~9,A,b,C,d,E,F,"-"," ",degree,r,h,n,t, u with _, u,S,t,o,P
/*
static int8_t TubeTab[] = {0x3f,0x06,0x5b,0x4f,
                           0x66,0x6d,0x7d,0x07,
                           0x7f,0x6f,0x77,0x7c,
                           0x39,0x5e,0x79,0x71,
                           0x40,0x00,0x63,0x50,
                           0x74,0x54,0x78,0x6a,0x1d,
                           0x6d,0x78,0x5c,0x73 };
*/
// 0~9,A,b,C,d,E,F,"-"," ",degree,r,h,n,t, u with _, u with -,S,t,o,P

Display::Display(uint8_t clk,uint8_t data) : ledDisp(clk,data)
{
  #ifdef EXTRADEBUG
   Serial.println("DISPLAY: set brigth");
  #endif
  ledDisp.set(BRIGHT_TYPICAL); //BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7
  //ledDisp.set(BRIGHTEST); //para leds verdes
  #ifdef EXTRADEBUG
   Serial.println("DISPLAY: init");
  #endif
  ledDisp.init();
  #ifdef EXTRADEBUG
   Serial.println("DISPLAY: point");
  #endif
  ledDisp.point(POINT_ON);
  #ifdef EXTRADEBUG
   Serial.println("DISPLAY: exit constructor");
  #endif
}  

void Display::check(int veces)
{
  uint8_t t[5];
  t[4]=0;

  for(int repeat=0;repeat<veces;repeat++)
  {
    clearDisplay();
    for(int i=0;i<10;i++)
    {
      for(int j=0;j<4;j++)
        t[j]=i;
      printRaw(t);
      delay(300);
    }
    print("----");
  }
}

void Display::print(const char *str)
{
  uint8_t t[5];
  for(int i=0;i<4;i++){
    for(unsigned int j=0;j<sizeof(CharTab);j++){
      if(*(str+i) == CharTab[j]) {
        t[i] = j;
        break;
      }
      //Espacio en blanco si no lo encontramos
      t[i] = 17;
    }
  }
  t[4] = 0;
  printRaw(t);
}

void Display::print(int n)
{
  ledDisp.point(POINT_OFF);
  ledDisp.display((int16_t) n);
}

void Display::printRaw(uint8_t text[])
{
  memcpy(actual,text,5);
  if (text[4] == 1) ledDisp.point(POINT_ON);
  else ledDisp.point(POINT_OFF);
  ledDisp.display((int8_t *)text);
}

void Display::blink(int veces)
{
  for (int i=0; i<veces; i++) {
    clearDisplay();
    delay(500);
    printRaw(actual);
    delay(500);
  }
}

void Display::clearDisplay()
{
    ledDisp.point(POINT_OFF);
    ledDisp.clearDisplay();
    ledDisp.point(POINT_ON);
}

void Display::printTime(int m,int s)
{
  uint8_t t[5];
  t[4] = 1;
  t[2] = s / 10;
  t[3] = s % 10;
  t[0] = m / 10;
  t[1] = m % 10;
  printRaw(t);
}

void Display::refreshDisplay(void)
{
  printRaw(actual);
}
