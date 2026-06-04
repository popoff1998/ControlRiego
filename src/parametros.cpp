
#include "Control.h"

bool abrirYDeserializarJson(const char* filename, JsonDocument& doc, size_t maxSize) {
    File file = LittleFS.open(filename, "r");
    if (!file) {
        LOG_WARN("Error abriendo el fichero para leer:", filename);
        return false;
    }
    #ifdef EXTRADEBUG
    printFile(filename);
    #endif
    size_t fileSize = file.size();
    if (fileSize == 0 || fileSize > maxSize) {
        file.close();
        LOG_ERROR("ERROR: Tamaño de", filename, "no válido:", fileSize, "bytes (cero o >", maxSize, ")");
        return false;
    }
    LOG_DEBUG("\t tamaño de", filename, "-->", fileSize, "bytes");
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        LOG_ERROR("Error al deserializar JSON de", filename, ":", error.c_str());
        return false;
    }
    return true;
}

int getJsonParamRange(JsonVariant docNode, int minVal, int maxVal, int defaultVal, const char* paramName) {
    // Si el nodo no existe en el JSON, usamos el valor por defecto sin warning
    if (docNode.isNull()) {
        return defaultVal;
    }
    int value = docNode.as<int>();
    // Validación de rango estricto
    if (value < minVal || value > maxVal) {
        LOG_WARN("Parametro '", paramName, "' fuera de rango (", minVal, " a ", maxVal, "). Leido: ", value, ". Usando default: ", defaultVal);
        return defaultVal;
    }
    return value;
}

bool saveConfig()
{
  LOG_INFO("saveConfigRequired=true  --> salvando parametros a fichero");
  saveConfigRequired = false;
  if (writeConfigToFile(parmFile)) {
    lcd.infoclear("SAVED parameters", BLINKDISPLAY, BIPOK);
    delay(config.msgdisplaymillis);
    config.initialized = true;  // para indicar que ya hay config válida en memoria
    return true;
  }
  else {
    lcd.infoclear("ERROR saving parms", BLINKDISPLAY, BIPKO);
    delay(config.msgdisplaymillis);
    statusError(E0); // error no recuperable al guardar parametros
    return false;
  }
}


