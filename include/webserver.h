#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>


//Prototipos
void startAP(void);

//Globales
#ifdef __MAIN__
    AsyncWebServer WebServer(80);
#endif