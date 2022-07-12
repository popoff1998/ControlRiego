//servidor web para actualizaciones OTA del FW o del filesystem
#ifdef WEBSERVER
   #include "Control.h"

   const char* host = "ardomo";
   int wsport = 8080;
   const char* update_path = "/update";
   const char* update_username = "admin";
   const char* update_password = "admin";

   ESP8266WebServer wserver(wsport);
   ESP8266HTTPUpdateServer httpUpdater;

   // Funcion que se ejecutara en la URI '/'
   void handleRoot() 
   {
      wserver.send(200, "text/plain", "Hola mundo!");
   }
   
   // Funcion que se ejecutara en URI desconocida
   void handleNotFound() 
   {
      wserver.send(404, "text/plain", "ERROR 404 - Not found");
   }

   void handleListFiles() {
      Dir dir = LittleFS.openDir("/");
      String result;

      result += "[\n";
      while (dir.next()) {
         if (result.length() > 4) { result += ","; }
         result += "  {";
         result += " \"name\": \"" + dir.fileName() + "\", ";
         result += " \"size\": " + String(dir.fileSize()) + ", ";
         result += " \"time\": " + String(dir.fileTime());
         result += " }\n";
         result += "]";
         wserver.sendHeader("Cache-Control", "no-cache");
         wserver.send(200, "text/javascript; charset=utf-8", result);
      }   
   }

   // This function is called when the sysInfo service was requested.
   void handleSysInfo() {
      String result;

      FSInfo fs_info;
      LittleFS.info(fs_info);

      result += "{\n";
      result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
      result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
      result += "  \"freeSketchSpace\": " + String(ESP.getFreeSketchSpace()) + ",\n";
      result += "  \"HeapFragmentation\": " + String(ESP.getHeapFragmentation()) + ",\n";
      result += "  \"MaxFreeBlockSize\": " + String(ESP.getMaxFreeBlockSize()) + ",\n";
      result += "  \"fsTotalBytes\": " + String(fs_info.totalBytes) + ",\n";
      result += "  \"fsUsedBytes\": " + String(fs_info.usedBytes) + ",\n";
      result += "}";
      wserver.sendHeader("Cache-Control", "no-cache");
      wserver.send(200, "text/javascript; charset=utf-8", result);
   }

   // This function is called when the parm service was requested.
   // Prints the content of a file to a string 
   String displayFile(const char *p_filename) {
      #ifdef TRACE
      Serial.printf("TRACE: in displayFile (%s) \n" , p_filename);
      #endif
         String result;
      if(!LittleFS.begin()){
         result += "{\n An Error has occurred while mounting LittleFS \n}";
         return result;
      }
      // Open file for reading
      File file = LittleFS.open(p_filename, "r");
      if (!file) {
         result += "{\n Failed to open config file \n}";
         return result;
      }
      //result += "{\n File Content: \n";
      while(file.available()){
            result += char(file.read());
      }
      //result += "\n }";
      file.close();
      return result;
   }

   void handleListParm() {
      //const char *parmFile = "/config_parm.json";       // fichero de parametros activos
      String result = displayFile(parmFile);
      wserver.sendHeader("Cache-Control", "no-cache");
      wserver.send(200, "text/javascript; charset=utf-8", result);
   }

   // This function is called when the default service was requested.
   void handleListDefault() {
      //const char *parmFile = "/config_parm.json";       // fichero de parametros activos
      String result = displayFile(defaultFile);
      wserver.sendHeader("Cache-Control", "no-cache");
      wserver.send(200, "text/javascript; charset=utf-8", result);
   }

   void defWebpages()
   {
      /*
      // Ruteo para '/'
      wserver.on("/", handleRoot);
      // Ruteo para '/inline' usando función lambda
      wserver.on("/inline", []() {
         wserver.send(200, "text/plain", "Esto tambien funciona");
      });
      */
      wserver.on("/$sysinfo", handleSysInfo);
      wserver.on("/$list", handleListFiles);
      wserver.on("/$parm", handleListParm);
      wserver.on("/$def", handleListDefault);
      // Ruteo para URI desconocida
      wserver.onNotFound(handleNotFound);
   }

   void setupWS()
   {
   if (!MDNS.begin(host)) Serial.println("Error iniciando mDNS");
   else Serial.println("mDNS iniciado");
   httpUpdater.setup(&wserver, update_path, update_username, update_password);
   defWebpages();
   wserver.begin();
   MDNS.addService("http", "tcp", wsport);
   MDNS.announce();
   Serial.println(F("[WS] HTTPUpdateServer ready!"));
   Serial.printf("[WS]    --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", host, wsport, update_path, update_username, update_password);
   }

   void procesaWebServer()
   {
      wserver.handleClient();
      MDNS.update();
   }  


#endif