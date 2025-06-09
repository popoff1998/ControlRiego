//servidor web para actualizaciones OTA del FW o del filesystem
// Adaptado de:
// @file WebServer.ino (GITHUB: arduino-esp32/libraries/WebServer/examples/WebServer/WebServer.ino)
// @brief Example WebServer implementation using the ESP32 WebServer
// and most common use cases related to web servers.
//
// * Setup a web server
// * redirect when accessing the url with servername only
// * get real time by using builtin NTP functionality
// * send HTML responses from Sketch (see builtinfiles.h)
// * use a LittleFS file system on the data partition for static files
// * use http ETag Header for client side caching of static files
// * use custom ETag calculation for static files
// * extended FileServerHandler for uploading and deleting static files
// * serve APIs using REST services (/api/list, /api/sysinfo)
// * define HTML response when no file/api/handler was found
//
// See also README.md for instructions and hints.
//
// Please use the following Arduino IDE configuration
//
// * Board: ESP32 Dev Module
// * Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
//     but LittleFS will be used in the partition (not SPIFFS)
// * other setting as applicable
//
// Changelog:
// 21.07.2021 creation, first version
// 08.01.2023 ESP32 version with ETag

#ifdef WEBSERVER
   #include "Control.h"
   
   // The text of builtin files are in this header file
   #include "builtinfiles.h"

   // enable the CUSTOM_ETAG_CALC to enable calculation of ETags by a custom function
   #define CUSTOM_ETAG_CALC

   // mark parameters not used in example
   #define UNUSED __attribute__((unused))

   // local time zone definition (Madrid)
   //#define TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"


   //int wsport = 8080;
   const char* update_path = "/$update";
   const char* update_username = "admin";
   const char* update_password = "admin";

   #ifdef DEVELOP
      #define TRACE2(...) Serial.printf(__VA_ARGS__)
      const bool httpUpdateDebug = true;  //enable serial debug msgs
   #else
      const bool httpUpdateDebug = false;
      #define TRACE2(...)           // TRACE2 output simplified, can be deactivated here
   #endif 

    WebServer wserver(WSPORT);
    HTTPUpdateServer httpUpdater(httpUpdateDebug);  

// ===== Simple functions used to answer simple GET requests =====

// This function is called when the WebServer was requested without giving a filename.
// This will redirect to the file index.htm when it is existing otherwise to the built-in $upload.htm page
void handleRedirect() 
{
  TRACE2("Redirect...\n");
  String url = "/index.htm";
  if (!LittleFS.exists(url)) { url = "/$upload.htm"; }
  wserver.sendHeader("Location", url, true);
  wserver.send(302);
}  // handleRedirect()


// This function is called when the WebServer was requested to list existing files in the filesystem.
// The request can contain the following arguments:
// - dir: the directory to be listed (default is '/')
// - file: the filter for the file names (default is '')
// a JSON array with file information is returned.
void handleListFiles() 
{
  TRACE2("handleListFiles, Argumentos recibidos:\n");
  for (int i = 0; i < wserver.args(); i++) {
      TRACE2("  %s: %s\n", wserver.argName(i).c_str(), wserver.arg(i).c_str());
  }
  String path = "/";
  if (wserver.hasArg("dir")) path = wserver.arg("dir");
  TRACE2("handleListFiles, listing: %s\n", path);
  String filter = "";
  if (wserver.hasArg("file")) {
    filter = wserver.arg("file");
    if (filter == "%PARMFILE%") { filter = parmFile; } // replace the %PARMFILE% with the real filename in variable parmFile
    if (filter == "%BACKUPFILE%") { filter = backupParmFile; } // replace the %BACKUPFILE% with the real filename in variable backupParmFile
    if (filter.startsWith("/")) { filter = filter.substring(1); }
    TRACE2("handleListFiles filter: %s\n", filter.c_str());
  }
  File dir = LittleFS.open(path, "r");
  String result;
  result += "[\n";
  while (File entry = dir.openNextFile()) {
    String filename = String(entry.name());
    if (filename.startsWith(filter) || filter == "") {
        if (result != "[\n") { result += ",\n"; }
        result += "  {";
        result += "\"type\": \"" + String(entry.isDirectory() ? "dir" : "file") + "\", ";
        result += "\"name\": \"" + String(entry.path()).substring(1) + "\", ";
        result += "\"size\": " + String(entry.size()) + ", ";
        result += "\"time\": " + String(entry.getLastWrite());
        result += "}";
    } // if    
  }  // while
  result += "\n]";
  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "text/json; charset=utf-8", result);
}  // handleListFiles()


