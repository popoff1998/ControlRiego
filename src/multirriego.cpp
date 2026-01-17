
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

// Devuelve grupo pulsado/seleccionado
int setGrupo() {
    int n_grupo;
    #ifdef GRP4
      n_grupo = setMultibyId(boton->bID);
    #endif
    #ifdef M3GRP
      n_grupo = setMultibyId(getMultiStatus());
    #endif
    if (n_grupo == 0) return 0; //error en setup de apuntadores 
    LOG_DEBUG("en MULTIRRIEGO, setMultibyId devuelve: Grupo", n_grupo,"(",multi.desc,") multi.size=" , *multi.size);
    for (int k=0; k < *multi.size; k++) LOG_DEBUG( "       multi.zserie: x" , multi.zserie[k]);
    return n_grupo;
}

// Asigna en multi valores o apuntadores de/a config del grupo cuyo bId(boton) se recibe
// y devuelve el numero del grupo (1...NUMGRUPOS+1) , 0 en caso de que no exista
int setMultibyId(uint16_t id)
{
  LOG_DEBUG("[setMultibyId] recibe id= 0x",DebugLogBase::HEX,id);
  //log_i("recibe id=x%x", id);

  for(int i=0; i<NUMGRUPOS+1; i++)
  {
    if(config.group[i].bID == id) {
      multi.id = &config.group[i].bID;
      multi.size = &config.group[i].size;
      multi.desc = config.group[i].desc;
      multi.ngrupo = i+1;
      for (int j=0; j < *multi.size; j++) {
        multi.serie[j] = ZONAS[config.group[i].zNumber[j]-1];  //obtiene el id del boton de cada zona (ojo: no viene en el json)
        multi.zserie[j] = config.group[i].zNumber[j];  //copia el numero de cada zona desde config
        #ifdef EXTRADEBUG 
          Serial.printf("  Zona%d   ", config.group[i].zNumber[j]);
          Serial.printf("bId: x%04x \n",multi.serie[j]); // bId(boton) asociado a la zona
          //Serial.println(Boton[zNumber2bIndex(config.group[i].zNumber[j])].bID,HEX); // bId(boton) asociado a la zona
        #endif  
      }
      LOG_DEBUG("[setMultibyId] devuelve GRUPO", multi.ngrupo,"(",multi.desc,") con",*multi.size,"zonas");
      return multi.ngrupo;
    }
  }
  statusError(E0); 
  LOG_ERROR(" ** [ERROR] setMultibyID devuelve -not found-");
  return 0;
}


// prepara el comienzo de un multirriego (normal o temporal)
bool setMultirriego()
{
  if(*multi.size > 0) {    // si grupo tiene zonas definidas
      multi.riegoON = true;
      multi.dynamic  = false;
      multi.actual = 0;
      multi.semaforo = true;
      LOG_INFO("MULTIRRIEGO iniciado: ", multi.desc);
      boton = &Boton[bID2bIndex(multi.serie[multi.actual])]; // simula pulsacion boton primera zona del grupo
      if (multi.temporal) ultimosRiegos(HIDE); // apaga leds zonas seleccionadas en el multirriego temporal
      else led(Boton[bID2bIndex(*multi.id)].led,ON); // enciende led del grupo pulsado si es normal
      sonido.bip(4);
      return true;
  }
  else {
      lcd.info(" >> GRUPO VACIO <<",2);  //muestra mensaje de grupo vacio
      sonido.bipKO();
      delay(config.msgdisplaymillis);
      lcd.info("",2);   //borra msg de <VACIO>
      return false;
  }      
}


void displayLedsGrupo(uint16_t *serie, int serieSize)
{
  led(Boton[bID2bIndex(*multi.id)].led,ON);
  int i;
  if(serieSize > 0) {
      for(i=0;i<serieSize;i++) {
        led(Boton[bID2bIndex(serie[i])].led,ON);
        delay(300);
        sonido.bip(i+1);
        delay(100*(i+1));
        led(Boton[bID2bIndex(serie[i])].led,OFF);
        delay(100);
      }
  }    
  led(Boton[bID2bIndex(*multi.id)].led,OFF);
}

void displayLCDGrupo(bool full, int line, int znumber)
{
  LOG_DEBUG("recibido full=",full,"line=",line);
  int posicion = 0;
  if(full) posicion = displayLCDGrupo(multi.zserie, *multi.size, line, 0);
  else {
      if( multi.actual+1 == *multi.size) lcd.info("", line);   // ultima zona por regar
       else posicion = displayLCDGrupo(multi.zserie, *multi.size, line, multi.actual+1);  //  display zonas quedan por regar
      if (znumber) {  // si hay zona salvada la mostramos con "+" a continuacion
        lcd.setCursor(posicion, 1);
        lcd.printf("+%d", znumber);
      }
  }    
  return;     
}

int displayLCDGrupo(uint16_t *serieZonas, int serieSize, int line, int start)
{
  LOG_DEBUG("recibido serieSize=",serieSize,"line=",line,"start=",start);
  int i,posicion = 0;
  if(serieSize > 0) {
      for(i=start; i<serieSize; i++) {
        if(i == serieSize-1) posicion += snprintf (&buff[posicion], MAXBUFF, "%d", serieZonas[i]);
        else posicion += snprintf (&buff[posicion], MAXBUFF, "%d-", serieZonas[i]);
        if (posicion >= LCDMAXLEN) break; // max 20 char alcanzados
      } 
      lcd.info(buff,line);
    }
    return posicion;
}

//imprime contenido actual de la estructura multiGroup
void printMultiGroup(int pgrupo)
{
  for(int j = 0; j < config.group[pgrupo].size; j++) {
    Serial.printf("  Zona%d   ", config.group[pgrupo].zNumber[j]);
    //Serial.println(Boton[zNumber2bIndex(config.group[pgrupo].zNumber[j])].bID,HEX); // bId(boton) asociado a la zona
    Serial.printf("bId: x%04x \n",ZONAS[(config.group[pgrupo].zNumber[j])-1]); // bId(boton) asociado a la zona
  }
  Serial.println();
}

