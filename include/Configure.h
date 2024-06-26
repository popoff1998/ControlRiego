#ifndef Configure_h
#define Configure_h
#include <Control.h>

class Configure
{
  private:
    struct Config_parm &config;
    bool _configuringTime;
    bool _configuringIdx;
    bool _configuringMulti;
    bool _configuringMultiTemp;
    bool _configuringESPtmpWarn;
    bool _configuringMenu;
    int _actualIdxIndex;
    int _actualGrupo;
    int _maxItems;
    int _currentItem;
    void configureTime_display(void);
    void configureMulti_display(void);
    void configureIdx_display(void);

  public:
    Configure(struct Config_parm&);
    void menu(void);
    void reset(void);
    void exit(void);
    void Time_process_start(void);
    void Time_process_end(void);
    void Idx_process_start(int);
    void Idx_process_end(void);
    void ESPtmpWarn_process_start(void);
    void ESPtmpWarn_process_end(void);
    void Multi_process_start(int);
    void Multi_process_update(void);
    void Multi_process_end(void);
    void MultiTemp_process_start(void);
    void MultiTemp_process_end(void);
    bool configuringTime(void);
    bool configuringIdx(void);
    bool configuringMulti(void);
    bool configuringMultiTemp(void);
    bool configuringESPtmpWarn(void);
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