// This function is called when the WebServer was requested to restart the ESP32.
void handleRestart() 
{
  TRACE2("Restarting ESP32...\n");
  wserver.send(200, "text/plain", "Restarting ESP32...");
  delay(500); // Give the client time to receive the response
  ESP.restart();
}


// This function is called when the sysInfo service was requested.
void handleSysInfo() 
{
  String result = sysInfo(); // get the system information as JSON string
  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "text/javascript; charset=utf-8", result);
}  // handleSysInfo()


void handleSaveConfig() 
{
  TRACE2("handleSaveConfig entrada");
    if (!wserver.hasArg("plain")) {
        wserver.send(400, "text/plain", "Bad Request: Missing JSON body");
        return;
    }
    // Obtener el cuerpo de la solicitud (JSON enviado por el cliente)
    String jsonBody = wserver.arg("plain");
    // Guardar el JSON en un archivo en el sistema de archivos
    File configFile = LittleFS.open(parmFile, "w");
    if (!configFile) {
        wserver.send(500, "text/plain", "Internal Server Error: Could not open file for writing");
        return;
    }
    // Escribir el contenido del JSON en el archivo
    configFile.print(jsonBody);
    configFile.close();
    // Responder al cliente con éxito
    wserver.send(200, "text/plain", "Configuration saved successfully");
}

void handleAdvancedPage() {
  // Verificar credenciales
  if (!wserver.authenticate(update_username, update_password)) {
      wserver.requestAuthentication(); // Solicitar autenticación si las credenciales son incorrectas
      return;
  }

  // Enviar la página si las credenciales son correctas
  File advancedFile = LittleFS.open("/advanced.htm", "r");
  if (!advancedFile) {
    wserver.send(500, "text/plain", "Internal Server Error: Could not open advanced.htm");
    return;
  }
  String advancedContent = advancedFile.readString();
  advancedFile.close();
  wserver.send(200, "text/html", advancedContent);
}


