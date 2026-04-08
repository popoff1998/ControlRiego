
#include "Control.h"

bool loadConfigFile(const char *p_filename)
{
  LOG_TRACE("");
  #ifdef EXTRADEBUG
    LOG_DEBUG("Contenido del fichero de configuración", p_filename, ":");
    printFile(p_filename);
  #endif
  File file = LittleFS.open(p_filename, "r");
  if(!file){
    LOG_ERROR("Failed to open file for reading", p_filename);
    return false;
  }
  size_t size = file.size();
  LOG_INFO("\t tamaño de", p_filename, "-->", size, "bytes");
  if (size > 4096) {
    LOG_ERROR("Config file size is too large");
    file.close();
    return false;
  }
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();  // cerramos el fichero lo antes posible para caso de errores
  if (error) {
    LOG_ERROR("\t  deserializeJson() failed: ", error.c_str());
    return false;
  }
  // 1. OBTENER ARRAYS Y VALIDAR TAMAÑO TOTAL ANTES DE ENTRAR EN BUCLES
  JsonArray arrayBotones = doc["botones"].as<JsonArray>();
  JsonArray arrayGrupos = doc["grupos"].as<JsonArray>();
  LOG_INFO("\t Zonas definidas en fichero:", arrayBotones.size());
  LOG_INFO("\t Grupos definidos en fichero:", arrayGrupos.size());
  if (arrayBotones.size() == 0) {
    LOG_ERROR("ERROR: No hay zonas en el fichero");
    return false;
  }
  if (arrayBotones.size() > NUMZONAS) {
    LOG_ERROR("ERROR: Demasiadas zonas:", arrayBotones.size(), ">", NUMZONAS);
    return false;
  }
  if (arrayGrupos.size() > NUMGRUPOS) {
    LOG_ERROR("ERROR: Demasiados grupos:", arrayGrupos.size(), ">", NUMGRUPOS);
    return false;
  }
  // Reseteamos flag y contadores antes de procesar
  config.initialized = false;
  int zonasCargadasOk = 0;
  int gruposCargadosOk = 0;  
  // 2. PROCESAR ZONAS
  LOG_TRACE("procesa zonas");
  for (JsonObject z : arrayBotones) {
      int i = z["zona"] | 0;  // numero de la zona definida
      if (i > NUMZONAS || i <= 0) {
          LOG_ERROR("ERROR: numero de zona incorrecto:", i);
          return false;
      }
      config.zona[i-1].idx = z["idx"] | 0;
      strlcpy(config.zona[i-1].desc, z["nombre"] | "", sizeof(config.zona[i-1].desc));
      zonasCargadasOk++;
  }
  if (zonasCargadasOk == arrayBotones.size()) {
      config.initialized = true;  // solo si todas las zonas se han cargado OK
  }
  // 3. PROCESAR GRUPOS
  LOG_TRACE("procesa grupos");
  for (JsonObject g : arrayGrupos) {
      int i = g["grupo"] | 0;  // numero del grupo definido
      if (i > NUMGRUPOS || i <= 0) {
          LOG_ERROR("ERROR: numero de grupo incorrecto:", i);
          return false;
      }
      strlcpy(config.group[i-1].desc, g["desc"] | "", sizeof(config.group[i-1].desc));
      JsonArray zonasArr = g["zonas"].as<JsonArray>();
      int count = zonasArr.size();
      if (count > ZONASXGRUPO) {
          LOG_ERROR("ERROR: Zonas en grupo", i, "exceden el máximo de:", ZONASXGRUPO);
          return false;
      }
      config.group[i-1].size = count;    //tamaño del grupo
      int j = 0;
      for(JsonVariant v : zonasArr) {
          config.group[i-1].zNumber[j++] = v.as<int>();
      }
      gruposCargadosOk++;
  }
  if (gruposCargadosOk != arrayGrupos.size()) {
      config.initialized = false;  // si falla algun grupo, no considera inicializada config
  }
  LOG_TRACE("procesa resto de parametros");
  //--------------  procesa parametro individuales   ----------------------------------------
  config.minutes = doc["tiempo"]["minutos"] | DEFAULTMINUTES;
  config.seconds = doc["tiempo"]["segundos"] | DEFAULTSECONDS;
  strlcpy(config.domoticz_ip, doc["domoticz"]["ip"] | "", sizeof(config.domoticz_ip));
  strlcpy(config.domoticz_port, doc["domoticz"]["port"] | "", sizeof(config.domoticz_port));
  strlcpy(config.ntpServer, doc["time"]["ntpServer"] | NTPSERVER_SPAIN, sizeof(config.ntpServer));
  strlcpy(config.TZ, doc["time"]["timeZone"] | TZ_Europe_Madrid, sizeof(config.TZ));
  config.warnESP32temp = doc["warnESP32temp"] | DFLT_MAX_ESP32_TEMP; 
  config.maxledlevel = doc["ledRGB"]["maxledlevel"] | DFLT_MAXLEDLEVEL; 
  config.dimmlevel = doc["ledRGB"]["dimmlevel"] | DFLT_DIMMLEVEL; 
  config.tempOffset = doc["tempOffset"] | DFLT_TEMP_OFFSET; 
  // leemos como bool (false si: 0 o false o ausente, true si: cualquier otro valor o true): 
  bool readValue = doc["tempRemote"] | (bool)DFLT_TEMP_DATA_REMOTE;  // DFLT_TEMP_DATA_REMOTE es 0 (int) pero lo convertimos a bool
  config.tempRemote = readValue ? 1 : 0;  // pasamos el bool a int (0 o 1)
  config.tempRemoteIdx = doc["tempRemoteIdx"] | 0; 
  config.msgdisplaymillis = doc["msgdisplaymillis"] | DFLT_MSGDISPLAYMS; 
  config.mute = doc["mute"] | false; 
  config.volume = doc["volume"] | DFLT_VOLUME; 
  config.finMelody = doc["finMelody"] | DFLT_FINMELODY; 
  config.showwifilevel = doc["showwifilevel"] | false; 
  config.xname = doc["xname"] | DEFAULTXNAME;
  config.verify = doc["verify"] | DEFAULTVERIFY;
  config.dynamic = doc["dynamic"] | DEFAULTDYNAMIC;
  config.lastr24 = doc["lastr24"] | DEFAULTLASTR24;
  config.logWarnToFile = doc["logWarnToFile"] | DFLT_LOGWARNTOFILE;
  //-------------------------------------------------------------------------------------------
  return config.initialized;
} // end loadConfigFile

