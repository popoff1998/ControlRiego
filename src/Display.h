#ifndef Display_h
#define Display_h
#include <TM1637.h>
#include <Control.h>

//Para el display
#define DISPCLK             31
#define DISPDIO             33

class Display
{
  private:
    //uint8_t *actual;
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
};

#endif
