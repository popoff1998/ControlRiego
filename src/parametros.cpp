
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
  LOG_INFO("\t tamaño de", p_filename, "-->", size, "bytes");
  if (size > 4096) {
    LOG_ERROR("Config file size is too large");
    return false;
  }

  #ifdef EXTRADEBUG
    printFile(p_filename);
  #endif

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    LOG_ERROR("\t  deserializeJson() failed: ", error.c_str());
    return false;
  }
  LOG_TRACE("procesa zonas");
  //--------------  procesa botones zona (IDX)  --------------------------------------------------
  for (JsonObject botones : doc["botones"].as<JsonArray>()) {
      int numzonas = botones.size(); // cantidad de zonas que vienen definidas en el fichero
      int i = botones["zona"] | 1;   // numero de la zona definida
      if (numzonas > NUMZONAS || i > NUMZONAS || i <= 0) {
        LOG_ERROR("ERROR cantidad de zonas o numero de la zona incorrecta. Mayor que:", NUMZONAS);
        return false;
      }  
      config.zona[i-1].idx = botones["idx"] | 0;
      strlcpy(config.zona[i-1].desc, botones["nombre"] | "", sizeof(config.zona[i-1].desc));
      i++;
  }
  LOG_TRACE("procesa grupos");
  //--------------  procesa grupos  ---------------------------------------------------------
  for (JsonObject grupos : doc["grupos"].as<JsonArray>()) {
      int numgroups = grupos.size(); // cantidad de grupos que vienen definidas en el fichero
      int i = grupos["grupo"] | 1;   // numero del grupo definido
      if (numgroups > NUMGRUPOS || i > NUMGRUPOS || i <= 0) {
        LOG_ERROR("ERROR cantidad de grupos o numero del grupo incorrecto. Mayor que:", NUMGRUPOS);
        LOG_TRACE("check numgroups", numgroups,"i=",i);
        return false;
      }  
      strlcpy(config.group[i-1].desc, grupos["desc"] | "", sizeof(config.group[i-1].desc)); 
      JsonArray zonas = grupos["zonas"].as<JsonArray>();
      int count = zonas.size();
      if (count > ZONASXGRUPO) {
        LOG_ERROR("ERROR zonas en el grupo", i,"mayor que:", ZONASXGRUPO);
        LOG_TRACE("check count zonas", count,"i=",i);
        return false;
      }  
      config.group[i-1].size = count;  //tamaño del grupo 
      int j = 0;
      for(JsonVariant zonas_item_elemento : zonas) {
        config.group[i-1].zNumber[j] = zonas_item_elemento;
        j++;
      }
      i++;
      LOG_TRACE("config initialized");
      config.initialized = 1; //solo marcamos como init config si pasa por este bucle
  }
  LOG_TRACE("procesa resto de parametros");
  //--------------  procesa parametro individuales   ----------------------------------------
  config.minutes = doc["tiempo"]["minutos"] | DEFAULTMINUTES;
  config.seconds = doc["tiempo"]["segundos"] | DEFAULTSECONDS;
  strlcpy(config.domoticz_ip, doc["domoticz"]["ip"] | "", sizeof(config.domoticz_ip));
  strlcpy(config.domoticz_port, doc["domoticz"]["port"] | "", sizeof(config.domoticz_port));
  strlcpy(config.ntpServer, doc["ntpServer"] | "", sizeof(config.ntpServer));
  config.warnESP32temp = doc["warnESP32temp"] | MAX_ESP32_TEMP; 
  config.maxledlevel = doc["ledRGB"]["maxledlevel"] | MAXLEDLEVEL; 
  config.dimmlevel = doc["ledRGB"]["dimmlevel"] | DIMMLEVEL; 
  config.tempOffset = doc["tempOffset"] | TEMP_OFFSET; 
  config.tempRemote = doc["tempRemote"] | TEMP_DATA_REMOTE; 
  config.tempRemoteIdx = doc["tempRemoteIdx"] | 0; 
  config.msgdisplaymillis = doc["msgdisplaymillis"] | MSGDISPLAYMILLIS; 
  config.mute = doc["mute"] | false; 
  config.showwifilevel = doc["showwifilevel"] | false; 
  config.xname = doc["xname"] | false;
  config.verify = doc["verify"] | true;
  config.dynamic = doc["dynamic"] | false;
  //-------------------------------------------------------------------------------------------
  file.close();
  LittleFS.end();
  if (!config.initialized) return false;
  return true;
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
  JsonDocument doc;
  //--------------  procesa botones (IDX)  --------------------------------------------------
  doc["botones"].as<JsonArray>();
  JsonArray botones = doc["botones"].to<JsonArray>();
  for (int i=0; i<NUMZONAS; i++) {
    botones[i]["zona"]   = i+1;
    botones[i]["idx"]    = config.zona[i].idx;
    botones[i]["nombre"] = config.zona[i].desc;
  }
  //--------------  procesa grupos  ---------------------------------------------------------
  JsonArray grupos = doc["grupos"].to<JsonArray>();
  for (int i=0; i<NUMGRUPOS; i++) {
    grupos[i]["grupo"]   = i+1;
    grupos[i]["desc"]    = config.group[i].desc;
    JsonArray zonas = grupos[i]["zonas"].to<JsonArray>();
    for(int j=0; j<config.group[i].size; j++) {
      //zonas[j] = config.group[i].zNumber[j];  // otra forma de hacer lo mismo
      zonas.add(config.group[i].zNumber[j]);
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
  doc["tempRemote"]         = config.tempRemote; 
  doc["tempRemoteIdx"]      = config.tempRemoteIdx; 
  doc["msgdisplaymillis"]   = config.msgdisplaymillis; 
  doc["mute"]               = config.mute;
  doc["showwifilevel"]      = config.showwifilevel;
  doc["xname"]              = config.xname;
  doc["verify"]             = config.verify;
  doc["dynamic"]            = config.dynamic;

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
  File origen = LittleFS.open(fileFrom, "r");
  if (!origen) {
    LOG_ERROR("- failed to open file ",fileFrom);
    return false;
  }
  else{
    // Delete existing file, otherwise the configuration is appended to the file
    LOG_DEBUG("borrando file destino",fileTo);
    if (LittleFS.exists(fileTo)) LittleFS.remove(fileTo);
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

//borrado de los ficheros de parametros y backup para resetear la configuracion
bool deleteParmFiles()
{
  LOG_TRACE("in deleteParmFiles");
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
  LOG_ERROR("An Error has occurred while mounting LittleFS");
  return false;
  }
  if (LittleFS.exists(parmFile)) LittleFS.remove(parmFile);
  if (LittleFS.exists(backupParmFile)) LittleFS.remove(backupParmFile);
  return true;
}

//init minimo de config para evitar fallos en caso de no poder cargar parametros de ficheros
void zeroConfig(Config_parm &config) {
  LOG_TRACE("");
  for (int j=0; j<config.n_Grupos; j++) {
    config.group[j].bID = GRUPOS[j];
    config.group[j].size = 0;
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
  Serial.printf("\ttemp (0 LOCAL / 1 REMOTE)= %d \n", config.tempRemote);
  Serial.printf("\ttempRemoteIdx= %d \n", config.tempRemoteIdx);
  Serial.printf("\tmsgdisplaymillis= %d \n", config.msgdisplaymillis);
  Serial.printf("\tmute= %d \n", config.mute);
  Serial.printf("\tshowwifilevel= %d \n", config.showwifilevel);
  Serial.printf("\txname= %d \n", config.xname);
  Serial.printf("\tverify= %d \n", config.verify);
  Serial.printf("\tdynamic= %d \n", config.dynamic);
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
