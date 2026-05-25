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
  // #include <TimeLib.h>
  #include <CountUpDownTimer.h>
  #include <ArduinoJson.h>
  #include <Ticker.h>
  #include <LittleFS.h>
  #include <Wire.h>
  // #include <esp_sntp.h>  // para poder cambiar el intervalo por defecto del ESP32 para sincronizar con el NTP
  
  #ifdef ESP32
    // #include <HTTPClient.h>  // pasado a ComDomoticz.cpp
    // #include <WiFi.h>        // ya lo incluye WiFiManager.h
    // #include <WebServer.h>   // pasado a webserver.cpp (aunque ya lo incluye WiFiManager.h)
    #ifdef TEMPLOCAL
      #include <Adafruit_Sensor.h>
      #include <DHT.h>
    #endif
  #endif
  
  //Librerias de terceros locales en carpeta /lib
  #include "Encoder.h" // libreria con la clase Encoder para el encoder rotatorio
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

  #define FORMAT_LITTLEFS_IF_FAILED true
  #ifndef clean_FS
    #define clean_FS false
  #endif

  // Macros y constantes utiles:
  #define ELEMENTCOUNT(x)  (sizeof(x) / sizeof(x[0])) // calcula el numero de elementos de un array
  #define UMBRAL_EPOCH 1767225600 // fecha 1/1/2026 en formato epoch (si la fecha es anterior se considera no valida)
       
  //-------------------------------------------------------------------------------------
  //                #define FW_VERSION  movido a platformio.ini   // version del software
  //-------------------------------------------------------------------------------------

  // Comportamiento General: valores fijos o por defecto los modificables por el usuario [*]
  #ifdef RELEASE
    #define DEFAULTMINUTES      10    // * tiempo de riego por defecto (minutos)
    #define DEFAULTSECONDS      0     // * tiempo de riego por defecto (segundos)
    #define RECONNECTINTERVAL   2     // tiempo en minutos para intentar reconexion a la wifi
    #define LONGINTERVAL        15    // tiempo en minutos para verificaciones largo plazo 
  #endif
  #ifdef DEVELOP
    #define DEFAULTMINUTES      0
    #define DEFAULTSECONDS      10
    #define RECONNECTINTERVAL   1      // tiempo en minutos para intentar reconexion a la wifi
    #define LONGINTERVAL        2      // tiempo en minutos para verificaciones largo plazo 
  #endif
  #define NTPSERVER_SPAIN     "es.pool.ntp.org"  // servidor NTP por defecto
  #define TZ_Europe_Madrid    "CET-1CEST,M3.5.0,M10.5.0/3"  // time zone en formato TZ posix
  #define NTP_TIMEOUT         7000    // tiempo de espera para recibir respuesta del servidor NTP en mseg
  #define STANDBYSECS         30      // tiempo en segundos para pasar a reposo desde standby (apagar pantalla y atenuar leds)
  #define BLINKDISPLAY        3       // numero de parpadeos de la pantalla
  #define BLINKMILLIS         500     // mseg entre parpadeo de la pantalla
  #define DFLT_MSGDISPLAYMS   1000    // * mseg se mantienen mensajes informativos
  #define MAXMINUTES          59      // corte automatico de seguridad a los 60 min. en los arduinos
  #define MINSECONDS          5       // minimo de segundos ajustables en el temporizador
  #define HOLDTIME            3000    // mseg que hay que mantener PAUSE pulsado para ciertas acciones
  #define MAXCONNECTRETRY     10      // numero maximo de reintentos de reconexion a la wifi tras el fallo en inicio
  #define VERIFY_INTERVAL     15      // intervalo en segundos entre verificaciones periodicas
  #define HTTPCLIENTCONNECTTIMEOUT  1000  // timeout (ms) para establecer conexion con el servidor Domoticz
  #define HTTPCLIENTRESPONSETIMEOUT 1000  // timeout (ms) para recibir respuesta del servidor Domoticz
  #define MAX_UPLOAD_KBYTES   30      // tamaño maximo del fichero para hacer upload en KB
  #define SWITCH_RETRIES      3       // numero de reintentos para parar o encender una zona de riego en el Domoticz
  #define DELAYRETRY          1500    // mseg de retardo entre reintentos
  #define I2C_CLOCK_SPEED     400000  // frecuencia del bus I2C en Hz (default 100000)
  #define LCD2004_address     0x27    // direccion bus I2C de la pantalla LCD
  #define ROTARY_ENCODER_STEPS 4      // TODO documentar
  #define DFLT_MAXLEDLEVEL    255     // * nivel maximo leds RGB (0 a 255)
  #define DFLT_DIMMLEVEL      50      // * nivel atenuacion leds RGB (0 a 255)
  #define DFLT_VOLUME         8       // * volumen por defecto (0 a 10)
  #define DFLT_FINMELODY      MIMI    // * melodia final riego grupo por defecto
  #define DFLT_MAX_ESP32_TEMP 80      // * max temp. ESP32 para mostrar aviso (con wifi funciona mal)
  #define DFLT_TEMP_OFFSET    0       // * correccion temperatura medida (en saltos segun TEMP_OFFSET_FACTOR)
  #define TEMP_OFFSET_FACTOR  50      // correccion temperatura factor ajuste (50% = x 0.5)
  #define DFLT_TEMP_DATA_REMOTE 0     // * fuente del dato de temperatura 0=local/1=remota
  #define DEFAULTXNAME        false   // * actualiza desc de botones con el Name del dispositivo que devuelve Domoticz
  #define DEFAULTVERIFY       true    // * verifica estado dispositivo en el Domoticz
  #define DEFAULTDYNAMIC      false   // * si true permite añadir/eliminar zonas durante el riego
  #define DEFAULTLASTR24      false   // * muestra leds ultimos riegos desde las 0h (false) o ultimas 24h (true)
  #define DFLT_LOGWARNTOFILE  false   // * si true LOG_WARN tambien se graba en el fichero de log de errores
  #define SHORTCUTSENABLED    true    // admite atajos en estado STOP
  #define ENCSWASPAUSE        true    // encoderSW simula PAUSE en estado CONFIGURANDO
  #define ZONASXGRUPO         9       // maximo de zonas en un grupo multirriego (9 para coja en pantalla, max. 16)
  #define DFLT_SCD_PORT       "8080"  // * puerto por defecto para conexión con Domoticz
                                      // [*] = configurables

 //----------------  dependientes del HW   (no modificar) ---------------------------
  #ifdef ESP32
    // GPIOs  I/O usables: 2 4 5 16 17 18 19 21 22 23 25 26 27 32 33  (usados 11 de 15)
    // GPIOs  I/O los reservo para JTAG: 12 13 14 15
    // GPIOs  I usables: 34 35 36 39 (usado 1 de 4)  (ojo no tienen pullup/pulldown interno, requieren resistencia externa)
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

  //----------------  dependientes del HW   (caso de 4 botones de grupos multirriego)  ---------------
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
  #endif

  //----------------  dependientes del HW   (caso de boton multirriego + selector 3 grupos)   ---------------
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
  #endif
  //----------------  fin dependientes del HW   ----------------------------------------

  //Para legibilidad del codigo
  #define ON  true
  #define OFF false
  #define NOSIGNALERROR false
  #define SHOW 1
  #define HIDE 0
  #define READ 1
  #define CLEAR 0
  #define REFRESH 1
  #define UPDATE 0
  #define NOBLINK 0
  #define BORRA1H 1
  #define BORRA2H 2
  #define RECUPERABLE 1
  #define NORECUPERABLE 0
  #define INICIO 0
  #define RESUME 1
  #define SHORT 1
  #define NEWMTEMP 1

  // constexpr calculado por el preprocesador y no modificable en tiempo de ejecucion
  constexpr uint16_t Zonas[] = {_ZONAS};  // array de todos los botones de zonas de riego disponibles
  constexpr uint16_t Grupos[]  = {_GRUPOS}; // array de todos los botones de grupos disponibles
  constexpr int NUMZONAS = ELEMENTCOUNT(Zonas); // numero de zonas (botones riego individual)
  constexpr int NUMGRUPOS = ELEMENTCOUNT(Grupos); // numero de grupos multirriego

