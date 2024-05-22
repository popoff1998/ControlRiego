#ifndef Configure_h
#define Configure_h
//#include <Control.h>

class Configure
{
  private:
    //class Display *display;
    bool _configuringTime;
    bool _configuringIdx;
    bool _configuringMulti;
    int _actualIdxIndex;
    int _actualGrupo;
    int _maxItems;
    int _currentItem;
  public:
    Configure(void);
    void start(void);
    void stop(void);
    void configureTime(void);
    void configureIdx(int);
    void configureMulti(int);
    bool configuringTime(void);
    bool configuringIdx(void);
    bool configuringMulti(void);
    bool configuring(void);
    int mostrar_menu(int);
    int getActualIdxIndex(void);
    int getActualGrupo(void);
    int get_maxItems(void);
    int get_currentItem(void);
};

#endif
