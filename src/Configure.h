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
    struct S_BOTON *confBoton;

  public:
    Configure(class ClickEncoder *,class Display *);
    void start(void);
    bool idx(struct S_BOTON *);
    bool defaultTime(void);
};

#endif
