Clase Encoder, basada en https://github.com/igorantolic/ai-esp32-rotary-encoder

Codigo refactorizado para optimizarlo y eliminar bugs manteniendo las apis externas, salvo las de soporte del 
boton del encoder que se ha eliminado :

- Desacoplar la ISR del resto de lógica:
    Hace una sola cosa: leer los pines lo más rápido posible, actualizar un contador bruto de fases y salir.
- Optimización del Filtrado de Fases:
    Mueve la división por N pasos (encoderSteps) fuera de la ISR. 
    La interrupción ahora solo acumula transiciones crudas de la máquina de estados, 
    delegando la división (una sola vez) al hilo principal de forma asíncrona solo cuando se consulta el valor.
- Corregir la Lógica de Límites y Circularidad:
    Realiza todo el control de límites y circularidad utilizando los valores finales procesados (pasos completos), 
    no sobre los ticks internos crudos antes de dividir.
- Modularizar el Cálculo de Aceleración:
    Cambia el cálculo de tiempo a micros() y calcula el bono a sumar a los pasos.
- Realizar una prelectura de los pines físicos del encoder justo en el momento del arranque:
    Se evita que se ignore el primer tic del encoder tras un reset
- Todas las funciones anteriores (salvo la ISR) se realizan en syncAndProcessMovement() cuando el programa principal
  llama a readEncoder() o al anterior via encoderChanged()  

  Optimizacion y bugs resueltos en la versión final:

    Respuesta inmediata tras reset: la prelectura de pines evita que se ignore el primer paso.

    Inmunidad al jitter intermedio: El acumulador local residualTicks absorbe las oscilaciones sin corromper el hardware.

    Eliminado bug de inversion de sentido: El filtro de inversion limpia los ticks pendientes

    Eliminado bug de paso por cero a rango negativo: al trabajar sobre pasos y no dividir por numero de ticks

    Sin bloqueos en los extremos (Muro): Al limpiar el residuo con el filtro de inversión, 
    el cambio de marcha responde al primer clic independientemente de cuánto se haya seguido moviendo contra los límites.

    Eficiencia de CPU: La rutina de interrupción realiza el minimo proceso. Dejando la logica a syncAndProcessMovement.                  