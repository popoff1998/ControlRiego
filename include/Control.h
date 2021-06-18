//TIPO DE RED
#ifndef control_h
#define control_h
//Modelo de red que vamos a usar
#define NET_HTTPCLIENT


#ifdef NODEMCU
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>
  #include <WifiUdp.h>
  #ifdef NET_HTTPCLIENT
    #include <ESP8266HTTPClient.h>
  #endif
#endif

#ifdef MEGA256
  #include <Ethernet.h>
  #include <EthernetUdp.h>
  #ifdef NET_HTTPCLIENT
    #include <ArduinoHttpClient.h>
  #endif
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

#ifdef NET_MQTTCLIENT
  #include <PubSubClient.h>
#endif
#include <ArduinoJson.h>

//Para mis clases
#include "Display.h"
#include "Configure.h"

#ifdef DEVELOP
 //Comportamiento general para PRUEBAS . DESCOMENTAR LO QUE CORRESPONDA
 #define DEBUG
 #define EXTRADEBUG
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
 #define EXTRADEBUG
 #define TRACE
 //#define EXTRATRACE
 #define VERBOSE
#endif


//No olvidarse de que para una nodemcu nueva es conveniente poner FORCEINITEEPROM a 1 la primera vez, después a 0
#define FORCEINITEEPROM     0

//Estructura de mi eeprom
struct __eeprom_data {
  uint8_t initialized;
  uint16_t  botonIdx[16];
  uint8_t   minutes;
  uint8_t   seconds;
};

//Funciones
void check(void);
void StaticTimeUpdate(void);
void domoticzSwitch(int,char *);
void refreshDisplay(void);
void refreshTime(void);
void stopRiego(uint16_t);
void stopAllRiego(void);
void checkBuzzer(void);
void bip(int);
void longbip(int);
void blinkPause(void);
void procesaBotones(void);
void procesaEstados(void);
void initRiego(uint16_t);

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
#define MAXMINUTES          60
#define MINSECONDS          5
#define HOLDTIME            3000
#define MAXCONNECTRETRY     10
#define DOMOTICZPORT        3380

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
enum {
  STANDBY       = 0x01,
  REGANDO       = 0x02,
  CONFIGURANDO  = 0x04,
  TERMINANDO    = 0x08,
  PAUSE         = 0x10,
  STOP          = 0x20,
  MULTIREGANDO  = 0x40,
  ERROR         = 0x80,
};

enum {
  ENABLED      = 0x01,
  DISABLED     = 0x02,
  ONLYSTATUS   = 0x04,
  ACTION       = 0x08,
  DUAL         = 0x10,
  HOLD         = 0x20,
};

enum {
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
  bSPARE14    = 0x1000,
  bSPARE15    = 0x2000,
  bSPARE16    = 0x4000,
  bPAUSE      = 0x8000,
};

//Pseudobotones
#define bCOMPLETO 0xFF01
#define bCONFIG   0xFF02

//TypeDefs
typedef union
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
} S_bFLAGS;

typedef struct S_BOTON {
  uint16_t   id;
  int   estado;
  int   ultimo_estado;
  int   led;
  S_bFLAGS  flags;
  char  desc[30];
  uint16_t   idx;
} S_BOTON;

typedef union {
  uint8_t estado;
  struct {
    uint8_t standby       : 1,
            regando       : 1,
            configurando  : 1,
            terminando    : 1,
            pausa         : 1,
            spare2        : 1,
            spare1        : 1,
            spare0        : 1;
  };
} U_Estado;

typedef struct {
  uint16_t id;
  uint16_t *serie;
  int size;
  int actual;
  char desc[20];
} S_MULTI;