bool loadConfigFromFile(const char *p_filename)
{
  LOG_TRACE("");
  JsonDocument doc;
  if (!abrirYDeserializarJson(p_filename, doc, MAX_FILE_SIZE)) return false;
  // 0. VALIDACIÓN DE CABECERA Y VERSIÓN
  if (doc["fileType"].as<String>() != "CCR_config") {
      LOG_ERROR("ERROR: El fichero no contiene un formato valido"); return false; }
  if (doc["version"].as<int>() != PARMVERSION) {
      LOG_ERROR("ERROR: Version de fichero incompatible (", doc["version"].as<int>(), " vs ", PARMVERSION, ")"); return false; }
  if (doc["SCDtype"].as<String>() != SCDTYPE) {
      LOG_ERROR("ERROR: Tipo de SCD incompatible (", doc["SCDtype"].as<String>(), " vs ", SCDTYPE, ")"); return false; }
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
  // Reseteamos flag antes de procesar
  config.initialized = false;
  // 2. PROCESAR ZONAS (Obligatoria al menos una)
  LOG_TRACE("procesa zonas");
  for (JsonObject z : arrayBotones) {
      int i = z["zona"] | 0;
      if (i > NUMZONAS || i <= 0) {
          LOG_ERROR("ERROR: numero de zona incorrecto:", i);
          return false;
      }
      // Control de duplicados: Si el idx ya no es 0 o desc ya no está vacía
      if (config.zona[i-1].idx != 0 || config.zona[i-1].desc[0] != '\0') {
          LOG_ERROR("ERROR: Zona", i, "duplicada en el fichero");
          return false;
      }
      config.zona[i-1].idx = z["idx"] | 0;
      strlcpy(config.zona[i-1].desc, z["nombre"] | "", sizeof(config.zona[i-1].desc));
  }
  // Si hemos salido del bucle de zonas vivos, la configuracion ya es estructuralmente VALIDA
  config.initialized = true; 
  // 3. PROCESAR GRUPOS (Opcionales, pero si existen deben ser perfectos)
  LOG_TRACE("procesa grupos");
  for (JsonObject g : arrayGrupos) {
      int i = g["grupo"] | 0;
      if (i > NUMGRUPOS || i <= 0) {
          LOG_ERROR("ERROR: numero de grupo incorrecto:", i);
          return false;
      }
      // Control de duplicados: Si el tamaño es mayor que 0 o desc ya no está vacía
      if (config.group[i-1].size > 0 || config.group[i-1].desc[0] != '\0') {
          LOG_ERROR("ERROR: Grupo", i, "duplicado en el fichero");
          return false;
      }
      strlcpy(config.group[i-1].desc, g["desc"] | "", sizeof(config.group[i-1].desc));
      JsonArray zonasArr = g["zonas"].as<JsonArray>();
      int count = zonasArr.size();
      if (count > ZONASXGRUPO) {
          LOG_ERROR("ERROR: Zonas en grupo", i, "exceden el máximo de:", ZONASXGRUPO);
          return false;
      }
      config.group[i-1].size = count;
      int j = 0;
      for(JsonVariant v : zonasArr) {
          if (j >= ZONASXGRUPO) break;
          config.group[i-1].zNumber[j++] = v.as<int>();
      }
  }
  LOG_TRACE("procesa resto de parametros");
  //--------------  procesa parametro individuales   ----------------------------------------
  strlcpy(config.SCD_ip, doc["domoticz"]["ip"] | "", sizeof(config.SCD_ip));
  strlcpy(config.SCD_port, doc["domoticz"]["port"] | DFLT_SCD_PORT, sizeof(config.SCD_port));
  strlcpy(config.SCD_user, doc["domoticz"]["user"] | "", sizeof(config.SCD_user));
  strlcpy(config.SCD_password, doc["domoticz"]["password"] | "", sizeof(config.SCD_password));
  strlcpy(config.ntpServer, doc["time"]["ntpServer"] | NTPSERVER_SPAIN, sizeof(config.ntpServer));
  strlcpy(config.TZ, doc["time"]["timeZone"] | TZ_Europe_Madrid, sizeof(config.TZ));
  // leemos como bool (false si: 0 o false o ausente, true si: cualquier otro valor o true): 
  bool readValue = doc["tempRemote"] | (bool)DFLT_TEMP_DATA_REMOTE;  // DFLT_TEMP_DATA_REMOTE es 0 (int) pero lo convertimos a bool
  config.tempRemote = readValue ? 1 : 0;  // pasamos el bool a int (0 o 1)
  config.tempRemoteIdx = doc["tempRemoteIdx"] | 0; 
  config.mute = doc["mute"] | false; 
  config.showwifilevel = doc["showwifilevel"] | false; 
  config.xname = doc["xname"] | DEFAULTXNAME;
  config.verify = doc["verify"] | DEFAULTVERIFY;
  config.dynamic = doc["dynamic"] | DEFAULTDYNAMIC;
  config.lastr24 = doc["lastr24"] | DEFAULTLASTR24;
  config.logWarnToFile = doc["logWarnToFile"] | DFLT_LOGWARNTOFILE;
  //---  procesa tiempo por defecto (de 5 a 59 segundos o de 0 a 59 minutos enteros) ---
  config.minutes = getJsonParamRange(doc["tiempo"]["minutos"], 0, 59, DEFAULTMINUTES, "tiempo.minutos");
  if (config.minutes == 0) { // si minutos es 0, entonces segundos puede ser de 5 a 59
    int minSecons = (DEFAULTSECONDS == 0) ? 5 : DEFAULTSECONDS; // evitamos combinacion 0-0 que no tiene sentido
    config.seconds = getJsonParamRange(doc["tiempo"]["segundos"], 5, 59, minSecons, "tiempo.segundos");
  } else config.seconds = 0; // si minutos es >0, entonces segundos tiene que ser 0
  //---  procesa parametros individuales con rango controlado (Todos enteros) ---
  config.maxledlevel   = getJsonParamRange(doc["ledRGB"]["maxledlevel"], 10, 255, DFLT_MAXLEDLEVEL, "ledRGB.maxledlevel"); 
  config.dimmlevel     = getJsonParamRange(doc["ledRGB"]["dimmlevel"], 10, config.maxledlevel, DFLT_DIMMLEVEL, "ledRGB.dimmlevel"); 
  config.warnESP32temp = getJsonParamRange(doc["warnESP32temp"], 40, 99, DFLT_MAX_ESP32_TEMP, "warnESP32temp"); 
  config.tempOffset    = getJsonParamRange(doc["tempOffset"], -5, 5, DFLT_TEMP_OFFSET, "tempOffset"); 
  config.msgdisplaymillis = getJsonParamRange(doc["msgdisplaymillis"], 1000, 4000, DFLT_MSGDISPLAYMS, "msgdisplaymillis"); 
  config.volume           = getJsonParamRange(doc["volume"], 1, 10, DFLT_VOLUME, "volume"); 
  config.finMelody        = getJsonParamRange(doc["finMelody"], 1, finMelodynum-1, DFLT_FINMELODY, "finMelody"); 
  //-------------------------------------------------------------------------------------------


  //-------------------------------------------------------------------------------------------
  if (config.SCD_ip[0] == '\0') LOG_WARN("IP de Domoticz no definida en el fichero de parámetros");
  return config.initialized;
} // end loadConfigFromFile

