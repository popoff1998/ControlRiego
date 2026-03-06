// Servidor web para actualizaciones OTA del FW o del filesystem, gestion de ficheros etc.
// Adaptado de:
//  WebServer.ino (GITHUB: arduino-esp32/libraries/WebServer/examples/WebServer/WebServer.ino) by Gerhard Riegler
//
#ifdef WEBSERVER
   #include "Control.h"
   #include <WebServer.h>
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

/**
 * Procesa la ruta del archivo solicitada por el cliente (argumento 'file'), 
 * y valida que exista en el sistema de archivos.
 *
 * Esta función realiza los siguientes pasos:
 * 1. Verifica la existencia del parámetro 'file' en la solicitud HTTP.
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
   - fullPath: Ruta completa del archivo (por ejemplo, "/dir/subdir/file.txt")
   - dirPath: Referencia a String donde se almacenará la ruta del directorio (por ejemplo, "/dir/subdir").
   - fileName: Referencia a String donde se almacenará el nombre del archivo (por ejemplo, "file.txt").
*/
void splitFilePath(String &fullPath, String &dirPath, String &fileName) {
  LOG_DEBUG("fullPath recibido:", fullPath);
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
    String filename = path.substring(path.lastIndexOf('/') + 1);
    size_t fileSize = download.size();
    LOG_DEBUG("filename:", filename, " size:", fileSize);
    wserver.sendHeader("Content-Disposition", "attachment; filename=\""+filename+"\"; filename*=UTF-8''"+filename);
    wserver.sendHeader("Content-Length", String(fileSize)); // Indica el tamaño al navegador para mostrar la barra de progreso
    wserver.sendHeader("Connection", "close");
    wserver.streamFile(download, "application/octet-stream");
    download.close();
}    

// ---------------------------
// Helper para control del Cache
// ---------------------------
/*
 * Configura las cabeceras de caché (Cache-Control, ETag) y comprueba 
 * si el recurso se puede reusar desde la caché del navegador (304 Not Modified).
 * @param path La ruta del archivo (ej. "/index.htm" o "/datos/log.json").
 * @param file El objeto File abierto, usado para obtener el LastWrite Time.
 * @param notInmutable Indica si se debe usar un ETag basado en timestamp+tamaño
 * @return true  si la respuesta 304 fue enviada y se debe cortar el procesamiento, 
 *         false si se debe servir el contenido (200 OK).
 * - Estáticos: immutable (1 año) + ETag basado en LastWrite y tamaño
 * - /Datos o si notInmutable=true: no-cache + ETag (Firmware o LastWrite+tamaño).
 */
static bool checkAndSendCacheHeaders(const String &path, File &file, bool notInmutable) {
    // ETag por defecto: combinamos el LastWrite y el tamaño del archivo para detectar cambios.
    String etagValue = String(file.getLastWrite()) + "-" + String(file.size());
    #ifdef RELEASE
        // if (notInmutable) etagValue = String(FW_VERSION) + "-FW";
        if (path.startsWith("/datos/") || notInmutable) {
            // Documentos que queremos que el navegador REVALIDE siempre
            wserver.sendHeader("Cache-Control", "no-cache");
        } else {
            // Activos estáticos puros: se debe enviar el fichero, pero el navegador no volverá a preguntar durante un año
            wserver.sendHeader("Cache-Control", "public, max-age=31536000, immutable"); 
        }
    #else
        // En modo desarrollo, siempre forzamos revalidación de todos los recursos para facilitar pruebas
        wserver.sendHeader("Cache-Control", "no-cache");
    #endif
    // Enviamos siempre el ETag, incluso para los inmutables
    wserver.sendHeader("ETag", etagValue);
    // Lógica de comprobación 304 (Si el navegador ya lo tiene)
    String receivedEtag = wserver.header("If-None-Match");
    if (receivedEtag.length() > 0 && receivedEtag == etagValue) { 
        wserver.send(304);
        LOG_DEBUG("Sent 304 Not Modified for path:", path, "ETag:", etagValue); 
        return true; // se ha enviado el 304, se debe cortar el procesamiento (no enviar el fichero)
    }
    return false; // Hay que enviar el archivo + (200 OK)
}

