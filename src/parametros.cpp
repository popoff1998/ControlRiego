
#include "Control.h"

bool loadConfigFile(const char *p_filename, Config_parm &cfg)
{
  LOG_TRACE("");
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    LOG_ERROR("An Error has occurred while mounting LittleFS");
    return false;
  }
  File file = LittleFS.open(p_filename, "r");
  if(!file){
    LOG_ERROR("Failed to open file for reading");
    return false;
  }
  size_t size = file.size();
  if (size > 2048) {
    LOG_ERROR("Config file size is too large");
    return false;
  }
  LOG_INFO("\t tamaño de", p_filename, "-->", size, "bytes");

  DynamicJsonDocument doc(3072);
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    LOG_ERROR("\t  deserializeJson() failed: ", error.f_str());
    return false;
  }
  Serial.printf("\t memoria usada por el jsondoc: (%d) \n" , doc.memoryUsage());  //TODO ¿eliminar msg?
  //--------------  procesa botones (IDX)  --------------------------------------------------
  int numzonas = doc["numzonas"] | 0; // carga 0 si no viene este elemento
  if (numzonas != cfg.n_Zonas) {
    LOG_ERROR("ERROR numero de zonas incorrecto");
    return false;
  }  
  for (JsonObject botones_item : doc["botones"].as<JsonArray>()) {
    int i = botones_item["zona"] | 1; 
    cfg.botonConfig[i-1].idx = botones_item["idx"] | 0;
    strlcpy(cfg.botonConfig[i-1].desc, botones_item["nombre"] | "", sizeof(cfg.botonConfig[i-1].desc));
    i++;
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  cfg.minutes = doc["tiempo"]["minutos"] | 0; // 0
  cfg.seconds = doc["tiempo"]["segundos"] | 10; // 10
  strlcpy(cfg.domoticz_ip, doc["domoticz"]["ip"] | "", sizeof(cfg.domoticz_ip));
  strlcpy(cfg.domoticz_port, doc["domoticz"]["port"] | "", sizeof(cfg.domoticz_port));
  strlcpy(cfg.ntpServer, doc["ntpServer"] | "", sizeof(cfg.ntpServer));
  int numgroups = doc["numgroups"] | 1;
  if (numgroups != cfg.n_Grupos) {
    LOG_ERROR("ERROR numero de grupos incorrecto");
    return false;
  }  
  //--------------  procesa grupos  ---------------------------------------------------------
  for (JsonObject groups_item : doc["grupos"].as<JsonArray>()) {
    int i = groups_item["grupo"] | 1; // 1, 2, 3
    cfg.groupConfig[i-1].id = GRUPOS[i-1];  //obtiene el id del boton de ese grupo (ojo: no viene en el json)
    cfg.groupConfig[i-1].size = groups_item["size"] | 1;
    if (cfg.groupConfig[i-1].size == 0) {
      cfg.groupConfig[i-1].size =1;
      LOG_ERROR("ERROR tamaño del grupo incorrecto, es 0 -> ponemos 1");
    }
    //Serial.printf("[loadConfigFile] procesando GRUPO%d size=%d id=x%x \n",i,cfg.groupConfig[i-1].size,cfg.groupConfig[i-1].id); //DEBUG
    strlcpy(cfg.groupConfig[i-1].desc, groups_item["desc"] | "", sizeof(cfg.groupConfig[i-1].desc)); 
    JsonArray array = groups_item["zonas"].as<JsonArray>();
    int count = array.size();
    if (count != cfg.groupConfig[i-1].size) {
      LOG_ERROR("ERROR tamaño del grupo incorrecto");
      return false;
    }  
    int j = 0;
    for(JsonVariant zonas_item_elemento : array) {
      cfg.groupConfig[i-1].serie[j] = zonas_item_elemento.as<int>();
      j++;
    }
    i++;
  cfg.initialized = 1; //solo marcamos como init config si pasa por este bucle
  }
  file.close();
  LittleFS.end();
  if (cfg.initialized) return true;
  else return false;
}

