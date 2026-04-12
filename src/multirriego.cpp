
#include "Control.h"

#ifdef M3GRP
  // devuelve posicion del selector de multirriego
  uint16_t getMultiStatus()
  {
    if (Boton[getBotonIndex(bGRUPO1)].estado) return bGRUPO1;
    if (Boton[getBotonIndex(bGRUPO2)].estado) return bGRUPO2;
    if (Boton[getBotonIndex(bGRUPO3)].estado) return bGRUPO3;
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
    LOG_DEBUG("en MULTIRRIEGO, setMultibyId devuelve: Grupo", n_grupo,"(",multi.desc,") multi.size=" , *multi.size);
    for (int k=0; k < *multi.size; k++) LOG_DEBUG( "       multi.w_zserie: x" , multi.w_zserie[k]);
    return n_grupo;
}

// Asigna en multi valores o apuntadores de/a config del grupo cuyo bId(boton) se recibe
// y devuelve el numero del grupo (1...NUMGRUPOS+1) , 0 en caso de que no exista
int setMultibyId(uint16_t id)
{
  LOG_DEBUG("[setMultibyId] recibe id= 0x",DebugLogBase::HEX,id);

  for(int i=0; i<NUMGRUPOS+1; i++)
  {
    if(Grupos[i] == id) {
      multi.id = &Grupos[i];
      multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
      multi.size = &config.group[i].size;
      multi.desc = config.group[i].desc;
      multi.ngrupo = i+1;
      for (int j=0; j < *multi.size; j++) {
        multi.zserie_boton[j] = Zonas[config.group[i].zNumber[j]-1];  //obtiene el id del boton de cada zona (ojo: no viene en el json)
        multi.w_zserie[j] = config.group[i].zNumber[j];  //copia el numero de cada zona desde config
        #ifdef EXTRADEBUG2 
          Serial.printf("  Zona%d   ", config.group[i].zNumber[j]);
          Serial.printf("bId: x%04x \n",multi.zserie_boton[j]); // bId(boton) asociado a la zona
        #endif  
      }
      LOG_DEBUG(" devuelve GRUPO", multi.ngrupo,"(",multi.desc,") con",*multi.size,"zonas");
      return multi.ngrupo;
    }
  }
  char msg[64];
  snprintf(msg, sizeof(msg), "!!! BUG: bID %04X no encontrado en Grupos[]", id);
  stopHW(msg); // El sistema se detiene aquí
  return -1;   // Nunca se alcanzará
}

// Asigna en multi valores o apuntadores para multirriego temporal
void setMultiTemp(bool newTemp)
{
    if (!newTemp) led(Boton[getBotonIndex(*multi.id)].led,OFF); // apagamos led del grupo si venimos de un grupo normal
    multi.id = nullptr; // no hay botón asociado al grupo temporal
    int sizeInicial = (newTemp) ? 0 : *multi.size; // 0 o el tamaño del grupo en curso
    multi.w_size = sizeInicial ; // inicializamos contador temporal elementos del grupo
    multi.size = &multi.w_size; // el tamaño del grupo temporal es el de multi.w_size
    multi.temporal = true;
    multi.desc = "TEMPORAL";
    multi.ngrupo = 0; // el numero de grupo temporal es 0 (no existe en config)
    LOG_DEBUG(" devuelve GRUPO", multi.ngrupo,"(",multi.desc,") con",*multi.size,"zonas");
    displayTipoGrupo(); // actualiza LCD con tipo de grupo
}


// prepara el comienzo de un multirriego (normal o temporal)
bool startMultirriego()
{
  if(*multi.size > 0) {    // si grupo tiene zonas definidas
      multi.riegoON = true;
      multi.noFactorizado  = false;
      multi.actual = 0;
      multi.semaforo = true;
      LOG_INFO("MULTIRRIEGO iniciado: ", multi.desc);
      boton = &Boton[getBotonIndex(multi.zserie_boton[multi.actual])]; // simula pulsacion boton primera zona del grupo
      if (multi.temporal) ultimosRiegos(HIDE); // apaga leds zonas seleccionadas en el multirriego temporal
      else led(Boton[getBotonIndex(*multi.id)].led,ON); // enciende led del grupo pulsado si es normal
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

// muestra en leds y LCD las zonas del grupo
void displayLedsGrupo(uint16_t *serie, int serieSize)
{
  led(Boton[getBotonIndex(*multi.id)].led,ON); // enciende led del grupo
  int i;
  if(serieSize > 0) {  // si el grupo tiene zonas definidas muestra leds de las zonas del grupo
      for(i=0;i<serieSize;i++) {
        led(Boton[getBotonIndex(serie[i])].led,ON);
        delay(300);
        sonido.bip(i+1);
        delay(100*(i+1));
        led(Boton[getBotonIndex(serie[i])].led,OFF);
        delay(100);
      }
  }    
  led(Boton[getBotonIndex(*multi.id)].led,OFF); // apaga led del grupo
}


/**
 * Muestra las zonas del grupo apuntado por la estructura multi en el LCD con formato "1-2-3+9"
 * @param modo    FULL    - muestra todas las zonas del grupo (multi.w_zserie)
 *                RESTO   - muestra las zonas restantes por regar (a partir de multi.actual+1)
 *                WORKING - muestra las zonas añadidas mientras se configura un grupo (a partir de multi.w_size)
 * @param line    fila del LCD (1-4).
 * @param znumber zona extra opcional (se muestra con +).
 */
int displayLCDGrupo(display_modo modo, int line, int znumber) {
    LOG_DEBUG("recibido modo=",modo,"line=",line,"znumber=",znumber);
    int pos = 0;
    buff[0] = '\0'; // vaciar buffer antes de usarlo (caso de RESTO con todo regado o WORKING sin zonas añadidas)
    int size, inicio;
    if (modo == WORKING) {
        size = multi.w_size; // Usamos el contador de configuración
        inicio = 0;          // Siempre desde el principio
    } else {
        size = *multi.size;   // Usamos el tamaño del grupo de riego
        inicio = (modo == RESTO) ? (multi.actual + 1) : 0;
    }
    size = min(size, ZONASXGRUPO); // Aseguramos no exceder el máximo definido (por si acaso)
    // Construye la cadena con las zonas a mostrar
    for (int i = inicio; i < size; i++) {
        if (pos > (LCDMAXLEN - 2)) break; // no hay espacio para mostrar más zonas
        if (pos > 0) pos += snprintf(&buff[pos], MAXBUFF - pos, "-");
        pos += snprintf(&buff[pos], MAXBUFF - pos, "%u", multi.w_zserie[i]);
    }
    // Añade zona extra si se ha pasado (+znumber)
    if (znumber > 0 && pos <= (LCDMAXLEN - 2)) pos += snprintf(&buff[pos], MAXBUFF - pos, "+%d", znumber);
    if (pos == 0 && modo == FULL) pos = snprintf(buff, MAXBUFF, " >> GRUPO VACIO <<");
    lcd.info(buff, line); // Muestra en LCD
    return pos;
}

//imprime contenido actual de la estructura multiGroup
void printMultiGroup(int pgrupo)
{
  for(int j = 0; j < config.group[pgrupo].size; j++) {
    Serial.printf("  Zona%d   ", config.group[pgrupo].zNumber[j]);
    Serial.printf("bId: x%04x \n",Zonas[(config.group[pgrupo].zNumber[j])-1]); // bId(boton) asociado a la zona
  }
  Serial.println();
}

