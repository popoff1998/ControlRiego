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
   Serial.println(F("[WS] HTTPUpdateServer ready!"));
   Serial.printf("[WS]    --> Open http://%s.local:%d%s in your browser and login with username '%s' and password '%s'\n\n", host, wsport, update_path, update_username, update_password);
   }

   void procesaWebServer()
   {
      wserver.handleClient();
      MDNS.update();
   }  

#endif