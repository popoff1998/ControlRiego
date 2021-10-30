
#include "Control.h"

//valores y tamaño de las series por defecto 
// (ojo ver NOTA1 en Control.h --> FORCEINITEEPROM=1 para actualizarlos)
   
uint16_t Grupo1[16]    = { bZONA1, bZONA2, bZONA3 };
uint16_t Grupo2[16]    = { bZONA1, bZONA2, bZONA3, bZONA4, bZONA5, bZONA6, bZONA7 };
uint16_t Grupo3[16]    = { bZONA4, bZONA5, bZONA6, bZONA7 };
int size_Grupo1 = 3;
int size_Grupo2 = 7;
int size_Grupo3 = 4;

S_MULTI multiGroup [] =  { 
      //id,        serie        size        actual     descripcion
      { bGRUPO1,   Grupo1,    size_Grupo1,    0,       "CESPED"},
      { bGRUPO2  , Grupo2,    size_Grupo2,    0,       "COMPLETO"},
      { bGRUPO3,   Grupo3,    size_Grupo3,    0,       "GOTEOS"}
};

//devuelve posicion del selector de multirriego
uint16_t getMultiStatus()
{
  if (Boton[bId2bIndex(bGRUPO1)].estado) return bGRUPO1;
  if (Boton[bId2bIndex(bGRUPO3)].estado) return bGRUPO3;
  return bGRUPO2  ;
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

//imprime contenido actual de la estructura multiGroup
void printMultiGroup()
{
  S_MULTI *multi;
  for (int j=0; j<n_Grupos; j++) {
    multi = getMultibyIndex(j);
    Serial.printf("\n Grupo%d : %d elementos \n", j+1, multi->size);
    for (int i=0;i < multi->size; i++) {
      Serial.printf("     elemento %d: x",i+1);
      Serial.println(multi->serie[i],HEX);
    }  
  }  
  Serial.println()
  ;
}

int nGrupos()
{
  return sizeof(multiGroup)/sizeof(S_MULTI);
}
