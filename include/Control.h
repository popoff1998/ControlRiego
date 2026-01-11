#ifndef control_h
  #define control_h

  #ifdef M3GRP      // opcion con boton multirriego + selector 3 grupos multirriego
    #define MULTIRRIEGO bMULTIRRIEGO 
  #else
    #define GRP4     // por defecto GRP4: 4 botones de grupos multirriego
    #define MULTIRRIEGO bGRUPO1 ... bGRUPO4 
  #endif


  #include <DNSServer.h>
  #include <WiFiManager.h> 
  #include <SPI.h>
  #include <Time.h>
  #include <TimeLib.h>
  // #include <esp_sntp.h>  // para poder cambiar el intervalo por defecto del ESP32 para sincronizar con el NTP
  #include <CountUpDownTimer.h>
  #include <ArduinoJson.h>
  #include <Ticker.h>
  #include <LittleFS.h>
  #include <Wire.h>
  
  #ifdef ESP32
    #include <HTTPClient.h>
    #include <WiFi.h>
    #include <WebServer.h>
    #ifdef TEMPLOCAL
      #include <Adafruit_Sensor.h>
      #include <DHT.h>
    #endif
  #endif
  
  //Librerias de terceros locales en carpeta /lib
  #include "AiEsp32RotaryEncoder.h" // libreria para el encoder rotatorio AiEsp32RotaryEncoder
  #include "MCP23017.h"  // expansor E/S MCP23017
  #include "pitches.h"   // notas musicales
  //Para mis Tipos
  #include "TiposGlobales.h"
  //Para mis clases
  #include "Configure.h"
  #include "DisplayLCD.h"
  #include "Sonidos.h"

  #include "DebuglogSetup.h"

  #ifdef DEVELOP
    #define HOSTNAME "ardomot"
  #else
    #define HOSTNAME "ardomo"
  #endif  
  #define WSPORT 8080

  /* You only need to format LittleFS the first time you run a
  test or else use the LITTLEFS plugin to create a partition
  https://github.com/lorol/arduino-esp32littlefs-plugin */
  
  #define FORMAT_LITTLEFS_IF_FAILED true
  #ifndef clean_FS
    #define clean_FS false
  #endif
  //#define CONFIG_LITTLEFS_SPIFFS_COMPAT 1  // modo compatibilidad con SPIFFS

  #define ELEMENTCOUNT(x)  (sizeof(x) / sizeof(x[0]))
       
  //-------------------------------------------------------------------------------------
  //                #define FW_VERSION  movido a platformio.ini   // version del software
  //-------------------------------------------------------------------------------------

  //Comportamiento General
  #ifdef RELEASE
    #define DEFAULTMINUTES      10    // * tiempo de riego por defecto (minutos)
    #define DEFAULTSECONDS      0     // * tiempo de riego por defecto (segundos)
    #define RECONNECTINTERVAL   2       // tiempo en minutos para intentar reconexion a la wifi
  #endif
  #ifdef DEVELOP
    #define DEFAULTMINUTES      0
    #define DEFAULTSECONDS      10
    #define RECONNECTINTERVAL   1       // tiempo en minutos para intentar reconexion a la wifi
  #endif
  #define NTPSERVER_SPAIN     "es.pool.ntp.org"  // servidor NTP por defecto
  #define TZ_Europe_Madrid    "CET-1CEST,M3.5.0,M10.5.0/3"  // time zone en formato TZ posix
  #define NTP_TIMEOUT         7000    // tiempo de espera para recibir respuesta del servidor NTP en mseg
  #define STANDBYSECS         30      // tiempo en segundos para pasar a reposo desde standby (apagar pantalla y atenuar leds)
  #define DEFAULTBLINK        3       // numero de parpadeos de la pantalla
  #define DEFAULTBLINKMILLIS  500     // mseg entre parpadeo de la pantalla
  #define MSGDISPLAYMILLIS    1000    // * mseg se mantienen mensajes informativos
  #define MAXMINUTES          59      // corte automatico de seguridad a los 60 min. en los arduinos
  #define MINSECONDS          5       // minimo de segundos ajustables en el temporizador
  #define HOLDTIME            3000    // mseg que hay que mantener PAUSE pulsado para ciertas acciones
  #define MAXCONNECTRETRY     10      // numero maximo de reintentos de reconexion a la wifi tras el fallo en inicio
  #define VERIFY_INTERVAL     15      // intervalo en segundos entre verificaciones periodicas
  #define HTTPCLIENTCONNECTTIMEOUT  1000  // timeout (ms) para establecer conexion con el servidor Domoticz
  #define HTTPCLIENTRESPONSETIMEOUT 1000  // timeout (ms) para recibir respuesta del servidor Domoticz
  #define MAX_UPLOAD_KBYTES   30      // tamaño maximo del fichero para hacer upload en KB
  #define DEFAULT_SWITCH_RETRIES 3    // numero de reintentos para parar o encender una zona de riego en el Domoticz
  #define DELAYRETRY          1500    // mseg de retardo entre reintentos
  #define MAXLEDLEVEL         255     // * nivel maximo leds RGB (0 a 255)
  #define DIMMLEVEL           50      // * nivel atenuacion leds RGB (0 a 255)
  #define DEFAULTVOLUME       8       // * volumen por defecto (0 a 10)
  #define DEFAULTFINMELODY    MIMI    // * melodia final riego grupo por defecto
  #define I2C_CLOCK_SPEED     400000  // frecuencia del bus I2C en Hz (default 100000)
  #define LCD2004_address     0x27    // direccion bus I2C de la pantalla LCD
  #define ROTARY_ENCODER_STEPS 4      // TODO documentar
  #define MAX_ESP32_TEMP      80      // * max temp. ESP32 para mostrar aviso (con wifi funciona mal)
  #define TEMP_OFFSET         0       // * correccion temperatura sensor local o remoto
  #define TEMP_OFFSET_FACTOR  50      // * correccion temperatura factor ajuste (50% = x 0.5)
  #define TEMP_DATA_REMOTE    0       // * fuente del dato de temperatura 0=local/1=remota
  #define SHORTCUTSENABLED    true    // admite atajos en estado STOP
  #define ENCSWASPAUSE        true    // encoderSW simula PAUSE en estado CONFIGURANDO
  #define LOGWARNTOFILE       true   // LOG_WARN tambien se graba en el fichero de log de errores
                                      // [*] = configurables

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
  #define FULL 1
  #define RESTO 0
  #define REFRESH 1
  #define UPDATE 0
  #define NOBLINK 0
  #define BORRA1H 1
  #define BORRA2H 2
  #define LCDON 0
  #define RECUPERABLE 1
  #define NORECUPERABLE 0
  #define INICIO 0
  #define RESUME 1


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
  const int NUMZONAS = ELEMENTCOUNT(ZONAS); // numero de zonas (botones riego individual)
  const int NUMGRUPOS = ELEMENTCOUNT(GRUPOS); // numero de grupos multirriego

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
    uint8_t preinitParm   : 1,
            initParm      : 1,
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
    uint16_t   bID;       // ID del boton (bitmask)
    bool  estado;
    bool  ultimo_estado;
    int   led;            // pin del led asociado al boton (0 si no tiene)
    S_bFLAGS  flags;      // flags varios
    char  desc[20];       // descripcion por defecto del boton
    uint16_t   znumber;   // numero de zona (1 a n) o 0 si no es zona
  } ;

  // estructura para el estado general del sistema (State Machine)
  struct S_Estado {
    m_estados estado = STANDBY; 
    estado_tipos tipo   = LOCAL;
    error_tipos error  = NOERROR;
    // Campos de flags/modos de operación
    bool connected = false;
    bool modoDEMO = false;
    bool noWIFI = false;
    bool reposo = false;    
    bool failedStopRiego = false;
    bool recoverableError = false;
  } ;

  struct S_timeRiego {
    time_t inicio; 
    time_t final; 
    time_t reinicio; 
    time_t total; 
  } ;

  struct S_tm {
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    int  value = 0;
  } ;


  //estructura para salvar un grupo
  struct Grupo_parm {
    uint16_t bID;          // boton del grupo
    int size = 0;          // cantidad de zonas asociadas al grupo 
    uint16_t zNumber[ZONASXGRUPO];  // ojo! numero de las zonas, no es el boton asociado a ellas
    char desc[20] = "";    // descripcion del grupo
  } ;

  //estructura para salvar parametros de una zona
  #ifdef DOMOTICZ
  struct Zona_parm {
    char  desc[20] = "";     // nombre de la zona a mostrar en el display
    uint16_t   idx = 0;      // identificador de la zona en el SCD (IDX en Domoticz)
  } ;
  #endif

  //estructura para parametros configurables
  struct Config_parm {
    bool initialized = false;
    static const int  n_Zonas = NUMZONAS;    //no modificable por fichero de parámetros (depende HW) 
    Zona_parm zona[n_Zonas];
    static const int  n_Grupos = NUMGRUPOS;  //no modificable por fichero de parámetros (depende HW)
    Grupo_parm group[n_Grupos+1];            // +1 para sitio para grupo temporal n+1
    char domoticz_ip[40] = "";               // IP o nombre del servidor Domoticz
    char domoticz_port[6] = "";              // puerto del servidor Domoticz
    char ntpServer[40] = NTPSERVER_SPAIN;       // servidor NTP por defecto
    char TZ[50] = TZ_Europe_Madrid;             // time zone por defecto en formato TZ posix
    uint8_t   minutes = DEFAULTMINUTES;         // tiempo de riego por defecto
    uint8_t   seconds = DEFAULTSECONDS;         // tiempo de riego por defecto
    int  warnESP32temp = MAX_ESP32_TEMP;        // temperatura ESP32 maxima con aviso 
    int  maxledlevel = MAXLEDLEVEL;             // nivel brillo maximo led RGB 
    int  dimmlevel = DIMMLEVEL;                 // nivel atenuacion led RGB 
    int  tempOffset = TEMP_OFFSET;              // correccion temperatura sensor local DHTxx 
    int  tempRemote = TEMP_DATA_REMOTE;         // si true obtiene temperatura via Domoticz
    int  tempRemoteIdx = 0;                     // IDX del sensor remoto en Domoticz
    int  msgdisplaymillis = MSGDISPLAYMILLIS;   // tiempo que se muestran mensajes (mseg.) 
    int  volume = DEFAULTVOLUME;                // volumen sonidos por defecto
    int  finMelody = DEFAULTFINMELODY;          // melodia final riego grupo por defecto
    bool mute = OFF;                            // sonidos activos
    bool showwifilevel = OFF;                   // muestra en standby nivel de la señal wifi
    bool xname = false;                         // actualiza desc de botones con el Name del dispositivo que devuelve Domoticz
    bool verify = true;                         // verifica estado dispositivo en el Domoticz
    bool dynamic = false;                       // si true permite añadir/eliminar zonas durante el riego
    bool lastr24 = false;                       // muestra leds ultimos riegos desde las 0h (false) o ultimas 24h (true)
    bool shortcuts = SHORTCUTSENABLED;          // admite atajos de teclas en estado STOP
    bool encSWasPause = ENCSWASPAUSE;           // simulacion PAUSE en modo CONFIGURANDO con encoderSW
    bool logWarnToFile = LOGWARNTOFILE;         // si true los LOG_WARN tambien se graban en el fichero de log de errores
  };

  // estructura del multirriego activo 
  // (algunos son pointer al multirriego correspondiente en config *)
  struct S_MULTI {
    bool riegoON  = false;  // multirriego activo
    bool temporal = false;  // grupo multirriego es temporal
    bool dynamic  = false;  // grupo multirriego es dinámico (a partir de un riego de zona individual, no factorizado)
    bool semaforo = false;  // procesar siguiente zona del multirriego
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

  // estructura para salvar el estado de un riego en curso
  struct S_Riego_estado {
    uint16_t bID = 0;            // id del boton de la zona en curso (bZona_x)
    uint16_t znumber = 0;        // numero de la zona en curso (Zona_x)
    uint8_t  minutes = 0;        // minutos restantes del riego en curso
    uint8_t  seconds = 0;        // segundos restantes del riego en curso
    // CountUpDownTimer timer;         // temporizador del riego en curso ??
    // bool groupvalid = false;        // flag de datos grupo en curso validos
    // S_MULTI multirriego;            // estructura con los datos del multirriego en curso (si lo hay)
  };

  // estructura para los errores (tipo y descripcion)
  struct ErrorEntry {
      error_tipos id;
      const char* descripcion;
  };


   //Globales a _MAIN_ (Control.cpp)
  #ifdef __MAIN__
    #ifdef GRP4     // matriz Boton para caso de 9 zonas y 4 botones de grupos multirriego
      S_BOTON Boton [] =  { 
        //bID         S   uS  LED          FLAGS                             DESC     NUMBER  
        {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1",   0    },
        {bZONA2   ,   0,  0,  lZONA2   ,   ENABLED | ACTION,                 "ZONA2",   0    },
        {bZONA3   ,   0,  0,  lZONA3   ,   ENABLED | ACTION,                 "ZONA3",   0    },
        {bZONA4   ,   0,  0,  lZONA4   ,   ENABLED | ACTION,                 "ZONA4",   0    },
        {bZONA5   ,   0,  0,  lZONA5   ,   ENABLED | ACTION,                 "ZONA5",   0    },
        {bZONA6   ,   0,  0,  lZONA6   ,   ENABLED | ACTION,                 "ZONA6",   0    },
        {bZONA7   ,   0,  0,  lZONA7   ,   ENABLED | ACTION,                 "ZONA7",   0    },
        {bZONA8   ,   0,  0,  lZONA8   ,   ENABLED | ACTION,                 "ZONA8",   0    },
        {bZONA9   ,   0,  0,  lZONA9   ,   ENABLED | ACTION,                 "ZONA9",   0    },
        {bGRUPO1  ,   0,  0,  lGRUPO1  ,   ENABLED | ACTION,                 "GRUPO1",  0    },
        {bGRUPO2  ,   0,  0,  lGRUPO2  ,   ENABLED | ACTION,                 "GRUPO2",  0    },
        {bGRUPO3  ,   0,  0,  lGRUPO3  ,   ENABLED | ACTION,                 "GRUPO3",  0    },
        {bGRUPO4  ,   0,  0,  lGRUPO4  ,   ENABLED | ACTION,                 "GRUPO4",  0    },
        {bPAUSE   ,   0,  0,  0        ,   ENABLED | ACTION | DUAL | HOLD,   "PAUSE",   0    },
        {bSTOP    ,   0,  0,  0        ,   ENABLED | ACTION | DUAL,          "STOP",    0    }
      };
    #endif
    
    #ifdef M3GRP     // matriz Boton para caso de 9 zonas, boton multirriego y selector de 3 grupos multirriego
      S_BOTON Boton [] =  { 
        //bID         S   uS  LED          FLAGS                             DESC     NUMBER
        {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1",        },
        {bZONA2 ,     0,  0,  lZONA2 ,     ENABLED | ACTION,                 "ZONA2",        },
        {bZONA3    ,  0,  0,  lZONA3    ,  ENABLED | ACTION,                 "ZONA3",        },
        {bZONA4    ,  0,  0,  lZONA4    ,  ENABLED | ACTION,                 "ZONA4",        },
        {bZONA5    ,  0,  0,  lZONA5    ,  ENABLED | ACTION,                 "ZONA5",        },
        {bZONA6 ,     0,  0,  lZONA6 ,     ENABLED | ACTION,                 "ZONA6",        },
        {bZONA7  ,    0,  0,  lZONA7  ,    ENABLED | ACTION,                 "ZONA7",        },
        {bZONA8  ,    0,  0,  lZONA8  ,    ENABLED | ACTION,                 "ZONA8",        },
        {bZONA9,      0,  0,  lZONA9  ,    ENABLED | ACTION,                 "ZONA9",        },
        {bGRUPO1,     0,  0,  lGRUPO1,     ENABLED | ONLYSTATUS | DUAL,      "GRUPO1",       },
        {bGRUPO2  ,   0,  0,  lGRUPO2  ,   ENABLED | ONLYSTATUS | DUAL,      "GRUPO2",       },
        {bGRUPO3,     0,  0,  lGRUPO3,     ENABLED | ONLYSTATUS | DUAL,      "GRUPO3",       },
        {bMULTIRRIEGO,0,  0,  0,           ENABLED | ACTION,                 "MULTIRRIEGO",  },
        {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE",        },
        {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",         }
      };
    #endif

    int NUM_S_BOTON = ELEMENTCOUNT(Boton);
    
    const char *parmFile         = "/datos/config_parm.json";   // fichero de parametros activos
    const char *backupParmFile   = "/datos/config_backup.json"; // fichero de respaldo de los parametros
    const char *lastRiegosFile   = "/datos/lastRiegos.json";    // fichero de ultimos riegos de zonas
    const char *lastGruposFile   = "/datos/lastGrupos.json";    // fichero de ultimos riegos de grupos
    const char *logErrorFile     = "/datos/logError.txt";       // fichero de log de errores
    const char *logErrorFilePrev = "/datos/logError_prev.txt";  // fichero de log de errores previo (renombrado al superar tamano maximo)
    S_MULTI multi;     //estructura con variables del grupo de multirriego activo
    S_BOTON  *boton;   // apuntador al boton en curso en la matriz Boton[]
    S_Estado Estado;   // estructura con el estado actual de la maquina de estados
    S_tm tm;           // variables contador de tiempo
    DisplayLCD lcd(LCD2004_address, 20, 4);  // 20 caracteres x 4 lineas
    Config_parm config; //estructura parametros configurables y runtime
    Sonidos sonido;     // se pasa por referencia la estructura config al constructor de la clase
    S_initFlags initFlags ; // flags de inicializacion (borrado de parametros o wifi)
    CountUpDownTimer timer(DOWN); // temporizador cuenta atras
    S_BOTON  *ultimoBotonZona;    // apuntador al ultimo boton de zona pulsado/tratado en la matriz Boton[]
    S_simFlags simular;           // estructura flags para simular errores
    Configure    *configure;
    AiEsp32RotaryEncoder rotaryEncoder(ENCDT,ENCCLK,-1, -1, ROTARY_ENCODER_STEPS);
    Ticker tic_CountDownTimer;       //para llamar a la funcion de cuenta atras del temporizador
    Ticker tic_LedRecon;     //para parpadeo led LEDB con LEDR activo (morado)
    Ticker tic_LedError;     //para parpadeo led ERROR (LEDR)
    Ticker tic_LedZona;      //para parpadeo led zona de riego
    Ticker tic_LedZonas24h;  //para parpadeo led zonas regadas ultimas 24h
    Ticker tic_verificaciones;       //para verificaciones periodicas
    S_timeRiego lastRiegos[NUMZONAS];
    S_timeRiego lastGrupos[NUMGRUPOS];
    S_Riego_estado riegoSaved; // estructura con el estado del riego en curso
    uint factorRiegos[NUMZONAS];
    bool backlightOff = false;
    bool flagV = OFF;
    bool flagVtimer = OFF;
    bool timeOK = false;
    bool factorRiegosLeido = false;
    bool encoderSW = false;
    bool checkReconInterval = false; // verificaciones de conexion cada RECONNECTINTERVAL minutos
    bool webServerAct = false;
    bool saveConfig = false;
    bool riegoFromPause = false;
    bool inSetup = true;
    bool fsOK = false;  // filesystem ok
    unsigned long standbyTime;
    // int  ledID = 0;
    int numloops = 0;
    char amanecer[] = "NO TIME";
    char anochecer[] = "NO TIME";
    char buff[MAXBUFF];
    
    #ifdef TEMPLOCAL 
    DHT dht(DHTPIN, TEMPLOCAL);
    #endif
    
    #else
    // ademas de en main, son globales a todos los modulos:
    extern int NUM_S_BOTON;
    extern S_BOTON Boton [];
    extern S_BOTON  *boton;
    extern S_BOTON  *ultimoBotonZona;
    extern S_MULTI multi;
    extern S_Estado Estado;
    extern S_tm tm;
    extern DisplayLCD lcd;
    extern Sonidos sonido;
    extern Config_parm config;
    extern S_simFlags simular;
    extern bool webServerAct;
    extern bool saveConfig;
    extern bool checkReconInterval;
    extern bool encoderSW;
    extern const char *parmFile; 
    extern const char *backupParmFile;
    extern const char *lastRiegosFile;
    extern const char *lastGruposFile;
    extern const char *logErrorFile;
    extern const char *logErrorFilePrev;
    extern char buff[];

    
  #endif

// *****************************************************************************************
//  Funciones (prototipos)
// *****************************************************************************************
void actLedError(void);
void apagaLeds(void);
int  bID2bIndex(uint16_t);
void blinkDisplay(void);
// bool checkErrorgetFactor(int);
void check(void);
bool checkSCD(void);
int  checkWifi(bool level=false);
void cleanFS(void);
String convertFileSize(const size_t);
bool copyConfigFile(const char *, const char *);
void debugloops(void);
bool deleteDatos(void);
void deleteParmSignal(uint);
bool deviceSwitch(uint8_t zona, const char *msg, int retries);
void dimmerLeds(bool);
void displayDemo(void);
void displayEstadoRemoto(const char *estado_texto);
void displayLedsGrupo(uint16_t *, int);
void displayLCDGrupo(bool, int line=4, int znumber=0);
int  displayLCDGrupo(uint16_t *, int, int , int );
void displayMultiTemporal(void);
void displayNoFactorizado(void);
void displayTimer(uint8_t, uint8_t, uint8_t, uint8_t);
void displayEstadoRemoto(estado_tipos tipo);
void enciendeLeds(void);
void endWS(void);
static const char* errorToString(error_tipos);
void filesInfo(void);
void finalTimeGrupo(S_timeRiego&, time_t tZona = 0);
void finalTimeLastRiego(S_timeRiego&);
void flagVerificaciones(void);
void gestionarTamanoLog();
bool getDiaNoche(char*, char*);
String getDomoticzSettingsInfo(const char*);
int getFactor(uint8_t zona, bool &factorRiegosLeido);
uint16_t getMultiStatus(void);
float getRemoteTemperature();
const char* getTimestamp();
void handleDynamicZoneChange();
void handleEncGrupoInStandby(int n_grupo);
void handleEncGrupoInStop(int n_grupo);
void handleEncPauseInPause();
void handleEncPauseInRegando();
void handleEncPauseInStandby();
void handleEncPauseInStop();
void handleEncStopInStandby();
void handleGroupConfig();
void handleGrupoInStandby(int n_grupo);
bool handleHoldPause();
void handleParameterConsolidation();
void handlePauseInError();
void handlePauseInPause();
void handlePauseInRegando();
void handlePauseInStandby();
void handlePauseInStop();
void handleStartMultiTemp();
void handleStopInError();
void handleStopInRegandoPauseTerm();
void handleStopInStandby();
void inicioTimeLastRiego(S_timeRiego&, const char* texto = nullptr, bool resume=false);
void initEncoder(void);
void initFactorRiegos(void);
void initGPIOs(void);
void initHardware(void);
void initLastGrupos(void);
void initLastRiegos(void);
void initLCD(void);
void initLeds(void);
void initMCP23017 (void);
bool initRiego(bool resume=false);
void initWire(void);
void led(uint8_t,int);
int  ledlevel(void);
void ledPWM(uint8_t, int);
void ledRGB(int,int,int);
bool ledStatusId(int);
void ledYellow(int);
void leerEncoderSW();
void leeSerial(void);
void listAllFilesInDir(fs::FS &fs, String dir_path);
void listDir(fs::FS &fs, const char * dirname, uint8_t levels, uint8_t depth = 0);
bool loadConfigFile(const char*);
void mcpIinit(void);
void mcpOinit(void);
void memoryInfo(void);
void pararLedsWifiAP();
void parpadeoLedPWM(int id);
void parpadeoLedZona(int);
void parpadeoLedZonas24h(time_t);
S_BOTON *parseInputs(bool);
void printCharArray(char*, size_t);
void printFactoresRiego();
void printFile(const char*);
void printMultiGroup(int);
void printMulti(void);
void printParms();
void procesaBotones(void);
void procesaBotonMultirriego(void);
void procesaBotonPause(void);
void procesaBotonStop(void);
void procesaBotonZona(void);
bool procesaDynamic(void);
void procesaEncoderClock(void);
void procesaEncoderConfig(void);
void procesaEstadoConfigurando(void);
void procesaEstadoError(void);
void procesaEstadoPause(void);
void procesaEstadoRegando(void);
void procesaEstadoStandby(void);
void procesaEstados(void);
void procesaEstadoStop(void);
void procesaEstadoTerminando(void);
void procesaWebServer(void);
bool queryStatus(uint8_t, const char *);
float readTemp();
String readLogFile(int zona);
void refreshTime(void);
void reposoOFF(void);
void reposoON(bool lcdOFF=true);
void resetESP32();
void resetFlags(void);
void resetLCD(void);
void resetLeds(void);
void restoreRiego(void);
bool saveConfigFile(const char*);
void saveRiego(int znumber, int bID, int minutes, int seconds);
void scSorpresa();
void scWebserver();
bool serialDetect(void);
void setbIDgrupos();
void setClock(void);
void setConnected(bool);
void setEncoderMenu(int menuitems, int currentitem = 0);
void setEncoderRange(int , int , int , int);
void setEncoderTime(void);
void setEstado(m_estados estado, int bnum = 0, estado_tipos tipo = LOCAL, velocidad_parpadeo ledblink = FIJO);
int  setGrupo();
void setledRGB(void);
int  setMultibyId(uint16_t);
bool setMultirriego();
void setParpadeo(Ticker &t, velocidad_parpadeo vel, void (*f_callback)(int), int ledid);
void setParpadeo(Ticker &t, velocidad_parpadeo vel, int ledid=0);
void setStateMachine(m_estados estado, estado_tipos tipo = LOCAL);
void setzNumber(void);
void setupConfig(void);
void setupEstadoFinal(void);
void setupInit(void);
void setupParm(void);
void setupRedWM(S_initFlags&);
void setupWS();
void showInfoZona(int zIndex);
void showTemp(void);
void showTimeLastRiego(S_timeRiego&, int, int);
void simulaPauseIfEncoderSW();
void simulaPauseIfEncoderSW2();
void startConfigPortal();
void startZoneWatering();
void StaticTimeUpdate(bool);
void statusError(error_tipos, bool recoverable=false, velocidad_parpadeo zonablink = FIJO, velocidad_parpadeo errorblink = FIJO);
bool stopAllRiego(void);
bool stopRiego(uint16_t id, bool update = true, bool alertIfFails = true);
String sysInfo(void);
bool testButton(uint16_t, bool);
time_t tLoc(void);
void timeByFactor(int,uint8_t *,uint8_t *);
void timerTick(void);
void  tmvalue(void);
String TS2Date(time_t);
String TS2Hour(time_t);
void ultimosRiegos(int);
void updateZoneDescription(int i);
bool validaBoton();
void Verificaciones(void);
bool VerifyRecoveryWifi(bool checkReconInterval);
void VerifyRecoverySCD(void);
void wifiClearSignal(uint);
bool wifiReconnect(void);
void zeroConfig();
int  zNumber2bIndex(uint16_t);

// *****************************************************************************************
// Funciones (templates) para gestion de las tablas de registro de riegos de zonas y grupos
// ***************************************************************************************** 

template<typename T>
void saveTablaToFile(const char* filename, const char* arrayName, T* tabla, size_t size) {
    if(Estado.modoDEMO) return; // no guardar en modo demo
    JsonDocument doc;
    JsonArray arr = doc[arrayName].to<JsonArray>();
    for (size_t i = 0; i < size; i++) {
        JsonObject obj = arr.add<JsonObject>(); 
        obj["inicio"] = tabla[i].inicio;
        obj["final"]  = tabla[i].final;
        obj["total"]  = tabla[i].total;
    }
    File file = LittleFS.open(filename, "w");
    if (!file) {
        LOG_ERROR("Error abriendo el fichero para guardar", arrayName);
        return;
    }
    serializeJson(doc, file);
    file.close();
    LOG_INFO(arrayName, "guardado correctamente.");
}

template<typename T>
bool loadTablaFromFile(const char* filename, const char* arrayName, T* tabla, size_t size) {
    File file = LittleFS.open(filename, "r");
    if (!file) {
        LOG_WARN("Error abriendo el fichero para leer", arrayName);
        return false;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        LOG_ERROR("Error al deserializar JSON:", error.c_str());
        return false;
    }
    JsonArray arr = doc[arrayName];
    if (!arr) {
        LOG_ERROR("No se encontró el array", arrayName, "en el fichero", filename);
        return false;
    }
    for (size_t i = 0; i < size && i < arr.size(); i++) {
        JsonObject obj = arr[i];
        tabla[i].inicio = obj["inicio"] | 0;
        tabla[i].final  = obj["final"]  | 0;
        tabla[i].total  = obj["total"]  | 0;
    }
    LOG_INFO(arrayName, "cargado correctamente.");
    return true;
}

// *****************************************************************************************
// Ejemplo de template con proceso variable al que se le pasa la funcion a ejecutar
// que puede tener varias instrucciones (lambda function)
// ***************************************************************************************** 

template<typename T, typename F>
void procesaArray(T* array, size_t size, F func) {
    for (size_t i = 0; i < size; ++i) {
        func(array[i]);
    }
}

// EJEMPLO llamada con varias instrucciones en el callback:
// procesaArray(lastRiegos, NUMZONAS, [](S_timeRiego& r){
//     r.inicio = 0;
//     r.final = 0;
//     r.total = 0;
//     Serial.println("Elemento reseteado");
// });

#endif  // control_h
