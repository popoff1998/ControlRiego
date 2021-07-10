#include "Control.h"

void startAP()
{
    const char ssid[] = "ardomoaqua";
    IPAddress local_ip(192,168,200,1);
    IPAddress gateway(192,168,200,1);
    IPAddress subnet(255,255,255,0);
    
   WiFi.mode(WIFI_AP);
   WiFi.softAPConfig(local_ip,gateway,subnet);
   WiFi.softAP(ssid); 
}