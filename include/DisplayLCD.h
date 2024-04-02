#ifndef DisplayLCD_h
  #define DisplayLCD_h
  #include "Control.h"
  #include "LiquidCrystal_I2C.h"

  #define LCDBIGROW 2           // linea por defecto para timer en numeros grandes
  #define LCDBIGCOL 7           // columna por defecto para timer en numeros grandes
  #define LCDMAXLEN 20          // numero maximo de caracteres por linea pantalla lcd
  #define MAXBUFF LCDMAXLEN+1   // tamaño maximo del buffer (mas 0 terminacion)

  class DisplayLCD
  {
   private:
      LiquidCrystal_I2C lcdDisp;
      bool _displayOff;
      unsigned long _lastBlinkPause;
      const char* _blankline = "                    ";
      void DefineLargeChar(void);
      void printTwoNumber(uint8_t , uint8_t , uint8_t );
      void printColons(uint8_t , uint8_t);
      void printNoColons(uint8_t , uint8_t);
      void infoEstadoI(const char *estado, const char *zona);
      const char* verify(const char* info2, int size);

   public:
      DisplayLCD(uint8_t lcd1_Addr,uint8_t lcd1_cols,uint8_t lcd1_rows);
      void initLCD(void);
      void blinkLCD(const char *,int);
      void blinkLCD(int);
      void clear(void);
      void clear(int);
      void check(void);
      void print(const char *);
      void print(int);
      void setCursor(uint8_t, uint8_t);
      void setBacklight(bool);	// alias for backlight() and nobacklight()
      void backlight(void);
      void nobacklight(void);
      void displayON(void);      // muestra texto del display
      void displayOFF(void);     // oculta texto del display
      void displayNewIDX(uint8_t n_zona, uint8_t newidx);
      void displayTime(uint8_t minute, uint8_t second);
      void displayTime(uint8_t minute, uint8_t second, uint8_t col, uint8_t line);
      void infoclear(const char *info);
      void infoclear(const char *info, int line);
      void infoclear(const char *info, int dnum, int btype, int bnum);
      void infoEstado(const char* estado);
      void infoEstado(const char* estado, const char* zona);
      void info(const char* info, int line);
      void info(const char* info, int line, int size);
  };

#endif  /*DisplayLCD_h*/