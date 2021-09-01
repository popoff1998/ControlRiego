#ifndef control_h
  #define control_h
  
  #ifdef NODEMCU
    #include <WifiUdp.h>
    #include <ESP8266HTTPClient.h>
    #include <ESP8266WiFi.h>
    #include <DNSServer.h>
    #include <ESP8266WebServer.h >
    #include <WiFiManager.h> 
  #endif
  
  #ifdef MEGA256
    #include <Ethernet.h>
    #include <EthernetUdp.h>
    #include <ArduinoHttpClient.h>
  #endif

  #include <SPI.h>
  #include <NTPClient.h>
  #include <Time.h>
  #include <Timezone.h>
  #ifdef MEGA256
    #include <TimerOne.h>
  #endif
  #include <ClickEncoder.h>
  #include <CountUpDownTimer.h>
  #include <Streaming.h>
  #include <ArduinoJson.h>
  #include <Ticker.h>

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

  #define VERSION  "1.3.4-1"

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

  #ifdef MEGA256
    #define CD4021B_LATCH         43
    #define CD4021B_CLOCK         45
    #define CD4021B_DATA          47

    #define HC595_DATA            26
    #define HC595_LATCH           28
    #define HC595_CLOCK           45

    #define ENCCLK              35
    #define ENCDT               37
    #define ENCSW               24
    #define BUZZER              41
  #endif

  #ifdef NODEMCU
    #define ENCCLK              D0
    #define ENCDT               D1
    #define ENCSW               100
    #define BUZZER              2
    #ifdef NEWPCB
      #define HC595_DATA            D8
      #define HC595_LATCH           D4
      #define HC595_CLOCK           D5
      #define CD4021B_CLOCK         D5
      #define CD4021B_LATCH         D6
      #define CD4021B_DATA          D7
      #define LEDR                  4
      #define LEDG                  5
      #define LEDB                  3 
      #define lCESPED               6
      #define lCOMPLETO             7
      #define lGOTEOS               8
      #define lTURBINAS             10
      #define lPORCHE               11
      #define lCUARTILLO            12
      #define lGOTEOALTO            13
      #define lGOTEOBAJO            14
      #define lOLIVOS               15
      #define lROCALLA              16
    #else
      #define HC595_DATA            D8
      #define HC595_LATCH           D4
      #define HC595_CLOCK           D5
      #define CD4021B_CLOCK         D5
      #define CD4021B_LATCH         D6
      #define CD4021B_DATA          D7
      #define LEDR                  7
      #define LEDG                  6
      #define LEDB                  0 //No se está usando ningun led RGB
      #define lCESPED               8
      #define lCOMPLETO             5
      #define lGOTEOS               3
      #define lTURBINAS             15
      #define lPORCHE               14
      #define lCUARTILLO            13
      #define lGOTEOALTO            16
      #define lGOTEOBAJO            10
      #define lOLIVOS               11
      #define lROCALLA              12
    #endif

  #endif

  //Para legibilidad del codigo
  #define ON  1
  #define OFF 0
  #define SHOW 1
  #define HIDE 0

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

  enum _botones {
    bTURBINAS   = 0x0001,
    bPORCHE     = 0x0002,
    bCUARTILLO  = 0x0004,
    bGOTEOALTO  = 0x0008,
    bOLIVOS     = 0x0010,
    bMULTIRIEGO = 0x0020,
    bROCALLA    = 0x0040,
    bGOTEOBAJO  = 0x0080,
    bSPARE13    = 0x0100,
    bGOTEOS     = 0x0200,
    bCESPED     = 0x0400,
    bSTOP       = 0x0800,
    bENCODER    = 0x1000,
    bSPARE15    = 0x2000,
    bSPARE16    = 0x4000,
    bPAUSE      = 0x8000,
  };

  //Pseudobotones
  #define bCOMPLETO 0xFF01
  #define bCONFIG   0xFF02

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
    char serverAddress[40];
    char DOMOTICZPORT[6];
    char ntpServer[40];
    int       numgroups;     // numero de grupos de multirriego
    _eeprom_group groups[]; // grupos de multirriego
  };

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

  //mantenemos aqui definicion completo a efectos de display de lo regado:
  #define NUMRIEGOS sizeof(COMPLETO)/sizeof(COMPLETO[0])

  #ifdef __MAIN__

    uint16_t COMPLETO[]  = {bTURBINAS, bPORCHE, bCUARTILLO, bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA};
    
      S_BOTON Boton [] =  { 
        //ID,          S   uS  LED          FLAGS                             DESC          IDX
        {bTURBINAS,   0,  0,  lTURBINAS,   ENABLED | ACTION,                 "TURBINAS",   25},
        {bPORCHE,     0,  0,  lPORCHE,     ENABLED | ACTION,                 "PORCHE",     27},
        {bCUARTILLO,  0,  0,  lCUARTILLO,  ENABLED | ACTION,                 "CUARTILLO",  58},
        {bGOTEOALTO,  0,  0,  lGOTEOALTO,  ENABLED | ACTION,                 "GOTEOALTO",  59},
        {bOLIVOS,     0,  0,  lOLIVOS,     ENABLED | ACTION,                 "OLIVOS",     61},
        {bMULTIRIEGO, 0,  0,  0,           ENABLED | ACTION,                 "MULTIRIEGO",  0},
        {bROCALLA,    0,  0,  lROCALLA,    ENABLED | ACTION,                 "ROCALLA",    30},
        {bGOTEOBAJO,  0,  0,  lGOTEOBAJO,  ENABLED | ACTION,                 "GOTEOBAJO",  24},
        {bSPARE13,    0,  0,  0,           DISABLED,                         "SPARE13",     0},
        {bGOTEOS,     0,  0,  lGOTEOS,     ENABLED | ONLYSTATUS | DUAL,      "GOTEOS",      0},
        {bCESPED,     0,  0,  lCESPED,     ENABLED | ONLYSTATUS | DUAL,      "CESPED",      0},
        {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",        0},
        {bENCODER,    0,  0,  0,           ENABLED | ONLYSTATUS | DUAL,      "ENCODER",     0},
        {bSPARE15,    0,  0,  0,           DISABLED,                         "SPARE15",     0},
        {bSPARE16,    0,  0,  0,           DISABLED,                         "SPARE16",     0},
        {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE",       0},
        {bCOMPLETO,   0,  0,  lCOMPLETO,   DISABLED,                         "COMPLETO",    0},
        {bCONFIG,     0,  0,  0,           DISABLED,                         "CONFIG",      0}
                        };

    int NUM_S_BOTON = sizeof(Boton)/sizeof(Boton[0]);

    time_t   lastRiegos[NUMRIEGOS];
    uint     factorRiegos[NUMRIEGOS];
    uint ledOrder[] = {lTURBINAS, lPORCHE, lCUARTILLO, lGOTEOALTO, lGOTEOBAJO, lOLIVOS, lROCALLA,
                        LEDR, LEDG, lCESPED, lCOMPLETO, lGOTEOS};
    S_MULTI *multi;
    //numero de grupos de multirriego:
    int n_Grupos;
    //parametros de conexion a la red
    bool connected;
    bool NONETWORK;
    bool falloAP;
    //Para configuracion por web portal (valores por defecto)
    // (ojo ver NOTA1 en Control.h --> FORCEINITEEPROM=1 para actualizarlos)
    char serverAddress[40] = "192.168.100.60";
    char DOMOTICZPORT[6] = "3380";
    char ntpServer[40] = "192.168.100.60";
    bool saveConfig = false;

    S_initFlags initFlags ;

    char version_n[10];

  #else
    extern S_BOTON Boton [];
    extern uint ledOrder[];
    extern int n_Grupos;
    extern bool connected;
    extern bool NONETWORK;
    extern bool falloAP;
    extern char serverAddress[40];
    extern char DOMOTICZPORT[6];
    extern char ntpServer[40];
    extern bool saveConfig;
    extern char version_n;
    extern S_initFlags initFlags;
    extern int NUM_S_BOTON;

  #endif

  //Globales
  #ifdef __MAIN__
    //Segun la arquitectura
    #ifdef MEGA256
      EthernetClient client;
    #endif

    #ifdef NODEMCU
      WiFiClient client;
    #endif

    //Globales
    CountUpDownTimer T(DOWN);
    S_Estado Estado;
    ClickEncoder *Encoder;
    Display     *display;
    Configure *configure;
    uint8_t minutes = DEFAULTMINUTES;
    uint8_t seconds = DEFAULTSECONDS;
    int value = minutes;
    int savedValue;
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

    //Para Ethernet
    byte mac[] = {
    0xDE, 0xAD, 0xBE, 0xBF, 0xFE, 0xED
    };
    IPAddress ip(192, 168, 100, 80);
    byte gateway[] = {192, 168, 100, 100};
    byte subnet[] = {255, 255, 255, 0};
    byte server[] = {192,168,100,60};

    #ifdef MEGA256
      HttpClient httpclient = HttpClient(client,serverAddress,port);
    #endif
    #ifdef NODEMCU
      HTTPClient httpclient;
    #endif

  #endif

  //Funciones (prototipos)
  void apagaLeds(void);
  void bip(int);
  int  bId2bIndex(uint16_t);
  void blinkPause(void);
  void check(void);
  bool checkWifi(void);
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
  S_MULTI *getMultibyId(uint16_t);
  S_MULTI *getMultibyIndex(int);
  uint16_t getMultiStatus(void);
  String *httpGetDomoticz(String *);
  uint idarrayRiego(uint16_t);
  void initCD4021B(void);
  void initClock(void);
  void initFactorRiegos(void);
  void initHC595(void);
  void initLastRiegos(void);
  void initLeds(void);
  void initRiego(uint16_t);
  void led(uint8_t,int);
  void ledRGB(int,int,int);
  bool ledStatusId(int);
  void longbip(int);
  int  nGrupos();
  void parpadeoLedON(void);
  S_BOTON *parseInputs();
  void printMultiGroup(void);
  void procesaBotones(void);
  void procesaEeprom(void);
  void procesaEncoder(void);
  void procesaEstados(void);
  void refreshTime(void);
  void refreshDisplay(void);
  void saveWifiCallback(void);
  void setupEstado(void);
  void setupInit(void);
  void setupRed(void);
  void setupRedWM(void);
  void StaticTimeUpdate(void);
  void stopRiego(uint16_t);
  void stopAllRiego(bool);
  bool testButton(uint16_t, bool);
  void timeByFactor(int,uint8_t *,uint8_t *);
  void ultimosRiegos(int);
  void Verificaciones(void);
  void wifiClearSignal(uint);

#endif
