// Servidor web para actualizaciones OTA del FW o del filesystem, gestion de ficheros etc.
// Adaptado de:
//  WebServer.ino (GITHUB: arduino-esp32/libraries/WebServer/examples/WebServer/WebServer.ino) by Gerhard Riegler
//
// * Setup a web server
// * redirect when accessing the url with servername only
// * get real time by using builtin NTP functionality
// * send HTML responses from Sketch (see builtinfiles.h)
// * use a LittleFS file system on the data partition for static files
// * use http ETag Header for client side caching of static files
// * use custom ETag calculation for static files
// * extended FileServerHandler for get, uploading and deleting static files
// * serve APIs using REST services (/api/list, /api/sysinfo)
// * define HTML response when no file/api/handler was found
//
#ifdef WEBSERVER
   #include "Control.h"
   #include <ESPmDNS.h>
   
   #include "OTAupdateServer.h"  // HTTPUpdateServer adapted to use LittleFS
   #include "builtinfiles.h"     // The text of builtin files are in this header file

   // enable the CUSTOM_ETAG_CALC to enable calculation of ETags by a custom function
  //  #define CUSTOM_ETAG_CALC

   // mark parameters not used in example
   #define UNUSED __attribute__((unused))

   const char* const update_path = "/$update";
   const char* const update_username = "admin";
   const char* const update_password = "admin";

   #ifdef DEVELOP
      const bool httpUpdateDebug = true;  //enable serial debug msgs
   #else
      const bool httpUpdateDebug = false;
   #endif 

    WebServer wserver(WSPORT);
    HTTPUpdateServer httpUpdater(httpUpdateDebug);  

// ---------------------------
// Helpers (send, utilidades)
//  (las funciones definidas como static solo son visibles en este fichero)
// ---------------------------
static void sendNoCacheJSON(const String &payload) {
  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "application/json; charset=utf-8", payload);
}

String replaceTokens(const String &content) {
   String result = content;
   result.replace("%PARMFILE%",   String(parmFile));
   result.replace("%BACKUPFILE%", String(backupParmFile));
   result.replace("%LASTRIEGOS%", String(lastRiegosFile));
   result.replace("%LASTGRUPOS%", String(lastGruposFile));
   return result;
}

/**
 * Procesa la ruta del archivo solicitada por el cliente (argumento 'file'), 
 * resuelve cualquier token dinámico y valida la ruta final en el sistema de archivos.
 *
 * Esta función realiza los siguientes pasos:
 * 1. Verifica la existencia del parámetro 'file' en la solicitud HTTP.
 * 2. Reemplaza los tokens (marcadores de posición) en la ruta obtenida.
 * 3. Asegura que la ruta comience con un '/'.
 * 4. Verifica que el archivo final exista en el sistema de archivos (LittleFS).
 * 5. Envía una respuesta de error 400 al cliente si alguna validación falla.
 *
 * @param outPath Referencia a una String donde se almacenará la ruta final
 * y validada del archivo (ej. "/config/data.json").
 * @return true si la ruta fue obtenida, resuelta y validada correctamente; 
 * false si falta el argumento o el archivo no existe.
 */
static bool resolveFilePath(String &outPath) {
    if (!wserver.hasArg("file")) {
      wserver.send(400, "text/plain", "Bad Request: Missing 'file' parameter");
      return false;
    } 
    outPath = wserver.arg("file");
    LOG_DEBUG("arg 'file' recibido:", outPath);
    outPath = replaceTokens(outPath);
    LOG_DEBUG("arg 'file' tras reemplazo de tokens:", outPath);
    if (!outPath.startsWith("/")) { outPath = "/" + outPath; }
    if (!LittleFS.exists(outPath)) {
      wserver.send(400, "text/plain", "Bad Request: file not found");
      LOG_ERROR("No existe: ", outPath);
      return false;
    } 
    return true;
}

