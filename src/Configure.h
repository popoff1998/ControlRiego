#ifndef Configure_h
#define Configure_h
//#include <Display.h>
#include <Control.h>

class Configure
{
  private:
    //class ClickEncoder *encoder;
    class Display *display;
    //int clkvalue;
    //struct S_BOTON *confBoton;
    bool _configuringTime;
    bool _configuringIdx;
    int _actualIdxIndex;
  public:
    //Configure(class ClickEncoder *,class Display *);
    Configure(class Display *);
    void start(void);
    void stop(void);
    //bool idx(struct S_BOTON *);
    //bool defaultTime(void);
    void configureTime(void);
    void configureIdx(int);
    bool configuringTime(void);
    bool configuringIdx(void);
    int getActualIdxIndex(void);
};

#endif
