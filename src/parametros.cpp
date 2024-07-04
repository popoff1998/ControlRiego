
#include "Control.h"

bool loadConfigFile(const char *p_filename, Config_parm &config)
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
  if (numzonas != config.n_Zonas) {
    LOG_ERROR("ERROR numero de zonas incorrecto");
    return false;
  }  
  for (JsonObject botones_item : doc["botones"].as<JsonArray>()) {
    int i = botones_item["zona"] | 1; 
    config.zona[i-1].idx = botones_item["idx"] | 0;
    strlcpy(config.zona[i-1].desc, botones_item["nombre"] | "", sizeof(config.zona[i-1].desc));
    i++;
  }
  //--------------  procesa grupos  ---------------------------------------------------------
  int numgroups = doc["numgroups"] | 1;
  if (numgroups != config.n_Grupos) {
    LOG_ERROR("ERROR numero de grupos incorrecto");
    return false;
  }  
  for (JsonObject groups_item : doc["grupos"].as<JsonArray>()) {
    int i = groups_item["grupo"] | 1; // 1, 2, 3
    config.group[i-1].bID = GRUPOS[i-1];  //obtiene el bID del boton de ese grupo (ojo: no viene en el json)
    config.group[i-1].size = groups_item["size"] | 0;
    //Serial.printf("[loadConfigFile] procesando GRUPO%d size=%d id=x%x \n",i,config.group[i-1].size,config.group[i-1].id); //DEBUG
    strlcpy(config.group[i-1].desc, groups_item["desc"] | "", sizeof(config.group[i-1].desc)); 
    JsonArray array = groups_item["zonas"].as<JsonArray>();
    int count = array.size();
    if (count != config.group[i-1].size) {
      LOG_ERROR("ERROR tamaño del grupo incorrecto");
      return false;
    }  
    int j = 0;
    for(JsonVariant zonas_item_elemento : array) {
      config.group[i-1].zNumber[j] = zonas_item_elemento.as<int>();
      j++;
    }
    i++;
  config.initialized = 1; //solo marcamos como init config si pasa por este bucle
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  config.minutes = doc["tiempo"]["minutos"] | 0; // 0
  config.seconds = doc["tiempo"]["segundos"] | 10; // 10
  strlcpy(config.domoticz_ip, doc["domoticz"]["ip"] | "", sizeof(config.domoticz_ip));
  strlcpy(config.domoticz_port, doc["domoticz"]["port"] | "", sizeof(config.domoticz_port));
  strlcpy(config.ntpServer, doc["ntpServer"] | "", sizeof(config.ntpServer));
  config.warnESP32temp = doc["warnESP32temp"] | MAX_ESP32_TEMP; 
  config.maxledlevel = doc["ledRGB"]["maxledlevel"] | MAXLEDLEVEL; 
  config.dimmlevel = doc["ledRGB"]["dimmlevel"] | DIMMLEVEL; 
  config.tempOffset = doc["tempOffset"] | TEMP_OFFSET; 
  config.msgdisplaymillis = doc["msgdisplaymillis"] | MSGDISPLAYMILLIS; 
  config.mute = doc["mute"] | false; 
  //-------------------------------------------------------------------------------------------
  file.close();
  LittleFS.end();
  if (config.initialized) return true;
  else return false;
}

