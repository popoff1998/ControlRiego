#include "control.h"

//-----------------------  DEFINICIONES PARA CARACTERES GRANDES  (2X3)    ----------------------------------------------

#define B 0x20
#define FF 0xFF
#define C 0xA5

//custom characters
byte cc1[8] = {0x07,0x0F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}; //binarycode
byte cc2[8] = {0x1F,0x1F,0x1F,0x00,0x00,0x00,0x00,0x00};
byte cc3[8] = {0x1C,0x1E,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F};
byte cc4[8] = {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x0F,0x07};
byte cc5[8] = {0x00,0x00,0x00,0x00,0x00,0x1F,0x1F,0x1F};
byte cc6[8] = {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1E,0x1C};
byte cc7[8] = {0x1F,0x1F,0x1F,0x00,0x00,0x00,0x1F,0x1F};
byte cc8[8] = {0x1F,0x00,0x00,0x00,0x00,0x1F,0x1F,0x1F};

// 0 1 2 3 4 5 6 7 8 9
char bn1[] = {
  1, 2, 3,  2, 3, B,  2, 7, 3,  2, 7, 3,  4, 5, FF, FF, 7, 7,  1, 7, 7,  2, 2, 6,  1, 7, 3,  1, 7, 3
};
char bn2[] = {
  4, 5, 6,  B, FF, B,  1, 8, 8,  5, 8, 6,  B, B, FF,  8, 8, 6,  4, 8, 6,  B, 1, B,  4, 8, 6,  5, 5, 6
};

//-----------------------  FIN DEFINICIONES PARA CARACTERES GRANDES  (2X3)    ------------------------------------------



DisplayLCD::DisplayLCD(uint8_t lcd_Addr,uint8_t lcd_cols,uint8_t lcd_rows) : lcdDisp(lcd_Addr, lcd_cols, lcd_rows)
{ 
  #ifdef EXTRADEBUG
   Serial.printf( "soy el Constructor de DisplayLCD numeros pasados: 0x%x %d %d\n", lcd_Addr , lcd_cols , lcd_rows );
  #endif
}  


void DisplayLCD::initLCD() {
  LOG_TRACE("[LCD] ");
  lcdDisp.init();
  clear();
  setBacklight(ON);

  DefineLargeChar(); // Create the custom characters

  setCursor(5, 0);
  print("Ardomo Aqua");
  setCursor(0, 2);
  print("Inicializando");
  #ifdef DEVELOP
    setCursor(0, 3);
    print("(dev)");
  #endif
  int longitud = strlen(VERSION);
  setCursor(LCDMAXLEN-(longitud+1), 3);
  print("v" VERSION);
}


void DisplayLCD::clear(int mitad)
{
  if(!mitad) {
    LOG_TRACE("[LCD] BORRA lcd");
    lcdDisp.clear();
    return;
  }
  LOG_TRACE("[LCD] borra ",mitad,"ª mitad lcd");
  if(mitad == BORRA1H) {
    setCursor(0, 0);
    lcdDisp.print(_blankline);
    setCursor(0, 1);
    lcdDisp.print(_blankline);
    return;
  }
  if(mitad == BORRA2H) {
    setCursor(0, 2);
    lcdDisp.print(_blankline);
    setCursor(0, 3);
    lcdDisp.print(_blankline);
  }
}

void DisplayLCD::setCursor(uint8_t col, uint8_t row)
{
  lcdDisp.setCursor(col, row);
}


void DisplayLCD::displayON()
{
  lcdDisp.display();
  _displayOff = false;
}

void DisplayLCD::displayOFF()
{
  lcdDisp.noDisplay();
  _displayOff = true;
}

void DisplayLCD::setBacklight(bool value)				// alias for backlight() and nobacklight()
{
  LOG_TRACE("[LCD] backlight:", value);
  lcdDisp.setBacklight(value);
}

void DisplayLCD::print(const char * text) {
  LOG_TRACE("[LCD] recibido: '",text,"'");
  lcdDisp.print(text);
}

void DisplayLCD::print(const String &s) {
  lcdDisp.print(s);
}

void DisplayLCD::print(const int numero) {
  lcdDisp.print(numero);
}

/* 
void DisplayLCD::blinkLCD(const char *info,int veces) //muestra texto recibido parpadeando n veces
{
    LOG_TRACE("[LCD] '",info, "' x",veces);
    for (int i=0; i<veces; i++) {
      clear();
      delay(DEFAULTBLINKMILLIS);
      setCursor(7, 1);
      lcdDisp.print(info);
      delay(DEFAULTBLINKMILLIS);
    }
}
 */

void DisplayLCD::blinkLCD(int veces) //parpadea contenido actual de la pantalla n veces
{
  if(veces) {                       
    // parpadea pantalla n veces
      LOG_TRACE("[LCD]blink LCD  x",veces);
      for (int i=0; i<veces; i++) {
        displayOFF(); // setBacklight(OFF);
        delay(DEFAULTBLINKMILLIS);
        displayON(); // setBacklight(ON);
        delay(DEFAULTBLINKMILLIS);
      }
  }
}


void DisplayLCD::infoEstado(const char *estado, const char *zona) {
    LOG_DEBUG("[LCD]  Recibido: ", estado, zona);
    setCursor(0, 0);
    lcdDisp.print(_blankline);
    setCursor(0, 0);
    lcdDisp.print(estado);
    setCursor(11, 0);
    int size = strlen(zona);
    char infocut[10];
    if(size>9) {
      LOG_DEBUG("* zona recibido de longitud =",size);
      strlcpy(infocut, zona, sizeof(infocut)); 
      LOG_DEBUG("* nombre zona (infocut) acortado a =",infocut);
      lcdDisp.print(infocut);
    }
    else lcdDisp.print(zona);  
    //lcdDisp.print(zona);  
}    

