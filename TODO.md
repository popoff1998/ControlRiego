PENDIENTES
==========

- parametros de conexion a Domoticz por interfax web
- boton Pause reflejado en Domoticz (en ambos sentidos)
- Si estado REGANDO tiempo de riego restante reflejado en Domoticz

HECHOS
======
- el modo CONFIGURACION debe permitir definir los botones que pertenecen a un grupo de riegos, incluido su orden (en modo ConF pulsar multirriego). V1.2
- Paso de modo NORMAL a modo NONETWORK y viceversa para pruebas o demo (encoderSW + PAUSA). V1.2
- encoderSW + boton de riego -> muestra factor de riego asociado a ese boton. V1.2
- SW restart (STOP + encoderSW + mantener PAUSA). Si se mantiene pulsado encoderSW en inicializacion:
  selector multi arriba -> escritura de la eeprom con valores por defecto.
  selector multi abajo  -> borrado red wifi almacenada. V1.3 
- Si no se ha podido conectar a la wifi entra en modo AP para permitir configuracion por portalweb. Monitorizacion periodica conexion wifi. V1.3
- la puesta en hora por NTP si no se ha hecho correctamente se reintenta periodicamente sin bloqueo del sistema. V1.3
- hay que respetar factor riego=0 cuando se lee (para que se salte ese boton) V1.3
- se resalta el error en caso de no poder parar un riego comenzado (LEDR parpadea y bips periodicos) V1.3
- en modo DEBUG activacion simulacion errores por serial input. V1.3
- en estado ERROR pulsando Stop se reinicia V1.3.1



