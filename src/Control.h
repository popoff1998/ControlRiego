//TIPO DE RED
#ifndef control_h
#define control_h
#define NET_HTTPCLIENT
//#define MEGA2561

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

//Para mis clases
#include "Display.h"
#include "Configure.h"

//Estructura de mi eeprom
struct __eeprom_data {
  uint8_t initialized;
  uint16_t  botonIdx[16];
  uint8_t   minutes;
  uint8_t   seconds;
};

#define ADDRBASE_BOTON      1

#define STANDBYSECS         15
#define DEFAULTMINUTES      10
#define DEFAULTSECONDS      0
#define DEFAULTBLINK        5
#define DEFAULTBLINKMILLIS  500
#define MINMINUTES          0
#define MAXMINUTES          60
#define MINSECONDS          5
#define HOLDTIME            3000
#define FORCEINITEEPROM     0



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
  #define ENCCLK              D2
  #define ENCDT               D3
  #define ENCSW               100
  #define BUZZER              D8

  #define CD4021B_LATCH         D5
  #define CD4021B_CLOCK         D6
  #define CD4021B_DATA          D7

  #define HC595_DATA            44
  #define HC595_LATCH           46
  #define HC595_CLOCK           45

#endif


#define ON  1
#define OFF 0

#define SHOW 1
#define HIDE 0

//Estados
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

//Para los botones
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
  bPAUSE      = 0x0008,
  bGOTEOALTO  = 0x0010,
  bGOTEOBAJO  = 0x0020,
  bENCODER    = 0x0040,
  bSTOP       = 0x0080,
  bCESPED     = 0x0100,
  bGOTEOS     = 0x0200,
  bSPARE11    = 0x0400,
  bSPARE12    = 0x0800,
  bSPARE13    = 0x1000,
  bSPARE14    = 0x2000,
  bSPARE15    = 0x4000,
  bMULTIRIEGO = 0x8000,
};

//Pseudobotones
#define bCOMPLETO 0xFF01
#define bCONFIG   0xFF02

#define NUMBOTONES 3
#ifdef __MAIN__
  S_BOTON Boton [] =  { {bTURBINAS, 0, 0, 1,    ENABLED | ACTION,               "TURBINAS",62},
                        {bPORCHE,   0, 0, 2,    ENABLED | ACTION,               "PORCHE",63},
                        {bCUARTILLO, 0, 0, 3,   ENABLED | ACTION,               "CUARTILLO",64},
                        {bPAUSE, 0, 0, 0,       ENABLED | ACTION | DUAL | HOLD, "PAUSE",0},
                        {bGOTEOALTO, 0, 0, 4,   ENABLED | ACTION,               "GOTEOALTO",60},
                        {bGOTEOBAJO, 0, 0, 5,   ENABLED | ACTION,               "GOTEOBAJO",61},
                        {bENCODER, 0, 0, 0,     DISABLED | ACTION,               "ENCODER",0},
                        {bSTOP, 0, 0, 0,        ENABLED | ACTION | DUAL,        "STOP",0},
                        {bCESPED, 0, 0, 6,      ENABLED | ONLYSTATUS | DUAL,    "CESPED",0},
                        {bGOTEOS, 0, 0, 8,      ENABLED | ONLYSTATUS | DUAL,    "GOTEOS",0},
                        {bSPARE11, 0, 0, 0,     DISABLED,                       "SPARE11",0},
                        {bSPARE12, 0, 0, 0,     DISABLED,                       "SPARE12",0},
                        {bSPARE13, 0, 0, 0,     DISABLED,                       "SPARE13",0},
                        {bCONFIG, 0, 0, 16,     DISABLED,                       "CONFIG",0},
                        {bCOMPLETO, 0, 0, 7,     DISABLED,                      "COMPLETO",0},
                        {bMULTIRIEGO, 0, 0, 0,  ENABLED | ACTION,               "MULTIRIEGO",0}
                      };

   uint16_t CESPED[3]    = {bTURBINAS, bPORCHE, bCUARTILLO};
   uint16_t GOTEOS[2]    = {bGOTEOALTO, bGOTEOBAJO};
   uint16_t COMPLETO[5]  = {bTURBINAS, bPORCHE, bCUARTILLO, bGOTEOALTO, bGOTEOBAJO};
   time_t lastRiegos[5];

#else
  extern S_BOTON Boton [];
#endif

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

//Globales
#ifdef __MAIN__
  //Variables para el display
  int8_t TimeDisp[] = {0,0,0,0,0};
  int8_t StopDisp[] = {0,25,26,27,28};
  int8_t ErrorDisp[] = {0,17,14,19,19};

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
  int port = 8080;
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
#else
  extern uint8_t minutes;
  extern uint8_t seconds;
#endif

//Prototipos
S_BOTON *leerBotones(void);
void initCD4021B(void);
void initHC595(void);
S_BOTON *parseInputs();
int bId2bIndex(uint16_t);
uint16_t getMultiStatus(void);
void led(uint8_t,int);
void apagaLeds(void);
void longbip(int);
void bip(int);
void procesaEncoder(void);

#endif