/*
 Construct a JSON array with file information from the given directory.
  Parameters:
   - dir: Opened directory File object to read entries from
   - filter: String prefix to filter file names (only files starting with this prefix are included). 
             If empty, all files are included.
   - outResult: String reference where the resulting JSON array will be stored.
  The resulting JSON array has entries with the following fields:
      - type: "file" or "dir"
      - name: full path of the file
      - size: size in bytes
      - time: last modification time as a Unix timestamp
*/
static void buildFileListJSON(File &dir, const String &filter, String &outResult) {
    char buffer[256]; 
    outResult = "[\n";
    outResult.reserve(1024); // Reservar el máximo o 1K
    bool firstEntry = true;
    
    while (File entry = dir.openNextFile()) {
        String filename = String(entry.name());
        if (filename.startsWith(filter) || filter == "") {
            const char* separator = firstEntry ? "" : ",\n";
            const char* type = entry.isDirectory() ? "dir" : "file";
            const char* path = entry.path(); 
            int len = snprintf(buffer,256, 
                "%s  {\"type\": \"%s\", \"name\": \"%s\", \"size\": %lu, \"time\": %lu}",
                separator,
                type,
                path,
                (unsigned long)entry.size(),
                (unsigned long)entry.getLastWrite()
            );
            outResult += buffer;
            firstEntry = false;
        }
    }
    outResult += "\n]";
}

static void sendFileAttachment(const String &path) {
    LOG_DEBUG("path:", path);
    File download = LittleFS.open(path);
    if (!download) {
      wserver.send(404, "text/plain", "File not found");
      LOG_ERROR("Not found:", path);
      return;
    }  
    String filename = path.substring(path.lastIndexOf('/') + 1);
    LOG_DEBUG("filename:", filename);
    // wserver.sendHeader("Content-Type", "text/text");
    wserver.sendHeader("Content-Disposition", "attachment; filename=\""+filename+"\"; filename*=UTF-8''"+filename);
    wserver.sendHeader("Connection", "close");
    wserver.streamFile(download, "application/octet-stream");
    download.close();
}    

// ---------------------------
// Server utils 
// ---------------------------

// MIME type mapping for file extensions
static const struct {
  const char* ext;
  const char* mime;
} mimeTypes[] = {
  {".htm",   "text/html"},
  {".html",  "text/html"},
  {".css",   "text/css"},
  {".js",    "application/javascript"},
  {".json",  "application/json"},
  {".xml",   "text/xml"},
  {".png",   "image/png"},
  {".gif",   "image/gif"},
  {".jpg",   "image/jpeg"},
  {".jpeg",  "image/jpeg"},
  {".ico",   "image/x-icon"},
  {".pdf",   "application/pdf"},
  {".zip",   "application/zip"},
  {".gz",    "application/gzip"},
  {nullptr,  "text/plain"}  // default fallback
};

const char* GetContentType(const String &filename) {
  int lastDot = filename.lastIndexOf('.');
  if (lastDot < 0) return "text/plain";
  String ext = filename.substring(lastDot);
  ext.toLowerCase();
  for (int i = 0; mimeTypes[i].ext != nullptr; i++) {
    if (ext == mimeTypes[i].ext) {
      return mimeTypes[i].mime;
    }
  }
  return "text/plain";
}  

void printArgs() {
  for (int i = 0; i < wserver.args(); i++) {LOG_DEBUG("  ", wserver.argName(i), ": ", wserver.arg(i));}
}  
    
void serveFile(String path, String contentType) {
   LOG_DEBUG("Serving file:", path, "contentType:", contentType);
   // Intentar abrir el archivo; si no existe, probar con .gz
   String filePath = path;
   bool isGzipped = false;
   File file = LittleFS.open(filePath, "r");
   if (!file) {
      String gzPath = path + ".gz";
      LOG_DEBUG("File not found, trying gzip version:", gzPath);
      file = LittleFS.open(gzPath, "r");
      if (!file) {
         LOG_ERROR("Failed to open file or gzip version:", path);
         wserver.send(404, "text/plain", "File Not Found");
         return;
      }
      filePath = gzPath;
      isGzipped = true;
      LOG_DEBUG("Serving gzip version:", filePath);
   }
   // Si es un archivo comprimido, informar al navegador con Content-Encoding
   if (isGzipped) {
      wserver.sendHeader("Content-Encoding", "gzip");
      LOG_DEBUG("Added Content-Encoding: gzip header");
   }
   // Siempre leemos archivos HTML para reemplazar tokens, incluso en JavaScript embebido
   if (contentType == "text/html" || path.endsWith(".htm") || path.endsWith(".html") || 
       contentType == "text/javascript" || contentType == "application/javascript" || 
       path.endsWith(".js")) {
      String content = file.readString();
      LOG_TRACE("Content before token replacement (first 100 chars):", content.substring(0, 100));
      content = replaceTokens(content);
      LOG_TRACE("Content after token replacement (first 100 chars):", content.substring(0, 100));
      wserver.send(200, contentType, content);
   } else {
      size_t sent = wserver.streamFile(file, contentType);
   }
   file.close();
}

