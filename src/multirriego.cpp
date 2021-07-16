#include <Control.h>

//metodo NUEVO asignacion botones a grupos de multirriego (valores iniciales) en Control.cpp

//valores de las series por defecto (ojo ver NOTA1 en Control.h --> FORCEINITEEPROM=1 para actualizarlos)
uint16_t Grupo1[]    = {bTURBINAS, bPORCHE, bCUARTILLO};
uint16_t Grupo2[]    = {bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA };
uint16_t Grupo3[]    = {bTURBINAS, bPORCHE, bCUARTILLO, bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA};

S_MULTI multiGroup [] =  { 
    //id,        serie   size   actual     descripcion
    { bCESPED,   Grupo1,    3,    0,       "CESPED"},
    { bGOTEOS,   Grupo2,    4,    0,       "GOTEOS"},
    { bCOMPLETO, Grupo3,    7,    0,       "COMPLETO"}
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
