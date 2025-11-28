// Servidor web para actualizaciones OTA del FW o del filesystem, gestion de ficheros etc.
// Adaptado de:
//  WebServer.ino (GITHUB: arduino-esp32/libraries/WebServer/examples/WebServer/WebServer.ino) by Gerhard Riegler
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

   int logDays = 15; // dias de log a mostrar por defecto
   bool restartRequired = false; //indica si es necesario reiniciar el sistema para aplicar cambios

   #ifdef DEVELOP
      const bool httpUpdateDebug = true;  //enable serial debug msgs
      const bool showtest_section = true;  //enable testing section in advanced.htm
   #else
      const bool httpUpdateDebug = false;
      const bool showtest_section = false;
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

// Lista de tokens y sus valores de sustitución
struct TokenData {
    const char* token;
    String value;
} tokenList[] = {
        {"%PARMFILE%",   String(parmFile)},
        {"%BACKUPFILE%", String(backupParmFile)},
        {"%LASTRIEGOS%", String(lastRiegosFile)},
        {"%LASTGRUPOS%", String(lastGruposFile)},
        {"%VERSION%",    String(FW_VERSION)},
        {"%DIAS%",       String(logDays)},
        {"%SHOWTEST%",   showtest_section ? "true" : "false"} 
    };
void replaceTokens(String &content) {
    const size_t numTokens = sizeof(tokenList) / sizeof(tokenList[0]);
    for (size_t i = 0; i < numTokens; i++) {
        const char* currentToken = tokenList[i].token;
        const String& currentValue = tokenList[i].value;
        if (content.indexOf(currentToken) != -1) {
            content.replace(currentToken, currentValue);
            LOG_DEBUG("Replaced token:", currentToken, "with value:", currentValue);
        }
    }
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
    replaceTokens(outPath);
    if (!outPath.startsWith("/")) { outPath = "/" + outPath; }
    if (!LittleFS.exists(outPath)) {
      wserver.send(400, "text/plain", "Bad Request: file not found");
      LOG_ERROR("No existe: ", outPath);
      return false;
    } 
    return true;
}

