#ifndef Configure_h
#define Configure_h
#include <Control.h>

class Configure
{
  private:
    //  Las opciones del menu se muestran en pantalla en el orden en que se definen aqui,
    //  la primera opcion debe ser IDX_MULT y la ultima __ENDLINE__ ,
    //  el resto se pueden mover libremente.
    //  Para añadir una nueva opcion, añadirla en el enum _menuItems y en el array opcionesMenuConf
    //  y añadir el case correspondiente en procesaSelectMenu
    enum _menuItems {     
      IDX_MULT      = 0,  //  botones IDX/MULT debe ser fijo primer item
      DFLT_TIME     ,   
      COPY_BACKUP   ,
      WIFI_PARM     ,
      WEBSERVER_ACT ,
      LOAD_BACKUP   ,
      ESP32_TEMP    ,
      LED_DIMM_LVL  , 
      LED_MAX_LVL   ,
      TEMP_ADJ      ,
      TEMP_SOURCE   ,
      REM_TEMP_IDX  , 
      MSG_TIME      ,
      MUTE          ,   
      VOLUME        ,
      FIN_MELODY    ,
      NIVEL_WIFI    ,
      XNAME_ONOFF   ,
      VERIFY_ONOFF  ,
      DYNAMIC       ,
      __ENDLINE__         //  ultimo item fijo (= numero de lineas del menu - 1)
    };
    struct Config_parm &config;
    int _actualIdxIndex;
    int _actualGrupo;
    int _maxItems;
    int _currentItem;
    int _rangeFactor;
    int *configValuep;
    int _data_pos[__ENDLINE__ + 1];  //  posicion de los datos en la linea de menu
    bool _data_pos_valid;
    char _currenItemText[18];
    union {
      uint8_t all_configureflags;
      struct { uint8_t
          _configuringTime        : 1,
          _configuringIdx         : 1,
          _configuringMulti       : 1,
          _configuringMultiTemp   : 1,
          _configuringRange       : 1,
          _configuringMenu        : 1,
          _configuringMelody      : 1,
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
    void Idx_process_update(void);
    void Idx_process_end(void);
    void Time_process_start(void);
    void Time_process_update(void);
    void Time_process_end(void);
    void Range_process_start(int min, int max, int aceleracion=100, int rangefactor=100);
    void Range_process_update(void);
    void Range_process_end(void);
    void Multi_process_start(int);
    void Multi_process_update(void);
    void Multi_process_end(bool);
    void MultiTemp_process_start(void);
    void MultiTemp_process_end(void);
    bool configuringTime(void);
    bool configuringIdx(void);
    bool configuringMulti(void);
    bool configuringMultiTemp(void);
    bool configuringRange(void);
    bool configuringMelody(void);
    bool statusMenu(void);
    int  showMenu(int);
    void procesaSelectMenu(void);
    int  get_ActualIdxIndex(void);
    int  get_ActualGrupo(void);
    int  get_maxItems(void);
    int  get_currentItem(void);
    int  get_datapos(void);
};

#endif