bool saveConfigFile(const char *p_filename)
{
  LOG_TRACE("TRACE: in saveConfigFile");
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
  doc["time"]["ntpServer"]  = config.ntpServer;
  doc["time"]["timeZone"]   = config.TZ;
  doc["warnESP32temp"]      = config.warnESP32temp; 
  doc["ledRGB"]["maxledlevel"]  = config.maxledlevel; 
  doc["ledRGB"]["dimmlevel"]    = config.dimmlevel; 
  doc["tempOffset"]         = config.tempOffset;
  doc["tempRemote"]         = config.tempRemote!=0; // guardamos como bool (false si 0, true si cualquier otro valor)
  doc["tempRemoteIdx"]      = config.tempRemoteIdx; 
  doc["msgdisplaymillis"]   = config.msgdisplaymillis; 
  doc["mute"]               = config.mute;
  doc["volume"]             = config.volume;
  doc["finMelody"]          = config.finMelody;
  doc["showwifilevel"]      = config.showwifilevel;
  doc["xname"]              = config.xname;
  doc["verify"]             = config.verify;
  doc["dynamic"]            = config.dynamic;
  doc["lastr24"]            = config.lastr24;
  doc["logWarnToFile"]      = config.logWarnToFile;
  //-------------------------------------------------------------------------------------------
  // Serialize JSON to file
  #ifdef EXTRADEBUG 
    LOG_DEBUG("Contenido del jsondoc a grabar en ",p_filename,":");
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
  return true;
} // end saveConfigFile