// ---------------------------
// Server utils 
// ---------------------------

// En tipos de texto se añade charset Unicode para mostrar caracteres especiales correctamente
static const struct {
  const char* ext;
  const char* mime;
} mimeTypes[] = {
  {".htm",   "text/html; charset=utf-8"},      // Añadido charset
  {".html",  "text/html; charset=utf-8"},      // Añadido charset
  {".css",   "text/css; charset=utf-8"},       // Recomendado para CSS con símbolos
  {".js",    "application/javascript; charset=utf-8"},
  {".json",  "application/json; charset=utf-8"},
  {".xml",   "text/xml; charset=utf-8"},
  {".png",   "image/png"},
  {".gif",   "image/gif"},
  {".jpg",   "image/jpeg"},
  {".jpeg",  "image/jpeg"},
  {".ico",   "image/x-icon"},
  {".pdf",   "application/pdf"},
  {".zip",   "application/zip"},
  {".gz",    "application/gzip"},
  {nullptr,  "text/plain; charset=utf-8"}      // Fallback 
};

const char* GetContentType(const String &filename) {
  int lastDot = filename.lastIndexOf('.');
  if (lastDot < 0) return "text/plain; charset=utf-8"; 
  String ext = filename.substring(lastDot);
  ext.toLowerCase();
  for (int i = 0; mimeTypes[i].ext != nullptr; i++) {
    if (ext == mimeTypes[i].ext) {
      return mimeTypes[i].mime;
    }
  }
  // Si no tiene extensión, devolvemos el fallback con UTF-8
  return "text/plain; charset=utf-8"; // Fallback con UTF-8
}

void printArgs() {
  for (int i = 0; i < wserver.args(); i++) {LOG_DEBUG("  ", wserver.argName(i), ": ", wserver.arg(i));}
}  

/**
 * @brief Sirve un archivo estático desde el sistema de archivos LittleFS al cliente, 
 * gestionando el caching del navegador.
 * * Esta función busca el archivo solicitado por 'path', priorizando la versión sin comprimir 
 * y cayendo a la versión .gz si no encuentra la primera. Establece encabezados de caché 
 * ETag/Cache-Control y maneja el envío de contenido.
 * @param path          Ruta al archivo solicitado dentro de LittleFS (ej: "/index.html").
 * @param contentType   Tipo MIME del contenido (ej: "text/html", "application/javascript").
 */
void serveFile(String path, String contentType) {
    LOG_DEBUG("Serving file:", path, "contentType:", contentType);
    // 1. Manejo de archivos .gz (Comprobación y apertura)
    String filePath = path;
    bool isGzipped = false;
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        String gzPath = path + ".gz";
        file = LittleFS.open(gzPath, "r");
        if (!file) {
            LOG_ERROR("Failed to open file or gzip version:", path);
            wserver.send(404, "text/plain", "File Not Found");
            return;
        }
        filePath = gzPath; isGzipped = true; LOG_DEBUG("Serving gzip version:", filePath);
    }
    // 2. Lógica de Caching y Contenido
    // (paginas html no inmutables para que navegador consulte  
    //  y servidor verifique auth en aquellas que lo requieran)
    bool notInmutable = (contentType.startsWith("text/html"));
    if (checkAndSendCacheHeaders(path, file, notInmutable)) {
        file.close();
        return; // se ha enviado un 304, no es necesario enviar el contenido
    }
    // enviar fichero y cabeceras
    size_t sent = wserver.streamFile(file, contentType);
    if (sent == 0 && file.size() > 0) LOG_ERROR("Error crítico: El stream falló para el archivo", path); 
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
  LOG_DEBUG("Redirecting to /index.htm or /$upload");
  String url = "/index.htm";
  if (!LittleFS.exists(url)) { url = "/$upload"; }
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
  displayRestar();
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
  sendNoCacheJSON(result);
}

