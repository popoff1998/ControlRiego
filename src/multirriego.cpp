#include <Control.h>

//metodo NUEVO asignacion botones a grupos de multirriego (valores iniciales) en Control.cpp


S_MULTI multiGroup [] =  { 
    //id,        serie        size        actual     descripcion
    { bCESPED,   Grupo1,    size_Grupo1,    0,       "CESPED"},
    { bGOTEOS,   Grupo2,    size_Grupo2,    0,       "GOTEOS"},
    { bCOMPLETO, Grupo3,    size_Grupo3,    0,       "COMPLETO"}
};


S_MULTI getMulti(uint16_t multistatus)
{
  int i;
  for(i=0;i<sizeof multiGroup;i++)
  {
    if(multiGroup[i].id == multistatus)
         break;
  }
  return multiGroup[i];
}

// muestra (enciende leds) elementos y orden de cada grupo de multirriego:
void displayGrupo(uint16_t *serie, int serieSize)
{
  int i;
    
  for(i=0;i<serieSize;i++) {
    led(Boton[bId2bIndex(serie[i])].led,ON);
    delay(300);
    led(Boton[bId2bIndex(serie[i])].led,OFF);
  }
}