bool copyFile(const char *fileFrom, const char *fileTo) {
  File origen = LittleFS.open(fileFrom, "r");
  if (!origen) {
    LOG_ERROR("Failed to open file for reading",fileFrom);
    return false;
  }  
  LOG_INFO("copiando",fileFrom,"en",fileTo);
  File destino = LittleFS.open(fileTo, "w"); // "w" ya sobrescribe, no hace falta borrar antes
  if (!destino) {
    LOG_ERROR("Failed to open file for writing",fileTo);
    origen.close();
    return false;
  }
  uint8_t buffer[64]; // Un buffer pequeño para no agotar la RAM
  while (origen.available()) {
    int bytesLeidos = origen.read(buffer, sizeof(buffer));
    destino.write(buffer, bytesLeidos);
  }
  destino.close();
  origen.close();
  LOG_TRACE("copiado ",fileFrom," en ",fileTo, "OK returning true");
  return true;
} // end copyFile

//borrado de los ficheros de parametros,backup,riegos, logs... para resetear la configuracion
bool deleteDatos()
{
  LOG_TRACE("Iniciando borrado de contenidos en /datos");
  bool bRC = true;
  File root = LittleFS.open("/datos");
  if (!root) {
    LOG_WARN("Error: No se pudo abrir el directorio /datos (¿existe?)");
    return false;
  }
  File file = root.openNextFile();
  while (file) {
    String fileName = file.path(); 
    file.close(); // cerramos el fichero antes de borrarlo
    LOG_DEBUG("Borrando: " + fileName);
    if (!LittleFS.remove(fileName)) {
      LOG_WARN("Fallo al borrar: " + fileName);
      bRC = false;
    }
    file = root.openNextFile();
  }
  root.close();
  return bRC;
}


//init minimo de config para evitar fallos en caso de no poder cargar parametros de ficheros
void zeroConfig() {
  LOG_TRACE("");
  config = Config_parm(); //reset estructura config a valores por defecto
  for (int j=0; j<config.n_Grupos; j++) {
    config.group[j].size = 0;
  }  
}

void cleanFS() {
  LOG_WARN("  [cleanFS]Wait. . .Borrando File System!!!");
  LittleFS.format();
  LOG_WARN("Done!");
}

