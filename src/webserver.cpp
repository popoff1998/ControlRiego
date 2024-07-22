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
   #define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"


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
    // actualmente HTTPUpdateServer no soporta LittleFS por lo que la carga del file system falla con:
    //  "Update error: Bad Size Given"
    //hay un issue abierto para que lo soporte. TODO: actualizar HTTPUpdateServer cuando lo cierren:
    //  https://github.com/espressif/arduino-esp32/issues/9347


// ===== Simple functions used to answer simple GET requests =====

// This function is called when the WebServer was requested without giving a filename.
// This will redirect to the file index.htm when it is existing otherwise to the built-in $upload.htm page
void handleRedirect() {
  TRACE2("Redirect...\n");
  String url = "/index.htm";

  if (!LittleFS.exists(url)) { url = "/$upload.htm"; }

  wserver.sendHeader("Location", url, true);
  wserver.send(302);
}  // handleRedirect()


// This function is called when the WebServer was requested to list all existing files in the filesystem.
// a JSON array with file information is returned.
void handleListFiles() {
  File dir = LittleFS.open("/", "r");
  String result;

  result += "[\n";
  while (File entry = dir.openNextFile()) {
    if (result.length() > 4) { result += ",\n"; }
    result += "  {";
    result += "\"type\": \"file\", ";
    result += "\"name\": \"" + String(entry.name()) + "\", ";
    result += "\"size\": " + String(entry.size()) + ", ";
    result += "\"time\": " + String(entry.getLastWrite());
    result += "}";
  }  // while

  result += "\n]";
  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "text/javascript; charset=utf-8", result);
}  // handleListFiles()


// This function is called when the sysInfo service was requested.
void handleSysInfo() {
  String result;

  result += "{\n";
  result += "  \"Chip Model\": " + String(ESP.getChipModel()) + ",\n";
  result += "  \"Chip Cores\": " + String(ESP.getChipCores()) + ",\n";
  result += "  \"Chip Revision\": " + String(ESP.getChipRevision()) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"fsTotalBytes\": " + String(LittleFS.totalBytes()) + ",\n";
  result += "  \"fsUsedBytes\": " + String(LittleFS.usedBytes()) + ",\n";
  result += "  \"ESP32 temperature\": " + String(temperatureRead()) + "ºC,\n";
  result += "}";

  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "text/javascript; charset=utf-8", result);
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
  bool canHandle(HTTPMethod requestMethod, String UNUSED uri) override {
    return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE));
  }  // canHandle()


  bool canUpload(String uri) override {
    // only allow upload on root fs level.
    return (uri == "/");
  }  // canUpload()


  bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override {
    // ensure that filename starts with '/'
    String fName = requestUri;
    if (!fName.startsWith("/")) { fName = "/" + fName; }

    TRACE2("handle %s\n", fName.c_str());

    if (requestMethod == HTTP_POST) {
      // all done in upload. no other forms.

    } else if (requestMethod == HTTP_DELETE) {
      if (LittleFS.exists(fName)) {
        TRACE2("DELETE %s\n", fName.c_str());
        LittleFS.remove(fName);
      }
    }  // if

    wserver.send(200);  // all done.
    return (true);
  }  // handle()


  // uploading process
  void
  upload(WebServer UNUSED &server, String UNUSED _requestUri, HTTPUpload &upload) override {
    // ensure that filename starts with '/'
    static size_t uploadSize;

    if (upload.status == UPLOAD_FILE_START) {
      String fName = upload.filename;

      // Open the file for writing
      if (!fName.startsWith("/")) { fName = "/" + fName; }
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

   void defWebpages() {

   TRACE2("Setup ntp...\n");
   configTzTime(TIMEZONE, "es.pool.ntp.org");

  TRACE2("Register redirect...\n");

  // register a redirect handler when only domain name is given.
  wserver.on("/", HTTP_GET, handleRedirect);

  TRACE2("Register service handlers...\n");

  // serve a built-in htm page
  wserver.on("/$upload.htm", []() {
    wserver.send(200, "text/html", FPSTR(uploadContent));
  });

  // register some REST services
  wserver.on("/$list", HTTP_GET, handleListFiles);
  wserver.on("/$sysinfo", HTTP_GET, handleSysInfo);

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
      if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) LOG_ERROR("could not mount the filesystem...");
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
      TRACE2("IP=%s\n", WiFi.localIP().toString());
      LOG_INFO("[ConF][WS] activado webserver para actualizaciones OTA de SW o filesystem");
      lcd.infoclear("OTA Webserver act", DEFAULTBLINK, BIPOK);
      snprintf(buff, MAXBUFF, "\"%s.local:%d\"", WiFi.getHostname(), WSPORT);
      lcd.info(buff, 3);
      snprintf(buff, MAXBUFF, "%s:%d" , WiFi.localIP().toString(), WSPORT);
      lcd.info(buff,4);
      //delay(config.msgdisplaymillis);
   }

   void procesaWebServer()
   {
      wserver.handleClient();
   }  

   void endWS()
   {
      TRACE2("cerrando filesystem...\n");
      LittleFS.end();
      TRACE2("terminando MDNS...\n");
      MDNS.end();
      TRACE2("terminando webserver...\n");
      wserver.stop();
      webServerAct = false;
   }


#endif