PENDIENTES
==========

- posibilidad de salir del modo ERROR a NONETWORK y a modo NORMAL. En modo NORMAL se deben de producir todas las inicializaciones del setup que hayan podido producir error. 
- si un contenido incorrecto de la eeprom ha generado un error debemos habilitar un mecanismo para poder entrar en configuracion en cualquier caso.

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