void printParms() {
  Serial.println(F("contenido estructura parametros configuracion: "));
  //--------------  imprime array botones (IDX)  --------------------------------------------------
  Serial.printf("\tnumzonas= %d \n", config.n_Zonas);
  Serial.println(F("\tBotones: "));
  for(int i=0; i<config.n_Zonas; i++) {
    Serial.printf("\t\t Zona%d: IDX=%d (%s) \n", i+1, config.zona[i].idx, config.zona[i].desc);
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
  Serial.printf("\tntpServer= %s / timezone= %s \n", config.ntpServer, config.TZ);
  //--------------  imprime parametro individuales   ----------------------------------------
  Serial.printf("\tminutes= %d / seconds= %d \n", config.minutes, config.seconds);
  Serial.printf("\twarnESP32temp= %d \n", config.warnESP32temp);
  Serial.printf("\tmaxledlevel= %d / dimmlevel= %d \n", config.maxledlevel, config.dimmlevel);
  Serial.printf("\ttempOffset (x %.1f)= %d \n", TEMP_OFFSET_FACTOR/100.0, config.tempOffset);
  Serial.printf("\ttemp mode= %s (raw: %d) \n", (config.tempRemote != 0) ? "REMOTE" : "LOCAL", config.tempRemote);
  Serial.printf("\ttempRemoteIdx= %d \n", config.tempRemoteIdx);
  Serial.printf("\tmsgdisplaymillis= %d \n", config.msgdisplaymillis);
  Serial.printf("\tmute= %s \n", config.mute ? "TRUE" : "FALSE");
  Serial.printf("\tvolume= %d \n", config.volume);
  Serial.printf("\tfinMelody= %d \n", config.finMelody);
  Serial.printf("\tshowwifilevel= %s \n", config.showwifilevel ? "TRUE" : "FALSE");
  Serial.printf("\txname= %s \n", config.xname ? "TRUE" : "FALSE");
  Serial.printf("\tverify= %s \n", config.verify ? "TRUE" : "FALSE");
  Serial.printf("\tdynamic= %s \n", config.dynamic ? "TRUE" : "FALSE");
  Serial.printf("\tlastr24= %s \n", config.lastr24 ? "TRUE" : "FALSE");
  Serial.printf("\tdebugmode= %s \n", config.logWarnToFile ? "TRUE" : "FALSE");
  Serial.println("----------------------------------------------------------------\n");
}

// void printParms2() {
//   Serial.println(F("\n--- CONTENIDO ESTRUCTURA CONFIGURACIÓN ---"));
//   // Zonas
//   Serial.printf("Zonas definidas (MAX %d):\n", config.n_Zonas);
//   for(int i = 0; i < config.n_Zonas; i++) {
//     // Si la zona no tiene IDX, quizás no esté configurada
//     if (config.zona[i].idx != 0) {
//       Serial.printf("  [%d] IDX:%d | Desc: %s\n", i + 1, config.zona[i].idx, config.zona[i].desc);
//     }
//   }
//   // Grupos
//   Serial.printf("Grupos definidos (MAX %d):\n", config.n_Grupos);
//   for(int i = 0; i < config.n_Grupos; i++) {
//     if (config.group[i].size > 0) {
//       Serial.printf("  G%d: %s (Zonas: %d)\n", i + 1, config.group[i].desc, config.group[i].size);
//       Serial.print(F("      Lista IDs: "));
//       for(int j = 0; j < config.group[i].size; j++) {
//         Serial.printf("%d%s", config.group[i].zNumber[j], (j == config.group[i].size - 1) ? "" : ", ");
//       }
//       Serial.println();
//     }
//   }
//   // Red y Tiempo
//   Serial.println(F("Conexión y Sincronización:"));
//   Serial.printf("  Domoticz: %s:%s\n", config.domoticz_ip, config.domoticz_port);
//   Serial.printf("  NTP: %s | TZ: %s\n", config.ntpServer, config.TZ);
//   // Parámetros de Sistema (Booleanos convertidos a texto para lectura rápida)
//   Serial.println(F("Parámetros de Sistema:"));
//   Serial.printf("  Riego defecto: %02d:%02d\n", config.minutes, config.seconds);
//   Serial.printf("  Alertas: TempESP32 > %d°C | Mute: %s | Vol: %d\n", 
//                 config.warnESP32temp, config.mute ? "SI" : "NO", config.volume);
//   Serial.printf("  Flags: Dinámico:%s | Verify:%s | LastR24:%s | XName:%s\n",
//                 config.dynamic ? "SI" : "NO", config.verify ? "SI" : "NO", 
//                 config.lastr24 ? "SI" : "NO", config.xname ? "SI" : "NO");
//   Serial.println(F("------------------------------------------\n"));
// }


void filesInfo() 
{
  float fileTotalKB = (float)LittleFS.totalBytes() / 1024.0; 
  float fileUsedKB = (float)LittleFS.usedBytes() / 1024.0; 
  Serial.print("__________________________\n");
  Serial.println(F("File system (LittleFS): "));
  Serial.print(F("    Total KB: ")); Serial.print(fileTotalKB); Serial.println(F(" KB"));
  Serial.print(F("    Used KB: ")); Serial.print(fileUsedKB); Serial.println(F(" KB"));
  Serial.print("__________________________\n");
  listDir(LittleFS, "/", 1); // List the directories up to one level beginning at the root directory
  Serial.print("__________________________\n");
}


void listDir(fs::FS &fs, const char * dirname, uint8_t levels, uint8_t depth) {
    // Función auxiliar local para generar la indentación (tabulaciones)
    auto printIndent = [depth]() {
        for (uint8_t i = 0; i < depth; i++) Serial.print("   "); // Tres espacios por nivel
    };
    printIndent();
    Serial.printf("Listing directory: %s\r\n", dirname);
    File root = fs.open(dirname);
    if (!root) {
        if (depth == 0) Serial.println("- failed to open directory");
        else { printIndent(); Serial.printf("- failed to open directory: %s\r\n", dirname); }
        return;
    }
    if (!root.isDirectory()) { Serial.println(" - not a directory"); return; }
    File file = root.openNextFile();
    while (file) {
        printIndent(); // Aplica la indentación antes de imprimir el elemento
        time_t t = file.getLastWrite();
        struct tm * tmstruct = localtime(&t);
        char timeStr[20]; // Buffer para la cadena de tiempo: YYYY-MM-DD hh:mm:ss + NULL
        snprintf(timeStr, sizeof(timeStr), "%d-%02d-%02d %02d:%02d:%02d",
                 (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday,
                 tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
        if (file.isDirectory()) {
            Serial.printf("[DIR] %s", file.name()); // Usamos file.name() directamente
            Serial.printf(" LAST WRITE: %s\r\n", timeStr);
            if (levels > 0) { // Comprobación de niveles restantes
                String subDirPath = dirname;
                if (!subDirPath.endsWith("/")) { subDirPath += "/"; }
                subDirPath += file.name();
                listDir(fs, subDirPath.c_str(), levels - 1, depth + 1);
            }
        } else {
            Serial.printf("[FILE] %s", file.path());
            Serial.printf("\tSIZE: %lu bytes", (unsigned long)file.size());
            Serial.printf(" LAST WRITE: %s\r\n", timeStr);
        }
        file = root.openNextFile();
    }
}

String sysInfo() {
    JsonDocument doc; 
    uint32_t sketchTotalPartitionSize = ESP.getFreeSketchSpace();
    uint32_t sketchUsed = ESP.getSketchSize();
    int sketchPercentUsed = (sketchUsed * 100) / sketchTotalPartitionSize;
    uint32_t fsUsed = LittleFS.usedBytes();
    uint32_t fsTotal = LittleFS.totalBytes();
    int filesPercentUsed = (fsUsed * 100) / fsTotal;
    doc["FW version"] = String(FW_VERSION) + " Built on " __DATE__ " at " __TIME__;
    doc["esp_idf_version"] = esp_get_idf_version();
    doc["arduino_version"] = String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH);
    doc["Chip Model"] = ESP.getChipModel();
    doc["Chip Cores"] = ESP.getChipCores();
    doc["Chip Revision"] = ESP.getChipRevision();
    doc["FlashSize"] = convertFileSize(ESP.getFlashChipSize());
    doc["SketchSpace "] = sketchTotalPartitionSize; 
    doc["SketchSize (percent used)"] = String(sketchUsed) + "   (" + String(sketchPercentUsed) + "%)"; 
    doc["HeapSize"] = ESP.getHeapSize();
    doc["FreeHeap"] = ESP.getFreeHeap();
    doc["MaxAllocHeap (largest free block)"] = ESP.getMaxAllocHeap();
    doc["MinFreeHeap (lowes since boot)"] = ESP.getMinFreeHeap();
    doc["File System Total"] = convertFileSize(fsTotal);
    doc["File System Used (percent used)"] = convertFileSize(fsUsed) + "   (" + String(filesPercentUsed) + "%)";
    doc["ESP32 temperature"] = String(temperatureRead(), 2) + " ºC"; 
    String output;
    serializeJsonPretty(doc, output); 
    return output;
}

String convertFileSize(const size_t bytes)
  {
    if(bytes < 1024)
    {
      return String(bytes) + " B";
    }
    else if (bytes < 1048576)
    {
      return String(bytes / 1024) + " KB";  //sin decimales
      //return String(bytes / 1024.0) + " KB";
    }
    return String(bytes / 1048576.0) + " MB";
  }

// Prints the content of a file to the Serial 
void printFile(const char *p_filename) {
  LOG_TRACE("printFile (",p_filename,")");
  // Open file for reading
  File file = LittleFS.open(p_filename, "r");
  if (!file) {
    LOG_ERROR("Failed to open file", p_filename);
    return;
  }
  Serial.printf("\n File %s Content: \n", p_filename);
  while(file.available()){
    Serial.write(file.read());
  }
  Serial.println(F("\n\n"));
  file.close();
}

// funciones solo usadas en DEVELOP
#ifdef EXTRADEBUG

void memoryInfo() 
{
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
