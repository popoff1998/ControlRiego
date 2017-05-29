#ifndef Configure_h
#define Configure_h
//#include <Display.h>
#include <Control.h>

class Configure
{
  private:
    class ClickEncoder *encoder;
    class Display *display;
    int clkvalue;

  public:
    Configure(class ClickEncoder *,class Display *);
    void start(void);
    void idx(struct S_BOTON *);
};

#endif