bool saveConfigFile(const char *p_filename, Config_parm &cfg)
{
  LOG_TRACE("TRACE: in saveConfigFile");
  //memoryInfo();
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    LOG_ERROR("An Error has occurred while mounting LittleFS");
    return false;
  }
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(p_filename);
  File file = LittleFS.open(p_filename, "w");
  if(!file){
    LOG_ERROR("Failed to open file for writing");
    return false;
  }
  DynamicJsonDocument doc(3072);
  //--------------  procesa botones (IDX)  --------------------------------------------------
  doc["numzonas"] = NUMZONAS;
  doc["botones"].as<JsonArray>();
  JsonArray array_botones = doc.createNestedArray("botones");
  for (int i=0; i<NUMZONAS; i++) {
    array_botones[i]["zona"]   = i+1;
    array_botones[i]["idx"]    = cfg.botonConfig[i].idx;
    array_botones[i]["nombre"] = cfg.botonConfig[i].desc;
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  doc["tiempo"]["minutos"]  = cfg.minutes; 
  doc["tiempo"]["segundos"] = cfg.seconds;
  doc["domoticz"]["ip"]     = cfg.domoticz_ip;
  doc["domoticz"]["port"]   = cfg.domoticz_port;
  doc["ntpServer"]          = cfg.ntpServer;
  //--------------  procesa grupos  ---------------------------------------------------------
  doc["numgroups"]          = NUMGRUPOS;
  JsonArray array_grupos = doc.createNestedArray("grupos");
  for (int i=0; i<NUMGRUPOS; i++) {
    array_grupos[i]["grupo"]   = i+1;
    array_grupos[i]["desc"]    = cfg.groupConfig[i].desc;
    array_grupos[i]["size"]    = cfg.groupConfig[i].size;
    JsonArray array_zonas = array_grupos[i].createNestedArray("zonas");
    for(int j=0; j<cfg.groupConfig[i].size; j++) {
      array_zonas[j] = cfg.groupConfig[i].serie[j];
    }  
  }
  // Serialize JSON to file
  #ifdef EXTRADEBUG 
    serializeJsonPretty(doc, Serial); 
  #endif
  int docsize = serializeJson(doc, file);
  if (docsize == 0) LOG_ERROR("Failed to write to file");
  else LOG_DEBUG("    tamaño del jsondoc: (",docsize,")");
  LOG_DEBUG("    memoria usada por el jsondoc: (",doc.memoryUsage(),")");
  file.close();
  LittleFS.end();
  #ifdef EXTRADEBUG
    printFile(p_filename);
  #endif
  //memoryInfo();
  return true;
}


bool copyConfigFile(const char *fileFrom, const char *fileTo)
{
  LOG_TRACE("in copyConfigFile");
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
  LOG_ERROR("An Error has occurred while mounting LittleFS");
  return false;
  }
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(fileTo);
  File origen = LittleFS.open(fileFrom, "r");
  if (!origen) {
    LOG_ERROR("- failed to open file ",fileFrom);
    return false;
  }
  else{
    LOG_INFO("copiando",fileFrom,"en",fileTo);
    File destino = LittleFS.open(fileTo, "w+");
    if(!destino){
      LOG_ERROR("Failed to open file for writing",fileTo);
      return false;
    }
    else {
      while(origen.available()){
        destino.print(origen.readStringUntil('\n')+"\n");
      }
      destino.close(); 
    }
    origen.close();  
    LittleFS.end();  
    return true;
  } 
}

//init minimo de config para evitar fallos en caso de no poder cargar parametros de ficheros
void zeroConfig(Config_parm &cfg) {
  LOG_TRACE("");
  for (int j=0; j<cfg.n_Grupos; j++) {
    cfg.groupConfig[j].id = GRUPOS[j];
    cfg.groupConfig[j].size = 1;
    cfg.groupConfig[j].serie[0]=j+1;
  }  
}

void cleanFS() {
  LOG_WARN("  [cleanFS]Wait. . .Borrando File System!!!");
  LittleFS.format();
  LOG_WARN("Done!");
}

