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
    void configureTime_process(struct Config_parm&);
    void configureMulti_process(void);
    void configureIdx_process(void);

  public:
    Configure(void);
    void menu(void);
    void exit(struct Config_parm&);
    void configureTime(struct Config_parm&);
    bool configuringTime(void);
    void configuringTime_process_end(struct Config_parm&);
    void configureIdx(int);
    bool configuringIdx(void);
    void configuringIdx_process_end(struct Config_parm&);
    void configureMulti(int);
    bool configuringMulti(void);
    void configuringMulti_process_update(void);
    void configuringMulti_process_end(struct Config_parm&);
    void configureMultiTemp(void);
    bool configuringMultiTemp(void);
    void configuringMultiTemp_process_end(struct Config_parm&);
    bool statusMenu(void);
    int mostrar_menu(int);
    void procesaSelectMenu(struct Config_parm&);
    int getActualIdxIndex(void);
    int getActualGrupo(void);
    int get_maxItems(void);
    int get_currentItem(void);
};

#endif
