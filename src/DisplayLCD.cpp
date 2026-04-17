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



DisplayLCD::DisplayLCD(uint8_t lcd_Addr,uint8_t lcd_cols,uint8_t lcd_rows) : LiquidCrystal_I2C(lcd_Addr, lcd_cols, lcd_rows)
{ 
  #ifdef EXTRADEBUG
   Serial.printf( "soy el Constructor de DisplayLCD numeros pasados: 0x%x %d %d\n", lcd_Addr , lcd_cols , lcd_rows );
  #endif
}  


void DisplayLCD::initLCD() {
  LOG_TRACE("[LCD] ");
  LiquidCrystal_I2C::init();
  clear();
  setBacklight(ON);
  DefineLargeChar(); // Create the custom characters

  setCursor(5, 0);
  print("Ardomo Aqua");
  setCursor(0, 2);
  print("Inicializando");
  int longitud = strlen(FW_VERSION);
  #ifdef DEVELOP
    longitud>13 ? setCursor(15,2) : setCursor(0, 3);
    print("(dev)");
  #endif
  if (longitud<19) {
    setCursor(LCDMAXLEN-(longitud+1), 3);
    print("v" FW_VERSION);
  } else {
    info("v" FW_VERSION, 4);  // si FW_VERSION es muy larga la truncamos
  }  
}


void DisplayLCD::clear(int mitad)
{
  if(!mitad) {
    LOG_TRACE("[LCD] BORRA lcd");
    LiquidCrystal_I2C::clear();
    return;
  }
  LOG_TRACE("[LCD] borra ",mitad,"ª mitad lcd");
  if(mitad == BORRA1H) {
    setCursor(0, 0);
    LiquidCrystal_I2C::print(_blankline);
    setCursor(0, 1);
    LiquidCrystal_I2C::print(_blankline);
    return;
  }
  if(mitad == BORRA2H) {
    setCursor(0, 2);
    LiquidCrystal_I2C::print(_blankline);
    setCursor(0, 3);
    LiquidCrystal_I2C::print(_blankline);
  }
}

void DisplayLCD::setCursor(uint8_t col, uint8_t row)
{
  LiquidCrystal_I2C::setCursor(col, row);
  LiquidCrystal_I2C::noBlink();
  LiquidCrystal_I2C::noCursor();
}


void DisplayLCD::setCursorBlink(uint8_t col, uint8_t row)
{
  LiquidCrystal_I2C::setCursor(col, row);
  LiquidCrystal_I2C::blink();
  //LiquidCrystal_I2C::cursor();
}


void DisplayLCD::displayON()
{
  LiquidCrystal_I2C::display();
  _displayOff = false;
}

void DisplayLCD::displayOFF()
{
  LiquidCrystal_I2C::noDisplay();
  _displayOff = true;
}

bool DisplayLCD::get__displayOff()
{
  return _displayOff;
}

void DisplayLCD::setBacklight(bool value)				// alias for backlight() and nobacklight()
{
  LOG_TRACE("[LCD] backlight:", value);
  LiquidCrystal_I2C::setBacklight(value);
}


void DisplayLCD::blinkLCD(int veces) //parpadea contenido actual de la pantalla n veces
{
  if(veces) {                       
    // parpadea pantalla n veces
      LOG_TRACE("[LCD]blink LCD  x",veces);
      for (int i=0; i<veces; i++) {
        displayOFF();
        delay(BLINKMILLIS);
        displayON();
        delay(BLINKMILLIS);
      }
  }
}

// muestra el estado de riego en curso y nombre de la zona en la primera linea del LCD
void DisplayLCD::infoEstado(const char *estado, const char *zona, int bnum) {
    LOG_DEBUG("[LCD]  Recibido: ", estado, zona);
    setCursor(0, 0);
    LiquidCrystal_I2C::print(_blankline);
    setCursor(0, 0);
    LiquidCrystal_I2C::print(estado);
    setCursor(11, 0);
    infoCut(zona, 9); // muestra el nombre de la zona con un maximo de 9 caracteres
    if(bnum) sonido.bip(bnum);
}    

// muestra el texto pasado con un maximo de max caracteres
void DisplayLCD::infoCut(const char *texto, uint8_t max) {
    int size = strlen(texto);
    if(size>max) {
      char infocut[max+1];
      LOG_WARN("* texto recibido de longitud =",size);
      strlcpy(infocut, texto, sizeof(infocut)); 
      LOG_WARN("* texto acortado a =", static_cast<const char*>(infocut));
      LiquidCrystal_I2C::print(infocut);
    }
    else LiquidCrystal_I2C::print(texto);  
}

