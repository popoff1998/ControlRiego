#ifndef Configure_h
#define Configure_h
#include <Control.h>

class Configure
{
  private:
    class Display *display;
    bool _configuringTime;
    bool _configuringIdx;
    bool _configuringMulti;
    int _actualIdxIndex;
  public:
    Configure(class Display *);
    void start(void);
    void stop(void);
    void configureTime(void);
    void configureIdx(int);
    void configureMulti(void);
    bool configuringTime(void);
    bool configuringIdx(void);
    bool configuringMulti(void);
    int getActualIdxIndex(void);
};

#endif