bool saveConfigFile(const char *p_filename, Config_parm &config)
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
    array_botones[i]["idx"]    = config.zona[i].idx;
    array_botones[i]["nombre"] = config.zona[i].desc;
  }
  //--------------  procesa grupos  ---------------------------------------------------------
  doc["numgroups"]          = NUMGRUPOS;
  JsonArray array_grupos = doc.createNestedArray("grupos");
  for (int i=0; i<NUMGRUPOS; i++) {
    array_grupos[i]["grupo"]   = i+1;
    array_grupos[i]["desc"]    = config.group[i].desc;
    array_grupos[i]["size"]    = config.group[i].size;
    JsonArray array_zonas = array_grupos[i].createNestedArray("zonas");
    for(int j=0; j<config.group[i].size; j++) {
      array_zonas[j] = config.group[i].zNumber[j];
    }  
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  doc["tiempo"]["minutos"]  = config.minutes; 
  doc["tiempo"]["segundos"] = config.seconds;
  doc["domoticz"]["ip"]     = config.domoticz_ip;
  doc["domoticz"]["port"]   = config.domoticz_port;
  doc["ntpServer"]          = config.ntpServer;
  doc["warnESP32temp"]      = config.warnESP32temp; 
  doc["ledRGB"]["maxledlevel"]  = config.maxledlevel; 
  doc["ledRGB"]["dimmlevel"]    = config.dimmlevel; 
  doc["tempOffset"]         = config.tempOffset; 
  doc["msgdisplaymillis"]         = config.msgdisplaymillis; 
  doc["mute"]               = config.mute;

  // Serialize JSON to file
  #ifdef EXTRADEBUG 
    serializeJsonPretty(doc, Serial); 
  #endif
  //int docsize = serializeJson(doc, file);
  int docsize = serializeJsonPretty(doc, file);
  if (docsize == 0) {
    LOG_ERROR("Failed to write to file");
    return false;
  }
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
  if (LittleFS.exists(fileTo)) LittleFS.remove(fileTo);
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
void zeroConfig(Config_parm &config) {
  LOG_TRACE("");
  for (int j=0; j<config.n_Grupos; j++) {
    config.group[j].bID = GRUPOS[j];
    config.group[j].size = 0;
    // config.group[j].size = 1;
    // config.group[j].zNumber[0]=j+1;
  }  
}

void cleanFS() {
  LOG_WARN("  [cleanFS]Wait. . .Borrando File System!!!");
  LittleFS.format();
  LOG_WARN("Done!");
}

void printParms(Config_parm &config) {
  Serial.println(F("contenido estructura parametros configuracion: "));
  //--------------  imprime array botones (IDX)  --------------------------------------------------
  Serial.printf("\tnumzonas= %d \n", config.n_Zonas);
  Serial.println(F("\tBotones: "));
  for(int i=0; i<config.n_Zonas; i++) {
    Serial.printf("\t\t Zona%d: IDX=%d (%s) l=%d \n", i+1, config.zona[i].idx, config.zona[i].desc, sizeof(config.zona[i].desc));
  }
  //--------------  imprime array y subarray de grupos  ----------------------------------------------
  Serial.printf("\tnumgroups= %d \n", config.n_Grupos);
  for(int i = 0; i < config.n_Grupos; i++) {
    Serial.printf("\tGrupo%d: size=%d (%s)\n", i+1, config.group[i].size, config.group[i].desc);
    for(int j = 0; j < config.group[i].size; j++) {
      Serial.printf("\t\t Zona%d \n", config.group[i].zNumber[j]);
    }
  }
  //--------------  imprime parametro conexion   ----------------------------------------
  Serial.printf("\tdomoticz_ip= %s / domoticz_port= %s \n", config.domoticz_ip, config.domoticz_port);
  Serial.printf("\tntpServer= %s \n", config.ntpServer);
  //--------------  imprime parametro individuales   ----------------------------------------
  Serial.printf("\tminutes= %d / seconds= %d \n", config.minutes, config.seconds);
  Serial.printf("\twarnESP32temp= %d \n", config.warnESP32temp);
  Serial.printf("\tmaxledlevel= %d / dimmlevel= %d \n", config.maxledlevel, config.dimmlevel);
  Serial.printf("\ttempOffset= %d \n", config.tempOffset);
  Serial.printf("\tmsgdisplaymillis= %d \n", config.msgdisplaymillis);
  Serial.printf("\tmute= %d \n", config.mute);
  Serial.println("----------------------------------------------------------------");
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
