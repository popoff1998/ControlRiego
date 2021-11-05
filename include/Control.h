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

  /* NOTA1: es necesario ejecutar una vez con FORCEINITEEPROM=1 para actualizar los valores asignados en el 
    programa a:
          - estructura BOTONES (idX)
          - estructura multiGroup (botones y tamaño de cada grupo de multirriegos)
          - tiempo por defecto para el riego
          - parametros conexion a Domoticz y Ntp
      ya que estos valores se graban en la eeprom y se usan los leidos de ella en setup.    
    después poner a 0 y volver a cargar */
  
  #define FORCEINITEEPROM     0

  #define VERSION  "2.0"

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
  #define NUMGRUPOS           3  // numero de grupos multirriego
  #define NUMZONAS            7  // numero de zonas (botones riego individual)


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

  //----------------------------------------------------------------------------------

  //estructura para salvar un grupo
  struct Grupo_parm {
    uint16_t id;
    int size;
    uint16_t serie[16];
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
    static const int  n_Zonas = NUMZONAS; //no modificable por fichero de parámetros (depende HW) 
    Boton_parm botonConfig[n_Zonas];
    uint8_t   minutes = DEFAULTMINUTES; 
    uint8_t   seconds = DEFAULTSECONDS;
    char domoticz_ip[40];
    char domoticz_port[6];
    char ntpServer[40];
    static const int  n_Grupos = NUMGRUPOS;  //no modificable por fichero de parámetros (depende HW)
    Grupo_parm groupConfig[n_Grupos];
  };

  //--------------------------------------------------------------------------------------


  //estructura para salvar grupos de multirriego en la eeprom:
  struct _eeprom_group {
    int size;
    uint16_t serie[16];
  } ;

  //Estructura de mi eeprom (se reserva más tamaño despues en funcion del numero de grupos)
  struct __eeprom_data {
    uint8_t   initialized;
    uint16_t  botonIdx[16]; // IDX de cada boton para el Domoticz
    uint8_t   minutes;      // minutos de riego por defecto 
    uint8_t   seconds;      // segundos de riego por defecto
    char domoticz_ip[40];
    char domoticz_port[6];
    char ntpServer[40];
    int       numgroups;     // numero de grupos de multirriego
    _eeprom_group groups[]; // grupos de multirriego
  };

  //estructura de un grupo de multirriego *nueva
  // (todos son pointer al multirriego correspondiente en config menos:    )
  // (          actual  <- es una variable de trabajo durante el multirriego)
  // (          serie[] <- contiene el id del boton, no el numero de la zona de config)
  struct S_MULTI2 {
    uint16_t *id;
    uint16_t serie[16];
    int *size;
    int actual;
    char *desc;
  } ;

  //estructura de un grupo de multirriego
  struct S_MULTI {
    uint16_t id;
    uint16_t *serie;
    int size;
    int actual;
    char desc[20];
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
    uint8_t initEeprom    : 1,
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

  #define NUMRIEGOS sizeof(COMPLETO)/sizeof(COMPLETO[0])

   //Globales a todos los módulos
  #ifdef __MAIN__

    // dependientes del HW (número, orden y caracteristicas) :
        // lista de todos los botones de zonas de riego disponibles:
        uint16_t COMPLETO[]  = { bZONA1 , bZONA2 , bZONA3 , bZONA4 , bZONA5 , bZONA6 , bZONA7 };
        // lista de todos los botones (selector) de grupos disponibles:
        uint16_t GRUPOS[]  = { bGRUPO1 , bGRUPO2 , bGRUPO3 };
        // lista de todos los leds disponibles (para su encendido en el autochequeo):
        uint ledOrder[] = { lZONA1 , lZONA2 , lZONA3 , lZONA4 , lZONA5 , lZONA6 , lZONA7 ,
                            LEDR , LEDG , lGRUPO1 , lGRUPO2 , lGRUPO3 };
        
        S_BOTON Boton [] =  { 
          //ID,          S   uS  LED          FLAGS                             DESC          IDX
          {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "TURBINAS",   25},
          {bZONA2 ,     0,  0,  lZONA2 ,     ENABLED | ACTION,                 "PORCHE",     27},
          {bZONA3    ,  0,  0,  lZONA3    ,  ENABLED | ACTION,                 "CUARTILLO",  58},
          {bZONA4    ,  0,  0,  lZONA4    ,  ENABLED | ACTION,                 "GOTEOALTO",  59},
          {bZONA5    ,  0,  0,  lZONA5    ,  ENABLED | ACTION,                 "GOTEOBAJO",  24},
          {bZONA6 ,     0,  0,  lZONA6 ,     ENABLED | ACTION,                 "OLIVOS",     61},
          {bZONA7  ,    0,  0,  lZONA7  ,    ENABLED | ACTION,                 "ROCALLA",    30},
          {bSPARE13,    0,  0,  0,           DISABLED,                         "SPARE13",     0},
          {bSPARE15,    0,  0,  0,           DISABLED,                         "SPARE15",     0},
          {bSPARE16,    0,  0,  0,           DISABLED,                         "SPARE16",     0},
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
    // fin dependientes del HW

    time_t   lastRiegos[NUMRIEGOS];
    uint     factorRiegos[NUMRIEGOS];
    S_MULTI *multi;
    S_MULTI2 multi2;  //estructura con variables del multigrupo activo
    //numero de grupos de multirriego:
    //int n_Grupos;
    //parametros de conexion a la red
    bool connected;
    bool NONETWORK;
    bool VERIFY;
    bool falloAP;
    //Para configuracion por web portal (valores por defecto)
    // (ojo ver NOTA1 en Control.h --> FORCEINITEEPROM=1 para actualizarlos)
    //char domoticz_ip[40] = "192.168.1.50";
    //char domoticz_port[6] = "8080";
    //char ntpServer[40] = "es.pool.ntp.org";
    //char domoticz_ip[40] = "192.168.100.60";
    //char domoticz_port[6] = "3380";
    //char ntpServer[40] = "192.168.100.60";
    bool saveConfig = false;

    S_initFlags initFlags ;

    char version_n[10];

  #else
    extern S_BOTON Boton [];
    extern uint ledOrder[];
    extern uint16_t GRUPOS[];
    extern uint16_t COMPLETO[];
    //extern int n_Grupos;
    extern bool connected;
    extern bool NONETWORK;
    extern bool VERIFY;
    extern bool falloAP;
    //extern char domoticz_ip[40];
    //extern char domoticz_port[6];
    //extern char ntpServer[40];
    extern bool saveConfig;
    extern char version_n;
    extern S_initFlags initFlags;
    extern int NUM_S_BOTON;
    extern S_MULTI2 multi2;

  #endif

  //Globales a este módulo
  #ifdef __MAIN__

    //Segun la arquitectura
    #ifdef NODEMCU
      WiFiClient client;
      HTTPClient httpclient;
    #endif
    //Globales
    CountUpDownTimer T(DOWN);
    S_Estado Estado;
    ClickEncoder *Encoder;
    Display     *display;
    Configure *configure;
    uint8_t minutes;
    uint8_t seconds;
    //int value = minutes;
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
  int  bId2bIndex(uint16_t);
  void blinkPause(void);
  void check(void);
  bool checkWifi(void);
  void cleanFS(void);
  void configModeCallback (WiFiManager *);
  void dimmerLeds(void);
  void displayGrupo(uint16_t *, int);
  bool domoticzSwitch(int,char *);
  void eepromWriteSignal(uint);
  void eepromWriteGroups(void);
  void eepromWriteRed(void);
  void enciendeLeds(void);
  void flagVerificaciones(void);
  int  getFactor(uint16_t);
  bool getMultibyId2(uint16_t , Config_parm&);
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
  int  nGrupos();
  void parpadeoLedON(void);
  void parpadeoLedZona(void);
  S_BOTON *parseInputs(bool);
  void printFile(const char*);
  void printMulti2(void);
  void printMultiGroup(Config_parm&);
  void printParms(Config_parm&);
  void procesaBotones(void);
  void procesaEeprom(void);
  void procesaEncoder(void);
  void procesaEstados(void);
  void refreshTime(void);
  void refreshDisplay(void);
  void saveWifiCallback(void);
  void setupEstado(void);
  void setupInit(void);
  void setupParm(const char*);
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

#endif
