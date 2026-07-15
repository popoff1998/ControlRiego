
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

// Devuelve grupo pulsado/seleccionado y lo apunta en multi.
int setGrupo()
{
  LOG_DEBUG("Recibe id=0x",DebugLogBase::HEX,boton->bID);
  int i = getGroupIndex(boton->bID); // busca el id del boton en Grupos[] y devuelve su indice (grupo-1)
  multi.id = boton; // guarda el apuntador a Boton[] del grupo pulsado/seleccionado
  multi.w_size = 0 ; // inicializamos contador temporal elementos del grupo
  multi.size = &config.group[i].size;
  multi.desc = config.group[i].desc;
  multi.ngrupo = i+1;
  // para cada zona del grupo: obtiene su apuntador en Boton[]
  for (int j=0; j < *multi.size; j++) {
    // Obtenemos el bID numérico de la zona
    uint16_t zonaBid = Zonas[config.group[i].zNumber[j] - 1];
    // Buscamos su índice en Boton[] y guardamos el puntero directo
    multi.zserie_pBoton[j] = getBotonPointer(zonaBid);    
    multi.w_zserie[j] = config.group[i].zNumber[j];  //copia el numero de cada zona desde config
  }
  #ifdef EXTRADEBUGMULTI
    printMulti();
  #endif
  LOG_DEBUG("Devuelve GRUPO", multi.ngrupo,"(",multi.desc,") con",*multi.size,"zonas");
  return multi.ngrupo;
}

// Asigna en multi valores o apuntadores para multirriego temporal
void setMultiTemp(bool newTemp)
{
    if (!newTemp) led(multi.id->led, OFF); // apagamos led del grupo si venimos de un grupo normal
    multi.id = nullptr; // no hay botón asociado al grupo temporal
    int sizeInicial = (newTemp) ? 0 : *multi.size; // 0 o el tamaño del grupo en curso
    multi.w_size = sizeInicial ; // inicializamos contador temporal elementos del grupo
    multi.size = &multi.w_size; // el tamaño del grupo temporal es el de multi.w_size
    multi.temporal = true;
    multi.desc = "TEMPORAL";
    multi.ngrupo = 0; // el numero de grupo temporal es 0 (no existe en config)
    LOG_DEBUG(" devuelve GRUPO", multi.ngrupo,"(",multi.desc,") con",*multi.size,"zonas");
    if (Estado.estado == PAUSE) displayTipoGrupo(); // en cambio dinamico actualiza LCD con nuevo tipo de grupo
}


// prepara el comienzo de un multirriego (normal o temporal)
bool startMultirriego()
{
  if (multi.size == nullptr) return false; // multi no ha sido inicializado
  if (*multi.size > 0) {    // si grupo tiene zonas definidas
      multi.riegoON = true;
      multi.noFactorizado  = false;
      multi.actualIndex = 0;
      LOG_INFO("MULTIRRIEGO iniciado: ", multi.desc);
      boton = multi.zserie_pBoton[multi.actualIndex]; // simula pulsacion boton primera zona del grupo
      Estado.botonSemaforo = true; // y lo indica para no leer botones
      if (multi.temporal) ultimosRiegos(HIDE); // apaga leds zonas seleccionadas en el multirriego temporal
      else led(multi.id->led, ON); // enciende led del grupo pulsado si es normal
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

// muestra en los leds el boton del grupo y las zonas del grupo apuntado por la estructura multi
void displayLedsGrupo()
{
  if (multi.size == nullptr) return; // multi no ha sido inicializado
  led(multi.id->led, ON); // enciende led del grupo
  if (*multi.size > 0) {  // si el grupo tiene zonas definidas muestra leds de las zonas del grupo
      for (int i = 0; i < *multi.size; i++) {
          led(multi.zserie_pBoton[i]->led, ON);
          delay(300);
          sonido.bip(i + 1);
          delay(100 * (i + 1));
          led(multi.zserie_pBoton[i]->led, OFF);
          delay(100);
      }
  }    
  led(multi.id->led, OFF); // apaga led del grupo
}

/**
 * Muestra las zonas del grupo apuntado por la estructura multi en el LCD con formato "1-2-3+9"
 * @param modo    FULL    - muestra todas las zonas del grupo (multi.w_zserie) o GRUPO VACIO si no tiene zonas
 *                RESTO   - muestra las zonas restantes por regar (a partir de multi.actual+1)
 *                WORKING - muestra las zonas añadidas mientras se configura un grupo (a partir de multi.w_size)
 * @param line    fila del LCD (1-4).
 * @param znumber zona extra opcional (se muestra con +).
 */
int displayLCDGrupo(display_modo modo, int line, int znumber) {
    LOG_DEBUG("recibido modo=",modo,"line=",line,"znumber=",znumber);
    if (multi.size == nullptr) return 0; // multi no ha sido inicializado
    int pos = 0;
    buff[0] = '\0'; // vaciar buffer antes de usarlo (caso de RESTO con todo regado o WORKING sin zonas añadidas)
    const int size = (modo == WORKING) ? multi.w_size : *multi.size; // usamos el contador de configuración o el tamaño del grupo según el modo
    const int inicio = (modo == RESTO) ? (multi.actualIndex + 1) : 0; // inicio a partir de multi.actual+1 para RESTO, o desde el principio para FULL y WORKING
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

//imprime contenido de la estructura de un grupo en config
void printMultiGroup(int pgrupo)
{
  for(int j = 0; j < config.group[pgrupo].size; j++) {
    Serial.printf("  Zona%d   ", config.group[pgrupo].zNumber[j]);
    Serial.printf("bId: x%04x \n",Zonas[(config.group[pgrupo].zNumber[j])-1]); // bId(boton) asociado a la zona
  }
  Serial.println();
}

//imprime contenido de la estructura multi
void printMulti()
{
    if(multi.size == nullptr) return;  
    if(multi.id != nullptr)   
      Serial.printf("MULTI Grupo: %s (Boton_id: x%04x) | Zonas totales: %d\n", multi.desc, multi.id->bID, *multi.size);
    else   
      Serial.printf("MULTI Grupo: %s | Zonas totales: %d\n", multi.desc, *multi.size);
    for(int j = 0; j < *multi.size; j++) {
      // Imprime el ID hexadecimal y el nombre legible de la zona (ej: "ZONA1")
      Serial.printf("  -> [%d] %s (id: x%04x)\n", j + 1, multi.zserie_pBoton[j]->desc, multi.zserie_pBoton[j]->bID);
    }
  Serial.println();
}