// muestra info (hasta un maximo de 20 caracteres) en la linea pasada (1, 2 ,3 o 4)
void DisplayLCD::info(const char* info, int line) {
    int size = strlen(info);
    if(size>MAXBUFF-1) {
      char infocut[MAXBUFF];
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
    LiquidCrystal_I2C::print(_blankline);
    setCursor(0, line-1);
    LiquidCrystal_I2C::print(info);
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
void DisplayLCD::infoclear(const char *info, int dnum, sonido_bips btype, int bnum) {
    LOG_DEBUG("[LCD]  Recibido: '",info, "'   (blink=",dnum, ") biptype=",btype,"(veces=",bnum,")");
    clear();
    if(info=="STOP") setCursor(8,1);
    else setCursor(0, 0);
    LiquidCrystal_I2C::print(info);
      if (btype == LONGBIP) sonido.longbip(bnum);
      if (btype == LOWBIP) sonido.lowbip(bnum);
      if (btype == BIP) sonido.bip(bnum);
      if (btype == BIPOK) sonido.bipOK();
      if (btype == BIPKO) sonido.bipKO();
      if (btype == BIPFIN) sonido.bipFIN();
    if(dnum) lcd.blinkLCD(dnum);
}

void DisplayLCD::displayTemp(int temperature) 
{
  LOG_TRACE("temperatura recibida=",temperature,"temp ESP32=",temperatureRead());
  if(temperatureRead() > config.warnESP32temp) {   // aviso de temperatura excesiva del ESP32
    setCursor(14, 0); print("!"); sonido.bip(2);
    setCursor(15, 0); print(temperatureRead());
  }
  else {
    setCursor(14, 0);
    if (temperature == 999) print(" --");
    else LiquidCrystal_I2C::printf(" %2d",temperature);
  }  
  setCursor(17, 0); print("\xDF" "C"); // xDF = caracter grado centigrado
  print(config.tempRemote>0? "." : " "); // punto indica si temp remota 
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
    LiquidCrystal_I2C::createChar(1, cc1);
    LiquidCrystal_I2C::createChar(2, cc2);
    LiquidCrystal_I2C::createChar(3, cc3);
    LiquidCrystal_I2C::createChar(4, cc4);
    LiquidCrystal_I2C::createChar(5, cc5);
    LiquidCrystal_I2C::createChar(6, cc6);
    LiquidCrystal_I2C::createChar(7, cc7);
    LiquidCrystal_I2C::createChar(8, cc8);
}

void DisplayLCD::printTwoNumber(uint8_t number, uint8_t position, uint8_t line)  // muestra dos digitos en bigchar
{
  // Print position is NO hardcoded
  int digit0; // To represent the ones
  int digit1; // To represent the tens
  digit0 = number % 10;
  digit1 = number / 10;

  // Line 1 of the two-digit number
  //LiquidCrystal_I2C::setCursor(position, 0);
  LiquidCrystal_I2C::setCursor(position, line);  //linea superior 1 = segunda linea
  LiquidCrystal_I2C::write(bn1[digit1 * 3]);
  LiquidCrystal_I2C::write(bn1[digit1 * 3 + 1]);
  LiquidCrystal_I2C::write(bn1[digit1 * 3 + 2]);
  //LiquidCrystal_I2C::write(B); // Blank
  LiquidCrystal_I2C::write(bn1[digit0 * 3]);
  LiquidCrystal_I2C::write(bn1[digit0 * 3 + 1]);
  LiquidCrystal_I2C::write(bn1[digit0 * 3 + 2]);

  // Line 2 of the two-digit number
  //LiquidCrystal_I2C::setCursor(position, 1);
  LiquidCrystal_I2C::setCursor(position, line+1);  //linea inferior 2 = tercera linea
  LiquidCrystal_I2C::write(bn2[digit1 * 3]);
  LiquidCrystal_I2C::write(bn2[digit1 * 3 + 1]);
  LiquidCrystal_I2C::write(bn2[digit1 * 3 + 2]);
  //LiquidCrystal_I2C::write(B); // Blank
  LiquidCrystal_I2C::write(bn2[digit0 * 3]);
  LiquidCrystal_I2C::write(bn2[digit0 * 3 + 1]);
  LiquidCrystal_I2C::write(bn2[digit0 * 3 + 2]);
}

void DisplayLCD::printColons(uint8_t position, uint8_t line)
{
  LiquidCrystal_I2C::setCursor(position, line);
  LiquidCrystal_I2C::write (C);
  LiquidCrystal_I2C::setCursor(position, line+1);
  LiquidCrystal_I2C::write (C);
}

void DisplayLCD::printNoColons(uint8_t position, uint8_t line)
{
  LiquidCrystal_I2C::setCursor(position, line);
  LiquidCrystal_I2C::write (B);
  LiquidCrystal_I2C::setCursor(position, line+1);
  LiquidCrystal_I2C::write (B);
}

