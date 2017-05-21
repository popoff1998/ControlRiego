//Defines
#include <TimerOne.h>
#include <ClickEncoder.h>
#include <Time.h>
#include <avr/pgmspace.h>
#include <TM1637.h>
#include <CountUpDownTimer.h>

#define STANDBYSECS         15
#define DEFAULTMINUTES      1
#define DEFAULTBLINK        5
#define DEFAULTBLINKMILLIS  500
#define MINMINUTES          1
#define MAXMINUTES          60

//Para CD4021B
#define CD4021B_LATCH         7
#define CD4021B_CLOCK         8
#define CD4021B_DATA          9

//Para el display
#define DISPCLK             3
#define DISPDIO             4
#define ENCCLK              5
#define ENCDT               6
#define ENCSW               2
#define BUZZER              13

//Estados
enum {
  STANDBY       = 0x01,
  REGANDO       = 0x02,
  CONFIGURANDO  = 0x04,
  TERMINANDO    = 0x08,
  PAUSE         = 0x10,
  STOP          = 0x20,
  MULTIRIEGO    = 0x40,
};

//Para los botones
typedef union
{
  uint8_t all_flags;
  struct
  {
    uint8_t enabled     : 1,
            disabled    : 1,
            onlystatus  : 1,
            action      : 1,
            dual        : 1,
            spare2      : 1,
            spare1      : 1,
            spare0      : 1;
  };
} S_bFLAGS;

typedef struct {
  uint16_t   id;
  int   estado;
  int   ultimo_estado;
  int   led;
  S_bFLAGS  flags;
  char  desc[30];
} S_BOTON;

enum {
  ENABLED      = 0x01,
  DISABLED     = 0x02,
  ONLYSTATUS   = 0x04,
  ACTION       = 0x08,
  DUAL         = 0x10,
};

enum {
  bTURBINAS   = 0x0001,
  bPORCHE     = 0x0002,
  bCUARTILLO  = 0x0004,
  bPAUSE      = 0x0008,
  bGOTEOALTO  = 0x0010,
  bGOTEOBAJO  = 0x0020,
  bSPARE7     = 0x0040,
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

#define NUMBOTONES 3
#ifdef __MAIN__
  S_BOTON Boton [] =  { {bTURBINAS, 0, 0, 0,    ENABLED | ACTION, "TURBINAS"},
                        {bPORCHE,   0, 0, 0,    ENABLED | ACTION, "PORCHE"},
                        {bCUARTILLO, 0, 0, 0,   ENABLED | ACTION, "CUARTILLO"},
                        {bPAUSE, 0, 0, 0,   ENABLED | ACTION, "PAUSE"},
                        {bGOTEOALTO, 0, 0, 0,      DISABLED, "GOTEOALTO"},
                        {bGOTEOBAJO, 0, 0, 0,      DISABLED, "GOTEOBAJO"},
                        {bSPARE7, 0, 0, 0,      DISABLED, "SPARE7"},
                        {bSTOP, 0, 0, 0,        ENABLED | ACTION | DUAL, "STOP"},
                        {bCESPED, 0, 0, 0,      ENABLED | ONLYSTATUS | DUAL, "CESPED"},
                        {bGOTEOS, 0, 0, 0,      ENABLED | ONLYSTATUS | DUAL, "GOTEOS"},
                        {bSPARE11, 0, 0, 0,     DISABLED, "SPARE11"},
                        {bSPARE12, 0, 0, 0,     DISABLED, "SPARE12"},
                        {bSPARE13, 0, 0, 0,     DISABLED, "SPARE13"},
                        {bSPARE14, 0, 0, 0,     DISABLED, "SPARE14"},
                        {bSPARE15, 0, 0, 0,     DISABLED, "SPARE15"},
                        {bMULTIRIEGO, 0, 0, 0,  ENABLED | ACTION, "MULTIRIEGO"}
                      };

   uint16_t CESPED[3]    = {bTURBINAS, bPORCHE, bCUARTILLO};
   uint16_t GOTEOS[2]    = {bGOTEOALTO, bGOTEOBAJO};
   uint16_t COMPLETO[5]  = {bTURBINAS, bPORCHE, bCUARTILLO, bGOTEOALTO, bGOTEOBAJO};

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

typedef union {
  uint16_t all;
  struct {
    uint16_t  bit1     :1,
              bit2     :1,
              bit3     :1,
              bit4     :1,
              bit5     :1,
              bit6     :1,
              bit7     :1,
              bit8     :1,
              bit9     :1,
              bit10    :1,
              bit11    :1,
              bit12    :1,
              bit13    :1,
              bit14    :1,
              bit15    :1,
              bit16    :1;
  };
} U_Mux;

//Globales
#ifdef __MAIN__
  U_Mux mInput;
  U_Mux mOutput;

  //Variables para el display
  int8_t TimeDisp[] = {0x00,0x00,0x00,0x00};
  //int8_t StopDisp[] = {0x6d,0x78,0x5c,0x73};
  int8_t StopDisp[] = {25,26,27,28};
  //Globales
  CountUpDownTimer T(DOWN);
  U_Estado Estado;
  TM1637 tm1637(DISPCLK,DISPDIO);
  ClickEncoder *Encoder;
  int minutes = DEFAULTMINUTES;
  int value = minutes;
  bool tiempoTerminado;
  bool reposo = false;
  unsigned long standbyTime;
  bool displayOff = false;
  unsigned long lastBlinkPause;

  //Prototipos
  S_BOTON *leerBotones(void);
  void initCD4021B(void);
  S_BOTON *parseInputs();
  int bId2bIndex(uint16_t);

#else
  extern U_Mux mInput;
  extern U_Mux mOutput;
#endif
