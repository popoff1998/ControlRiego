#ifndef Configure_h
#define Configure_h
#include <Control.h>

class Configure
{
  private:
    bool _configuringTime;
    bool _configuringIdx;
    bool _configuringMulti;
    bool _configuringMultiTemp;
    bool _configuringMenu;
    int _actualIdxIndex;
    int _actualGrupo;
    int _maxItems;
    int _currentItem;
    void configureTime_display(struct Config_parm&);
    void configureMulti_display(void);
    void configureIdx_display(void);

  public:
    Configure(void);
    void menu(void);
    void reset(void);
    void exit(struct Config_parm&);
    void Time_process_start(struct Config_parm&);
    void Time_process_end(struct Config_parm&);
    void Idx_process_start(struct Config_parm&, int);
    void Idx_process_end(struct Config_parm&);
    void Multi_process_start(int);
    void Multi_process_update(void);
    void Multi_process_end(struct Config_parm&);
    void MultiTemp_process_start(void);
    void MultiTemp_process_end(struct Config_parm&);
    bool configuringTime(void);
    bool configuringIdx(void);
    bool configuringMulti(void);
    bool configuringMultiTemp(void);
    bool statusMenu(void);
    int  showMenu(int);
    void procesaSelectMenu(struct Config_parm&);
    int  get_ActualIdxIndex(void);
    int  get_ActualGrupo(void);
    int  get_maxItems(void);
    int  get_currentItem(void);
};

#endif
