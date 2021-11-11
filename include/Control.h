#ifndef control_h
  #define control_h

  #ifndef _GNU_SOURCE  // LOCAL WORK-AROUND
   #define _GNU_SOURCE // evitar error uint no definido tras update a espressif8266 3.2.0
  #endif               // ver: https://community.platformio.org/t/error-acessing-eeprom-of-esp8266-after-plattform-update/22747/2

  #ifdef NODEMCU
    #include <WifiUdp.h>
    #include <ESP8266HTTPClient.h>
    #include <ESP8266WiFi.h>
    #include <DNSServer.h>
    #include <ESP8266WebServer.h >
    #include <WiFiManager.h> 
  #endif
  
  #include <SPI.h>
  #include <NTPClient.h>
  #include <Time.h>
  #include <Timezone.h>
  #include <ClickEncoder.h>
  #include <CountUpDownTimer.h>
  #include <Streaming.h>
  #include <ArduinoJson.h>
  #include <Ticker.h>
  #include <LittleFS.h>


  //Para mis clases
  #include "Display.h"
  #include "Configure.h"

  #ifdef DEVELOP
    //Comportamiento general para PRUEBAS . DESCOMENTAR LO QUE CORRESPONDA
    #define DEBUG
    //#define EXTRADEBUG
    #define TRACE
    //#define EXTRATRACE
    #define VERBOSE
  #endif

  #ifdef RELEASE
    //Comportamiento general para uso normal . DESCOMENTAR LO QUE CORRESPONDA
    //#define DEBUG
    //#define EXTRADEBUG
    //#define TRACE
    //#define EXTRATRACE
    #define VERBOSE
  #endif

  #ifdef DEMO
    //Comportamiento general para DEMO . DESCOMENTAR LO QUE CORRESPONDA
    #define DEBUG
    //#define EXTRADEBUG
    //#define TRACE
    //#define EXTRATRACE
    #define VERBOSE
  #endif

  #define VERSION  "2.0.2"

  #define xNAME  //actualiza desc de botones con el Name del dispositivo que devuelve Domoticz

  //Comportamiento General
  #define STANDBYSECS         15
  #ifdef RELEASE
    #define DEFAULTMINUTES      10
    #define DEFAULTSECONDS      0
  #endif
  #ifdef DEVELOP
    #define DEFAULTMINUTES      0
    #define DEFAULTSECONDS      10
  #endif
  #ifdef DEMO
    #define DEFAULTMINUTES      0
    #define DEFAULTSECONDS      7
  #endif
  #define DEFAULTBLINK        5
  #define DEFAULTBLINKMILLIS  500
  #define MINMINUTES          0
  #define MAXMINUTES          59  // corte automatico de seguridad a los 60 min. en los arduinos
  #define MINSECONDS          5
  #define HOLDTIME            3000
  #define MAXCONNECTRETRY     10

 //----------------  dependientes del HW   ----------------------------------------

  #ifdef NODEMCU
    #define ENCCLK                D0
    #define ENCDT                 D1
    #define ENCSW                 100
    #define BUZZER                2
    #define HC595_DATA            D8
    #define HC595_LATCH           D4
    #define HC595_CLOCK           D5
    #define CD4021B_CLOCK         D5
    #define CD4021B_LATCH         D6
    #define CD4021B_DATA          D7
    #define LEDR                  4
    #define LEDG                  5
    #define LEDB                  3 
    #define lGRUPO1               6
    #define lGRUPO2               7
    #define lGRUPO3               8
    #define lZONA1                10
    #define lZONA2                11
    #define lZONA3                12
    #define lZONA4                13
    #define lZONA5                14
    #define lZONA6                15
    #define lZONA7                16
  #endif
 //----------------  fin dependientes del HW   ----------------------------------------


  //Para legibilidad del codigo
  #define ON  1
  #define OFF 0
  #define SHOW 1
  #define HIDE 0
  #define READ 1
  #define CLEAR 0

  //Enums
  enum _estados {
    STANDBY       = 0x01,
    REGANDO       = 0x02,
    CONFIGURANDO  = 0x04,
    TERMINANDO    = 0x08,
    PAUSE         = 0x10,
    STOP          = 0x20,
    MULTIREGANDO  = 0x40,
    ERROR         = 0x80,
  };

  enum _flags {
    ENABLED      = 0x01,
    DISABLED     = 0x02,
    ONLYSTATUS   = 0x04,
    ACTION       = 0x08,
    DUAL         = 0x10,
    HOLD         = 0x20,
  };

  //----------------  dependientes del HW   ----------------------------------------
  // ojo esta es la posición del bit de cada boton en el stream serie - no modificar -
  enum _botones {
    bZONA1      = 0x0001,
    bZONA2      = 0x0002,
    bZONA3      = 0x0004,
    bZONA4      = 0x0008,
    bZONA6      = 0x0010,
    bMULTIRIEGO = 0x0020,
    bZONA7      = 0x0040,
    bZONA5      = 0x0080,
    bSPARE13    = 0x0100,
    bGRUPO3     = 0x0200,
    bGRUPO1     = 0x0400,
    bSTOP       = 0x0800,
    bENCODER    = 0x1000,
    bSPARE15    = 0x2000,
    bSPARE16    = 0x4000,
    bPAUSE      = 0x8000,
  };

  //Pseudobotones
  #define bGRUPO2   0xFF01
  #define bCONFIG   0xFF02

  //----------------  dependientes del HW (número, orden)  ----------------------------
    // lista de todos los botones de zonas de riego disponibles:
  #define _ZONAS  bZONA1 , bZONA2 , bZONA3 , bZONA4 , bZONA5 , bZONA6 , bZONA7
    // lista de todos los botones (selector) de grupos disponibles:
  #define _GRUPOS bGRUPO1 , bGRUPO2 , bGRUPO3
  #define _NUMZONAS            7  // numero de zonas (botones riego individual)
  #define _NUMGRUPOS           3  // numero de grupos multirriego
 //----------------  fin dependientes del HW   ----------------------------------------

  //estructura para salvar un grupo
  struct Grupo_parm {
    uint16_t id;
    int size;
    uint16_t serie[16];  // ojo! numero de la zona, no es el boton asociado a ella
    char desc[20];
  } ;

  //estructura para salvar parametros de un boton
  struct Boton_parm {
    char  desc[30];
    uint16_t   idx;
  } ;

  //estructura para parametros configurables
  struct Config_parm {
    uint8_t   initialized=0;
    static const int  n_Zonas = _NUMZONAS; //no modificable por fichero de parámetros (depende HW) 
    Boton_parm botonConfig[n_Zonas];
    uint8_t   minutes = DEFAULTMINUTES; 
    uint8_t   seconds = DEFAULTSECONDS;
    char domoticz_ip[40];
    char domoticz_port[6];
    char ntpServer[40];
    static const int  n_Grupos = _NUMGRUPOS;  //no modificable por fichero de parámetros (depende HW)
    Grupo_parm groupConfig[n_Grupos];
  };

  // estructura de un grupo de multirriego *nueva
  // (todos son pointer al multirriego correspondiente en config menos los indicados)
  struct S_MULTI {
    uint16_t *id;        //apuntador al id del selector grupo en estructura config (bGrupo_x)
    uint16_t serie[16];  //contiene los id de los botones del grupo (bZona_x)
    uint16_t *zserie;    //apuntador a config con las zonas del grupo (Zona_x)
    int *size;           //apuntador a config con el tamaño del grupo
    int w_size;          //variable auxiliar durante ConF
    int actual;          //variable auxiliar durante un multirriego 
    char *desc;          //apuntador a config con la descripcion del grupo
  } ;

  union S_bFLAGS
  {
    uint8_t all_flags;
    struct
    {
      uint8_t enabled       : 1,
              disabled      : 1,
              onlystatus    : 1,
              action        : 1,
              dual          : 1,
              hold          : 1,
              holddisabled  : 1,
              spare0        : 1;
    };
  };
  struct S_initFlags     {
    uint8_t initParm    : 1,
            initWifi      : 1,
            spare1        : 1;
  };

  struct S_BOTON {
    uint16_t   id;
    int   estado;
    int   ultimo_estado;
    int   led;
    S_bFLAGS  flags;
    char  desc[30];
    uint16_t   idx;
  } ;

  struct S_Estado {
    uint8_t estado; 
  } ;

  const uint16_t ZONAS[] = {_ZONAS};
  const uint16_t GRUPOS[]  = {_GRUPOS};
  const int NUMZONAS = sizeof(ZONAS)/sizeof(ZONAS[0]); // (7) numero de zonas (botones riego individual)
  const int NUMGRUPOS = sizeof(GRUPOS)/sizeof(GRUPOS[0]); // (3) // numero de grupos multirriego

   //Globales a todos los módulos
  #ifdef __MAIN__

    uint ledOrder[] = { lZONA1 , lZONA2 , lZONA3 , lZONA4 , lZONA5 , lZONA6 , lZONA7 ,
                        LEDR , LEDG , lGRUPO1 , lGRUPO2 , lGRUPO3 };
    
    S_BOTON Boton [] =  { 
      //ID,          S   uS  LED          FLAGS                             DESC          IDX
      {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1",       0},
      {bZONA2 ,     0,  0,  lZONA2 ,     ENABLED | ACTION,                 "ZONA2",       0},
      {bZONA3    ,  0,  0,  lZONA3    ,  ENABLED | ACTION,                 "ZONA3",       0},
      {bZONA4    ,  0,  0,  lZONA4    ,  ENABLED | ACTION,                 "ZONA4",       0},
      {bZONA5    ,  0,  0,  lZONA5    ,  ENABLED | ACTION,                 "ZONA5",       0},
      {bZONA6 ,     0,  0,  lZONA6 ,     ENABLED | ACTION,                 "ZONA6",       0},
      {bZONA7  ,    0,  0,  lZONA7  ,    ENABLED | ACTION,                 "ZONA7",       0},
      {bSPARE13,    0,  0,  0,           DISABLED,                         "spare13",     0},
      {bSPARE15,    0,  0,  0,           DISABLED,                         "spare15",     0},
      {bSPARE16,    0,  0,  0,           DISABLED,                         "spare16",     0},
      {bENCODER,    0,  0,  0,           ENABLED | ONLYSTATUS | DUAL,      "ENCODER",     0},
      {bMULTIRIEGO, 0,  0,  0,           ENABLED | ACTION,                 "MULTIRIEGO",  0},
      {bGRUPO1,     0,  0,  lGRUPO1,     ENABLED | ONLYSTATUS | DUAL,      "CESPED",      0},
      {bGRUPO2  ,   0,  0,  lGRUPO2  ,   DISABLED,                         "COMPLETO",    0},
      {bGRUPO3,     0,  0,  lGRUPO3,     ENABLED | ONLYSTATUS | DUAL,      "GOTEOS",      0},
      {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE",       0},
      {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",        0},
      {bCONFIG,     0,  0,  0,           DISABLED,                         "CONFIG",      0}
    };
    int NUM_S_BOTON = sizeof(Boton)/sizeof(Boton[0]);

    S_MULTI multi;  //estructura con variables del multigrupo activo
    bool connected;
    bool NONETWORK;
    bool VERIFY;
    bool falloAP;
    bool saveConfig = false;

    S_initFlags initFlags ;

    char version_n[10];

  #else
    extern S_BOTON Boton [];
    extern uint ledOrder[];
    extern bool connected;
    extern bool NONETWORK;
    extern bool VERIFY;
    extern bool falloAP;
    extern bool saveConfig;
    extern char version_n;
    extern S_initFlags initFlags;
    extern int NUM_S_BOTON;
    extern S_MULTI multi;

  #endif

  //Globales a este módulo
  #ifdef __MAIN__

    //Segun la arquitectura
    #ifdef NODEMCU
      WiFiClient client;
      HTTPClient httpclient;
    #endif
    //Globales
    time_t   lastRiegos[NUMZONAS];
    uint     factorRiegos[NUMZONAS];
    CountUpDownTimer T(DOWN);
    S_Estado Estado;
    ClickEncoder *Encoder;
    Display     *display;
    Configure *configure;
    uint8_t minutes;
    uint8_t seconds;
    char  descFR[30];
    int value;
    int savedValue;
    int ledID = 0;
    bool tiempoTerminado;
    bool reposo = false;
    unsigned long standbyTime;
    bool displayOff = false;
    unsigned long lastBlinkPause;
    bool multiriego = false;
    bool multiSemaforo = false;
    bool holdPause = false;
    unsigned long countHoldPause;
    bool flagV = OFF;
    int ledState = LOW;
    bool timeOK = false;
    bool factorRiegosOK = false;
    bool errorOFF = false;
    bool simErrorOFF = false;
    bool displayOFF = false;

  #endif

  //Funciones (prototipos)
  void apagaLeds(void);
  void bip(int);
  void bipOK(int);
  int  bId2bIndex(uint16_t);
  void blinkPause(void);
  void check(void);
  bool checkWifi(void);
  void cleanFS(void);
  void configModeCallback (WiFiManager *);
  bool copyConfigFile(const char*, const char*);
  void dimmerLeds(void);
  void displayGrupo(uint16_t *, int);
  bool domoticzSwitch(int,char *);
  void filesInfo(void);
  void loadDefaultSignal(uint);
  void enciendeLeds(void);
  void flagVerificaciones(void);
  int  getFactor(uint16_t);
  int setMultibyId(uint16_t , Config_parm&);
  uint16_t getMultiStatus(void);
  String *httpGetDomoticz(String *);
  uint idarrayRiego(uint16_t);
  void initCD4021B(void);
  void initClock(void);
  void initFactorRiegos(void);
  void initHC595(void);
  void initLastRiegos(void);
  void initLeds(void);
  bool initRiego(uint16_t);
  void led(uint8_t,int);
  void ledRGB(int,int,int);
  bool ledStatusId(int);
  bool loadConfigFile(const char*, Config_parm&);
  void longbip(int);
  void memoryInfo(void);
  int  nGrupos();
  void parpadeoLedON(void);
  void parpadeoLedZona(void);
  S_BOTON *parseInputs(bool);
  void printFile(const char*);
  void printMulti(void);
  void printMultiGroup(Config_parm&, int);
  void printParms(Config_parm&);
  void procesaBotones(void);
  void procesaEncoder(void);
  void procesaEstados(void);
  void refreshTime(void);
  void refreshDisplay(void);
  bool saveConfigFile(const char*, Config_parm&);
  void saveWifiCallback(void);
  bool setupConfig(const char*, Config_parm&);
  void setupEstado(void);
  void setupInit(void);
  void setupParm(void);
  void setupRedWM(Config_parm&);
  void StaticTimeUpdate(void);
  void statusError(String, int n);
  bool stopRiego(uint16_t);
  void stopAllRiego(bool);
  bool testButton(uint16_t, bool);
  void timeByFactor(int,uint8_t *,uint8_t *);
  void ultimosRiegos(int);
  void Verificaciones(void);
  void wifiClearSignal(uint);
  void zeroConfig(Config_parm&);

#endif