// muestra info (hasta un maximo de 20 caracteres) en la linea pasada (1, 2 ,3 o 4)
void DisplayLCD::info(const char* info, int line) {
    int size = strlen(info);
    char infocut[MAXBUFF];
    if(size>MAXBUFF-1) {
      LOG_DEBUG("*info recibido de longitud =",size);
      strlcpy(infocut, info, sizeof(infocut)); 
      lcd.info(infocut, line, size);
    }  
    else lcd.info(info, line, size);
}    

// muestra info (seran ya un maximo de 20 caracteres) en la linea pasada (1, 2 ,3 o 4)
void DisplayLCD::info(const char* info, int line, int size) {
    LOG_DEBUG("[LCD]  Recibido: '", info, "'   (longitud original: ", size, " linea: ", line, ")");
    setCursor(0, line-1);
    lcdDisp.print(_blankline);
    setCursor(0, line-1);
    lcdDisp.print(info);
}    
void DisplayLCD::infoclear(const char *info, int line) {
    LOG_DEBUG("[LCD]  Recibido: ", info, "linea: ", line);
    clear();
    lcd.info(info,line);
}

/**
 * @brief muestra en el display texto informativo y suenan bips de aviso
 * 
 * @param info = texto a mostrar en el display
 * @param dnum = veces que parpadea el texto en el display
 * @param btype = tipo de bip emitido
 * @param bnum = numero de bips emitidos
 */
void DisplayLCD::infoclear(const char *info, int dnum, int btype, int bnum) {
    LOG_DEBUG("[LCD]  Recibido: '",info, "'   (blink=",dnum, ") btype=",btype,"bnum=",bnum);
    clear();
    if(info=="STOP") setCursor(8,1);
    else setCursor(0, 0);
    lcdDisp.print(info);
      if (btype == LONGBIP) longbip(bnum);
      if (btype == LOWBIP) lowbip(bnum);
      if (btype == BIP) bip(bnum);
      if (btype == BIPOK) bipOK();
      if (btype == BIPKO) bipKO();
      if (btype == BIPFIN) bipFIN(bnum);
    if(dnum) lcd.blinkLCD(dnum);
}

void DisplayLCD::displayTemp(int temperature, int warnESP32temp) 
{
  LOG_DEBUG("temperatura recibida=",temperature,"temp ESP32=",temperatureRead());
  if(temperatureRead() > warnESP32temp) {   // aviso de temperatura excesiva del ESP32
    setCursor(14, 0); print("!"); bip(2);
    setCursor(15, 0); print(temperatureRead());
  }
  else {
    setCursor(14, 0);
    if (temperature == 999) print(" --");
    else lcdDisp.printf(" %2d",temperature);
  }  
  setCursor(17, 0); print("\xDF" "C"); // xDF = caracter grado centigrado
}

void DisplayLCD::displayTime(uint8_t minute, uint8_t second, uint8_t col, uint8_t line) 
{
  printTwoNumber(minute, col, line);
  printColons(col+6, line);
  printTwoNumber(second, col+7, line);
}

// Funciones auxiliares:

void DisplayLCD::DefineLargeChar()    // send custom characters to the display 
{ 
    lcdDisp.createChar(1, cc1);
    lcdDisp.createChar(2, cc2);
    lcdDisp.createChar(3, cc3);
    lcdDisp.createChar(4, cc4);
    lcdDisp.createChar(5, cc5);
    lcdDisp.createChar(6, cc6);
    lcdDisp.createChar(7, cc7);
    lcdDisp.createChar(8, cc8);
}

void DisplayLCD::printTwoNumber(uint8_t number, uint8_t position, uint8_t line)  // muestra dos digitos en bigchar
{
  // Print position is NO hardcoded
  int digit0; // To represent the ones
  int digit1; // To represent the tens
  digit0 = number % 10;
  digit1 = number / 10;

  // Line 1 of the two-digit number
  //lcdDisp.setCursor(position, 0);
  lcdDisp.setCursor(position, line);  //linea superior 1 = segunda linea
  lcdDisp.write(bn1[digit1 * 3]);
  lcdDisp.write(bn1[digit1 * 3 + 1]);
  lcdDisp.write(bn1[digit1 * 3 + 2]);
  //lcdDisp.write(B); // Blank
  lcdDisp.write(bn1[digit0 * 3]);
  lcdDisp.write(bn1[digit0 * 3 + 1]);
  lcdDisp.write(bn1[digit0 * 3 + 2]);

  // Line 2 of the two-digit number
  //lcdDisp.setCursor(position, 1);
  lcdDisp.setCursor(position, line+1);  //linea inferior 2 = tercera linea
  lcdDisp.write(bn2[digit1 * 3]);
  lcdDisp.write(bn2[digit1 * 3 + 1]);
  lcdDisp.write(bn2[digit1 * 3 + 2]);
  //lcdDisp.write(B); // Blank
  lcdDisp.write(bn2[digit0 * 3]);
  lcdDisp.write(bn2[digit0 * 3 + 1]);
  lcdDisp.write(bn2[digit0 * 3 + 2]);
}

void DisplayLCD::printColons(uint8_t position, uint8_t line)
{
  lcdDisp.setCursor(position, line);
  lcdDisp.write (C);
  lcdDisp.setCursor(position, line+1);
  lcdDisp.write (C);
}

void DisplayLCD::printNoColons(uint8_t position, uint8_t line)
{
  lcdDisp.setCursor(position, line);
  lcdDisp.write (B);
  lcdDisp.setCursor(position, line+1);
  lcdDisp.write (B);
}

