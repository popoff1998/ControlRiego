#ifndef DisplayLCD_h
  #define DisplayLCD_h
  
  #include "TiposGlobales.h"
  #include "LiquidCrystal_I2C.h"

  #define LCDBIGROW 2           // linea por defecto para timer en numeros grandes
  #define LCDBIGCOL 7           // columna por defecto para timer en numeros grandes
  #define LCDMAXLEN 20          // numero maximo de caracteres por linea pantalla lcd
  #define MAXBUFF LCDMAXLEN+1   // tamaño maximo del buffer (mas 0 terminacion)

  class DisplayLCD : public  LiquidCrystal_I2C
  {
   private:
      bool _displayOff;
      unsigned long _lastBlinkPause;
      const char* _blankline = "                    ";
      void DefineLargeChar(void);
      void printTwoNumber(uint8_t , uint8_t , uint8_t );
      void printColons(uint8_t , uint8_t);
      void printNoColons(uint8_t , uint8_t);

   public:
      DisplayLCD(uint8_t lcd1_Addr,uint8_t lcd1_cols,uint8_t lcd1_rows);
      void initLCD(void);
      void blinkLCD(int);
      void clear(int mitad=0);
      void setCursor(uint8_t, uint8_t);  // sobreescibe a LiquidCrystal_I2C::setCursor
      void setCursorBlink(uint8_t, uint8_t);
      void setBacklight(bool);	// alias for backlight() and nobacklight()
      void displayON(void);      // muestra texto del display
      void displayOFF(void);     // oculta texto del display
      void displayTemp(int, int);
      void displayTime(uint8_t minute, uint8_t second, uint8_t col=LCDBIGCOL, uint8_t line=LCDBIGROW);
      void infoclear(const char *info, int line=1);
      void infoclear(const char *info, int dnum, sonido_bips btype, int bnum=0);
      void infoEstado(const char* estado, const char* zona, int bnum=0);
      void info(const char* info, int line);
      void info(const char* info, int line, int size);
      void infoCut(const char *texto, uint8_t max);
      bool get__displayOff(void);
      // template para imprimir diferentes tipos de datos que admita LiquidCrystal_I2C::print() cuando es privada
      // template<typename T>
      // void print(const T& value) {LiquidCrystal_I2C::print(value);}
};


#endif  /*DisplayLCD_h*/