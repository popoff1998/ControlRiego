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

// ---------------------------
// Helpers (send, utilidades)
// ---------------------------
static void sendNoCacheJSON(const String &payload) {
    wserver.sendHeader("Cache-Control", "no-cache");
    wserver.send(200, "application/json; charset=utf-8", payload);
}

static void sendTextResponse(int code, const char* contentType, const String &payload) {
    wserver.send(code, contentType, payload);
}

static String obtainFileName() {
    if (!wserver.hasArg("file")) {
      wserver.send(400, "text/plain", "Bad Request: Missing 'file' parameter");
      return "";
    }
    String filename = wserver.arg("file");
    if (!LittleFS.exists(filename)) {
      wserver.send(400, "text/plain", "Bad Request: file not found");
      return "";
    }
    return filename;  
}

static String buildFileListJSON(File &dir, const String &filter) {
    String result = "[\n";
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
      }
    }
    result += "\n]";
    return result;
}

static void sendFileAttachment(const String &filename) {
    File download = LittleFS.open("/"+filename);
    if (!download) {
      wserver.send(404, "text/plain", "File not found");
      return;
    }
    wserver.sendHeader("Content-Type", "text/text");
    wserver.sendHeader("Content-Disposition", "attachment; filename="+filename);
    wserver.sendHeader("Connection", "close");
    wserver.streamFile(download, "application/octet-stream");
    download.close();
}

// ---------------------------
// Server utils (moved here)
// ---------------------------
String GetContentType(String filename) {
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".json")) return "application/json";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void serveFile(String path, String contentType) {
   TRACE2("Serving file: %s contentType: %s \n", path.c_str(), contentType.c_str());
   File file = LittleFS.open(path, "r");
   size_t sent = wserver.streamFile(file, contentType);
   file.close();
}

void serveFile(String path) {
   String contentType = GetContentType(path);
   serveFile(path, contentType);
}   

bool HandleFileReadGzip(String path) {
  if (path.endsWith("/")) path += "index.html";
  Serial.println("handleFileRead: " + path);
  if (LittleFS.exists(path)) {
    serveFile(path, GetContentType(path));
    return true;
  } else {
    String pathWithGz = path + ".gz";
    if (LittleFS.exists(pathWithGz)) {
      serveFile(pathWithGz, GetContentType(path));
      return true;
    }
  }
  Serial.println("\tFile Not Found");
  return false;
}

// ---------------------------
// Handlers (agrupados y claros)
// ---------------------------

// redirect to index or upload
void handleRedirect() {
  TRACE2("Redirect...\n");
  String url = "/index.htm";
  if (!LittleFS.exists(url)) { url = "/$upload.htm"; }
  wserver.sendHeader("Location", url, true);
  wserver.send(302);
}

// list files as JSON
// This function is called when the WebServer was requested to list existing files in the filesystem.
// The request can contain the following arguments:
// - dir: the directory to be listed (default is '/')
// - file: the filter for the file names (default is '')
// a JSON array is returned with the file information.
void handleListFiles() {
  TRACE2("handleListFiles, Argumentos recibidos:\n");
  for (int i = 0; i < wserver.args(); i++) {
      TRACE2("  %s: %s\n", wserver.argName(i).c_str(), wserver.arg(i).c_str());
  }
  String path = "/";
  if (wserver.hasArg("dir")) path = wserver.arg("dir");
  TRACE2("handleListFiles, listing: %s\n", path.c_str());
  String filter = "";
  if (wserver.hasArg("file")) {
    filter = wserver.arg("file");
    if (filter == "%PARMFILE%") { filter = parmFile; }
    if (filter == "%BACKUPFILE%") { filter = backupParmFile; }
    if (filter.startsWith("/")) { filter = filter.substring(1); }
    TRACE2("handleListFiles filter: %s\n", filter.c_str());
  }
  File dir = LittleFS.open(path, "r");
  String result = buildFileListJSON(dir, filter);
  sendNoCacheJSON(result);
}

