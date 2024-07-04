#ifndef Configure_h
#define Configure_h
#include <Control.h>

class Configure
{
  private:
    struct Config_parm &config;
    int _actualIdxIndex;
    int _actualGrupo;
    int _maxItems;
    int _currentItem;
    int *configValuep;
    union {
      uint8_t all_configureflags;
      struct { uint8_t
          _configuringTime        : 1,
          _configuringIdx         : 1,
          _configuringMulti       : 1,
          _configuringMultiTemp   : 1,
          _configuringRange       : 1,
          _configuringMenu        : 1,
          spare0                  : 1,
          spare1                  : 1;
      };
    };
    void configureMulti_display(void);


  public:
    Configure(struct Config_parm&);
    void menu(int item=-1);
    void reset(void);
    void exit(void);
    void Idx_process_start(int);
    void Idx_process_end(void);
    void Time_process_start(void);
    void Time_process_end(void);
    void Range_process_start(int, int, int=100);
    void Range_process_end(void);
    void Multi_process_start(int);
    void Multi_process_update(void);
    void Multi_process_end(void);
    void MultiTemp_process_start(void);
    void MultiTemp_process_end(void);
    bool configuringTime(void);
    bool configuringIdx(void);
    bool configuringMulti(void);
    bool configuringMultiTemp(void);
    bool configuringRange(void);
    bool statusMenu(void);
    int  showMenu(int);
    void procesaSelectMenu(void);
    int  get_ActualIdxIndex(void);
    int  get_ActualGrupo(void);
    int  get_maxItems(void);
    int  get_currentItem(void);
};

#endif