/* 
  Divide una ruta de archivo completa en su directorio y nombre de archivo.
  Parámetros:
   - fullPath: Ruta completa del archivo (por ejemplo, "/dir/subdir/file.txt").
   - dirPath: Referencia a String donde se almacenará la ruta del directorio (por ejemplo, "/dir/subdir").
   - fileName: Referencia a String donde se almacenará el nombre del archivo (por ejemplo, "file.txt").
*/
void splitFilePath(String &fullPath, String &dirPath, String &fileName) {
  LOG_DEBUG("fullPath recibido:", fullPath);
  replaceTokens(fullPath);
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
    outResult = "[\n";
    outResult.reserve(1024); 
    char buffer[256]; 
    bool firstEntry = true;
    while (File entry = dir.openNextFile()) {
        const char* entryName = entry.name();
        if (filter.length() == 0 || strncmp(entryName, filter.c_str(), filter.length()) == 0) { 
            const char* separator = firstEntry ? "" : ",\n";
            const char* type = entry.isDirectory() ? "dir" : "file";
            const char* path = entry.path(); 
            int size = snprintf(buffer, 256, 
                "%s {\"type\": \"%s\", \"name\": \"%s\", \"size\": %lu, \"time\": %lu}",
                separator,
                type,
                path,
                (unsigned long)entry.size(),
                (unsigned long)entry.getLastWrite()
            );
            if (size > 0 && size < 256) {
                outResult.reserve(outResult.length() + size + 100); // OJO solo se Re-reserva si se supera la capacidad reservada actual
                outResult.concat(buffer);
                firstEntry = false;
            }
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
   // En el caso de archivos html o javascript siempre los leemos para reemplazar tokens
   // antes de enviarlos al cliente (salvo que esten comprimidos .gz)
   if (contentType == "text/html" || path.endsWith(".htm") || path.endsWith(".html") || 
       contentType == "text/javascript" || contentType == "application/javascript" || 
       path.endsWith(".js")) {
      String content = file.readString();
      replaceTokens(content);
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
  buildFileListJSON(dir, filter, result);
  sendNoCacheJSON(result);
}

// restart device
void handleRestart() {
  LOG_DEBUG("Restarting ESP32... / restartRequired flag is", restartRequired);
  wserver.send(200, "text/plain", "Restarting ESP32...");
  delay(500);
  ESP.restart();
}

// end webserver
void handleEndWS() {  
  LOG_DEBUG("handleEndWS called");
  wserver.send(200, "text/plain", "Ending WebServer...");
  delay(500);
  endWS();
}

// system info
void handleSysInfo() {
  LOG_TRACE("handleSysInfo called");
  String result = sysInfo();
  wserver.sendHeader("Cache-Control", "no-cache");
  wserver.send(200, "text/javascript; charset=utf-8", result);
}

// save parmfile (body contains JSON)
void handleSaveConfig() {
  LOG_TRACE("handleSaveConfig called");
  if (!wserver.hasArg("plain")) {
      wserver.send(400, "text/plain", "Bad Request: Missing JSON body");
      return;
  }
  String jsonBody = wserver.arg("plain");
  JsonDocument doc;  // Validar JSON entrante
  DeserializationError error = deserializeJson(doc, jsonBody);
  if (error) {
      String errorMsg = "JSON Deserialization failed: ";
      errorMsg += error.c_str();
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
  restartRequired = true; //indica que es necesario reiniciar el sistema para aplicar cambios
  wserver.send(200, "text/plain", "Configuration saved successfully");
}

void handleSetRestartRequired() {
  restartRequired = true;
  wserver.send(200, "text/plain", "OK"); // 200 OK
}

// advanced page (requires auth)
void handleAdvancedPage() {
  if (!wserver.authenticate(update_username, update_password)) {
      wserver.requestAuthentication();
      return;
  }
  serveFile("/advanced.htm", "text/html");
}

// parmfile_editraw page (requires auth)
void handleEditRawPage() {
  if (!wserver.authenticate(update_username, update_password)) {
      wserver.requestAuthentication();
      return;
  }
  serveFile("/parmfile_editRaw.htm", "text/html");
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

// NOTE on Token Resolution Architecture:
// Token replacement (%PARMFILE%, %BACKUPFILE%, etc.) is handled centrally in serveFile()
// for all static file requests via the FileServerHandler.
// Client-side can use apiGetJson() which auto-detects tokens (%) and routes to /token_file.
// The /token_file endpoint below serves the same purpose as serveFile() for explicit token requests.

// ------------------------------------------------------------------------
// FileServerHandler 
// Custom RequestHandler que maneja aquellas peticiones no especificamente 
// gestionadas por otros handlers definidos en defWebpagesHandles().
// Esta clase es el corazón de la arquitectura de reemplazo de tokens:
// Intercepta peticiones GET de archivos estáticos y las enruta a través de
// serveFile(), que reemplaza tokens dinámicos (%PARMFILE%, etc.) en archivos
// HTML/JS/CSS antes de enviarlos al cliente.
// Los clientes que necesitan enviar tokens; pueden usar apiGetJson() en JS
// que detecta automáticamente tokens (%) y enruta a /token_file.
// ------------------------------------------------------------------------
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
          LOG_TRACE("GET request for:", fName);
          String pathToServe = fName;
          if (pathToServe.endsWith("/")) pathToServe += "index.html";
          if (LittleFS.exists(pathToServe) || LittleFS.exists(pathToServe + ".gz")) {
            serveFile(pathToServe); // serveFile() will automatically try .gz version if the original doesn't exist
            return true;
          } else return false; // Not found here; let others handle
        }
        if (requestMethod == HTTP_POST) {
          LOG_DEBUG("POST request for:", fName);
          handleOK = true;
        }
        if (requestMethod == HTTP_COPY) {
          String fileFrom , fileTo;
          if (fName == "/BACKUP") {fileFrom = parmFile; fileTo = backupParmFile;}
          if (fName == "/RESTORE") {fileFrom = backupParmFile; fileTo = parmFile; restartRequired = true;}
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
            wserver.send(500, "text/plain", "Internal Server Error. Cannot open file for writing");
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
    wserver.on("/parmfile_editRaw.htm",    HTTP_GET,  handleEditRawPage); // requiere auth
    // Rutas que devuelven/esperan un JSON renombradas a /api/ para coherencia
    wserver.on("/api/list",        HTTP_GET,  handleListFiles);
    wserver.on("/api/sysinfo",     HTTP_GET,  handleSysInfo);
    wserver.on("/api/restart",     HTTP_GET,  handleRestart); // mas facil de manejar GET que POST (y borra pagina)
    wserver.on("/api/endWS",       HTTP_GET,  handleEndWS);
    wserver.on("/api/showZONElog", HTTP_GET,  handleShowZONElog);
    wserver.on("/api/save_config", HTTP_POST, handleSaveConfig);
    wserver.on("/api/setrestart",  HTTP_GET,  handleSetRestartRequired);
    wserver.on("/download",        HTTP_GET,  handleDownload);
    wserver.on("/token_file",      HTTP_GET,  handleDownload);  // resolves tokens from client requests
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
  restartRequired = false;
  Serial.printf("[WS] HTTPUpdateServer ready!\n   --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", WiFi.getHostname(), WSPORT, update_path, update_username, update_password);
  LOG_INFO("[WS] Activado webserverIP address: ", WiFi.localIP(), ":", WSPORT);
  // logDays = getDomoticzSettingsInfo("LightHistoryDays").toInt(); //lee los dias de log a mostrar por defecto desde Domoticz
  displayWSinfo();
}

void procesaWebServer() {
  wserver.handleClient();
}

void endWS() {
  LOG_INFO("Terminando webserver...");
  if (restartRequired) handleRestart(); // reinicia si es necesario para aplicar cambios
  MDNS.end();
  wserver.stop();
  webServerAct = false;
}

#endif