// restart device
void handleRestart() {
  TRACE2("Restarting ESP32...\n");
  wserver.send(200, "text/plain", "Restarting ESP32...");
  delay(500);
  ESP.restart();
}

// system info
void handleSysInfo() {
  String result = sysInfo();
  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "text/javascript; charset=utf-8", result);
}

// save config (body contains JSON)
void handleSaveConfig() {
  TRACE2("handleSaveConfig entrada");
  if (!wserver.hasArg("plain")) {
      wserver.send(400, "text/plain", "Bad Request: Missing JSON body");
      return;
  }
  String jsonBody = wserver.arg("plain");
  File configFile = LittleFS.open(parmFile, "w");
  if (!configFile) {
      wserver.send(500, "text/plain", "Internal Server Error: Could not open file for writing");
      return;
  }
  configFile.print(jsonBody);
  configFile.close();
  wserver.send(200, "text/plain", "Configuration saved successfully");
}

// advanced page (requires auth)
void handleAdvancedPage() {
  if (!wserver.authenticate(update_username, update_password)) {
      wserver.requestAuthentication();
      return;
  }
  File advancedFile = LittleFS.open("/advanced.htm", "r");
  if (!advancedFile) {
    wserver.send(500, "text/plain", "Internal Server Error: Could not open advanced.htm");
    return;
  }
  String advancedContent = advancedFile.readString();
  advancedFile.close();
  #ifdef DEVELOP
  wserver.send(200, "text/html", "<script>var showtest = true;</script>"+advancedContent);
  #else
  wserver.send(200, "text/html", "<script>var showtest = false;</script>"+advancedContent);
  #endif
}

// show zone log (reads log file / obtains Domoticz data)
void handleShowZONElog() {
  int zona = wserver.arg("zona").toInt();
  LOG_DEBUG("Zona recibida:", zona);
  String json = readLogFile(zona); // obtiene del Domoticz el log de riegos de la zona
  sendNoCacheJSON(json);
}

// download endpoint wrapper
void handleDownload() {
  String filename = obtainFileName();
  if (!filename.isEmpty()) sendFileAttachment(filename);
}

// servedirect endpoint wrapper
void handleShowFile() {
  String filename = obtainFileName();
  if (!filename.isEmpty()) serveFile(filename);
}

// ---------------------------
// FileServerHandler 
// ---------------------------
class FileServerHandler : public RequestHandler {
    public:
      FileServerHandler() {
        TRACE2("FileServerHandler is registered\n");
      }
      bool canHandle(HTTPMethod requestMethod, String UNUSED uri) override {
        return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE) || (requestMethod == HTTP_COPY));
      }
      bool canUpload(String uri) override {
        TRACE2("canUpload uri received: %s\n", uri.c_str());
        return (uri == "/");
      }
      bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override {
        String fName = wserver.urlDecode(requestUri); // elimina codificacion URL %..
        if (!fName.startsWith("/")) { fName = "/" + fName; }
        bool handleOK = false;
        TRACE2("handle %s\n", fName.c_str());
        if (requestMethod == HTTP_POST) {
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
          wserver.send(200, "text/plain", "OK");
          return (true);
        } else {
          wserver.send(500, "text/plain", "ERROR");
          return (false);
        }
      }
      void upload(WebServer UNUSED &server, String UNUSED _requestUri, HTTPUpload &upload) override {
        static size_t uploadSize;
        if (upload.status == UPLOAD_FILE_START) {
          String fName = upload.filename;
          if (fName == "%PARMFILE%") { fName = parmFile; }
          if (!fName.startsWith("/")) { fName = "/" + fName; }
          TRACE2("start uploading file %s...\n", fName.c_str());
          if (LittleFS.exists(fName)) LittleFS.remove(fName);
          _fsUploadFile = LittleFS.open(fName, "w");
          uploadSize = 0;
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (_fsUploadFile) {
            size_t written = _fsUploadFile.write(upload.buf, upload.currentSize);
            if (written < upload.currentSize) {
              TRACE2("  write error!\n");
              _fsUploadFile.close();
              String fName = upload.filename;
              if (!fName.startsWith("/")) { fName = "/" + fName; }
              LittleFS.remove(fName);
            }
            uploadSize += upload.currentSize;
          }
        } else if (upload.status == UPLOAD_FILE_END) {
            TRACE2("finished.\n");
          if (_fsUploadFile) {
            _fsUploadFile.close();
            TRACE2(" %d bytes uploaded.\n", upload.totalSize);
          }
        }
      }
    protected:
      File _fsUploadFile;
};