/* --------------------------------------------------------------------------------------
 *                                Estructuras
 * -------------------------------------------------------------------------------------- */

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

  // flags de inicializacion (borrado de parametros o wifi)
  struct S_initFlags     {
    uint8_t preinitParm   : 1,
            initParm      : 1,
            initWifi      : 1,
            spare1        : 1;
  };

  // flags de simulacion en modo DEVELOP
  union S_simFlags  {
    uint8_t all_simFlags;
    struct
    {
    uint8_t ErrorOFF       : 1,
            ErrorON        : 1,
            ErrorVerifyON  : 1,
            ErrorVerifyOFF : 1,
            ErrorPause     : 1;
    };
  } ;

  struct S_BOTON {
    const uint16_t bID;     // Fijo: Identificador de bit del boton (bitmask)
    bool  estado;           // Variable: Estado actual
    bool  ultimo_estado;    // Variable: Estado previo
    const int   led;        // Fijo: Pin del LED asociado al boton (0 si no tiene)
    S_bFLAGS  flags;        // flags varios
    const char* const desc; // Fijo: descripcion por defecto del boton
  } ;

  // estructura con el estado general del sistema (State Machine)
  struct S_Estado {
    m_estados estado = STANDBY; 
    estado_tipos tipo   = LOCAL;
    error_tipos error  = NOERROR;
    // Campos de flags/modos de operación
    bool inSetup = true;
    bool connected = false;
    bool modoDEMO = false;
    bool noWIFI = false;
    bool reposo = false;    
    bool failedStopRiego = false;
    bool recoverableError = false;
    bool errorInformado = false;  // para no repetir logs del mismo error "silencioso" (fallo wifi en standby o readRemoteTemp)
  } ;

  struct S_timeRiego {
    time_t inicio; 
    time_t final; 
    time_t reinicio; 
    time_t total; 
  } ;

