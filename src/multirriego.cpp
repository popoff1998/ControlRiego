
#include "Control.h"

#ifdef M3GRP
  // devuelve posicion del selector de multirriego
  uint16_t getMultiStatus()
  {
    if (Boton[bID2bIndex(bGRUPO1)].estado) return bGRUPO1;
    if (Boton[bID2bIndex(bGRUPO2)].estado) return bGRUPO2;
    if (Boton[bID2bIndex(bGRUPO3)].estado) return bGRUPO3;
    return bGRUPO2  ;
  }
#endif

//asigna en multi valores o apuntadores de/a config del grupo cuyo id se recibe
// y devuelve el numero del grupo (1,2,3) , 0 en caso de que no exista
int setMultibyId(uint16_t id, Config_parm &config)
{
  LOG_DEBUG("[setMultibyId] recibe id= 0x",DebugLogBase::HEX,id);
  //log_i("recibe id=x%x", id);

  for(int i=0; i<NUMGRUPOS+1; i++)
  {
    if(config.group[i].bID == id) {
      multi.id = &config.group[i].bID;
      multi.size = &config.group[i].size;
      multi.desc = config.group[i].desc;
      for (int j=0; j < *multi.size; j++) {
        multi.serie[j] = ZONAS[config.group[i].zNumber[j]-1];  //obtiene el id del boton de cada zona (ojo: no viene en el json)
        multi.zserie[j] = config.group[i].zNumber[j];  //obtiene el numero de cada zona
        #ifdef EXTRADEBUG 
          Serial.printf("  Zona%d   bId: 0x", config.group[i].zNumber[j]);
          Serial.println(Boton[config.group[i].zNumber[j]-1].bID,HEX); //bId(boton) asociado a la zona
        #endif  
      }
      LOG_DEBUG("[setMultibyId] devuelve GRUPO", i+1);
      return i+1;
    }
  }
  LOG_ERROR(" ** [ERROR] setMultibyID devuelve -not found-");
  statusError(E0); 
  return 0;
}

void displayGrupo(uint16_t *serie, int serieSize)
{
  led(Boton[bID2bIndex(*multi.id)].led,ON);
  int i;
  for(i=0;i<serieSize;i++) {
    led(Boton[bID2bIndex(serie[i])].led,ON);
    delay(300);
    bip(i+1);
    delay(100*(i+1));
    led(Boton[bID2bIndex(serie[i])].led,OFF);
    delay(100);
  }
  led(Boton[bID2bIndex(*multi.id)].led,OFF);
}

void displayLCDGrupo(uint16_t *serieZonas, int serieSize, int line)
{
  LOG_DEBUG("recibido serieSize=",serieSize,"line=",line);
  int i,n = 0;
  for(i=0;i<serieSize;i++) {
    n += snprintf (&buff[n], MAXBUFF, "%d-", serieZonas[i]);
    if (n >= LCDMAXLEN) break; // max 20 char alcanzados
  }  
  lcd.info(buff,line);
}

//imprime contenido actual de la estructura multiGroup
void printMultiGroup(Config_parm &config, int pgrupo)
{
  for(int j = 0; j < config.group[pgrupo].size; j++) {
    Serial.printf("  Zona%d   bId: x", config.group[pgrupo].zNumber[j]);
    Serial.println(Boton[config.group[pgrupo].zNumber[j]-1].bID,HEX); // bId(boton) asociado a la zona
  }
  Serial.println();
}

