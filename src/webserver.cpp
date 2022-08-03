//servidor web para actualizaciones OTA del FW o del filesystem
#ifdef WEBSERVER
   #include "Control.h"
   
   // The text of builtin files are in this header file
   #include "builtinfiles.h"

   // mark parameters not used in example
   #define UNUSED __attribute__((unused))

   // TRACE2 output simplified, can be deactivated here
   #define TRACE2(...) Serial.printf(__VA_ARGS__)


   //const char* host = "ardomo";
   int wsport = 8080;
   const char* update_path = "/$update";
   const char* update_username = "admin";
   const char* update_password = "admin";

   ESP8266WebServer wserver(wsport);
   ESP8266HTTPUpdateServer httpUpdater;

   // convierte timestamp a fecha hora
   String TS2Date(time_t t)
   {
   char buff[32];
   sprintf(buff, "%02d-%02d-%02d %02d:%02d:%02d", day(t), month(t), year(t), hour(t), minute(t), second(t));
   return buff;
   }



   // ===== Simple functions used to answer simple GET requests =====

   // This function is called when the WebServer was requested without giving a filename.
   // This will redirect to the file index.htm when it is existing otherwise to the built-in $upload.htm page
   void handleRedirect() {
   TRACE2("Redirect...");
   String url = "/index.htm";

   if (!LittleFS.exists(url)) { url = "/$upload.htm"; }

   wserver.sendHeader("Location", url, true);
   wserver.send(302);
   }  // handleRedirect()

   // This function is called when the WebServer was requested to list all existing files in the filesystem.
   // a JSON array with file information is returned.
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
      // jc.addProperty("size", dir.fileSize());
   }  // while
   result += "]";
   wserver.sendHeader("Cache-Control", "no-cache");
   wserver.send(200, "text/javascript; charset=utf-8", result);
   }  // handleListFiles()

   // This function is called when the WebServer was requested to list all existing files in the filesystem.
   void handleListFiles2() {
   //LittleFS.begin();
   FSInfo fs_info;
   LittleFS.info(fs_info);
   Dir dir = LittleFS.openDir("/");
   String result;

   float fileTotalKB = (float)fs_info.totalBytes / 1024.0; 
   float fileUsedKB = (float)fs_info.usedBytes / 1024.0; 
   result += "__________________________\n";
   result += "File system (LittleFS): \n";
   result += "    Total KB: " + String(fileTotalKB) + " KB \n";
   result += "    Used  KB: " + String(fileUsedKB) + " KB \n";
   result += "    Maximum open files: "  + String(fs_info.maxOpenFiles) + "\n";
   result += "__________________________\n\n";

   result += "LittleFS directory {/} :\n\n";
   result += "\t\t\t\ttamaño \tcreado \t\t\tmodificado \n";
   while (dir.next()) {
      time_t fct = dir.fileCreationTime();
      time_t fwt = dir.fileTime();
      result += "\t";
      result += dir.fileName() ;
      result += "\t" + String(dir.fileSize());
      result += "\t" + TS2Date(fct);
      result += "\t" + TS2Date(fwt);
      result += " \n";
   }  // while
   wserver.sendHeader("Cache-Control", "no-cache");
   wserver.send(200, "text/plain; charset=utf-8", result);
   }  // handleListFiles()



   // This function is called when the sysInfo service was requested.
   void handleSysInfo() {
   //LittleFS.begin();
   FSInfo fs_info;
   LittleFS.info(fs_info);
   String result;

   float fileTotalKB = (float)fs_info.totalBytes / 1024.0; 
   float fileUsedKB = (float)fs_info.usedBytes / 1024.0; 

   result += "__________________________\n";
   result += "SysInfo :\n";
   result += "\t flashSize : \t\t" + String(ESP.getFlashChipSize()) + "\n";
   result += "\t freeSketchSpace : \t" + String(ESP.getFreeSketchSpace()) + "\n";
   result += "\t freeHeap : \t\t" + String(ESP.getFreeHeap()) + "\n";
   result += "\t HeapFragmentation : \t" + String(ESP.getHeapFragmentation()) + "\n";
   result += "\t MaxFreeBlockSize : \t" + String(ESP.getMaxFreeBlockSize()) + "\n";
   result += "__________________________\n";
   result += "File system (LittleFS): \n";
   result += "\t    Total KB: " + String(fileTotalKB) + " KB \n";
   result += "\t    Used  KB: " + String(fileUsedKB) + " KB \n";
   result += "\t    Maximum open files: "  + String(fs_info.maxOpenFiles) + "\n";
   result += "__________________________\n\n";
   result += "\n";
   wserver.sendHeader("Cache-Control", "no-cache");
   wserver.send(200, "text/plain; charset=utf-8", result);
   }  // handleSysInfo()



   // ===== Request Handler class used to answer more complex requests =====

   // The FileServerHandler is registered to the web server to support DELETE and UPLOAD of files into the filesystem.
   class FileServerHandler : public RequestHandler {
   public:
   // @brief Construct a new File Server Handler object
   // @param fs The file system to be used.
   // @param path Path to the root folder in the file system that is used for serving static data down and upload.
   // @param cache_header Cache Header to be used in replies.
   FileServerHandler() {
      TRACE2("FileServerHandler is registered\n");
   }


   // @brief check incoming request. Can handle POST for uploads and DELETE.
   // @param requestMethod method of the http request line.
   // @param requestUri request ressource from the http request line.
   // @return true when method can be handled.
   bool canHandle(HTTPMethod requestMethod, const String UNUSED &_uri) override {
      return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE));
   }  // canHandle()


   bool canUpload(const String &uri) override {
      // only allow upload on root fs level.
      return (uri == "/");
   }  // canUpload()


   bool handle(ESP8266WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
      // ensure that filename starts with '/'
      String fName = requestUri;
      if (!fName.startsWith("/")) { fName = "/" + fName; }

      if (requestMethod == HTTP_POST) {
         // all done in upload. no other forms.

      } else if (requestMethod == HTTP_DELETE) {
         if (LittleFS.exists(fName)) { LittleFS.remove(fName); }
      }  // if

      wserver.send(200);  // all done.
      return (true);
   }  // handle()


   // uploading process
   void upload(ESP8266WebServer UNUSED &server, const String UNUSED &_requestUri, HTTPUpload &upload) override {
      // ensure that filename starts with '/'
      String fName = upload.filename;
      if (!fName.startsWith("/")) { fName = "/" + fName; }

      if (upload.status == UPLOAD_FILE_START) {
         // Open the file
         if (LittleFS.exists(fName)) { LittleFS.remove(fName); }  // if
         _fsUploadFile = LittleFS.open(fName, "w");

      } else if (upload.status == UPLOAD_FILE_WRITE) {
         // Write received bytes
         if (_fsUploadFile) { _fsUploadFile.write(upload.buf, upload.currentSize); }

      } else if (upload.status == UPLOAD_FILE_END) {
         // Close the file
         if (_fsUploadFile) { _fsUploadFile.close(); }
      }  // if
   }    // upload()

   protected:
   File _fsUploadFile;
   };

   void defWebpages() {
      TRACE2("Register service handlers...\n");

   // serve a built-in htm page
   wserver.on("/$upload.htm", []() {
      wserver.send(200, "text/html", FPSTR(uploadContent));
   });

   // register a redirect handler when only domain name is given.
   wserver.on("/", HTTP_GET, handleRedirect);

   // register some REST services
   wserver.on("/$list", HTTP_GET, handleListFiles);
   wserver.on("/$sysinfo", HTTP_GET, handleSysInfo);

   // UPLOAD and DELETE of files in the file system using a request handler.
   wserver.addHandler(new FileServerHandler());

   // enable CORS header in webserver results
   wserver.enableCORS(true);

   // enable ETAG header in webserver results from serveStatic handler
   //wserver.enableETag(true);

   // serve all static files
   wserver.serveStatic("/", LittleFS, "/");

   // handle cases when file is not found
   wserver.onNotFound([]() {
      // standard not found in browser.
      wserver.send(404, "text/html", FPSTR(notFoundContent));
   });

   }


   void setupWS()
   {
      if (!LittleFS.begin()) TRACE2("could not mount the filesystem...\n");
      if (!MDNS.begin(HOSTNAME)) Serial.println("Error iniciando mDNS");
      else Serial.println("mDNS iniciado");
      httpUpdater.setup(&wserver, update_path, update_username, update_password);
      defWebpages();
      MDNS.addService("http", "tcp", wsport);
      MDNS.announce();
      wserver.begin();
      Serial.println(F("[WS] HTTPUpdateServer ready!"));
      Serial.printf("[WS]    --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", WiFi.getHostname(), wsport, update_path, update_username, update_password);
      TRACE2("hostname=%s\n", WiFi.getHostname());
   }

   void procesaWebServer()
   {
      wserver.handleClient();
      MDNS.update();
   }  

   void endWS()
   {
      TRACE2("cerrando filesystem...\n");
      LittleFS.end();
      TRACE2("terminando MDNS...\n");
      MDNS.end();
      TRACE2("terminando webserver...\n");
      wserver.stop();
   }


#endif