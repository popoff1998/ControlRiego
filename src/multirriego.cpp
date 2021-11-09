
#include "Control.h"

//devuelve posicion del selector de multirriego
uint16_t getMultiStatus()
{
  if (Boton[bId2bIndex(bGRUPO1)].estado) return bGRUPO1;
  if (Boton[bId2bIndex(bGRUPO3)].estado) return bGRUPO3;
  return bGRUPO2  ;
}

//asigna en multi valores o apuntadores en/a config del grupo cuyo id se recibe
// y devuelve el numero del grupo (1,2,3) , 0 en caso de que no exista
int getMultibyId(uint16_t id, Config_parm &cfg)
{
  #ifdef TRACE
    Serial.printf("TRACE: in getMultibyId - recibe id=x%x \n", id);
  #endif
  for(int i=0; i<NUMGRUPOS; i++)
  {
    if(cfg.groupConfig[i].id == id) {
      multi.id = &cfg.groupConfig[i].id;
      multi.size = &cfg.groupConfig[i].size;
      multi.zserie = &cfg.groupConfig[i].serie[0];
      Serial.printf("  multi.size = %d \n", *multi.size);
      for (int j=0; j < *multi.size; j++) {
        multi.serie[j] = ZONAS[cfg.groupConfig[i].serie[j]-1];  //obtiene el id del boton de cada zona (ojo: no viene en el json)
        #ifdef DEBUG 
          Serial.printf("  Zona%d   id: x", cfg.groupConfig[i].serie[j]);
          Serial.println(Boton[cfg.groupConfig[i].serie[j]-1].id,HEX); //id(boton) asociado a la zona
        #endif  
      }
      multi.desc = cfg.groupConfig[i].desc;
      return i+1;
    }
  }
  Serial.println("[ERROR] getMultibyID devuelve -not found-");
  return 0;
}

void displayGrupo(uint16_t *serie, int serieSize)
{
  int i;
  for(i=0;i<serieSize;i++) {
    Serial.printf("[displayGrupo]  encendiendo led %d \n", bId2bIndex(serie[i])+1);
    led(Boton[bId2bIndex(serie[i])].led,ON);
    delay(300);
    bip(i+1);
    led(Boton[bId2bIndex(serie[i])].led,OFF);
    delay(100);
  }
}
//imprime contenido actual de la estructura multiGroup
void printMultiGroup(Config_parm &cfg)
{
  for(int i = 0; i < cfg.n_Grupos; i++) {
    Serial.printf("Grupo%d: size=%d (%s)\n", i+1, cfg.groupConfig[i].size, cfg.groupConfig[i].desc);
    for(int j = 0; j < cfg.groupConfig[i].size; j++) {
      Serial.printf("  Zona%d   id: x", cfg.groupConfig[i].serie[j]);
      Serial.println(Boton[cfg.groupConfig[i].serie[j]-1].id,HEX); //id(boton) asociado a la zona
    }
  }
  Serial.println();
}

