PENDIENTES
==========

- posibilidad de salir del modo ERROR a NONETWORK y a modo NORMAL. En modo NORMAL se deben de producir todas las inicializaciones del setup que hayan podido producir error.
- si un contenido incorrecto de la eeprom ha generado un error debemos habilitar un mecanismo para poder entrar en configuracion en cualquier caso.
- hay que respetar factor riego=0 cuando se lee (para que se salte ese boton)
- la puesta en hora por NTP si no se ha hecho correctamente debe de reintentarse periodicamente sin bloqueo del sistema.
- ver posibilidad (si no se ha podido conectar a la wifi) de entrar en modo AP para permitir configuracion por http.

HECHOS
======
- el modo CONFIGURACION debe permitir definir los botones que pertenecen a un grupo de riegos, incluido su orden.
