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

extern void handleRestart();

// Página HTML mínima integrada para la actualización OTA (servida si no existe /OTAupdate.htm en el dispositivo)
static const char serverOTA[] PROGMEM =
R"rawota(<!DOCTYPE html>
<html lang='es'>
<head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width,initial-scale=1'/>
    <title>OTA Fallback</title>
    <style>
        body{font-family:Arial,sans-serif;padding:30px;color:#222;line-height:1.6;max-width:500px;margin:0 auto}
        h2{color:#135a8a;border-bottom:2px solid #eee;padding-bottom:10px}
        .t{font-size:1.1em;font-weight:700;margin:25px 0 10px;display:block}
        input[type='file']{display:block;margin:15px 0;padding:10px;border:1px solid #ddd;border-radius:4px;width:100%;box-sizing:border-box}
        input[type='submit']{font-size:1.1em;padding:12px 25px;border:none;border-radius:5px;background:#007BFF;color:#fff;cursor:pointer;width:100%;transition:0.3s}
        input[type='submit']:hover{background:#c0116b}
        hr{margin:40px 0;border:0;border-top:1px solid #eee}
    </style>
</head>
<body>
    <h2>OTA Update</h2>
    <form method='POST' enctype='multipart/form-data' onsubmit='return v(this)'>
        <span class='t'>Firmware (.bin)</span>
        <input type='file' name='firmware' accept='.bin' required>
        <input type='submit' value='Update Firmware'>
    </form>
    <hr>
    <form method='POST' enctype='multipart/form-data' onsubmit='return v(this)'>
        <span class='t'>FileSystem (.bin/.image)</span>
        <input type='file' name='filesystem' accept='.bin,.image' required>
        <input type='submit' value='Update FileSystem'>
    </form>
    <script>
        function v(f){
            var i=f.querySelector('input[type=file]');
            if(!i.files.length){alert('Selecciona archivo');return false;}
            var b=f.querySelector('input[type=submit]');
            b.disabled=true;
            b.value='Enviando... (espera)';
            b.style.background='#A9A9A9';
            return true;
        }
    </script>
</body>
</html>)rawota";

static const char successResponse[] PROGMEM =
"<!DOCTYPE html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='15;URL=/'></head>"
"<body>Update Success! Rebooting...</body></html>";

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
            // lógica de selección:
            // - si page == "custom"  -> intentar servir OTAupdate.htm (si no existe, fallback a builtin)
            // - si page == "builtin" -> servir siempre la página integrada
            // - si page vacío -> comportamiento por defecto: se sirve la builtin
            if (_server->arg("page").equalsIgnoreCase("custom")) {
                    if (LittleFS.exists("/OTAupdate.htm")) {
                        File f = LittleFS.open("/OTAupdate.htm", "r");
                        if (f) {
                            _server->streamFile(f, "text/html");
                            f.close();
                            return;
                        }
                    }
                    LOG_WARN("Custom OTA page requested but /OTAupdate.htm not found.");
                }
                // Comportamiento por defecto (page vacío, "builtin" o error en custom): servir integrada
                LOG_INFO("Serving builtin page");
                _server->send(200, "text/html", FPSTR(serverOTA));
            });

        // handler for the /update form POST (once file upload finishes)
        _server->on(path.c_str(), HTTP_POST, [&]() {
            if (!_authenticated)
                return _server->requestAuthentication();
            if (Update.hasError()) {
                _server->send(418, F("text/plain"), String(F("Update error: ")) + _updaterError);
            }
            else {
                // decidir respuesta según parámetro 'served' presente en la URL ( ?served=custom or builtin )
                bool servedCustom = _server->arg("page").equalsIgnoreCase("custom");
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
                // restart con mensajes en display o directo según si se ha servido página custom o builtin
                servedCustom ? handleRestart() : ESP.restart();
            }
            }, [&]() {
                // handler for the file upload, get's the sketch bytes, and writes
                // them through the Update object
                HTTPUpload& upload = _server->upload();

                if (upload.status == UPLOAD_FILE_START) {
                    if (upload.filename.length() == 0) return; // Ignorar si no hay nombre de archivo
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