// ---------------------------
// Route registration
// ---------------------------
void defWebpages() {
    wserver.on("/", HTTP_GET, handleRedirect);
    wserver.on("/$upload.htm",     HTTP_GET, []() { wserver.send(200, "text/html", FPSTR(uploadContent)); }); // serve a built-in htm page
    wserver.on("/advanced.htm",    HTTP_GET,  handleAdvancedPage);
    // Rutas renombradas a /api/ para coherencia
    wserver.on("/api/list",        HTTP_GET,  handleListFiles);     // antes: "/$list"
    wserver.on("/api/sysinfo",     HTTP_GET,  handleSysInfo);       // antes: "/$sysinfo"
    wserver.on("/api/restart",     HTTP_GET,  handleRestart);       // antes: "/$restart"
    wserver.on("/api/showZONElog", HTTP_GET,  handleShowZONElog);   // sin cambio
    wserver.on("/api/config",      HTTP_POST, handleSaveConfig);    // antes: "/save_config"
    wserver.on("/download",        HTTP_GET,  handleDownload);      // descarga de ficheros
    wserver.on("/showfile",        HTTP_GET,  handleShowFile);      // muestra contenido fichero en el navegador
    // UPLOAD and DELETE of files in the file system using a request handler.
    wserver.addHandler(new FileServerHandler());
    // enable CORS header in webserver results
    wserver.enableCORS(true);
    wserver.serveStatic("/", LittleFS, "/");
    wserver.onNotFound([]() {wserver.send(404, "text/html", FPSTR(notFoundContent));}); // serve a built-in htm page
}

// ---------------------------
// Startup / lifecycle helpers
// ---------------------------

void displayWSinfo() {
  lcd.infoclear("Webserver activo", 1, BIPOK);
  snprintf(buff, MAXBUFF, "\"%s.local:%d\"", WiFi.getHostname(), WSPORT);
  lcd.info(buff, 3);
  int msgl = snprintf(buff, MAXBUFF, "%s:%d" , WiFi.localIP().toString().c_str(), WSPORT);
  lcd.info(buff, 4, msgl);
}

void setupWS() {
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
  LOG_INFO("[ConF][WS] IP address: ", WiFi.localIP(), ":", WSPORT);
  LOG_INFO("[ConF][WS] activado webserver para actualizaciones OTA de SW o filesystem");
  displayWSinfo();
}

void procesaWebServer() {
  wserver.handleClient();
}

void endWS() {
  TRACE2("terminando MDNS...\n");
  MDNS.end();
  TRACE2("terminando webserver...\n");
  wserver.stop();
  webServerAct = false;
}

#endif

/*
void PrintArgs() {
    TRACE2("Argumentos recibidos:\n");
    for (int i = 0; i < wserver.args(); i++) {
        TRACE2("  %s: %s\n", wserver.argName(i).c_str(), wserver.arg(i).c_str());
      }
    }
    
// URL decode function parseado de https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
static String urlDecode(const String &s) {
  TRACE2("urlDecode recibido: %s\n", s.c_str());
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < s.length()) {
      char hex[3] = { s[i+1], s[i+2], 0 };
      char decoded = (char) strtol(hex, nullptr, 16);
      out += decoded;
      i += 2;
    } else {
      out += c;
    }
  }
  TRACE2("urlDecode devuelto: %s\n", out.c_str());
  return out;
}
*/