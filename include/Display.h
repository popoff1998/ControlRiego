#ifndef Display_h
  #define Display_h
  #include <TM1637.h>
  #include <Control.h>

  //Para el display
  #ifdef MEGA256
    #define DISPCLK             31
    #define DISPDIO             33
  #endif

  #ifdef NODEMCU
    #ifdef NEWPCB
      #define DISPCLK             D3
      #define DISPDIO             D2
    #else
      #define DISPCLK             D2
      #define DISPDIO             D3
    #endif
  #endif

  class Display
  {
    private:
      TM1637 ledDisp;
      uint8_t actual[5];

    public:
      Display(uint8_t clk,uint8_t dio);
      void printRaw(uint8_t *);
      void print(const char *);
      void print(int);
      void printTime(int,int);
      void blink(int);
      void clearDisplay(void);
      void check(int);
      void refreshDisplay(void);
      
  };

#endif