// Definición de la estructura de control para el Ticker de parpadeo de los leds regados en ultimas 24h
struct S_ledsParpadeo {
    int leds[NUMZONAS];
    uint8_t cantidad;
};
  // variables contador de tiempo
  struct S_tm {
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    int  value = 0;
  } ;


  //estructura para salvar un grupo
  struct Grupo_parm {
    int size = 0;          // cantidad de zonas asociadas al grupo 
    uint16_t zNumber[ZONASXGRUPO] = {};  // numeros de las zonas del grupo inicializados a 0
    char desc[20] = "";    // descripcion del grupo
  } ;

  //estructura para salvar parametros de una zona
  #ifdef DOMOTICZ
  struct Zona_parm {
    char  desc[20] = "";     // nombre de la zona a mostrar en el display
    uint16_t   idx = 0;      // identificador de la zona en el SCD (IDX en Domoticz)
  } ;
  #endif

  //estructura para parametros configurables (creada inicialmente con valores por defecto)
  struct Config_parm {
    bool initialized = false;
    static const int  n_Zonas = NUMZONAS;       //no modificable por fichero de parámetros (depende HW) 
    Zona_parm zona[n_Zonas];                    // parametros de cada zona (descripcion y idx en Domoticz)
    static const int  n_Grupos = NUMGRUPOS;     //no modificable por fichero de parámetros (depende HW)
    Grupo_parm group[n_Grupos];                 // parametros de cada grupo (zonas asociadas y descripcion)
    char SCD_ip[40] = "";                       // IP o nombre del servidor SCD (Domoticz)
    char SCD_port[6] = DFLT_SCD_PORT;           // puerto del servidor SCD
    char SCD_user[21] = "";                     // usuario para autenticacion en SCD (opcional)
    char SCD_password[41] = "";                 // password para autenticacion en SCD (opcional)
    char ntpServer[40] = NTPSERVER_SPAIN;       // servidor NTP por defecto
    char TZ[50] = TZ_Europe_Madrid;             // time zone por defecto en formato TZ posix
    uint8_t   minutes = DEFAULTMINUTES;         // tiempo de riego por defecto
    uint8_t   seconds = DEFAULTSECONDS;         // tiempo de riego por defecto
    int  warnESP32temp = DFLT_MAX_ESP32_TEMP;   // temperatura ESP32 maxima con aviso 
    int  maxledlevel = DFLT_MAXLEDLEVEL;        // nivel brillo maximo led RGB 
    int  dimmlevel = DFLT_DIMMLEVEL;            // nivel atenuacion led RGB 
    int  tempOffset = DFLT_TEMP_OFFSET;         // correccion temperatura sensor local DHTxx 
    int  tempRemote = DFLT_TEMP_DATA_REMOTE;    // si true obtiene temperatura via Domoticz
    int  tempRemoteIdx = 0;                     // IDX del sensor remoto en Domoticz
    int  msgdisplaymillis = DFLT_MSGDISPLAYMS;  // tiempo que se muestran mensajes (mseg.) 
    int  volume = DFLT_VOLUME;                  // volumen sonidos por defecto
    int  finMelody = DFLT_FINMELODY;            // melodia final riego grupo por defecto
    bool mute = OFF;                            // sonidos activos
    bool showwifilevel = OFF;                   // muestra en standby nivel de la señal wifi
    bool xname = DEFAULTXNAME;                  // actualiza desc de botones con el Name del dispositivo que devuelve Domoticz
    bool verify = DEFAULTVERIFY;                // verifica estado dispositivo en el Domoticz
    bool dynamic = DEFAULTDYNAMIC;              // si true permite añadir/eliminar zonas durante el riego
    bool lastr24 = DEFAULTLASTR24;              // muestra leds ultimos riegos desde las 0h (false) o ultimas 24h (true)
    bool logWarnToFile = DFLT_LOGWARNTOFILE;    // si true los LOG_WARN tambien se graban en el fichero de log de errores
  };

  // estructura del multirriego activo (se esta regando o configurando)
  // (algunos son pointer al grupo correspondiente en config *)
  struct S_MULTI {
    // campos de estado del multirriego en curso
    bool riegoON  = false;  // multirriego activo
    bool temporal = false;  // grupo multirriego es temporal
    bool noFactorizado  = false;  // grupo multirriego es dinámico (a partir de un riego de zona individual, no factorizado)
    bool semaforo = false;  // procesar siguiente zona del multirriego
    int  actualIndex;            // variable auxiliar durante el riego: indice en multi.zserie_boton de la zona que se esta regando actualmente
    // campos de configuración del grupo multirriego en curso 
    int  ngrupo;            // numero del grupo al que apunta
    uint16_t zserie_boton[16];     //contiene los id de las zonas del grupo (bZona_x)
    uint16_t w_zserie[16];  //contiene las zonas del grupo (Zona_x)
    int  w_size;            //variable auxiliar durante ConF: numero de zonas configuradas en el grupo (tamaño del grupo)
    const uint16_t *id;     //apuntador al id del boton/selector grupo en Grupos[]. Solo lectura.
    int *size;              //apuntador a config con el tamaño del grupo
    const char *desc;       //apuntador a config con la descripcion del grupo. Solo lectura.
  } ;

