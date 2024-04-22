
#include "Control.h"

//devuelve posicion del selector de multirriego
uint16_t getMultiStatus()
{
  if (Boton[bID_bIndex(bGRUPO1)].estado) return bGRUPO1;
  if (Boton[bID_bIndex(bGRUPO3)].estado) return bGRUPO3;
  return bGRUPO2  ;
}

//asigna en multi valores o apuntadores de/a config del grupo cuyo id se recibe
// y devuelve el numero del grupo (1,2,3) , 0 en caso de que no exista
int setMultibyId(uint16_t id, Config_parm &cfg)
{
  LOG_TRACE("setMultibyId - recibe id= 0x",DebugLogBase::HEX,id);
  //log_i("recibe id=x%x", id);

  for(int i=0; i<NUMGRUPOS; i++)
  {
    if(cfg.groupConfig[i].id == id) {
      multi.id = &cfg.groupConfig[i].id;
      multi.size = &cfg.groupConfig[i].size;
      multi.zserie = &cfg.groupConfig[i].serie[0];
      multi.desc = cfg.groupConfig[i].desc;
      for (int j=0; j < *multi.size; j++) {
        multi.serie[j] = ZONAS[cfg.groupConfig[i].serie[j]-1];  //obtiene el id del boton de cada zona (ojo: no viene en el json)
        #ifdef EXTRADEBUG 
          Serial.printf("  Zona%d   id: 0x", cfg.groupConfig[i].serie[j]);
          Serial.println(Boton[cfg.groupConfig[i].serie[j]-1].id,HEX); //id(boton) asociado a la zona
        #endif  
      }
      return i+1;
    }
  }
  LOG_ERROR(" ** [ERROR] setMultibyID devuelve -not found-");
  return 0;
}

void displayGrupo(uint16_t *serie, int serieSize)
{
  led(Boton[bID_bIndex(*multi.id)].led,ON);
  int i;
  for(i=0;i<serieSize;i++) {
    //Serial.printf("[displayGrupo]  encendiendo led %d \n", bID_bIndex(serie[i])+1);
    led(Boton[bID_bIndex(serie[i])].led,ON);
    delay(300);
    bip(i+1);
    delay(100*(i+1));
    led(Boton[bID_bIndex(serie[i])].led,OFF);
    delay(100);
  }
  led(Boton[bID_bIndex(*multi.id)].led,OFF);
}

//imprime contenido actual de la estructura multiGroup
void printMultiGroup(Config_parm &cfg, int pgrupo)
{
  for(int j = 0; j < cfg.groupConfig[pgrupo].size; j++) {
    Serial.printf("  Zona%d   id: x", cfg.groupConfig[pgrupo].serie[j]);
    Serial.println(Boton[cfg.groupConfig[pgrupo].serie[j]-1].id,HEX); //id(boton) asociado a la zona
  }
  Serial.println();
}

