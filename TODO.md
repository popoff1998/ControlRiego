PENDIENTES
==========
- Boton Pause reflejado en Domoticz (en ambos sentidos)
- Si estado REGANDO tiempo de riego restante reflejado en Domoticz
- Migrar a JSON V7 y mas parametros configurables
- pseudogrupo riego temporal

HECHOS
======
- el modo CONFIGURACION debe permitir definir los botones que pertenecen a un grupo de riegos, incluido su orden (en modo ConF pulsar multirriego). V1.2
- Paso de modo NORMAL a modo NONETWORK y viceversa para pruebas o demo (encoderSW + PAUSA). V1.2
- encoderSW + boton de riego -> muestra factor de riego asociado a ese boton. V1.2
- SW restart (STOP + encoderSW + mantener PAUSA). Si se mantiene pulsado encoderSW en inicializacion:
    selector multi arriba -> escritura de la eeprom con valores por defecto.
    selector multi abajo  -> borrado red wifi almacenada. V1.3 
- Si no se ha podido conectar a la wifi entra en modo AP para permitir configuracion por portalweb.  
- Monitorizacion periodica conexion wifi. V1.3
- la puesta en hora por NTP si no se ha hecho correctamente se reintenta periodicamente sin
  bloqueo del sistema. V1.3
- Respeta factor riego=0 cuando se lee (para que se salte ese boton) V1.3
- se resalta el error en caso de no poder parar un riego comenzado (LEDR parpadea y bips
  periodicos) V1.3
- en modo DEBUG activacion simulacion errores por serial input. V1.3
- en estado ERROR pulsando Stop se reinicia V1.3.1
- reconexion si no wifi en arranque (corte corriente) V1.3.2
- parametros de conexion a Domoticz por interfax web (incluye OTA) V1.3.3
- actualiza factor riegos al pasar de NONETWORK a NORMAL (encoderSW+PAUSE) V1.3.4
- si falloAP reintenta conexion max 10 seg. V1.3.5
- bloqueo seguridad botones (STOP en Stanby) pasa a reposo tras 4 * STANDBYSECS V1.3.7
- permite configurar boton con IDX=0 para desactivarlo V1.4
- debug trace si Exception en modo DEVELOP (platformio.ini) V1.4
- no modifica parámetros de conexión (ip domoticz) el restaurar valores por defecto V1.4
- VERIFY completo al salir de modo DEMO (NONETWORK): conexion wifi y con Domoticz, parada todos los riegos) V1.4
**Version 2:**
- fichero parámetros en lugar de variables del pgm y eeprom (parmConfig.json) V2.0
- si error, parpadeo led zona que falla V2.0
- en modo configuración salvado parametros como default ((ConF + encoderSW +
  multi arriba + boton multirriego)) V2.1
- en modo configuración poder activar portal AP para configurar parámetros conexión (ConF + encoderSW +
  multi abajo + boton multirriego) V2.2
- si falloAP y existe red wifi almacenada reintenta conexion max 20 seg. V2.3
- mejoras display de informacion V2.3
- HTTPUpdateServer para actualizacion via OTA de FW y/o Filesystem V2.4
- encoderSw + PAUSE cancela riego zona en curso y pasa a la siguiente si multirriego V2.4
- menú de servicio: páginas $sysinfo, $list, $parm y $def V2.4 (experimental)
- si error al salir del Pause, permanece en esta y señala zona que falla V2.5
- verifica periódicamente si el riego en curso esta activo o en pause en Domoticz y lo refleja V2.5
- si al lanzar o detener un riego Domoticz informa de error, se reintenta varias veces antes de dar error V2.5
- mejoras en menu de servicio (webserver): posibilida de ver, borrar y actualizar ficheros individuales del file system V2.5
**Version 3:**
- adaptacion para ESP32 y expansores I/O MCP23017
- cambio libreria encoder por una que soporta interrupciones
- sustitucion display led de 7 segmentos por pantalla LCD de 20 caracteres x 4 lineas (bus I2C)
- opciones de inicio por pantalla V3.0.1
- ampliacion a 9 zonas / 4 grupos V3.0.2 (botones PAUSE y STOP conectados a mcpO V3.0.2a)
- opcion de compilacion para 4 botones de grupos o selector 3 grupos con boton de multirriego V3.0.3
- modificaciones en los indicadores leds RGB V3.0.3
- muestra dia/mes al mostrar hora actual. Sincronizacion del time por NTP cada 10 horas V3.0.3
- Opciones en modo configuracion por menu en pantalla, opciones MUTE V3.0.4
- pasados procesos de procesaEstadoConfigurando como funciones clase Configure en configure.cpp V3.0.5

