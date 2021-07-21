#include <Control.h>

//metodo NUEVO asignacion botones a grupos de multirriego (valores iniciales) 

//valores y tamaño de las series por defecto 
// (ojo ver NOTA1 en Control.h --> FORCEINITEEPROM=1 para actualizarlos)
uint16_t Grupo1[16]    = {bTURBINAS, bPORCHE, bCUARTILLO};
uint16_t Grupo2[16]    = {bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA };
uint16_t Grupo3[16]    = {bTURBINAS, bPORCHE, bCUARTILLO, bGOTEOALTO, bGOTEOBAJO, bOLIVOS, bROCALLA};
int size_Grupo1 = 3;
int size_Grupo2 = 4;
int size_Grupo3 = 7;

S_MULTI multiGroup [] =  { 
    //id,        serie        size        actual     descripcion
    { bCESPED,   Grupo1,    size_Grupo1,    0,       "CESPED"},
    { bGOTEOS,   Grupo2,    size_Grupo2,    0,       "GOTEOS"},
    { bCOMPLETO, Grupo3,    size_Grupo3,    0,       "COMPLETO"}
};

//devuelve posicion del selector de multirriego
uint16_t getMultiStatus()
{
  if (Boton[bId2bIndex(bCESPED)].estado) return bCESPED;
  if (Boton[bId2bIndex(bGOTEOS)].estado) return bGOTEOS;
  return bCOMPLETO;
}

//devuelve apuntador al elemento de multiGroup correspondiente el id pasado
S_MULTI *getMultibyId(uint16_t id)
{
  int i;
  
  for(i=0;i<(sizeof(multiGroup)/sizeof(S_MULTI));i++)
  {
    if(multiGroup[i].id == id) 
      return (S_MULTI *) &multiGroup[i];
  }
  return NULL;
}

//devuelve apuntador al elemento n de multiGroup
S_MULTI *getMultibyIndex(int n)
{
  return (S_MULTI *) &multiGroup[n];
}

// muestra (enciende leds) elementos y orden de cada grupo de multirriego:
void displayGrupo(uint16_t *serie, int serieSize)
{
  int i;
  for(i=0;i<serieSize;i++) {
    led(Boton[bId2bIndex(serie[i])].led,ON);
    delay(300);
    bip(i+1);
    led(Boton[bId2bIndex(serie[i])].led,OFF);
    delay(100);
  }
}