// save parmfile (body contains JSON)
void handleSaveConfig() {
  LOG_TRACE("handleSaveConfig called");
  if (!wserver.hasArg("plain")) {
      LOG_DEBUG("No JSON body received in request");
      wserver.send(400, "text/plain", "Bad Request: Missing JSON body");
      return;
  }
  String jsonBody = wserver.arg("plain");
  JsonDocument doc;  // Validar JSON entrante
  DeserializationError error = deserializeJson(doc, jsonBody);
  if (error) {
      String errorMsg = "JSON Deserialization failed: ";
      errorMsg += error.c_str();
      LOG_DEBUG(errorMsg);
      wserver.send(400, "text/plain", errorMsg);
      return;
  }
  File configFile = LittleFS.open(parmFile, "w");
  if (!configFile) {
      LOG_DEBUG("Failed to open config file for writing:", parmFile);
      wserver.send(500, "text/plain", "Internal Server Error: Could not open file for writing");
      return;
  }
  configFile.print(jsonBody);
  configFile.close();
  restartRequired = true; //indica que es necesario reiniciar el sistema para aplicar cambios
  LOG_DEBUG("Configuration saved, restartRequired set to true");
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

// Forzamos el volcado y cierre del log para liberar LittleFS
void handleListLogs() {
    refreshLogFile();
    serveFile("/errores.htm", "text/html");
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
  String json = readSCDLogFile(zona); // obtiene del Domoticz el log de riegos de la zona
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

// Devuelve un JSON con variables de interés para el cliente (ej. logDays, showtest_section, etc.)
void handleServerVars() {
    // Calculamos los valores dinámicos antes de llenar el JSON
    uint32_t maxFirmwareSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000; //0x1000 (margen de seguridad) y alineado a 4KB
    uint32_t maxFSSize = LittleFS.totalBytes();

    JsonDocument doc;
    doc["parmFile"]      = parmFile;
    doc["backupFile"]    = backupParmFile;
    doc["errorFile"]     = logErrorFile;
    doc["lastRiegos"]    = lastRiegosFile;
    doc["lastGrupos"]    = lastGruposFile;
    doc["version"]       = FW_VERSION;
    doc["logDays"]       = logDays;
    doc["logEnabled"]    = (LOG_FILE_GET_LEVEL() != DebugLogLevel::LVL_NONE);
    doc["logWarnFile"]   = config.logWarnToFile;
    doc["showTest"]      = showtest_section;
    doc["maxFW"]         = maxFirmwareSize;
    doc["maxFS"]         = maxFSSize;
    String response;
    serializeJson(doc, response);
    wserver.send(200, "application/json", response);    
}


// ------------------------------------------------------------------------
// FileServerHandler 
// Custom RequestHandler que maneja aquellas peticiones no especificamente 
// gestionadas por otros handlers definidos en defWebpagesHandles().
// Esta clase es el corazón de la arquitectura:
// Intercepta peticiones GET de archivos estáticos y las enruta a través de
// serveFile(), que selecciona la politica de cache y además intenta enviar el archivo
// comprimido .gz si no existe el original.
// ------------------------------------------------------------------------
class FileServerHandler : public RequestHandler {
    public:
      FileServerHandler() { }
      bool canHandle(HTTPMethod requestMethod, String uri) override {
        LOG_TRACE("uri:", uri, "Method:", requestMethod);
        // Intercept GET requests for existing files so we can perform cache control and gzip handling in serveFile().
        if (requestMethod == HTTP_GET) {
          // Exclude API and special endpoints explicitly so we don't intercept them
          if (uri.startsWith("/api/") || uri.startsWith("/$")) {
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
          // Serve file through our serveFile() so cache headers and gzip handling are applied
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
          handleOK = copyFile(fileFrom.c_str(), fileTo.c_str());
        }  
        if (requestMethod == HTTP_DELETE) {
          if (LittleFS.exists(fName)) {
            // Si el archivo es el log, forzamos el cierre total
            if (fName == logErrorFile) {
                LOG_INFO("Cerrando Manager de DebugLog...");
                LOG_FILE_CLOSE();
            }
            LOG_DEBUG("DELETE request for:", fName);
            handleOK = LittleFS.remove(fName);
            // Si era el log, lo volvemos a crear y enganchar
            if (fName == logErrorFile) {
                #ifdef DEBUGLOG_ENABLE_FILE_LOGGER
                LOG_ATTACH_FS_AUTO(LittleFS, logErrorFile, FILE_APPEND);
                LOG_INFO("Logger reiniciado en archivo nuevo.");
                #endif
            }
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
    // paginas html (las builting comienzan por $)
    wserver.on("/$upload",     HTTP_GET, []() { wserver.send(200, "text/html", FPSTR(uploadContent)); }); // serve a built-in htm page
    wserver.on("/advanced.htm",    HTTP_GET,  handleAdvancedPage); // requiere auth
    wserver.on("/errores.htm",     HTTP_GET,  handleListLogs); // fuerza cierre ficheros para actualizar timestamps
    wserver.on("/parmfile_editRaw.htm",    HTTP_GET,  handleEditRawPage); // requiere auth
    // apis que devuelven un JSON
    wserver.on("/api/list",        HTTP_GET,  handleListFiles);
    wserver.on("/api/sysinfo",     HTTP_GET,  handleSysInfo);
    wserver.on("/api/showZONElog", HTTP_GET,  handleShowZONElog);
    wserver.on("/api/serverVars",  HTTP_GET,  handleServerVars); // devuelve JSON con variables de interés para el cliente (ej. logDays)
    // otras apis
    wserver.on("/api/download",    HTTP_GET,  handleDownload);
    wserver.on("/api/save_config", HTTP_POST, handleSaveConfig);
    wserver.on("/api/endWS",       HTTP_GET,  handleEndWS);
    wserver.on("/api/setrestart",  HTTP_GET,  handleSetRestartRequired);
    wserver.on("/api/restart",     HTTP_GET,  handleRestart); // mas facil de manejar GET que POST (y borra pagina)
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
  #ifndef NODISPLAY
  lcd.infoclear("Webserver activo", 1, BIPOK);
  snprintf(buff, MAXBUFF, "\"%s.local:%d\"", WiFi.getHostname(), WSPORT);
  lcd.info(buff, 3);
  int msgl = snprintf(buff, MAXBUFF, "%s:%d" , WiFi.localIP().toString().c_str(), WSPORT);
  lcd.info(buff, 4, msgl);
  #endif
}

void displayRestar() {
  #ifndef NODISPLAY
  lcd.infoclear("RESTARTING...", 3);
  if (restartRequired) lcd.info("Parm updated:", 1);
  sonido.longbip(1);
  delay(config.msgdisplaymillis);
  #endif
}

void setupWS() {
  if (!MDNS.begin(HOSTNAME)) LOG_ERROR("Error iniciando mDNS");
  // --- Guardar en el servidor cabecera recibida para control de cacheado en el navegador ---
  const char *headerkeys[] = {
      "If-None-Match",        // Necesario para la validación ETag
      "If-Modified-Since"     // Necesario para la validación Last-Modified
  };
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
  wserver.collectHeaders(headerkeys, headerkeyssize);
  // ---------------------------
  httpUpdater.setup(&wserver, update_path, update_username, update_password);
  defWebpagesHandles();
  MDNS.addService("http", "tcp", WSPORT);
  wserver.begin();
  webServerAct = true;
  restartRequired = false;
  Serial.printf("[WS] HTTPUpdateServer ready!\n   --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", WiFi.getHostname(), WSPORT, update_path, update_username, update_password);
  PRINTLN("[WS] Activado webserverIP address: ", WiFi.localIP(), ":", WSPORT);
  //lee los dias de log que Domoticz guarda
  String response = getDomoticzSettingsInfo("LightHistoryDays");
  if (!response.startsWith("Err")) logDays = response.toInt();
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