bool writeConfigToFile(const char *p_filename)
{
  LOG_TRACE("");
  // "w" trunca el archivo automáticamente, no hace falta remove() previo
  File file = LittleFS.open(p_filename, "w");
  if(!file){
    LOG_ERROR("Failed to open file for writing", p_filename);
    return false;
  }
  JsonDocument doc;
  //--------------  FIRMA Y CONTROL DE VERSIÓN -----------------------
  doc["fileType"] = "CCR_config";
  doc["version"]  = PARMVERSION;
  doc["SCDtype"]  = SCDTYPE; // por si en el futuro queremos usar el mismo formato de fichero para otros tipos de sistemas de control domótico
  //--------------  procesa botones (IDX)  --------------------------------------------------
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
      zonas.add(config.group[i].zNumber[j]);
    }  
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  doc["tiempo"]["minutos"]  = config.minutes; 
  doc["tiempo"]["segundos"] = config.seconds;
  doc["domoticz"]["ip"]     = config.SCD_ip;
  doc["domoticz"]["port"]   = config.SCD_port;
  doc["domoticz"]["user"]   = config.SCD_user;
  doc["domoticz"]["password"] = config.SCD_password;
  doc["time"]["ntpServer"]  = config.ntpServer;
  doc["time"]["timeZone"]   = config.TZ;
  doc["ledRGB"]["maxledlevel"]  = config.maxledlevel; 
  doc["ledRGB"]["dimmlevel"]    = config.dimmlevel; 
  doc["warnESP32temp"]      = config.warnESP32temp; 
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
    file.close();
    return false;
  }
  else LOG_DEBUG("    tamaño del jsondoc: (",docsize,")");
  file.close();
  LOG_INFO("Parametros guardados OK en ", p_filename);
  return true;
} // end writeConfigToFile


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

void saveRiegosToFile(const char* filename, const char* arrayName, S_timeRiego* tabla, size_t size, bool initialize) {
    // no guardar en modo demo o si la hora o fecha no es correcta (antes del 1 de enero de 2026 00:00 GMT)
    if(!initialize && (Estado.modoDEMO || !timeOK || time(NULL)<UMBRAL_EPOCH)) return;
    JsonDocument doc;
    JsonArray arr = doc[arrayName].to<JsonArray>();
    for (size_t i = 0; i < size; i++) {
        JsonObject obj = arr.add<JsonObject>(); 
        obj["inicio"] = tabla[i].inicio;
        obj["final"]  = tabla[i].final;
        obj["total"]  = tabla[i].total;
    }
    File file = LittleFS.open(filename, "w");
    if (!file) {
        LOG_ERROR("Error abriendo el fichero para guardar", arrayName);
        return;
    }
    serializeJson(doc, file);
    file.close();
    LOG_DEBUG(arrayName, "guardado OK.");
}

