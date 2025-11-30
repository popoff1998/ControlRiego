/*
    OTAupdateServer.h - HTTP OTA Update Server class for ESP32 using LittleFS
    basado en HTTPUpdateServer.h de arduino-esp32 (GITHUB: arduino-esp32/libraries/HTTPUpdateServer)
    
    Nota: si existe una página /OTAupdate.htm en el dispositivo se servirá; 
    en caso contrario la página serverOTA integrada del servidor será usada.
*/

#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <LittleFS.h>
#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>

static const char serverOTA[] PROGMEM =
 R"(<!DOCTYPE html>
    <html lang='en'>
    <head>
        <meta charset='utf-8'>
        <meta name='viewport' content='width=device-width,initial-scale=1'/>
        <title>OTA update</title>
        <style>
            body {
                font-family: Arial, sans-serif;
            }
            .section-title {
                font-size: 1.5em;
                margin-top: 30px;
                margin-bottom: 10px;
            }
            input[type="submit"] {
                font-size: 1.2em; /* Aumentar el tamaño del texto */
                padding: 10px 20px; /* Aumentar el relleno interno */
                border: none;
                border-radius: 5px;
                background-color: #007BFF; /* Color de fondo */
                color: white; /* Color del texto */
                cursor: pointer;
                margin-left: 50px; /* Desplazar un poco a la derecha */
            }
            input[type="submit"]:hover {
                background-color: #c0116b; /* Color al pasar el cursor */
            }
            input[type="file"] {
                font-size: 1em; /* Aumentar el tamaño del texto */
                padding: 10px 5px; /* Aumentar el relleno interno */
            }
        </style>
    </head>
    <body>
        <form method='POST' action='' enctype='multipart/form-data'>
            <div class='section-title'>Firmware:</div>
            <input type='file' accept='.bin,.bin.gz' name='firmware'>
            <br><br>
            <input type='submit' value='Update Firmware'>
        </form>
        <hr>
        <form method='POST' action='' enctype='multipart/form-data'>
            <div class='section-title'>FileSystem:</div>
            <input type='file' accept='.bin,.bin.gz,.image' name='filesystem'>
            <br><br>
            <input type='submit' value='Update FileSystem'>
        </form>
    </body>
    </html>)";
static const char successResponse[] PROGMEM =
"<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

class HTTPUpdateServer
{
public:
    HTTPUpdateServer(bool serial_debug=false) {
        _serial_output = serial_debug;
        _server = NULL;
        _username = emptyString;
        _password = emptyString;
        _authenticated = false;
    }

    void setup(WebServer *server)
    {
        setup(server, emptyString, emptyString);
    }

    void setup(WebServer *server, const String& path)
    {
        setup(server, path, emptyString, emptyString);
    }

    void setup(WebServer *server, const String& username, const String& password)
    {
        setup(server, "/update", username, password);
    }

