
#include "Control.h"

//Config_parm config;

bool loadConfigFile(const char *filename, Config_parm &cfg)
{
  if(!LittleFS.begin()){
    Serial.println("[loadConfigFile] An Error has occurred while mounting LittleFS");
    return false;
  }
  File file = LittleFS.open(filename, "r");
  if(!file){
    Serial.println("[loadConfigFile] Failed to open file for reading");
    return false;
  }
  size_t size = file.size();
  if (size > 2048) {
    Serial.println("[loadConfigFile] Config file size is too large");
    return false;
  }
  Serial.printf("\n\n tamaño de %s --> %d bytes \n\n", filename, size);

  DynamicJsonDocument doc(3072);

  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.print(F("\n[loadConfigFile]  deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }
  //--------------  procesa botones (IDX)  --------------------------------------------------
  int numzonas = doc["numzonas"]; // 7
  if (numzonas != cfg.n_Zonas) Serial.print("ERROR numero de zonas incorrecto");
  for (JsonObject botones_item : doc["botones"].as<JsonArray>()) {
    int i = botones_item["zona"]; // 1, 2, 3, 4, 5, 6, 7
    cfg.botonConfig[i-1].idx = botones_item["idx"]; // 25, 27, 58, 59, 24, 61, 30
    strlcpy(cfg.botonConfig[i-1].desc, botones_item["nombre"], sizeof(cfg.botonConfig[i-1].desc)); // "TURBINAS", "PORCHE", "CUARTILLO", "GOTEOALTO", ...
    i++;
  }
  //--------------  procesa parametro individuales   ----------------------------------------
  cfg.minutes = doc["tiempo"]["minutos"]; // 0
  cfg.seconds = doc["tiempo"]["segundos"]; // 10
  strlcpy(cfg.domoticz_ip, doc["domoticz"]["ip"], sizeof(cfg.domoticz_ip)); // "192.168.1.50"
  strlcpy(cfg.domoticz_port, doc["domoticz"]["port"], sizeof(cfg.domoticz_port)); // "8080"
  strlcpy(cfg.ntpServer, doc["ntpServer"], sizeof(cfg.ntpServer)); // "ToMaS"
  int numgroups = doc["numgroups"]; // 3
  if (numgroups != cfg.n_Grupos) Serial.print("ERROR numero de grupos incorrecto");
  //--------------  procesa grupos  ---------------------------------------------------------
  for (JsonObject groups_item : doc["groups"].as<JsonArray>()) {
    int i = groups_item["grupo"]; // 1, 2, 3
    cfg.groupConfig[i-1].id = GRUPOS[i-1];  //obtiene el id del boton de ese grupo (ojo: no viene en el json)
    cfg.groupConfig[i-1].size = groups_item["size"]; // 3, 7, 4
    strlcpy(cfg.groupConfig[i-1].desc, groups_item["desc"], sizeof(cfg.groupConfig[i-1].desc)); 
    JsonArray array = groups_item["zonas"].as<JsonArray>();
    int count = array.size();
    if (count != cfg.groupConfig[i-1].size) Serial.print("ERROR tamaño del grupo incorrecto");
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

// Prints the content of a file to the Seria 
void printFile(const char *filename) {
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
  return;
  }
  // Open file for reading
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println(F("[printFile] Failed to open config file"));
    return;
  }
  Serial.println("[printFile] File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void printParms(Config_parm &cfg) {
  Serial.println("\n[printParms] Estructura parametros configuracion: \n");
  //--------------  imprime array botones (IDX)  --------------------------------------------------
  Serial.printf("numzonas= %d \n", cfg.n_Zonas);
  Serial.println("Botones: ");
  for(int i=0; i<7; i++) {
    Serial.printf("  Zona%d: IDX=%d %s \n", i+1, cfg.botonConfig[i].idx, cfg.botonConfig[i].desc);
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
      Serial.printf("  Zona%d \n", cfg.groupConfig[i].serie[j]);
    }
  }
}

void cleanFS() {
  Serial.println(F("\n\n[cleanFS]Wait. . .Borrando File System!!!"));
  LittleFS.format();
  Serial.println(F("Done!"));
}