bool loadRiegosFromFile(const char* filename, const char* arrayName, S_timeRiego* tabla, size_t size) {
    JsonDocument doc;
    if (!abrirYDeserializarJson(filename, doc, MAX_FILE_SIZE)) return false;
    JsonArray arr = doc[arrayName];
    if (!arr) {
        LOG_ERROR("No se encontró el array", arrayName, "en el fichero", filename);
        return false;
    }
    for (size_t i = 0; i < size && i < arr.size(); i++) {
        JsonObject obj = arr[i];
        tabla[i].inicio = obj["inicio"] | 0;
        tabla[i].final  = obj["final"]  | 0;
        tabla[i].total  = obj["total"]  | 0;
    }
    LOG_DEBUG(arrayName, "cargado OK.");
    return true;
}


//init minimo de config para evitar fallos en caso de no poder cargar parametros de ficheros
void zeroConfig() {
  LOG_TRACE("");
  config = Config_parm{}; //reset estructura config existente a valores por defecto
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
  Serial.printf("\tSCD_ip= %s / SCD_port= %s \n", config.SCD_ip, config.SCD_port);
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

// Función para parsear la URI de conexión a Domoticz y extraer IP, usuario y contraseña
// bool parseSCDuri(const String& uri) {
//     config.SCD_user[0] = config.SCD_password[0] = '\0'; // Reseteamos credenciales por si no vienen en la URI
//     if (uri.length() == 0) return false;
//     int atIndex = uri.indexOf('@');
//     String authPart = "";
//     String hostPart = "";
//     // 1. Dividir entre Credenciales y Host
//     if (atIndex != -1) {
//         authPart = uri.substring(0, atIndex); // "user:pass"
//         hostPart = uri.substring(atIndex + 1); // "ip"
//     } else hostPart = uri; // Solo hay ip, sin credenciales
//     // 2. Procesar Usuario y Password
//     if (authPart.length() > 0) {
//         int colonAuthIndex = authPart.indexOf(':');
//         if (colonAuthIndex != -1) {
//             strlcpy(config.SCD_user, authPart.substring(0, colonAuthIndex).c_str(), sizeof(config.SCD_user));
//             strlcpy(config.SCD_password, authPart.substring(colonAuthIndex + 1).c_str(), sizeof(config.SCD_password));
//         } else {
//             strlcpy(config.SCD_user, authPart.c_str(), sizeof(config.SCD_user));
//         }
//     }
//     strlcpy(config.SCD_ip, hostPart.c_str(), sizeof(config.SCD_ip));
//     return (config.SCD_ip[0] != '\0'); // Retornamos true si al menos existe el campo IP
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
    auto printIndent = [depth]() {
        for (uint8_t i = 0; i < depth; i++) Serial.print("   "); // Tres espacios por nivel
    };
    printIndent();
    Serial.printf("Listing directory: %s\r\n", dirname);
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;
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
            // Alineamos "[DIR] nombre" a 35 caracteres
            Serial.printf("[DIR]  %-30s LAST WRITE: %s\r\n", file.name(), timeStr);
            if (levels > 0) {
                String subDirPath = dirname;
                if (!subDirPath.endsWith("/")) subDirPath += "/";
                subDirPath += file.name();
                listDir(fs, subDirPath.c_str(), levels - 1, depth + 1);
            }
        } else {
            // EXPLICACIÓN: %-30s alinea a la izquierda y rellena con espacios hasta 30
            // %8lu alinea el tamaño a la derecha para que las unidades coincidan
            Serial.printf("[FILE] %-30s SIZE: %8lu bytes  LAST WRITE: %s\r\n", 
                          file.path(), 
                          (unsigned long)file.size(), 
                          timeStr);
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