void serveFile(String path) {
   String contentType = GetContentType(path);
   serveFile(path, contentType);
}   

// ---------------------------
// Handlers 
// ---------------------------

// redirect to index or upload
void handleRedirect() {
  LOG_DEBUG("Redirecting to /index.htm or /$upload.htm");
  String url = "/index.htm";
  if (!LittleFS.exists(url)) { url = "/$upload.htm"; }
  wserver.sendHeader("Location", url, true);
  wserver.send(302);
}

void splitFilePath(String &fullPath, String &dirPath, String &fileName) {
    LOG_DEBUG("fullPath recibido:", fullPath);
    fullPath = replaceTokens(fullPath);
    LOG_DEBUG("fullPath tras reemplazo de tokens:", fullPath);
  int lastSlash = fullPath.lastIndexOf('/');
  if (lastSlash == -1) {
    dirPath = "/";
    fileName = fullPath;
  } else {
    dirPath = fullPath.substring(0, lastSlash);
    if (dirPath == "") { dirPath = "/"; }
    fileName = fullPath.substring(lastSlash + 1);
  }
  LOG_DEBUG("fullPath=", fullPath, " dirPath=", dirPath, " fileName=", fileName);
}

// list files as JSON
// This function is called when the WebServer was requested to list existing files in the filesystem.
// The request can contain the following arguments:
// - dir: the directory to be listed (default is '/')
// - file: the filter for the file names (default is '')
// a JSON array is returned with the file information.
void handleListFiles() {
  LOG_DEBUG("Argumentos recibidos:");
  printArgs();
  String path = "/";
  if (wserver.hasArg("dir")) path = wserver.arg("dir");
  String filter = "";
  if (wserver.hasArg("file")) {
    String fullpath = wserver.arg("file");
    String fpath = "";
    splitFilePath(fullpath, fpath, filter); 
    if (fpath!="/") { path = fpath; }
    LOG_DEBUG("Filtering files with prefix:", filter, "in directory:", path);
  } else LOG_DEBUG("No file filter provided, listing all files in directory:", path);
  File dir = LittleFS.open(path, "r");
  String result;
  result.reserve(1024); // reservar para reducir reallocs
  buildFileListJSON(dir, filter, result);
  sendNoCacheJSON(result);
}

// restart device
void handleRestart() {
  LOG_DEBUG("Restarting ESP32...");
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
  if (!wserver.hasArg("plain")) {
      wserver.send(400, "text/plain", "Bad Request: Missing JSON body");
      return;
  }
  String jsonBody = wserver.arg("plain");
  JsonDocument doc;  // Validar JSON entrante
  DeserializationError error = deserializeJson(doc, jsonBody);
  if (error) {
      String errorMsg = "JSON Deserialization failed: ";
      errorMsg += error.c_str(); // Proporciona un mensaje de error útil
      wserver.send(400, "text/plain", errorMsg);
      return;
  }
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
  String path;
  if (resolveFilePath(path)) {
    LOG_DEBUG("path:", path);
    sendFileAttachment(path);
  }
}

// servedirect endpoint wrapper : /token_file is used to resolve tokens from client-sent filenames.
// Token resolution is handled centrally when serving files and clients should request concrete paths,
// but if a .gz version is requested, token replacement would not occur.
// This wrapper allows clients to request files with tokens and have them resolved here.
void handleTokenFile() {
  String path;
  if (resolveFilePath(path)) {
    serveFile(path);
  }
}

