#ifndef Configure_h
#define Configure_h

class Configure
{
  private:
    //  Las opciones del menu se muestran en pantalla en el orden en que se definen aqui,
    //  la primera opcion debe ser IDX_MULT y la ultima __ENDLINE__ ,
    //  el resto se pueden mover libremente en este enum, no siendo necesario hacerlo en otro lugar del codigo.
    //
    //  PARA AÑADIR UNA NUEVA OPCION: añadirla en este enum,
    //  en el array parteFija y case de la parte variable (si tuviera) del showMenu en Configure.cpp,
    //  y su tratamiento en el case correspondiente en procesaSelectMenu
    enum _menuItems {     
      IDX_MULT      = 0,  //  botones IDX/MULT debe ser fijo primer item
      DFLT_TIME     ,   
      COPY_BACKUP   ,
      WIFI_PARM     ,
      #ifdef WEBSERVER 
      WEBSERVER_ACT , 
      #endif
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
      LASTRIEGOS24  ,
      #ifdef LOGTOFILE 
      WARNTOLOG     , 
      #endif
      __ENDLINE__   ,   //  ultimo item fijo 
      NUM_ITEMS         //  (= numero de lineas del menu excluyendo esta)
    };
    int _actualZona;
    int _actualGrupo;
    int _maxItems;
    int _currentItem;
    int _rangeFactor;
    int *configValuep;
    int _data_pos[NUM_ITEMS];  //  posicion de los datos en la linea de menu
    bool _data_pos_valid;
    const char* _currenItemText;
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
          _MultiTempReady         : 1;
      };
    };
    void configureMulti_display(void);
    void reset(void);
    void toggle(bool &value);
  
  public:
    Configure();   // constructor
    void menu(int item=-1);
    void exit(void);
    void Idx_process_start(void);
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
    void Multi_process_end(void);
    void MultiTemp_process_start(void);
    void MultiTemp_process_end(void);
    void procesaSelectMenu(void);
    int  showMenu(int);
    int  get_currentItem() const { return _currentItem; }
    bool configuringTime() const { return _configuringTime; }
    bool configuringIdx() const { return _configuringIdx; }
    bool configuringRange() const { return _configuringRange; }
    bool configuringMulti() const { return _configuringMulti; }
    bool configuringMultiTemp() const { return _configuringMultiTemp; }
    bool configuringMelody() const { return _configuringMelody; }
    bool inMenu() const { return _configuringMenu; }
    bool get_MultiTempReady() const { return _MultiTempReady; }
};

#endif