#define NUMRIEGOS sizeof(COMPLETO)/sizeof(COMPLETO[0])
#ifdef __MAIN__
  
    S_BOTON Boton [] =  { 
      //ID,          S   uS  LED          FLAGS                             DESC          IDX
       {bTURBINAS,   0,  0,  lTURBINAS,   ENABLED | ACTION,                 "TURBINAS",   25},
       {bPORCHE,     0,  0,  lPORCHE,     ENABLED | ACTION,                 "PORCHE",     27},
       {bCUARTILLO,  0,  0,  lCUARTILLO,  ENABLED | ACTION,                 "CUARTILLO",  58},
       {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE",       0},
       {bGOTEOALTO,  0,  0,  lGOTEOALTO,  ENABLED | ACTION,                 "GOTEOALTO",  59},
       {bGOTEOBAJO,  0,  0,  lGOTEOBAJO,  ENABLED | ACTION,                 "GOTEOBAJO",  24},
       {bSPARE16,    0,  0,  0,           DISABLED | ACTION,                "ENCODER",     0},
       {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",        0},
       {bCESPED,     0,  0,  lCESPED,     ENABLED | ONLYSTATUS | DUAL,      "CESPED",      0},
       {bGOTEOS,     0,  0,  lGOTEOS,     ENABLED | ONLYSTATUS | DUAL,      "GOTEOS",      0},
       {bOLIVOS,     0,  0,  lOLIVOS,     ENABLED | ACTION,                 "OLIVOS",     61},
       {bROCALLA,    0,  0,  lROCALLA,    ENABLED | ACTION,                 "ROCALLA",    30},
       {bSPARE13,    0,  0,  0,           DISABLED,                         "SPARE13",     0},
       {bCONFIG,     0,  0,  0,           DISABLED,                         "CONFIG",      0},
       {bCOMPLETO,   0,  0,  lCOMPLETO,   DISABLED,                         "COMPLETO",    0},
       {bMULTIRIEGO, 0,  0,  0,           ENABLED | ACTION,                 "MULTIRIEGO",  0}
                      };
   uint16_t CESPED[]    = {bTURBINAS, bPORCHE, bCUARTILLO};
   uint16_t GOTEOS[]    = {bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA };
   uint16_t COMPLETO[]  = {bTURBINAS, bPORCHE, bCUARTILLO, bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA};
   time_t   lastRiegos[NUMRIEGOS];
   uint     factorRiegos[NUMRIEGOS];
   uint ledOrder[] = {lTURBINAS, lPORCHE, lCUARTILLO, lGOTEOALTO, lGOTEOBAJO, lOLIVOS, lROCALLA,
                      LEDR, LEDG, lCESPED, lCOMPLETO, lGOTEOS};
#else
  extern S_BOTON Boton [];
  extern uint ledOrder[];
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
  U_Estado Estado;
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
  S_MULTI multi;
  //Para Ethernet
  byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xBF, 0xFE, 0xED
  };
  IPAddress ip(192, 168, 100, 80);
  byte gateway[] = {192, 168, 100, 100};
  byte subnet[] = {255, 255, 255, 0};
  byte server[] = {192,168,100,60};
  int port = DOMOTICZPORT;

  #ifdef NET_MQTTCLIENT
    #define DEVICE_ID "Control"
    #define MQTT_SERVER "192.168.100.60"
    PubSubClient MqttClient;
    char clientID[50];
    char topic[50];
    char msg[50];
  #endif

  #ifdef NET_HTTPCLIENT
    char serverAddress[] = "192.168.100.60";
    #ifdef MEGA256
      HttpClient httpclient = HttpClient(client,serverAddress,port);
    #endif
    #ifdef NODEMCU
      HTTPClient httpclient;
    #endif
  #endif
#endif

//Prototipos
void initCD4021B(void);
void initHC595(void);
S_BOTON *parseInputs();
int bId2bIndex(uint16_t);
uint16_t getMultiStatus(void);
void led(uint8_t,int);
void apagaLeds(void);
void enciendeLeds(void);
void initLeds(void);
void longbip(int);
void bip(int);
void procesaEncoder(void);
void ledRGB(int,int,int);
int  getFactor(uint16_t);

#endif
