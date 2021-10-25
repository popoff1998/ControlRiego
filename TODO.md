PENDIENTES
==========
- boton Pause reflejado en Domoticz (en ambos sentidos)
- Si estado REGANDO tiempo de riego restante reflejado en Domoticz
- fichero parámetros por defecto en lugar de variables del pgm
- en modo configuración poder activar portal AP para configurar parámetros conexión (ConF + encoderSW + PAUSA)

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
- la puesta en hora por NTP si no se ha hecho correctamente se reintenta periodicamente sin bloqueo del sistema. V1.3
- Respeta factor riego=0 cuando se lee (para que se salte ese boton) V1.3
- se resalta el error en caso de no poder parar un riego comenzado (LEDR parpadea y bips periodicos) V1.3
- en modo DEBUG activacion simulacion errores por serial input. V1.3
- en estado ERROR pulsando Stop se reinicia V1.3.1
- reconexion si no wifi en arranque (corte corriente) V1.3.2
- parametros de conexion a Domoticz por interfax web (incluye OTA) V1.3.3
- actualiza factor riegos al pasar de NONETWORK a NORMAL (encoderSW+PAUSE) V1.3.4
- si falloAP reintenta conexion max 10 seg. V1.3.5
- bloqueo seguridad botones (STOP en Stanby) pasa a reposo tras 4 * STANDBYSECS V1.3.7



