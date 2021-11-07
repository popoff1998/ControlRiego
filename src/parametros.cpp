
#include "Control.h"

//Config_parm config;

bool loadConfigFile(const char *filename, Config_parm &cfg)
{
  #ifdef TRACE
    Serial.println("TRACE: in loadConfigFile");
  #endif

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return false;
  }
  File file = LittleFS.open(filename, "r");
  if(!file){
    Serial.println("Failed to open file for reading");
    return false;
  }
  size_t size = file.size();
  if (size > 2048) {
    Serial.println("Config file size is too large");
    return false;
  }
  Serial.printf("\t tamaño de %s --> %d bytes \n", filename, size);

  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.print(F("\t  deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }
  Serial.printf("\t memoria usada por el jsondoc: (%d) \n" , doc.memoryUsage());
  //--------------  procesa botones (IDX)  --------------------------------------------------
  int numzonas = doc["numzonas"]; // 7
  if (numzonas != cfg.n_Zonas) {
    Serial.print("ERROR numero de zonas incorrecto");
    return false;
  }  
  for (JsonObject botones_item : doc["botones"].as<JsonArray>()) {
    int i = botones_item["zona"]; // 1, 2, 3, 4, 5, 6, 7
    cfg.botonConfig[i-1].idx = botones_item["idx"];
    strlcpy(cfg.botonConfig[i-1].desc, botones_item["nombre"], sizeof(cfg.botonConfig[i-1].desc));
    //actualiza la descripcion leida en estructura Boton:
    strlcpy(Boton[i-1].desc,  cfg.botonConfig[i-1].desc, sizeof(Boton[i-1].desc));
    i++;
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  cfg.minutes = doc["tiempo"]["minutos"]; // 0
  cfg.seconds = doc["tiempo"]["segundos"]; // 10
  strlcpy(cfg.domoticz_ip, doc["domoticz"]["ip"], sizeof(cfg.domoticz_ip));
  strlcpy(cfg.domoticz_port, doc["domoticz"]["port"], sizeof(cfg.domoticz_port));
  strlcpy(cfg.ntpServer, doc["ntpServer"], sizeof(cfg.ntpServer));
  int numgroups = doc["numgroups"];
  if (numgroups != cfg.n_Grupos) {
    Serial.print("ERROR numero de grupos incorrecto");
    return false;
  }  
  //--------------  procesa grupos  ---------------------------------------------------------
  for (JsonObject groups_item : doc["groups"].as<JsonArray>()) {
    int i = groups_item["grupo"]; // 1, 2, 3
    cfg.groupConfig[i-1].id = GRUPOS[i-1];  //obtiene el id del boton de ese grupo (ojo: no viene en el json)
    cfg.groupConfig[i-1].size = groups_item["size"];
    strlcpy(cfg.groupConfig[i-1].desc, groups_item["desc"], sizeof(cfg.groupConfig[i-1].desc)); 
    JsonArray array = groups_item["zonas"].as<JsonArray>();
    int count = array.size();
    if (count != cfg.groupConfig[i-1].size) {
      Serial.print("ERROR tamaño del grupo incorrecto");
      return false;
    }  
    int j = 0;
    for(JsonVariant zonas_item_elemento : array) {
      cfg.groupConfig[i-1].serie[j] = zonas_item_elemento.as<int>();
      j++;
    }
    i++;
  }

  file.close();
  cfg.initialized = 1;
  return true;
}

bool saveConfigFile(const char *filename, Config_parm &cfg)
{
  #ifdef TRACE
    Serial.println("TRACE: in saveConfigFile");
  #endif
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return false;
  }
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(filename);
  File file = LittleFS.open(filename, "w");
  if(!file){
    Serial.println("Failed to open file for writing");
    return false;
  }
  DynamicJsonDocument doc(1536);
  //--------------  procesa botones (IDX)  --------------------------------------------------
  doc["numzonas"] = NUMZONAS;
  doc["botones"].as<JsonArray>();
  for (int i=0; i<NUMZONAS; i++) {
    doc["botones"][i]["zona"]   = i+1;
    doc["botones"][i]["idx"]    = cfg.botonConfig[i].idx;
    doc["botones"][i]["nombre"] = cfg.botonConfig[i].desc;
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  doc["tiempo"]["minutos"]  = cfg.minutes; 
  doc["tiempo"]["segundos"] = cfg.seconds;
  doc["domoticz"]["ip"]     = cfg.domoticz_ip;
  doc["domoticz"]["port"]   = cfg.domoticz_port;
  doc["ntpServer"]          = cfg.ntpServer;
  //--------------  procesa grupos  ---------------------------------------------------------
  doc["numgroups"]          = NUMGRUPOS;
  doc["groups"].as<JsonArray>();
  for (int i=0; i<NUMGRUPOS; i++) {
    doc["groups"][i]["grupo"]   = i+1;
    doc["groups"][i]["desc"]    = cfg.groupConfig[i].desc;
    doc["groups"][i]["size"]    = cfg.groupConfig[i].size;
    doc["groups"][i]["zonas"].as<JsonArray>();
    for(int j=0; j<cfg.groupConfig[i].size; j++) {
      doc["groups"][i]["zonas"][j] = cfg.groupConfig[i].serie[j];
    }
  }
  // Serialize JSON to file
  #ifdef DEBUG 
    serializeJsonPretty(doc, Serial); 
  #endif
  if (serializeJson(doc, file) == 0) Serial.println(F("Failed to write to file"));
  file.close();
  return true;
}


bool copyConfigFile(const char *fileFrom, const char *fileTo)
{
  #ifdef TRACE
    Serial.println("TRACE: in copyConfigFile");
  #endif
  if(!LittleFS.begin()) {
  Serial.println("An Error has occurred while mounting LittleFS");
  return false;
  }
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(fileTo);
  File origen = LittleFS.open(fileFrom, "r");
  if (!origen) {
    Serial.print("- failed to open file "); 
    Serial.println(fileFrom);
    return false;
  }
  else{
    Serial.printf("copiando %s en %s \n", fileFrom, fileTo);
    File destino = LittleFS.open(fileTo, "w+");
    if(!destino){
      Serial.println("Failed to open file for writing"); Serial.println(fileTo);
      return false;
    }
    else {
      while(origen.available()){
        destino.print(origen.readStringUntil('\n')+"\n");
      }
      destino.close(); 
    }
    origen.close();    
    return true;
  } 
}

void cleanFS() {
  Serial.println(F("\n\n[cleanFS]Wait. . .Borrando File System!!!"));
  LittleFS.format();
  Serial.println(F("Done!"));
}

void printParms(Config_parm &cfg) {
  Serial.println("Estructura parametros configuracion: \n");
  //--------------  imprime array botones (IDX)  --------------------------------------------------
  Serial.printf("numzonas= %d \n", cfg.n_Zonas);
  Serial.println("Botones: ");
  for(int i=0; i<7; i++) {
    Serial.printf("\t Zona%d: IDX=%d %s \n", i+1, cfg.botonConfig[i].idx, cfg.botonConfig[i].desc);
  }
  //--------------  imprime parametro individuales   ----------------------------------------
  Serial.printf("minutes= %d seconds= %d \n", cfg.minutes, cfg.seconds);
  Serial.printf("domoticz_ip= %s domoticz_port= %s \n", cfg.domoticz_ip, cfg.domoticz_port);
  Serial.printf("ntpServer= %s \n", cfg.ntpServer);
  Serial.printf("numgroups= %d \n", cfg.n_Grupos);
  //--------------  imprime array y subarray de grupos  ----------------------------------------------
  for(int i = 0; i < cfg.n_Grupos; i++) {
    Serial.printf("Grupo%d: size=%d (%s)\n", i+1, cfg.groupConfig[i].size, cfg.groupConfig[i].desc);
    for(int j = 0; j < cfg.groupConfig[i].size; j++) {
      Serial.printf("\t Zona%d \n", cfg.groupConfig[i].serie[j]);
    }
  }
}


// funciones solo usadas en DEVELOP
#ifdef DEBUG
// Prints the content of a file to the Serial 
void printFile(const char *filename) {
   #ifdef TRACE
    Serial.printf("TRACE: in printFile (%s) \n" , filename);
  #endif
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
  return;
  }
  // Open file for reading
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println(F("Failed to open config file"));
    return;
  }
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}
#endif
