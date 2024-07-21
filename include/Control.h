#ifndef control_h
  #define control_h

  #ifdef M3GRP      // opcion con boton multirriego + selector 3 grupos multirriego
    #define MULTIRRIEGO bMULTIRRIEGO 
  #else
    #define GRP4     // por defecto GRP4: 4 botones de grupos multirriego
    #define MULTIRRIEGO bGRUPO1 ... bGRUPO4 
  #endif

  /*
  * Uncommenting DEBUGLOG_DISABLE_LOG disables ASSERT and all log (Release Mode)
  * PRINT and PRINTLN are always valid even in Release Mode
  * #define DEBUGLOG_DISABLE_LOG
  * para cambiarlo posteriormente: LOG_SET_LEVEL(DebugLogLevel::LVL_TRACE);
  *  0: NONE, 1: ERROR, 2: WARN, 3: INFO, 4: DEBUG, 5: TRACE
  */
  #ifdef DEVELOP
    //Comportamiento general para PRUEBAS . DESCOMENTAR LO QUE CORRESPONDA
    #define DEBUGLOG_DEFAULT_LOG_LEVEL_TRACE
    //#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
    #define EXTRADEBUG
    //#define EXTRADEBUG2
    //#define EXTRATRACE
    #define VERBOSE
  #endif

  #ifdef RELEASE
    //Comportamiento general para uso normal . DESCOMENTAR LO QUE CORRESPONDA
    //#define DEBUGLOG_DISABLE_LOG
    #define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
    #define VERBOSE
  #endif

  #ifdef DEMO
    //Comportamiento general para DEMO . DESCOMENTAR LO QUE CORRESPONDA
    #define DEBUGLOG_DISABLE_LOG
  #endif
  #include <DebugLog.h>

  #include <DNSServer.h>
  #include <WifiUdp.h>
  #include <WiFiManager.h> 
  #include <SPI.h>
  #include <NTPClient.h>
  #include <Time.h>
  #include <Timezone.h>
  #include <AiEsp32RotaryEncoder.h>
  #include <CountUpDownTimer.h>
  #define ARDUINOJSON_ENABLE_COMMENTS 1
  #include <ArduinoJson.h>
  #include <Ticker.h>
  #include <LittleFS.h>
  #include <Wire.h>
  
  #ifdef ESP32
      #include <HTTPClient.h>
      #include <WiFi.h>
      #include <WebServer.h>
    #ifdef WEBSERVER
      #include <ESPmDNS.h>
      #include <HTTPUpdateServer.h>
    #endif
    #ifdef TEMPLOCAL
      #include <Adafruit_Sensor.h>
      #include <DHT.h>
    #endif
  #endif

  #include "MCP23017.h"  // expansor E/S MCP23017
  #include "pitches.h"   // notas musicales
  //Para mis clases
  #include "Configure.h"
  #include "DisplayLCD.h"


  #ifdef DEVELOP
    #define HOSTNAME "ardomot"
  #else
    #define HOSTNAME "ardomo"
  #endif  
  #define WSPORT 8080

  //#define CONFIG_LITTLEFS_SPIFFS_COMPAT 1  // modo compatibilidad con SPIFFS

  /* You only need to format LittleFS the first time you run a
   test or else use the LITTLEFS plugin to create a partition
   https://github.com/lorol/arduino-esp32littlefs-plugin */
   
  #define FORMAT_LITTLEFS_IF_FAILED true
  #ifndef clean_FS
    #define clean_FS false
  #endif
       

  //-------------------------------------------------------------------------------------
                            #define VERSION  "3.1-RC1"
  //-------------------------------------------------------------------------------------

  #define xNAME false //actualiza desc de botones con el Name del dispositivo que devuelve Domoticz

  //Comportamiento General
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
  #define STANDBYSECS         30      // tiempo en segundos para pasar a reposo desde standby (apagar pantalla y atenuar leds)
  #define NTPUPDATEINTERVAL   600     // tiempo en minutos para resincronizar el reloj del sistema con el servidor NTP
  #define RECONNECTINTERVAL   1       // tiempo en minutos para intentar reconexion a la wifi
  #define DEFAULTBLINK        5       // numero de parpadeos de la pantalla
  #define DEFAULTBLINKMILLIS  500     // mseg entre parpadeo de la pantalla
  #define MSGDISPLAYMILLIS    1000    // mseg se mantienen mensajes informativos
  #define MINMINUTES          0       // minimo de minutos ajustables en el temporizador
  #define MAXMINUTES          59      // corte automatico de seguridad a los 60 min. en los arduinos
  #define MINSECONDS          5       // minimo de segundos ajustables en el temporizador
  #define HOLDTIME            3000    // mseg que hay que mantener PAUSE pulsado para ciertas acciones
  #define MAXCONNECTRETRY     10      // numero maximo de reintentos de reconexion a la wifi tras el fallo en inicio
  #define VERIFY_INTERVAL     15      // intervalo en segundos entre verificaciones periodicas
  #define DEFAULT_SWITCH_RETRIES 5    // numero de reintentos para parar o encender una zona de riego en el Domoticz
  #define DELAYRETRY          2000    // mseg de retardo entre reintentos
  #define MAXLEDLEVEL         255     // nivel maximo leds RGB (0 a 255)
  #define DIMMLEVEL           50      // nivel atenuacion leds RGB (0 a 255)
  #define I2C_CLOCK_SPEED     400000  // frecuencia del bus I2C en Hz (default 100000)
  #define LCD2004_address     0x27    // direccion bus I2C de la pantalla LCD
  #define ROTARY_ENCODER_STEPS 4      // TODO documentar
  #define MAX_ESP32_TEMP      80      // max temp. ESP32 para mostrar aviso (con wifi funciona mal)
  #define TEMP_OFFSET         0       // correccion temperatura sensor local

 //----------------  dependientes del HW   ----------------------------------------
  #ifdef ESP32
    // GPIOs  I/O usables: 2 4 5 16 17 18 19 21 22 23 25 26 27 32 33  (15/15)
    // GPIOs  I/O los reservo para JTAG: 12 13 14 15
    // GPIOs  I usables: 34 35 36 39 (4/4)  (ojo no tienen pullup/pulldown interno, requieren resistencia externa)
    #define ENCCLK                GPIO_NUM_16
    #define ENCDT                 GPIO_NUM_17
    #define ENCBOTON              GPIO_NUM_34   // conectado a GPIO solo INPUT (no se trata por Encoder, se hace por programa)
    #define LEDR                  GPIO_NUM_27  
    #define LEDG                  GPIO_NUM_26 
    #define LEDB                  GPIO_NUM_25 
    #define I2C_SDA               GPIO_NUM_21
    #define I2C_SCL               GPIO_NUM_22
    #define I2C_SDA1              GPIO_NUM_33
    #define I2C_SCL1              GPIO_NUM_32
    #define BUZZER                GPIO_NUM_4
    #define DHTPIN                GPIO_NUM_23    // ojo debe ser de E/S!
    #define lZONA1                1             // mcpO GPA0
    #define lZONA2                2             // mcpO GPA1
    #define lZONA3                3             // mcpO GPA2
    #define lZONA4                4             // mcpO GPA3
    #define lZONA5                5             // mcpO GPA4
    #define lZONA6                6             // mcpO GPA5
    #define lZONA7                7             // mcpO GPA6
    #define lZONA8                8             // mcpO GPA7
    #define lZONA9                9             // mcpO GPB0 
    #define lGRUPO1               13            // mcpO GPB4
    #define lGRUPO2               14            // mcpO GPB5
    #define lGRUPO3               15            // mcpO GPB6
    #define lGRUPO4               16            // mcpO GPB7
    #define mcpOUT                0x20  //direccion del MCP23017 para salidas (leds)
    #define mcpIN                 0x21  //direccion del MCP23017 para entradas (botones)

  #endif
 //----------------  fin dependientes del HW   ----------------------------------------


  //Para legibilidad del codigo
  #define ON  1
  #define OFF 0
  #define SHOW 1
  #define HIDE 0
  #define READ 1
  #define CLEAR 0
  #define REFRESH 1
  #define UPDATE 0
  #define NOBLINK 0
  #define BORRA1H 1
  #define BORRA2H 2

  //Enums

  enum _bips {
    LONGBIP = 1,
    LOWBIP,
    BIP,
    BIPOK,
    BIPKO,
  };

  enum _estados {
    STANDBY       ,
    REGANDO       ,
    CONFIGURANDO  ,
    TERMINANDO    ,
    PAUSE         ,
    STOP          ,
    ERROR         ,
  };

  // literales para los estados en el display
  #define _ESTADOS "STANDBY" , "REGANDO:" , "CONFIGURANDO" , "TERMINANDO" , "PAUSA:" , "STOP" , "ERROR"
  const char nEstado[][15] = {_ESTADOS};

  enum error_tipos {
    NOERROR       = 0xFF,
    E0            = 0,
    E1            = 1,
    E2            = 2,
    E3            = 3,
    E4            = 4,
    E5            = 5,
  };

  enum estado_tipos {
    LOCAL       = 0,
    REMOTO      = 1,
  };

  enum _flags {
    ENABLED      = 0x01,
    disabled     = 0x02,  // DISABLED en mayusculas daba error al compilar por ya definido en una libreria
    ONLYSTATUS   = 0x04,
    ACTION       = 0x08,
    DUAL         = 0x10,
    HOLD         = 0x20,
  };

  //----------------  dependientes del HW   ----------------------------------------
  // ojo esta es la posición del bit de cada boton en el stream serie - no modificar -
  #ifdef GRP4
    enum _botones {
      bZONA1      = 0x0001,  // mcpI A0
      bZONA2      = 0x0002,  // mcpI A1
      bZONA3      = 0x0004,  // mcpI A2
      bZONA4      = 0x0008,  // mcpI A3
      bZONA5      = 0x0010,  // mcpI A4
      bZONA6      = 0x0020,  // mcpI A5
      bZONA7      = 0x0040,  // mcpI A6
      //          = 0x0080,  // mcpI A7  (NO USAR  para inputs)
      bZONA8      = 0x0100,  // mcpI B0
      bZONA9      = 0x0200,  // mcpI B1  
      bGRUPO1     = 0x0400,  // mcpI B2  (grupos deben ser consecutivos)
      bGRUPO2     = 0x0800,  // mcpI B3  (grupos deben ser consecutivos)
      bGRUPO3     = 0x1000,  // mcpI B4  (grupos deben ser consecutivos)
      bGRUPO4     = 0x2000,  // mcpI B5  (grupos deben ser consecutivos)
      bPAUSE      = 0x4000,  // mcpO B2  (OJO conectados a mcpO se integran como bits 15 y 16 de readInputs)
      bSTOP       = 0x8000,  // mcpO B3  (OJO conectados a mcpO se integran como bits 15 y 16 de readInputs)
      //          = 0x8000,  // mcpI B7  (NO USAR para inputs)
    };
      // lista de todos los botones de zonas de riego disponibles (el orden define la zona):
    #define _ZONAS  bZONA1 , bZONA2 , bZONA3 , bZONA4 , bZONA5 , bZONA6 , bZONA7 , bZONA8 , bZONA9
      // lista de todos los botones de grupos disponibles (el orden define el grupo):
    #define _GRUPOS bGRUPO1 , bGRUPO2 , bGRUPO3 , bGRUPO4
  //----------------  fin dependientes del HW   ----------------------------------------
    #define ZONASXGRUPO          9  // maximo de zonas en un grupo multirriego (9 para coja en pantalla, max. 16)

  #endif

  #ifdef M3GRP
    enum _botones {
      bZONA1      = 0x0001,  // mcpI A0
      bZONA2      = 0x0002,  // mcpI A1
      bZONA3      = 0x0004,  // mcpI A2
      bZONA4      = 0x0008,  // mcpI A3
      bZONA5      = 0x0010,  // mcpI A4
      bZONA6      = 0x0020,  // mcpI A5
      bZONA7      = 0x0040,  // mcpI A6
      //          = 0x0080,  // mcpI A7  (NO USAR  para inputs)
      bZONA8      = 0x0100,  // mcpI B0
      bZONA9      = 0x0200,  // mcpI B1  
      bGRUPO1     = 0x0400,  // mcpI B2  (grupos deben ser consecutivos)
      bGRUPO2     = 0x0800,  // mcpI B3  (grupos deben ser consecutivos)
      bGRUPO3     = 0x1000,  // mcpI B4  (grupos deben ser consecutivos)
      bMULTIRRIEGO= 0x2000,  // mcpI B5
      bPAUSE      = 0x4000,  // mcpO B2  (OJO conectados a mcpO se integran como bits 15 y 16 de readInputs)
      bSTOP       = 0x8000,  // mcpO B3  (OJO conectados a mcpO se integran como bits 15 y 16 de readInputs)
      //          = 0x8000,  // mcpI B7  (NO USAR para inputs)
    };

      // lista de todos los botones de zonas de riego disponibles (el orden define la zona):
    #define _ZONAS  bZONA1 , bZONA2 , bZONA3 , bZONA4 , bZONA5 , bZONA6 , bZONA7 , bZONA8 , bZONA9
      // lista de todos los botones (selector) de grupos disponibles (el orden define el grupo):
    #define _GRUPOS bGRUPO1 , bGRUPO2 , bGRUPO3 
  //----------------  fin dependientes del HW   ----------------------------------------
    #define ZONASXGRUPO          9  // maximo de zonas en un grupo multirriego (9 para coja en pantalla, max. 16)
  #endif

  const uint16_t ZONAS[] = {_ZONAS};
  const uint16_t GRUPOS[]  = {_GRUPOS};
  const int NUMZONAS = sizeof(ZONAS)/sizeof(ZONAS[0]); // numero de zonas (botones riego individual)
  const int NUMGRUPOS = sizeof(GRUPOS)/sizeof(GRUPOS[0]); // numero de grupos multirriego

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

  union S_simFlags
  {
    uint8_t all_simFlags;
    struct
    {
    uint8_t ErrorOFF       : 1,
            ErrorON        : 1,
            ErrorVerifyON  : 1,
            ErrorVerifyOFF : 1,
            ErrorPause     : 1;
    };
  };

  struct S_BOTON {
    uint16_t   bID;
    bool   estado;
    bool   ultimo_estado;
    int   led;
    S_bFLAGS  flags;
    char  desc[20];
    uint16_t   znumber;
  } ;

  struct S_Estado {
    uint8_t estado; 
    uint8_t tipo;
    uint8_t error;
  } ;

  struct S_timeRiego {
    time_t inicio; 
    time_t final; 
  } ;

  struct S_tm {
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    int  value = 0;
    int  savedValue = 0;
  } ;


  //estructura para salvar un grupo
  struct Grupo_parm {
    uint16_t bID;          // boton del grupo
    int size;              // cantidad de zonas asociadas al grupo 
    uint16_t zNumber[16];  // ojo! numero de las zonas, no es el boton asociado a ellas
    char desc[20];
  } ;

  //estructura para salvar parametros de un boton
  struct Zona_parm {
    char  desc[20];
    uint16_t   idx;        // IDX de la zona en Domoticz
  } ;

  //estructura para parametros configurables
  struct Config_parm {
    uint8_t   initialized=0;
    static const int  n_Zonas = NUMZONAS; //no modificable por fichero de parámetros (depende HW) 
    Zona_parm zona[n_Zonas];
    static const int  n_Grupos = NUMGRUPOS;  //no modificable por fichero de parámetros (depende HW)
    Grupo_parm group[n_Grupos+1];       // +1 para sitio para grupo temporal n+1
    char domoticz_ip[40];
    char domoticz_port[6];
    char ntpServer[40];
    uint8_t   minutes = DEFAULTMINUTES; 
    uint8_t   seconds = DEFAULTSECONDS;
    int   warnESP32temp = MAX_ESP32_TEMP;       // temperatura ESP32 maxima con aviso 
    int   maxledlevel = MAXLEDLEVEL;            // nivel brillo maximo led RGB 
    int   dimmlevel = DIMMLEVEL;                // nivel atenuacion led RGB 
    int   tempOffset = TEMP_OFFSET;             // correccion temperatura sensor local DHTxx 
    int   msgdisplaymillis = MSGDISPLAYMILLIS;  // tiempo que se muestran mensajes (mseg.) 
    bool mute = OFF;                            // sonidos activos
    bool showwifilevel = OFF;                   // muestra en standby nivel de la señal wifi
    bool xname = xNAME;                         // actualiza desc de botones con el Name del dispositivo que devuelve Domoticz
  };

  // estructura del multirriego activo 
  // (algunos son pointer al multirriego correspondiente en config *)
  struct S_MULTI {
    bool riegoON  = false;  //indicador de multirriego activo
    bool temporal = false;  //indicador de grupo multirriego es temporal
    bool semaforo = false;  //indicador de procesar siguiente zona del multirriego
    int ngrupo;             // numero del grupo al que apunta
    uint16_t *id;           //apuntador al id del boton/selector grupo en estructura config (bGrupo_x)
    uint16_t serie[16];     //contiene los id de los botones del grupo (bZona_x)
    uint16_t zserie[16];    //contiene las zonas del grupo (Zona_x)
    //uint16_t (*znumber)[16];    //apuntador a las zonas del grupo en estructura config (Zona_x)
    int *size;              //apuntador a config con el tamaño del grupo
    int w_size;             //variable auxiliar durante ConF
    int actual;             //variable auxiliar durante un multirriego 
    char *desc;             //apuntador a config con la descripcion del grupo
  } ;

  const char MESES[][12] = {"Ene.", "Feb.", "Mar.", "Abr.", "May.", "Jun.", "Jul.", "Ago.", "Sep.", "Oct.", "Nov.", "Dic."};

   //Globales a todos los módulos
  #ifdef __MAIN__
    #ifdef GRP4     // matriz Boton para caso de 9 zonas y 4 botones de grupos multirriego
      S_BOTON Boton [] =  { 
        //bID         S   uS  LED          FLAGS                             DESC        zNUMBER  
        {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1",       0},
        {bZONA2 ,     0,  0,  lZONA2 ,     ENABLED | ACTION,                 "ZONA2",       0},
        {bZONA3    ,  0,  0,  lZONA3    ,  ENABLED | ACTION,                 "ZONA3",       0},
        {bZONA4    ,  0,  0,  lZONA4    ,  ENABLED | ACTION,                 "ZONA4",       0},
        {bZONA5    ,  0,  0,  lZONA5    ,  ENABLED | ACTION,                 "ZONA5",       0},
        {bZONA6 ,     0,  0,  lZONA6 ,     ENABLED | ACTION,                 "ZONA6",       0},
        {bZONA7  ,    0,  0,  lZONA7  ,    ENABLED | ACTION,                 "ZONA7",       0},
        {bZONA8  ,    0,  0,  lZONA8  ,    ENABLED | ACTION,                 "ZONA8",       0},
        {bZONA9,      0,  0,  lZONA9  ,    ENABLED | ACTION,                 "ZONA9",       0},
        {bGRUPO1,     0,  0,  lGRUPO1,     ENABLED | ACTION,                 "GRUPO1",      0},
        {bGRUPO2  ,   0,  0,  lGRUPO2  ,   ENABLED | ACTION,                 "GRUPO2",      0},
        {bGRUPO3,     0,  0,  lGRUPO3,     ENABLED | ACTION,                 "GRUPO3",      0},
        {bGRUPO4,     0,  0,  lGRUPO4,     ENABLED | ACTION,                 "GRUPO4",      0},
        {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE",       0},
        {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",        0}
      };
    #endif
    
    #ifdef M3GRP     // matriz Boton para caso de 9 zonas, boton multirriego y selector de 3 grupos multirriego
      S_BOTON Boton [] =  { 
        //bID         S   uS  LED          FLAGS                             DESC         zNUMBER
        {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1",       0},
        {bZONA2 ,     0,  0,  lZONA2 ,     ENABLED | ACTION,                 "ZONA2",       0},
        {bZONA3    ,  0,  0,  lZONA3    ,  ENABLED | ACTION,                 "ZONA3",       0},
        {bZONA4    ,  0,  0,  lZONA4    ,  ENABLED | ACTION,                 "ZONA4",       0},
        {bZONA5    ,  0,  0,  lZONA5    ,  ENABLED | ACTION,                 "ZONA5",       0},
        {bZONA6 ,     0,  0,  lZONA6 ,     ENABLED | ACTION,                 "ZONA6",       0},
        {bZONA7  ,    0,  0,  lZONA7  ,    ENABLED | ACTION,                 "ZONA7",       0},
        {bZONA8  ,    0,  0,  lZONA8  ,    ENABLED | ACTION,                 "ZONA8",       0},
        {bZONA9,      0,  0,  lZONA9  ,    ENABLED | ACTION,                 "ZONA9",       0},
        {bGRUPO1,     0,  0,  lGRUPO1,     ENABLED | ONLYSTATUS | DUAL,      "GRUPO1",      0},
        {bGRUPO2  ,   0,  0,  lGRUPO2  ,   ENABLED | ONLYSTATUS | DUAL,      "GRUPO2",      0},
        {bGRUPO3,     0,  0,  lGRUPO3,     ENABLED | ONLYSTATUS | DUAL,      "GRUPO3",      0},
        {bMULTIRRIEGO,0,  0,  0,           ENABLED | ACTION,                 "MULTIRRIEGO", 0},
        {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE",       0},
        {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",        0}
      };
    #endif

    
    int NUM_S_BOTON = sizeof(Boton)/sizeof(Boton[0]);

    S_MULTI multi;  //estructura con variables del grupo de multirriego activo
    S_BOTON  *boton;
    S_tm tm;          // variables contador de tiempo
    bool connected;
    bool NONETWORK;
    bool NOWIFI;
    bool falloAP;
    bool webServerAct = false;
    bool saveConfig = false;
    
    const char *parmFile = "/config_parm.json";       // fichero de parametros activos
    const char *defaultFile = "/config_default.json"; // fichero de parametros por defecto

    DisplayLCD lcd(LCD2004_address, 20, 4);  // 20 caracteres x 4 lineas
    char buff[MAXBUFF];

  #else
    extern int NUM_S_BOTON;
    extern S_BOTON Boton [];
    extern S_MULTI multi;
    extern S_BOTON  *boton;
    extern S_tm tm;
    extern bool connected;
    extern bool NONETWORK;
    extern bool NOWIFI;
    extern bool falloAP;
    extern bool webServerAct;
    extern bool saveConfig;
    extern const char *parmFile; 
    extern const char *defaultFile;
    extern DisplayLCD lcd;
    extern char buff[];

  #endif

  #ifdef __MAIN__
    //Globales a este módulo
    Config_parm config; //estructura parametros configurables y runtime
    S_initFlags initFlags ;
    WiFiClient client;
    HTTPClient httpclient;
    WiFiUDP    ntpUDP;
    NTPClient timeClient(ntpUDP,config.ntpServer);
    TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
    TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
    Timezone CE(CEST, CET);
    TimeChangeRule *tcr;
    time_t utc;
    CountUpDownTimer T(DOWN);
    S_BOTON  *ultimoBotonZona;
    S_Estado Estado;
    S_simFlags simular; // estructura flags para simular errores
    Configure    *configure;
    AiEsp32RotaryEncoder rotaryEncoder(ENCDT,ENCCLK,-1, -1, ROTARY_ENCODER_STEPS);
    Ticker tic_parpadeoLedError;    //para parpadeo led ERROR (LEDR)
    Ticker tic_parpadeoLedZona;  //para parpadeo led zona de riego
    Ticker tic_verificaciones;   //para verificaciones periodicas
    S_timeRiego lastRiegos[NUMZONAS];
    S_timeRiego lastGrupos[NUMGRUPOS];
    uint factorRiegos[NUMZONAS];
    uint8_t prevseconds;
    uint8_t prevminutes;
    char  descDomoticz[20];
    int  ledID = 0;
    unsigned long standbyTime;
    bool displayOff = false;
    bool reposo = false;
    unsigned long lastBlinkPause;
    bool holdPause = false;
    unsigned long countHoldPause;
    bool flagV = OFF;
    bool timeOK = false;
    bool tempOK = false;
    bool factorRiegosOK = false;
    bool errorOFF = false;
    bool VERIFY;    // si true verifica periodicamente estado del riego en curso en Domoticz
    bool encoderSW = false;
    char errorText[7];
    unsigned long currentMillisLoop = 0;
    unsigned long lastMillisLoop = 0;
    unsigned long lastMillisVerify = 0;
    unsigned long NTPlastUpdate = 0;
    int numloops = 0;

    // definiciones bips y tonos:

    // bipOK notes in the melody:
    int bipOK_melody[] = { NOTE_C6, NOTE_D6, NOTE_E6, NOTE_F6, NOTE_G6, NOTE_A6, NOTE_B6 };
    const int bipOK_num = sizeof(bipOK_melody)/sizeof(bipOK_melody[0]); // numero de notas en la melodia
    int bipOK_duration = 50;  // duracion de cada tono en mseg.

    // bipKO notes in the melody:
    int bipKO_melody[] = { NOTE_B5, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_A3 };
    const int bipKO_num = sizeof(bipKO_melody)/sizeof(bipKO_melody[0]); // numero de notas en la melodia
    int bipKO_duration = 120;  // duracion de cada tono en mseg.
 
    #ifdef TEMPLOCAL 
      DHT dht(DHTPIN, TEMPLOCAL);
    #endif
 
  #endif  // __MAIN__

  //Funciones (prototipos)
  void actLedError(void);
  void apagaLeds(void);
  void bip(int);
  void bipOK(void);
  void bipKO(void);
  int  bID2bIndex(uint16_t);
  void blinkPause(void);
  void check(void);
  void checkTemp(void);
  int  checkWifi(bool level=false);
  void cleanFS(void);
  bool copyConfigFile(const char*, const char*);
  void debugloops(void);
  void dimmerLeds(bool);
  void displayGrupo(uint16_t *, int);
  void displayLCDGrupo(uint16_t *, int, int line=4, int start=0);
  void displayTimer(uint8_t, uint8_t, uint8_t, uint8_t);
  bool domoticzSwitch(int,char *, int);
  void enciendeLeds(void);
  void endWS(void);
  static const char* errorToString(uint8_t);
  void filesInfo(void);
  void finalTimeLastRiego(S_timeRiego&, int);
  void flagVerificaciones(void);
  int  getFactor(uint16_t);
  uint16_t getMultiStatus(void);
  String *httpGetDomoticz(String *);
  void setClock(void);
  void inicioTimeLastRiego(S_timeRiego&, int);
  void initEncoder(void);
  void initFactorRiegos(void);
  void initGPIOs(void);
  void initLastGrupos(void);
  void initLastRiegos(void);
  void initLCD(void);
  void initLeds(void);
  void initMCP23017 (void);
  bool initRiego(void);
  void initWire(void);
  void led(uint8_t,int);
  void ledYellow(int);
  void ledPWM(uint8_t, int);
  void ledRGB(int,int,int);
  bool ledStatusId(int);
  void leeSerial(void);
  void listDir(fs::FS &fs, const char * , uint8_t);
  bool loadConfigFile(const char*, Config_parm&);
  void loadDefaultSignal(uint);
  void longbip(int);
  void lowbip(int);
  void mcpIinit(void);
  void mcpOinit(void);
  void memoryInfo(void);
  void parpadeoLedError(void);
  void parpadeoLedWifi(void);
  void parpadeoLedZona(int);
  void parpadeoLedAP(void);
  S_BOTON *parseInputs(bool);
  void printCharArray(char*, size_t);
  void printFile(const char*);
  void printMulti(void);
  void printMultiGroup(Config_parm&, int);
  void printParms(Config_parm&);
  void procesaBotones(void);
  void procesaBotonMultiriego(void);
  void procesaBotonPause(void);
  void procesaBotonStop(void);
  void procesaBotonZona(void);
  void procesaEncoder(void);
  void procesaEstados(void);
  void procesaEstadoConfigurando(void);
  void procesaEstadoError(void);
  void procesaEstadoRegando(void);
  void procesaEstadoStandby(void);
  void procesaEstadoTerminando(void);
  void procesaEstadoStop(void);
  void procesaEstadoPause(void);
  void procesaWebServer(void);
  bool queryStatus(uint16_t, char *);
  void refreshTime(void);
  void reposoOFF(void);
  void resetFlags(void);
  void resetLCD(void);
  void resetLeds(void);
  bool saveConfigFile(const char*, Config_parm&);
  bool serialDetect(void);
  void setbIDgrupos(Config_parm&);
  void setEncoderMenu(int menuitems, int currentitem = 0);
  void setEncoderTime(void);
  void setEncoderRange(int , int , int , int);
  void setEstado(uint8_t estado, int bnum = 0);
  void setledRGB(void);
  int  ledlevel(void);
  int  setMultibyId(uint16_t , Config_parm&);
  bool setMultirriego(Config_parm&);
  bool setupConfig(const char*);
  void setupEstado(void);
  void setupInit(void);
  void setupParm(void);
  void setupRedWM(Config_parm&, S_initFlags&);
  void setupWS(Config_parm&);
  void setzNumber(void);
  void showTimeLastRiego(S_timeRiego&, int);
  void starConfigPortal(Config_parm&);
  void StaticTimeUpdate(bool);
  void statusError(uint8_t);
  bool stopRiego(uint16_t, bool update=true);
  bool stopAllRiego(void);
  bool testButton(uint16_t, bool);
  void timeByFactor(int,uint8_t *,uint8_t *);
  String TS2Date(time_t);
  String TS2Hour(time_t);
  void ultimosRiegos(int);
  void Verificaciones(void);
  void wifiClearSignal(uint);
  bool wifiReconnect(void);
  void zeroConfig(Config_parm&);
  int  zNumber2bIndex(uint16_t);

#endif  // control_h