// estructura para la zona activa (regando, terminando, parando, a parar...etc)  
  struct S_zonaEnCurso{
    S_BOTON* pBoton;   // Puntero al hardware (Leds, flags, ID) de Boton[]
    int zindex;        // Índice de la zona (0 a 8) para config 
    int znumber;       // Número de zona (1 a 9) para mostrar en el display
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

/* --------------------------------------------------------------------------------------
 *                     Variables Globales a _MAIN_ (Control.cpp)
 * -------------------------------------------------------------------------------------- */

  #ifdef __MAIN__
    #ifdef GRP4     // matriz Boton para caso de 9 zonas y 4 botones de grupos multirriego
      S_BOTON Boton [] =  { 
      // bID          S   uS  LED          FLAGS                             DESC  
        {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1"},
        {bZONA2   ,   0,  0,  lZONA2   ,   ENABLED | ACTION,                 "ZONA2"},
        {bZONA3   ,   0,  0,  lZONA3   ,   ENABLED | ACTION,                 "ZONA3"},
        {bZONA4   ,   0,  0,  lZONA4   ,   ENABLED | ACTION,                 "ZONA4"},
        {bZONA5   ,   0,  0,  lZONA5   ,   ENABLED | ACTION,                 "ZONA5"},
        {bZONA6   ,   0,  0,  lZONA6   ,   ENABLED | ACTION,                 "ZONA6"},
        {bZONA7   ,   0,  0,  lZONA7   ,   ENABLED | ACTION,                 "ZONA7"},
        {bZONA8   ,   0,  0,  lZONA8   ,   ENABLED | ACTION,                 "ZONA8"},
        {bZONA9   ,   0,  0,  lZONA9   ,   ENABLED | ACTION,                 "ZONA9"},
        {bGRUPO1  ,   0,  0,  lGRUPO1  ,   ENABLED | ACTION,                 "GRUPO1"},
        {bGRUPO2  ,   0,  0,  lGRUPO2  ,   ENABLED | ACTION,                 "GRUPO2"},
        {bGRUPO3  ,   0,  0,  lGRUPO3  ,   ENABLED | ACTION,                 "GRUPO3"},
        {bGRUPO4  ,   0,  0,  lGRUPO4  ,   ENABLED | ACTION,                 "GRUPO4"},
        {bPAUSE   ,   0,  0,  0        ,   ENABLED | ACTION | DUAL | HOLD,   "PAUSE"},
        {bSTOP    ,   0,  0,  0        ,   ENABLED | ACTION | DUAL,          "STOP"}
      };
    #endif
    
    #ifdef M3GRP     // matriz Boton para caso de 9 zonas, boton multirriego y selector de 3 grupos multirriego
      S_BOTON Boton [] =  { 
      // bID          S   uS  LED          FLAGS                             DESC
        {bZONA1   ,   0,  0,  lZONA1   ,   ENABLED | ACTION,                 "ZONA1"},
        {bZONA2 ,     0,  0,  lZONA2 ,     ENABLED | ACTION,                 "ZONA2"},
        {bZONA3    ,  0,  0,  lZONA3    ,  ENABLED | ACTION,                 "ZONA3"},
        {bZONA4    ,  0,  0,  lZONA4    ,  ENABLED | ACTION,                 "ZONA4"},
        {bZONA5    ,  0,  0,  lZONA5    ,  ENABLED | ACTION,                 "ZONA5"},
        {bZONA6 ,     0,  0,  lZONA6 ,     ENABLED | ACTION,                 "ZONA6"},
        {bZONA7  ,    0,  0,  lZONA7  ,    ENABLED | ACTION,                 "ZONA7"},
        {bZONA8  ,    0,  0,  lZONA8  ,    ENABLED | ACTION,                 "ZONA8"},
        {bZONA9,      0,  0,  lZONA9  ,    ENABLED | ACTION,                 "ZONA9"},
        {bGRUPO1,     0,  0,  lGRUPO1,     ENABLED | ONLYSTATUS | DUAL,      "GRUPO1"},
        {bGRUPO2  ,   0,  0,  lGRUPO2  ,   ENABLED | ONLYSTATUS | DUAL,      "GRUPO2"},
        {bGRUPO3,     0,  0,  lGRUPO3,     ENABLED | ONLYSTATUS | DUAL,      "GRUPO3"},
        {bMULTIRRIEGO,0,  0,  0,           ENABLED | ACTION,                 "MULTIRRIEGO",},
        {bPAUSE,      0,  0,  0,           ENABLED | ACTION | DUAL | HOLD,   "PAUSE"},
        {bSTOP,       0,  0,  0,           ENABLED | ACTION | DUAL,          "STOP",}
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
    Sonidos sonido;     // clase para gestionar sonidos con buzzer
    S_initFlags initFlags ; // flags de inicializacion (borrado de parametros o wifi)
    CountUpDownTimer timer(DOWN); // temporizador cuenta atras
    S_simFlags simular;           // estructura flags para simular errores
    Configure    *configure;
    Encoder rotaryEncoder(ENCDT,ENCCLK);
    Ticker tic_CountDownTimer;       //para llamar a la funcion de cuenta atras del temporizador
    Ticker tic_LedRecon;     //para parpadeo led LEDB con LEDR activo (morado)
    Ticker tic_LedError;     //para parpadeo led ERROR (LEDR)
    Ticker tic_LedZona;      //para parpadeo led zona de riego
    Ticker tic_LedZonas24h;  //para parpadeo led zonas regadas ultimas 24h
    Ticker tic_verificaciones;       //para verificaciones periodicas
    timeval tv;
    S_timeRiego lastRiegos[NUMZONAS];
    S_timeRiego lastGrupos[NUMGRUPOS];
    S_Riego_estado riegoSaved; // estructura con el estado del riego que se ha cancelado
    S_zonaEnCurso zonaEnCurso; // estructura con el estado de la zona en curso (regando, terminando, parando, a parar...etc)
    uint factorRiegos[NUMZONAS];
    bool flagV = OFF;
    bool flagVtimer = OFF;
    bool timeOK = false;
    bool factorRiegosLeido = false;
    bool encoderSW = false;
    bool simulaPausePrev = false;
    bool checkRecon = false; // verificaciones de conexion cada RECONNECTINTERVAL minutos
    bool checkLogSize = false; // verificaciones de tamano log errores cada LONGINTERVAL minutos
    bool webServerAct = false;
    bool saveConfig = false;
    bool riegoFromPause = false;
    bool fsOK = false;  // filesystem ok
    unsigned long standbyTime;
    int numloops = 0;
    char amanecer[] = "NO TIME";
    char anochecer[] = "NO TIME";
    char buff[MAXBUFF];
    
    #ifdef TEMPLOCAL 
    DHT dht(DHTPIN, TEMPLOCAL);
    #endif

/* --------------------------------------------------------------------------------------
 *                   ademas de en main, son globales a todos los modulos:
 * -------------------------------------------------------------------------------------- */

    #else
    extern int NUM_S_BOTON;
    extern S_BOTON Boton [];
    extern S_BOTON  *boton;
    extern S_MULTI multi;
    extern S_Estado Estado;
    extern S_zonaEnCurso zonaEnCurso;
    extern S_tm tm;
    extern DisplayLCD lcd;
    extern Sonidos sonido;
    extern Config_parm config;
    extern S_simFlags simular;
    extern bool webServerAct;
    extern bool saveConfig;
    extern bool checkRecon;
    extern bool encoderSW;
    extern bool simulaPausePrev;
    extern const char *parmFile; 
    extern const char *backupParmFile;
    extern const char *lastRiegosFile;
    extern const char *lastGruposFile;
    extern const char *logErrorFile;
    extern const char *logErrorFilePrev;
    extern char buff[];
  #endif


/* --------------------------------------------------------------------------------------
 *                     Declaracion de Funciones (prototipos)
 * -------------------------------------------------------------------------------------- */

void apagaLeds(void);
int  getBotonIndex(uint16_t);
void blinkDisplay(void);
void check(void);
bool checkAndInitFactorRiegos(bool signalError = true);
bool checkSCD(void);
int  checkWifi(bool level=false);
void cleanFS(void);
String convertFileSize(const size_t);
bool copyFile(const char *, const char *);
void debugloops(void);
bool deleteDatos(void);
void deleteParmSignal(uint);
bool deviceSwitch(uint8_t zona, const char *msg, int retries);
void dimmerLeds(bool);
void displayDemo(void);
void displayEstadoRemoto(const char *estado_texto);
void displayLedsGrupo(void);
int  displayLCDGrupo(display_modo modo = FULL, int line = 4, int znumber = 0);
void displayTipoGrupo();
void displayRestar();
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
int  getFactor(uint8_t zona, bool &factorRiegosLeido);
uint16_t getMultiStatus(void);
float getRemoteTemperature();
const char* getTimestamp();
int  getZonaIndex(uint16_t id);
void handleDynamicZoneChange(int znumber);
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
bool loadFactorRiegos(void);
void initFS();
void initGPIOs(void);
void initHardware(bool);
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
bool estadoLedId(int);
void ledYellow(int);
void leerEncoderSW();
void leeSerial(void);
void listDir(fs::FS &fs, const char * dirname, uint8_t levels, uint8_t depth = 0);
bool loadConfigFromFile(const char *);
void logStatus(const char *mensaje);
void logStatusF(const char *format, ...);
void mcpIinit(void);
void mcpOinit(void);
void memoryInfo(void);
void pararLedsWifiAP();
void parpadeoLedPWM(int id);
void parpadeoLedZona(int);
void parpadeoLedZonas(S_ledsParpadeo*);
S_BOTON *parseInputs(bool);
bool parseSCDuri(const String &uri);
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
bool procesaDynamic(int znumber);
void procesaEncoderTime(void);
void procesaEncoderConfig(void);
void procesaEstadoConfigurando(void);
void procesaEstadoError(void);
void procesaEstadoPause(void);
void procesaEstadoRegando(void);
void procesaEstadoStandby(void);
void procesaEstados(void);
void procesaEstadoStop(void);
void procesaEstadoTerminando(void);
void procesaIfWebServer();
void procesaWebServer(void);
bool queryStatus(uint8_t, const char *);
float readTemp();
String readSCDLogFile(int zona);
void refreshLogFile();
void refreshTime(void);
String registrarArranqueSistema();
void reposoOFF(void);
void reposoON(void);
void resetESP32();
void resetFlags(void);
void resetLCD(void);
void resetLeds(void);
void restoreRiego(void);
bool writeConfigToFile(const char*);
bool saveConfigToParmfile(void);
void saveRiego(int znumber, int bID, int minutes, int seconds);
void scSorpresa();
void scWebserver();
void scWifiLevel();
bool serialDetect(void);
void setClock(void);
void setConnected(bool);
void setEncoderMenu(int menuitems, int currentitem = 0);
void setEncoderRange(int , int , int , int);
void setEncoderTime(void);
void setEstado(m_estados estado, int bnum = 0, estado_tipos tipo = LOCAL, velocidad_parpadeo ledblink = NULO);
int  setGrupo();
void setLed(Ticker &t, estado_led estado, int ledid);
void setLedStatus(void);
void setLogToFile();
int  setMultibyId(uint16_t);
void setMultiTemp(bool newTemp = false);
void setParpadeo(Ticker &t, velocidad_parpadeo vel, void (*f_callback)(int), int ledid);
void setParpadeo(Ticker &t, velocidad_parpadeo vel);
void setStateMachine(m_estados estado, estado_tipos tipo = LOCAL);
void setupConfig(void);
void setupEstadoFinal(void);
void setupInit(void);
void setupParm(void);
void setupRedWM(S_initFlags&);
void setupWS();
void setZonaEnCurso(uint16_t bID);
void showInfoZona(int zIndex);
void showTemp(void);
void showTimeLastRiego(S_timeRiego&);
void showWifiLevel(int wifilevel);
void simulaPauseIfEncoderSW();
void startConfigPortal();
bool startMultirriego();
void startZoneWatering();
void StaticTimeUpdate(bool);
void statusError(error_tipos, bool recoverable=false, velocidad_parpadeo zonablink = NULO, velocidad_parpadeo errorblink = NULO);
bool stopAllRiegos(void);
void stopHW(const char* msg);
bool stopRiego(uint16_t id, bool update = true, bool alertIfFails = true, int retries = SWITCH_RETRIES);
String sysInfo(void);
bool testButton(uint16_t, bool);
time_t tLoc(void);
void timeByFactor(int,uint8_t *,uint8_t *);
void timerTick(void);
void  tmvalue(void);
void ultimosRiegos(int);
void updateZoneDescription(int i);
bool validaBoton();
void Verificaciones(void);
bool VerifyRecoveryWifi(bool checkRecon);
void VerifyRecoverySCD(void);
void wifiClearSignal(uint);
bool wifiReconnect(void);
void zeroConfig();


// *****************************************************************************************
// Funciones (templates) para gestion de las tablas de registro de riegos de zonas y grupos
// ***************************************************************************************** 

template<typename T>
void saveTablaToFile(const char* filename, const char* arrayName, T* tabla, size_t size) {
    // no guardar en modo demo o si la hora o fecha no es correcta (antes del 1 de enero de 2026 00:00 GMT)
    if(Estado.modoDEMO || !timeOK || time(NULL)<UMBRAL_EPOCH) return;
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
    LOG_DEBUG(arrayName, "guardado OK.");
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
    LOG_DEBUG(arrayName, "cargado OK.");
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



// *****************************************************************************************
// Funciones de tiempo para evitar usar TimeLib.h
// ***************************************************************************************** 

// Sustitutos directos para TimeLib
// #define day(t)    getDay(t)
// #define month(t)  getMonth(t)
// #define hour(t)   getHour(t)
// #define minute(t) getMinute(t)

#define SECS_PER_DAY 86400UL

// Función interna para obtener la estructura tm de una variable time_t, usando gmtime_r para que NO tenga en cuenta TZ
static inline struct tm getTimeStruct(time_t t) {
  struct tm tm_struct;
  gmtime_r(&t, &tm_struct);
  LOG_DEBUG("estructura devuelta:", asctime(&tm_struct));
  return tm_struct;
}

// static inline int getDay(time_t t)    { return getTimeStruct(t).tm_mday; }
// static inline int getMonth(time_t t)  { return getTimeStruct(t).tm_mon + 1; }
// static inline int getHour(time_t t)   { return getTimeStruct(t).tm_hour; }
// static inline int getMinute(time_t t) { return getTimeStruct(t).tm_min; }

#endif  // control_h