void file_download(String filename)
{
    TRACE2("file_download filename recibido: %s\n", filename.c_str());
    File download = LittleFS.open("/"+filename);
    TRACE2("  download name %s\n", download.name());
    TRACE2(" download size %d\n", download.size());
    if (download) 
    {
      wserver.sendHeader("Content-Type", "text/text");
      wserver.sendHeader("Content-Disposition", "attachment; filename="+filename);
      wserver.sendHeader("Connection", "close");
      wserver.streamFile(download, "application/octet-stream");
      download.close();
    } else wserver.send(404, "text/plain", "File not found");
}  // file_download()


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
      bool canHandle(HTTPMethod requestMethod, String UNUSED uri) override {
        return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE) || (requestMethod == HTTP_COPY));
      }  // canHandle()


      bool canUpload(String uri) override {
        // only allow upload on root fs level.
        TRACE2("canUpload uri received: %s\n", uri.c_str());
        return (uri == "/");
      }  // canUpload()


      bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override 
      {
        String fName = requestUri;
        if (!fName.startsWith("/")) { fName = "/" + fName; } // ensure that filename starts with '/'
        bool handleOK = false;
        TRACE2("handle %s\n", fName.c_str());
        if (requestMethod == HTTP_POST) {
          // all done in upload. no other forms.
          TRACE2("POST %s\n", fName.c_str());
          handleOK = true;
        } 
        if (requestMethod == HTTP_COPY) {
          String fileFrom , fileTo;
          TRACE2("COPY %s\n", fName.c_str());
          if (fName == "/BACKUP") {fileFrom = parmFile; fileTo = backupParmFile;}
          if (fName == "/RESTORE") {fileFrom = backupParmFile; fileTo = parmFile;}
          TRACE2("HTTP_COPY %s : %s to %s\n", fName.c_str(), fileFrom.c_str(), fileTo.c_str());
          handleOK = copyConfigFile(fileFrom.c_str(), fileTo.c_str());
        }  
        if (requestMethod == HTTP_DELETE) {
          if (LittleFS.exists(fName)) {
            TRACE2("DELETE %s\n", fName.c_str());
            handleOK = LittleFS.remove(fName);
          }
        }
        if (handleOK) {
          // send a 200 OK response to the client
          wserver.send(200, "text/plain", "OK");
          return (true);
        } else {
          wserver.send(500, "text/plain", "ERROR");
          return (false);
        }
      }  // handle()


      // uploading process
      void upload(WebServer UNUSED &server, String UNUSED _requestUri, HTTPUpload &upload) override 
      {
        static size_t uploadSize;
        if (upload.status == UPLOAD_FILE_START) {
          String fName = upload.filename;
          if (fName == "%PARMFILE%") { fName = parmFile; } // replace the %PARMFILE% with the real filename in variable parmFile
          // Open the file for writing
          if (!fName.startsWith("/")) { fName = "/" + fName; } // ensure that filename starts with '/'
          TRACE2("start uploading file %s...\n", fName.c_str());
          if (LittleFS.exists(fName)) {
            LittleFS.remove(fName);
          }  // if
          _fsUploadFile = LittleFS.open(fName, "w");
          uploadSize = 0;
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          // Write received bytes
          if (_fsUploadFile) {
            size_t written = _fsUploadFile.write(upload.buf, upload.currentSize);
            if (written < upload.currentSize) {
              // upload failed
              TRACE2("  write error!\n");
              _fsUploadFile.close();
              // delete file to free up space in filesystem
              String fName = upload.filename;
              if (!fName.startsWith("/")) { fName = "/" + fName; }
              LittleFS.remove(fName);
            }
            uploadSize += upload.currentSize;
            // TRACE2("free:: %d of %d\n", LittleFS.usedBytes(), LittleFS.totalBytes());
            // TRACE2("written:: %d of %d\n", written, upload.currentSize);
            // TRACE2("totalSize: %d\n", upload.currentSize + upload.totalSize);
          }  // if
        } else if (upload.status == UPLOAD_FILE_END) {
            TRACE2("finished.\n");
          // Close the file
          if (_fsUploadFile) {
            _fsUploadFile.close();
            TRACE2(" %d bytes uploaded.\n", upload.totalSize);
          }
        }  // if
      }  // upload()


    protected:
      File _fsUploadFile;
};

void defWebpages() 
{
    // TRACE2("Setup ntp...\n");
    // configTzTime(TIMEZONE, "es.pool.ntp.org");
    TRACE2("Register redirect...\n");
    // register a redirect handler when only domain name is given.
    wserver.on("/", HTTP_GET, handleRedirect);
    TRACE2("Register service handlers...\n");
    // serve a built-in htm page
    wserver.on("/$upload.htm", []() {
      wserver.send(200, "text/html", FPSTR(uploadContent));
    });
    // register some REST services
    wserver.on("/advanced.htm", HTTP_GET, handleAdvancedPage); // handle advanced.htm page with authentication
    wserver.on("/$list", HTTP_GET, handleListFiles);
    wserver.on("/$sysinfo", HTTP_GET, handleSysInfo);
    wserver.on("/$restart", HTTP_GET, handleRestart);
    wserver.on("/download", HTTP_GET, []() {
      // Extract the file name from the query parameter
      if (!wserver.hasArg("file")) {
        wserver.send(400, "text/plain", "Bad Request: Missing 'file' parameter");
        return;
      }
      String filename = wserver.arg("file");
      file_download(filename);
    });
    wserver.on("/save_config", HTTP_POST, handleSaveConfig);
    TRACE2("Register file system handlers...\n");
    // UPLOAD and DELETE of files in the file system using a request handler.
    wserver.addHandler(new FileServerHandler());
    // // enable CORS header in webserver results
    wserver.enableCORS(true);
    /*
      // enable ETAG header in webserver results (used by serveStatic handler)
    #if defined(CUSTOM_ETAG_CALC)
      // This is a fast custom eTag generator. It returns a value based on the time the file was updated like
      // ETag: 63bbceb5
      wserver.enableETag(true, [](FS &fs, const String &path) -> String {
        File f = fs.open(path, "r");
        String eTag = String(f.getLastWrite(), 16);  // use file modification timestamp to create ETag
        f.close();
        return (eTag);
      });
    #else
      // enable standard ETAG calculation using md5 checksum of file content.
      wserver.enableETag(true);
    #endif
    */
    // serve all static files
    wserver.serveStatic("/", LittleFS, "/");
    TRACE2("Register default (not found) answer...\n");
    // handle cases when file is not found
    wserver.onNotFound([]() {
      // standard not found in browser.
      wserver.send(404, "text/html", FPSTR(notFoundContent));
    });
}