// ---------------------------
// FileServerHandler 
// ---------------------------
class FileServerHandler : public RequestHandler {
    public:
      FileServerHandler() { }
      bool canHandle(HTTPMethod requestMethod, String uri) override {
        LOG_TRACE("uri:", uri, "Method:", requestMethod);
        // Intercept GET requests for existing files so we can perform token replacement
        if (requestMethod == HTTP_GET) {
          // Exclude API and special endpoints explicitly so we don't intercept them
          if (uri.startsWith("/api/") || uri.startsWith("/download") || uri.startsWith("/token_file") || uri.startsWith("/$")) {
            LOG_DEBUG("Excluding URI from file handler:", uri);
            return false;
          }
          String f = uri;
          if (!f.startsWith("/")) f = "/" + f;
          // strip query string if present
          int q = f.indexOf('?');
          if (q != -1) f = f.substring(0, q);
          // If the file exists in LittleFS (or a .gz version) we can handle it here
          if (LittleFS.exists(f) || LittleFS.exists(f + ".gz")) {
            LOG_TRACE("Will handle GET for existing file:", f);
            return true;
          }
          return false;
        }
        return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE) || (requestMethod == HTTP_COPY));
      }
      bool canUpload(String uri) override {
        LOG_TRACE("uri received:", uri);
        return (uri == "/");
      }
      bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override {
        String fName = wserver.urlDecode(requestUri); // elimina codificacion URL %..
        if (!fName.startsWith("/")) { fName = "/" + fName; }
        bool handleOK = false;
        if (requestMethod == HTTP_GET) {
          // Serve file through our serveFile() so token replacement happens
          // serveFile() will automatically try .gz version if the original doesn't exist
          LOG_DEBUG("GET request for:", fName);
          // If it's a directory, append index.html
          String pathToServe = fName;
          if (pathToServe.endsWith("/")) pathToServe += "index.html";
          if (LittleFS.exists(pathToServe) || LittleFS.exists(pathToServe + ".gz")) {
            serveFile(pathToServe);
            return true;
          } else {
            // Not found here; let others handle
            return false;
          }
        }
        if (requestMethod == HTTP_POST) {
          LOG_DEBUG("POST request for:", fName);
          handleOK = true;
        }
        if (requestMethod == HTTP_COPY) {
          String fileFrom , fileTo;
          if (fName == "/BACKUP") {fileFrom = parmFile; fileTo = backupParmFile;}
          if (fName == "/RESTORE") {fileFrom = backupParmFile; fileTo = parmFile;}
          LOG_DEBUG("Copying file from ", fileFrom, " to ", fileTo);
          handleOK = copyConfigFile(fileFrom.c_str(), fileTo.c_str());
        }  
        if (requestMethod == HTTP_DELETE) {
          if (LittleFS.exists(fName)) {
            LOG_DEBUG("DELETE request for:", fName);
            handleOK = LittleFS.remove(fName);
          }
        }
        if (handleOK) {
          wserver.send(200, "text/plain", "OK");
          return (true);
        } else {
          wserver.send(500, "text/plain", "ERROR");
          LOG_ERROR("Handle request error for:", fName);
          return (false);
        }
      }
      void upload(WebServer UNUSED &server, String UNUSED _requestUri, HTTPUpload &upload) override {
        const size_t MAX_UPLOAD_BYTES = MAX_UPLOAD_KBYTES*1024UL;
        static size_t uploadSize;
        static bool uploadTooLarge = false;

        if (upload.status == UPLOAD_FILE_START) {
          uploadTooLarge = false;
          uploadSize = 0;
          String fName = upload.filename; // puede venir como "/subdir/FILE.bin" (por el cliente)
          if (fName == "%PARMFILE%") { fName = parmFile; }
          // asegurar que empieza por '/'
          if (!fName.startsWith("/")) { fName = "/" + fName; }
          // colapsar "//" repetidos
          while (fName.indexOf("//") != -1) fName.replace("//", "/");
          // rechazar traversal de directorios
          if (fName.indexOf("..") != -1) {
            LOG_WARN("Upload rejected: filename contains '..' ->", fName);
            wserver.send(400, "text/plain", "Invalid filename");
            uploadTooLarge = true;
            return;
          }
          // OPCIONAL: limitar uploads a un directorio raíz (por seguridad). Cambia a "/" para permitir todo.
          const String uploadRoot = "/"; // <--- ajusta si quieres otra raíz o "/" para cualquier sitio
          if (uploadRoot != "/") {
            // si la ruta enviada no está ya bajo uploadRoot, la colocamos allí
            if (!fName.startsWith(uploadRoot + "/") && fName != uploadRoot) {
              // evitar duplicar slashes
              String tmp = fName;
              if (tmp.startsWith("/")) tmp = tmp.substring(1);
              fName = uploadRoot + "/" + tmp;
              while (fName.indexOf("//") != -1) fName.replace("//", "/");
            }
          }
          // crear directorio padre si no existe
          int lastSlash = fName.lastIndexOf('/');
          if (lastSlash > 0) {
            String dirPath = fName.substring(0, lastSlash);
            if (!LittleFS.exists(dirPath)) {
              LOG_DEBUG("Creating upload directory:", dirPath);
              // LittleFS::mkdir puede devolver false si falla; no siempre necesario en algunas implementaciones
              LittleFS.mkdir(dirPath);
            }
          }
          LOG_DEBUG("Start uploading file:", fName, " declared size:", upload.totalSize);
          // Si el cliente ha enviado totalSize y excede el límite, rechazar ya
          if (upload.totalSize > 0 && (size_t)upload.totalSize > MAX_UPLOAD_BYTES) {
            LOG_WARN("Upload rejected: declared size exceeds limit:", upload.totalSize);
            wserver.send(413, "text/plain", "File too large");
            uploadTooLarge = true;
            return;
          }
          if (LittleFS.exists(fName)) LittleFS.remove(fName);
          _fsUploadFile = LittleFS.open(fName, "w");
          if (!_fsUploadFile) {
            LOG_ERROR("Cannot open file for upload:", fName);
            wserver.send(500, "text/plain", "Internal Server Error");
            uploadTooLarge = true;
            return;
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (uploadTooLarge) return; // ya rechazado
          if (_fsUploadFile) {
            size_t written = _fsUploadFile.write(upload.buf, upload.currentSize);
            if (written < upload.currentSize) {
              LOG_ERROR("Error escribiendo fichero de upload");
              _fsUploadFile.close();
              String fName = upload.filename;
              if (!fName.startsWith("/")) { fName = "/" + fName; }
              LittleFS.remove(fName);
              wserver.send(500, "text/plain", "Write error");
              uploadTooLarge = true;
              return;
            }
            uploadSize += upload.currentSize;
            // Si el tamaño real supera el límite, cortar y notificar
            if (uploadSize > MAX_UPLOAD_BYTES) {
              LOG_WARN("Upload exceeded size limit, aborting. bytes:", uploadSize);
              _fsUploadFile.close();
              String fName = upload.filename;
              if (!fName.startsWith("/")) { fName = "/" + fName; }
              LittleFS.remove(fName);
              uploadTooLarge = true;
              wserver.send(413, "text/plain", "File too large");
              return;
            }
          }
        } else if (upload.status == UPLOAD_FILE_END) {
            LOG_DEBUG("Finished upload");
          if (_fsUploadFile) {
            _fsUploadFile.close();
            LOG_DEBUG("Upload completed, total size:", upload.totalSize ? upload.totalSize : uploadSize);
          }
          // Si fue rechazado por tamaño, ya se envió 413 anteriormente.
        }
      }
    protected:
      File _fsUploadFile;
};

