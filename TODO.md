# PENDIENTES
============
- boton Pause reflejado en Domoticz (en ambos sentidos)
- en Standby añadir a PAUSA comprobar zonas en off (si VERIFY ON)
- Si error de conexion durante el riego dar opcion de continuarlo al recuperarla ?

# HECHOS
========
## Version 1:
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
- VERIFY completo al salir de modo DEMO (NONETWORK): conexion wifi y con Domoticz, parada todos los riegos V1.4
## Version 2:
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
## Version 3:
- Nuevo HW: ESP32, expansores I/O MCP23017, pantalla LCD de 20 caracteres x 4 lineas (bus I2C)
- Cambio libreria encoder por una que soporta interrupciones
- Opciones de inicio por pantalla (borrar wifi o cargar parametros defecto) V3.1
- Ampliacion a 9 zonas / 4 grupos V3.1
- Leds de RED y de WIFI fusionados en led STATUS RGB V3.1
- Muestra dia/mes al mostrar hora actual. Sincronizacion del time por NTP periodicamente V3.1
- Modo configuracion por menu en pantalla V3.1
- Pseudogrupo riego temporal V3.1
- Refactoring  de procesaEstadoConfigurando y clase Configure  V3.1
- Mejoras información en pantalla (DEMO, zonas pendientes riego, temperatura ambiente, timestamp riegos) V3.1
- Simplificacion fichero de configuracion V3.1
- Configurando grupo, enc+pause vacia grupo (anulado en V3.3) V3.1
- Nuevo formato mandato comunicacion con Domoticz v3.1
- Mejoras información en pantalla (*Mtemp, -NF-) V3.2
- Si parametro dynamic=true se permite añadido/baja zonas durante el riego en pausa (riego de zona o mtemp) V3.2
- Menu configuracion: rangos ajustables sobre linea del menu V3.2
- Opciones de inicio por pantalla: reset parametros (borrado ficheros parm y backup) V3.2
- Volumen sonidos y melodia fin riego de grupo configurables por parametros en menu V3.2
- Si error de conexión en el arranque (wifi o Domoticz) se reintenta recuperarla periodicamente V3.2
- Webserver: gestion del fichero de parámetros (ver, descargar, crear, editar, etc) V3.2
- Webserver: menú de mantenimiento avanzado (Sysinfo,Files,Upload files, OTA update) V3.2
- Time, timezone, NTP con funciones nativas de C (ctime) y ESP32. V3.2
- Timezone configurable desde webserver (fichero de parametros) y desde modo AP (menu Setup) V3.2
- En info de zona y grupo: minutos de tiempo de riego real (excluidas pausas) V3.2
- Opcion configurable lastr24 muestra zonas regadas últimas 24h V3.2
- Las tablas de ultimos riegos de zonas y grupos se salvan y no se pierden con reset o apagado V3.3
- Puede continuarse riego primera zona cancelada de un multirriego al final de este (queda en PAUSA) V3.3
- Si riego en pausa: ENC+PAUSA lo cancela también V3.3
- En modo DEMO no se consolidan tiempos de riego (se recuperan los reales al salir) V3.3
- Atajo: STOP+ENC+GRUPO1 activa Webserver V3.3
- Refactorizado general para mejorar la legibilidad y el mantenimiento V3.3
- Modulo ComDomoticz.cpp para encapsular comunicacion con el SCD Domoticz V3.3
- Webserver: paginas con ultimos riegos y logs de riegos del Domoticz, mejoras de validacion y visuales V3.3
- En modo configuracion pueden usarse tanto PAUSE como el boton del encoder para seleccionar/validar V3.3
- Centralizacion de la mayoria de la UI (display, leds, sonidos) en setEstado y statusError V3.3
- Log de errores a fichero consultable desde el webserver V3.3
- Lectura sensor temperatura remoto cambia a local tras 15 min sin lecturas V3.3
- Si error en parámetros (E0) Stop activa webserver V3.3
- Si parametro dynamic=true tambien se permite modificar multiriego de grupo pasandolo a temporal V3.3
- Configurando grupo temporal: un segundo PAUSE en lugar de liberar STOP reinicia el proceso de definirlo V3.3
- En modo AP el portal de configuración wifi no se cierra pasado el timeout si hay un cliente conectado. V3.3 
- Menu de portal AP: Parametros pasan a pagina de Configuración Wifi y campos user/pw SCD V3.3
- Identificacion (opcional) con user/pw al SCD (Domoticz) usando httpclient.setAuthorization V3.3
- Fecha de ultimo riego: muestra tiempo pasado desde que se produjo si no hay fecha valida (no se salva) V3.3
- Sustitucion de libreria Timelib.h por funciones locales V3.3
- Nueva clase Encoder optimizada y corregidos los bugs de AiEspRotaryEncoder V3.3
- Cabecera del fichero de parametros y su tratamiento en el FW y webserver V3.3
- Webserver: mejoras visuales, de estilos y verificaciones. Reduccion tamaños ficheros V3.3
- Webserver: tabla de ficheros ordenable por nombre, tamaño o fecha V3.3
- Log del Domoticz consultable desde el webserver V3.3
- Webserver: opcion de seleccionar una carpeta en el PC para subir todos sus ficheros V3.3
- Riego diferido: se puede ajustar el comienzo del riego de una zona o grupo despues de un tiempo hh:mm V3.3