void setupWS(Config_parm &config)
{
  // if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) LOG_ERROR("could not mount the filesystem...");
  if (!MDNS.begin(HOSTNAME)) LOG_ERROR("Error iniciando mDNS");
  else LOG_INFO("mDNS iniciado");
  httpUpdater.setup(&wserver, update_path, update_username, update_password);
  defWebpages();
  MDNS.addService("http", "tcp", WSPORT);
  wserver.begin();
  webServerAct = true;
  LOG_INFO("[WS] HTTPUpdateServer ready!");
  Serial.printf("[WS]    --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", WiFi.getHostname(), WSPORT, update_path, update_username, update_password);
  TRACE2("hostname=%s\n", WiFi.getHostname());
  LOG_INFO("[ConF][WS] IP address: ", WiFi.localIP());
  LOG_INFO("[ConF][WS] activado webserver para actualizaciones OTA de SW o filesystem");
  lcd.infoclear("OTA Webserver act", DEFAULTBLINK, BIPOK);
  snprintf(buff, MAXBUFF, "\"%s.local:%d\"", WiFi.getHostname(), WSPORT);
  lcd.info(buff, 3);
  int msgl = snprintf(buff, MAXBUFF, "%s:%d" , WiFi.localIP().toString().c_str(), WSPORT);
  lcd.info(buff, 4, msgl);
}

void procesaWebServer()
{
  wserver.handleClient();
}  

void endWS()
{
  // TRACE2("cerrando filesystem...\n");
  // LittleFS.end();
  TRACE2("terminando MDNS...\n");
  MDNS.end();
  TRACE2("terminando webserver...\n");
  wserver.stop();
  webServerAct = false;
}

/*

  ServerUtils.hpp
  
  */

  String GetContentType(String filename)
  {
    if(filename.endsWith(".htm")) return "text/html";
    else if(filename.endsWith(".html")) return "text/html";
    else if(filename.endsWith(".css")) return "text/css";
    else if(filename.endsWith(".json")) return "text/json";
    else if(filename.endsWith(".xml")) return "text/xml";
    else if(filename.endsWith(".png")) return "image/png";
    else if(filename.endsWith(".gif")) return "image/gif";
    else if(filename.endsWith(".jpg")) return "image/jpeg";
    else if(filename.endsWith(".ico")) return "image/x-icon";
    else if(filename.endsWith(".js")) return "application/javascript";
    else if(filename.endsWith(".pdf")) return "application/x-pdf";
    else if(filename.endsWith(".zip")) return "application/x-zip";
    else if(filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
  }
  
  void ServeFile(String path)
  {
     File file = LittleFS.open(path, "r");
     size_t sent = wserver.streamFile(file, GetContentType(path));
     file.close();
  }
  
  void ServeFile(String path, String contentType)
  {
     File file = LittleFS.open(path, "r");
     size_t sent = wserver.streamFile(file, contentType);
     file.close();
  }
  
  bool HandleFileRead(String path) 
  { 
    if (path.endsWith("/")) path += "index.html";
    Serial.println("handleFileRead: " + path);
    
    if (LittleFS.exists(path)) 
    {
      ServeFile(path);
      return true;
    }
    Serial.println("\tFile Not Found");
    return false;
  }
  
  bool HandleFileReadGzip(String path) 
  { 
    if (path.endsWith("/")) path += "index.html";
    Serial.println("handleFileRead: " + path);
    
    if (LittleFS.exists(path)) 
    {
      ServeFile(path, GetContentType(path));
      return true;
    }
    else 
    {
      String pathWithGz = path + ".gz";
      if (LittleFS.exists(pathWithGz)) 
      {
        ServeFile(pathWithGz, GetContentType(path));
        return true;
      }
    }
    Serial.println("\tFile Not Found");
    return false;
  }
  
#endif