    void setup(WebServer *server, const String& path, const String& username, const String& password)
    {

        _server = server;
        _username = username;
        _password = password;

        // handler for the /update form page
        _server->on(path.c_str(), HTTP_GET, [&]() {
            if (_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
                return _server->requestAuthentication();

            // obtener versión compilada (si está definida) para inyectar en la página
                String version;
            #ifdef FW_VERSION
                version = String(FW_VERSION);
            #else
                version = String();
            #endif
            // Calcular el espacio máximo disponible:
            // - para firmware: ESP.getFreeSketchSpace() - 0x1000 (margen de seguridad) y alineado a 4KB
            uint32_t maxFirmwareSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            uint32_t maxFSSize = LittleFS.totalBytes();
            // leer parámetro opcional ?page=
            String page; if (_server->hasArg("page")) page = _server->arg("page");
            // Construir prefijo con VERSION, MAX_FIRMWARE_SIZE, MAX_FS_SIZE y marca SERVED (se usará también en JS del custom)
            String prefixCustom = String("<script>var VERSION = \"") + version + 
                                  String("\"; var SERVED = \"custom\"; var MAX_FIRMWARE = ") + String(maxFirmwareSize) + 
                                  String("; var MAX_FILESYSTEM = ") + String(maxFSSize) + String(";</script>");
            // lógica de selección:
            // - si page == "builtin" -> servir siempre la página integrada
            // - si page == "custom"  -> intentar servir OTAupdate.htm (si no existe, fallback a builtin)
            // - si page vacío -> comportamiento por defecto: si existe OTAupdate.htm servirla, si no fallback builtin
            if (page.equalsIgnoreCase("builtin")) {
                _server->send(200, "text/html", FPSTR(serverOTA));
                return;
            }
            // intentar servir custom si existe (page == "custom" o page is empty)
            if (LittleFS.exists("/OTAupdate.htm") && !page.equalsIgnoreCase("builtin")) {
                File f = LittleFS.open("/OTAupdate.htm", "r");
                if (f) {
                    String content = f.readString();
                    f.close();
                    content = prefixCustom + content; // inyectar script prefixCustom delante del contenido
                    _server->send(200, "text/html", content);
                    return;
                }
            }
            // fallback: serve built-in page
                _server->send(200, "text/html", FPSTR(serverOTA));
            });

        // handler for the /update form POST (once file upload finishes)
        _server->on(path.c_str(), HTTP_POST, [&]() {
            if (!_authenticated)
                return _server->requestAuthentication();
            if (Update.hasError()) {
                _server->send(418, F("text/html"), String(F("Update error: ")) + _updaterError);
            }
            else {
                // decidir respuesta según parámetro 'served' presente en la URL ( ?served=custom or builtin )
                bool servedCustom = _server->hasArg("served") && _server->arg("served").equalsIgnoreCase("custom");

                _server->client().setNoDelay(true);
                if (servedCustom) {
                    // responder con texto simple "responseOK" para custom
                    _server->send(200, "text/plain", "Update Success! Rebooting...");
                } else {
                    // respuesta tradicional (successResponse) para built-in
                    _server->send_P(200, PSTR("text/html"), successResponse);
                }
                delay(100);
                _server->client().stop();
                ESP.restart();
            }
            }, [&]() {
                // handler for the file upload, get's the sketch bytes, and writes
                // them through the Update object
                HTTPUpload& upload = _server->upload();

                if (upload.status == UPLOAD_FILE_START) {
                    _updaterError.clear();
                    if (_serial_output)
                        Serial.setDebugOutput(true);

                    _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
                    if (!_authenticated) {
                        if (_serial_output)
                            Serial.printf("Unauthenticated Update\n");
                        return;
                    }

                    if (_serial_output)
                        Serial.printf("Update: %s\n", upload.filename.c_str());
                    if (upload.name == "filesystem") {
                        //if (!Update.begin(SPIFFS.totalBytes(), U_SPIFFS)) {//start with max available size
                        if (!Update.begin(LittleFS.totalBytes(), U_SPIFFS)) {//start with max available size
                            if (_serial_output) Update.printError(Serial);
                        }
                    }
                    else {
                        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                        if (!Update.begin(maxSketchSpace, U_FLASH)) {//start with max available size
                            _setUpdaterError();
                        }
                    }
                }
                else if (_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()) {
                    if (_serial_output) Serial.printf(".");
                    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                        _setUpdaterError();
                    }
                }
                else if (_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()) {
                    if (Update.end(true)) { //true to set the size to the current progress
                        if (_serial_output) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                    }
                    else {
                        _setUpdaterError();
                    }
                    if (_serial_output) Serial.setDebugOutput(false);
                }
                else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
                    Update.end();
                    if (_serial_output) Serial.println("Update was aborted");
                }
                delay(0);
            });
    }

    void updateCredentials(const String& username, const String& password)
    {
        _username = username;
        _password = password;
    }

protected:
    void _setUpdaterError()
    {
        if (_serial_output) Update.printError(Serial);
        StreamString str;
        Update.printError(str);
        _updaterError = str.c_str();
    }

private:
    bool _serial_output;
    WebServer *_server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
};


#endif