// ---------------------------
// Route registration
// ---------------------------
void defWebpagesHandles() {
    wserver.on("/", HTTP_GET, handleRedirect);
    // paginas html builting comienzan por $
    wserver.on("/$upload.htm",     HTTP_GET, []() { wserver.send(200, "text/html", FPSTR(uploadContent)); }); // serve a built-in htm page
    wserver.on("/advanced.htm",    HTTP_GET,  handleAdvancedPage); // requiere auth
    // Rutas que devuelven/esperan un JSON renombradas a /api/ para coherencia
    wserver.on("/api/list",        HTTP_GET,  handleListFiles);
    wserver.on("/api/sysinfo",     HTTP_GET,  handleSysInfo);
    wserver.on("/api/restart",     HTTP_POST, handleRestart);
    wserver.on("/api/showZONElog", HTTP_GET,  handleShowZONElog);
    wserver.on("/api/save_config", HTTP_POST, handleSaveConfig);
    wserver.on("/download",        HTTP_GET,  handleDownload);
    wserver.on("/token_file",      HTTP_GET,  handleTokenFile);      // muestra contenido fichero en el navegador
    // GET, UPLOAD, COPY and DELETE of files in the file system using a request handler.
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
  httpUpdater.setup(&wserver, update_path, update_username, update_password);
  defWebpagesHandles();
  MDNS.addService("http", "tcp", WSPORT);
  wserver.begin();
  webServerAct = true;
  Serial.printf("[WS] HTTPUpdateServer ready!\n   --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", WiFi.getHostname(), WSPORT, update_path, update_username, update_password);
  LOG_INFO("[WS] Activado webserverIP address: ", WiFi.localIP(), ":", WSPORT);
  displayWSinfo();
}

void procesaWebServer() {
  wserver.handleClient();
}

void endWS() {
  MDNS.end();
  LOG_INFO("Terminando webserver...");
  wserver.stop();
  webServerAct = false;
}

#endif