void printParms(Config_parm &cfg) {
  Serial.println(F("contenido estructura parametros configuracion: "));
  //--------------  imprime array botones (IDX)  --------------------------------------------------
  Serial.printf("\tnumzonas= %d \n", cfg.n_Zonas);
  Serial.println(F("\tBotones: "));
  for(int i=0; i<cfg.n_Zonas; i++) {
    Serial.printf("\t\t Zona%d: IDX=%d (%s) l=%d \n", i+1, cfg.botonConfig[i].idx, cfg.botonConfig[i].desc, sizeof(cfg.botonConfig[i].desc));
  }
  //--------------  imprime parametro individuales   ----------------------------------------
  Serial.printf("\tminutes= %d seconds= %d \n", cfg.minutes, cfg.seconds);
  Serial.printf("\tdomoticz_ip= %s domoticz_port= %s \n", cfg.domoticz_ip, cfg.domoticz_port);
  Serial.printf("\tntpServer= %s \n", cfg.ntpServer);
  Serial.printf("\tnumgroups= %d \n", cfg.n_Grupos);
  //--------------  imprime array y subarray de grupos  ----------------------------------------------
  for(int i = 0; i < cfg.n_Grupos; i++) {
    Serial.printf("\tGrupo%d: size=%d (%s)\n", i+1, cfg.groupConfig[i].size, cfg.groupConfig[i].desc);
    for(int j = 0; j < cfg.groupConfig[i].size; j++) {
      Serial.printf("\t\t Zona%d \n", cfg.groupConfig[i].serie[j]);
    }
  }
}

void filesInfo() 
{
  LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED);
  float fileTotalKB = (float)LittleFS.totalBytes() / 1024.0; 
  float fileUsedKB = (float)LittleFS.usedBytes() / 1024.0; 
  Serial.print("__________________________\n");
  Serial.println(F("File system (LittleFS): "));
  Serial.print(F("    Total KB: ")); Serial.print(fileTotalKB); Serial.println(F(" KB"));
  Serial.print(F("    Used KB: ")); Serial.print(fileUsedKB); Serial.println(F(" KB"));
  Serial.print("__________________________\n");
  listDir(LittleFS, "/", 1); // List the directories up to one level beginning at the root directory
  LittleFS.end();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);
    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.print(file.name());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\t  SIZE: ");
            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("\t  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}


// funciones solo usadas en DEVELOP
#ifdef EXTRADEBUG

// Prints the content of a file to the Serial 
void printFile(const char *p_filename) {
  LOG_TRACE("printFile (",p_filename,")");
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    LOG_ERROR("An Error has occurred while mounting LittleFS");
  return;
  }
  // Open file for reading
  File file = LittleFS.open(p_filename, "r");
  if (!file) {
    LOG_ERROR("Failed to open config file");
    return;
  }
  Serial.println(F("File Content:"));
  while(file.available()){
    Serial.write(file.read());
  }
  Serial.println(F("\n\n"));
  file.close();
  LittleFS.end();
}

void memoryInfo() 
{
/*   LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED);//FSInfo fs_info;
 */  

  float fileTotalKB = (float)LittleFS.totalBytes() / 1024.0; 
  float fileUsedKB = (float)LittleFS.usedBytes() / 1024.0; 
  int freeHeadSize = (int)ESP.getFreeHeap() / 1024.0;
  float freeSketchSize = (float)ESP.getFreeSketchSpace() / 1024.0;
  Serial.print("\n#####################\n");
  Serial.print("__________________________\n\n");
  Serial.println(F("File system (LittleFS): "));
  Serial.print(F("    Total KB: ")); Serial.print(fileTotalKB); Serial.println(F(" KB"));
  Serial.print(F("    Used KB: ")); Serial.print(fileUsedKB); Serial.println(F(" KB"));
  listDir(LittleFS, "/", 1); // List the directories up to one level beginning at the root directory
  Serial.print("__________________________\n\n");
  Serial.printf("free RAM (max Head size): %d KB  <<<<<<<<<<<<<<<<<<<\n\n", freeHeadSize);
  Serial.printf("free SketchSpace: %f KB\n\n", freeSketchSize);
  Serial.println(F("#####################"));
  LittleFS.end();
}

void printCharArray(char *arr, size_t len)
{
    printf("arr: ");
    for (size_t i = 0; i < len; ++i) {
        printf("x%x, ", arr[i]);
    }
    printf("\n");
}

#endif
