#ifdef DOMOTICZ

#include "Control.h"

  //==================================================================================================//
 //=================================== COMUNICACION CON DOMOTICZ ====================================//
 //==================================================================================================// 
 
 HTTPClient httpclient;
 WiFiClient client;

  //-----------------------  API con Domoticz ------nuevo formato v2023.2 en adelante------
  #define COMMANDPRF    "/json.htm?type=command&param="
  #define SWITCHDEVICE  "switchlight&idx=%d&switchcmd=%s"
  #define QUERYDEVICE   "getdevices&rid=%d"
  #define GETSWITCHLOG  "getlightlog&idx=%d"
  #define GETSUNHOURS   "getSunRiseSet"
  #define GETSETTINGS   "getsettings"
  //---------------------------------------------------------------------------------------
  
//==================================================================================================//
//=================== Funciones primarias basicas y de ayuda     ===================================//
//==================================================================================================//

/**-------------------------------------------------------------------------------
 * Obtiene el SCD_ID de una zona dado su numero de zona (1 a NUMZONAS).
 */
uint16_t getSCD_ID(uint8_t zonaNumber) {
    // La zonaNumber va de 1 a N, el índice (zIndex) va de 0 a N-1.
    int zIndex = zonaNumber - 1; 
    if (zIndex >= 0 && zIndex < NUMZONAS) {
        return config.zona[zIndex].idx; // En el caso de Domoticz, el SCD_ID es el IDX
    }
    return 0; // Devolver 0 si la zona no es válida
}

// Función auxiliar que registra el error específico, el JSON completo, 
// y devuelve el código de error ("Err3").
String returnErr3(const String &fullResponse, const char* msg1, const char* msg2 = "", const char* msg3 = "") {
    LOG_ERROR(" ** [ERROR] ", msg1, msg2, msg3); 
    LOG_ERROR(" ** [ERROR] JSON de entrada: ", fullResponse.c_str());
    return "Err3";
}

/**------------------------------------------------------------------------------------
 * Procesa la respuesta JSON de Domoticz y devuelve el valor del campo solicitado.
 * @param response  La cadena JSON de Domoticz.
 * @param campo     El nombre del campo a extraer (ej: "Sunrise" o "Status").
 * @param level     Nivel donde se espera el campo (TOP_LEVEL o RESULT_ARRAY_0).
 * @return          El valor del campo como String, o un código de error ("Err3").
 */ 
enum JsonLevel {
    TOP_LEVEL = 0,     // Para campos como "Sunrise", "ServerTime"
    RESULT_ARRAY_0 = 1 // Para campos de dispositivo dentro de "result[0]"
};    
String parseResponse(const String &response, const char *campo, JsonLevel level)
{
    if (response.startsWith("Err")) return response;
    String respTrim = response;
    respTrim.trim(); // Sanitizar la respuesta
    JsonDocument jsondoc; 
    DeserializationError error = deserializeJson(jsondoc, respTrim);
    if (error) return returnErr3(respTrim, "deserializeJson() failed: ", error.c_str());
    JsonVariant field;
    if (level == TOP_LEVEL) {
        field = jsondoc[campo];
    } else if (level == RESULT_ARRAY_0) {
        // Validar que la ruta 'result[0]' exista y sea segura
        if (!jsondoc.containsKey("result") || !jsondoc["result"].is<JsonArray>()) {
          return returnErr3(respTrim, "parseResponse: 'result' no encontrado o no es array"); }
        field = jsondoc["result"][0][campo];
    } else {
          return returnErr3(respTrim, "parseResponse: nivel desconocido");    }
    // Verifica si el campo existe y no es null
    if (field.isNull()) {
          return returnErr3(respTrim, "parseResponse: campo '", campo, "' no encontrado o NULL");}
    // Extrae el valor como String.
    String contenido = field.as<String>();
    contenido.trim();
    // Informa si la String resultante esta vacía
    if (contenido.isEmpty()) LOG_DEBUG(" ** [WARNING] parseResponse: campo '", campo, "' vacío");
    else LOG_DEBUG("Campo '", campo, "': ", contenido);
    return contenido;
}

/**---------------------------------------------------------------
 * Comunicacion con Domoticz usando httpGet
 * HTTPClient tiene dos timeouts:
 *    ConnectTimeout: tiempo maximo para establecer la conexion con el servidor (default 5000ms)
 *    response Timeout: tiempo maximo para recibir la respuesta del servidor (default 5000ms)
 *  En este caso, al ser la conexión local, nos interesa reducir el ConnectTimeout y tambien el Timeout
 *  para evitar bloqueos largos en caso de que Domoticz no responda.
 */ 
String httpGetDomoticz(const String &message) 
{
  LOG_TRACE("");
  lcd.displayON();  // para evitar pantalla sin info en caso de retardo en la respuesta
  String tmpStr = "http://" + String(config.domoticz_ip) + ":" + config.domoticz_port + String(message);
  LOG_DEBUG("TMPSTR:", tmpStr);
  httpclient.begin(client, tmpStr);
  httpclient.setConnectTimeout(HTTPCLIENTCONNECTTIMEOUT);
  httpclient.setTimeout(HTTPCLIENTRESPONSETIMEOUT);
  String response = "{}";
  unsigned long currentMillis = millis();
  int httpCode = httpclient.GET();
  LOG_DEBUG("respuesta recibida en :", millis()-currentMillis, "ms");
  if(httpCode > 0) {
    if(httpCode == HTTP_CODE_OK) {
        response = httpclient.getString();
        #ifdef EXTRADEBUG
        Serial.print(F("httpGetDomoticz RESPONSE: "));Serial.println(response);
        #endif
    } else {  
        LOG_WARN("respuesta no OK de Domoticz, HTTP_CODE: ", httpCode, "(see RFC7231)");
        return "Err3";
      }  
    } else if(Estado.estado != ERROR) {   // para no repetir mensajes de error
        LOG_ERROR("ERROR comunicando con Domoticz: ", httpclient.errorToString(httpCode).c_str()); 
        return "Err2";
  }      
  //vemos si la respuesta recibida del Domoticz indica status error
  int pos = response.indexOf("\"status\" : \"ERR");
  if(pos != -1) {
    LOG_ERROR(" ** Domoticz a devuelto error: ", response.c_str()); 
    return "ErrX";
  }  
  httpclient.end();
  return response;
}  

/**-----------------------------------------------------------------------------------
 * Convierte una String (campo Description) a un valor entero para el factor de riego.
 * Devuelve 100 por defecto si no es un número válido.
 */
int convertFactorString(const String &response)
{
    char* factorstr = (char*)response.c_str(); // Obtener el puntero al contenido
    long int factor = strtol(factorstr, NULL, 10);
    // Controlar devolviendo 100 si la cadena está vacía (solo comentarios o vacío)
    if (factor == 0) {
      if (strlen(factorstr) == 0) return 100;    //campo comentarios vacio -> por defecto 100
      if (!isdigit(factorstr[0])) return 100;    //comentarios no comienzan por 0
    }
    LOG_DEBUG("Factor de riego leido: ", factor);
    return (int)factor;
}


//==================================================================================================//
//=================== Funciones intermedias COMUNICACION CON SCD ===================================//
//==================================================================================================//

/**------------------------------------------------------------------------------------
 * Envia mandato al SCD (Sistema de Control Domotico) y devuelve json con la respuesta
 */
String cmdtoSCD(const char* mandato)
{
  LOG_DEBUG(" comando: ", mandato);
  String message = COMMANDPRF + String(mandato);
  return httpGetDomoticz(message);
}

/**---------------------------------------------------------------
 * devuelve campo con informacion del dispositivo con el idx pasado
 */
String deviceInfo(int idx, const char *campo)
{
    char message[150];
    snprintf(message, sizeof(message), QUERYDEVICE, idx);
    // 1. Comunicación: Obtener la respuesta JSON
    String response = cmdtoSCD(message);
    if (response.startsWith("Err")) {
        LOG_ERROR(" ** [ERROR] IDX: ", idx, " [HTTP] GET... failed");
        return response; 
    }
    // 2. Procesamiento: Usar parseResponse, especificando que el campo está en RESULT_ARRAY_0
    return parseResponse(response, campo, RESULT_ARRAY_0);
}

/**---------------------------------------------------------------
 * Envia a domoticz orden de on/off del idx correspondiente.
 * Devuelve el código de error específico a través de Estado.error. 
 * En caso de error no lo activa ni genera alertas visuales o sonoras (lo hara la funcion llamante)
 */
bool deviceSwitch(uint8_t zona, const char *msg, int retries)
{
    uint16_t idx = getSCD_ID(zona);
    LOG_DEBUG("idx:", idx, " ", msg, "(", retries, "intentos)");
    // 1. Caso IDX=0 (Simulación OK)
    if(idx == 0) return true;
    // 2. Caso E1 (Error de WiFi)
    if(!Estado.connected && !Estado.modoDEMO) { Estado.error = E1; return false; }
    char message[150];
    snprintf(message, sizeof(message), SWITCHDEVICE, idx, msg);
    String response;
    for (int i = 0; i < retries; i++) { //activacion con reintentos
        if ((simular.ErrorON && strcmp(msg, "On") == 0) || (simular.ErrorOFF && strcmp(msg, "Off") == 0))
            response = "ErrX";
        else if (!Estado.modoDEMO)
            response = cmdtoSCD(message);
        if (response == "ErrX") {  // solo reintentamos si Domoticz informa del estado de la zona
            sonido.bip(1); // bip de "reintento"
            LOG_WARN("IDX:", idx, "fallo en", msg, "(intento", i+1, "de", retries, ")");
            delay(DELAYRETRY);
        } else
            break;
    }
    // 3. Caso Error (Fallo en la Conmutación o Comunicación)
    if (response.startsWith("Err")) {
        // Establecer el código de error explícitamente, sin lanzar alertas.
        if (response == "ErrX") {
            if (strcmp(msg, "On") == 0) Estado.error = E4; // E4: error al iniciar riego
            else Estado.error = E5; // E5: error al parar riego
        } else {
            Estado.error = E2; // E2: otro error al comunicar con domoticz
        }
        LOG_ERROR("IDX:", idx, "fallo en", msg);
        return false;
    }
    // 4. Caso OK
    Estado.error = NOERROR;
    return true;
}

/**---------------------------------------------------------------
 * lee factor de riego del Domoticz, almacenado en campo Description
 */
int getFactor(uint8_t zona, bool &factorRiegosLeido)
{
  LOG_TRACE("");
  uint16_t idx = getSCD_ID(zona);
  if(idx == 0) return 100; //si el IDX es 0 devolvemos 100 sin procesarlo (boton no asignado)
  factorRiegosLeido = false;
  String response = deviceInfo(idx, "Description");
  if (response.startsWith("Err")) {
      if (Estado.modoDEMO) return 999;  //si estamos en modoDEMO devolvemos 999 y no damos error
      if(response != "Err2") {
        if (config.verify) statusError(E3); //error de deserializacion, posible IDX inexistente
      } else statusError(E2, RECUPERABLE); //error de conexion con Domoticz recuperable
      LOG_WARN("GETFACTOR IDX: ", idx, " respuesta recibida: ", response.c_str());
      return 100;
  }
  // Si hemos leido correctamente campo Description (numero, campo vacio o solo con comentarios)
  // el IDX existe, consideramos leido OK el factor riego. 
  factorRiegosLeido = true;
  return convertFactorString(response);
} //fin getFactor

/**------------------------------------------------------------------------------
 * Obtiene del Domoticz el log de la zona en formato JSON
 * (ultimos 15 dias, es un parametro ajustable en el Domoticz -> log historico de luces/interruptores)
 */ 
String readLogFile(int zona)
{
  int idx = config.zona[zona-1].idx;
  LOG_DEBUG("zona:", zona, "idx:", idx);
  if(idx == 0) return "No asignado";
  char message[150];
  sprintf(message, GETSWITCHLOG,idx);
  return cmdtoSCD(message);
}

//lee info amanecer/anochecer del Domoticz
bool getDiaNoche(char* amanecer, char* anochecer)
{
  String response = cmdtoSCD(GETSUNHOURS);
  LOG_DEBUG("Respuesta recibida del Domoticz: ", response.c_str());
  if (response.startsWith("Err")) return false;
  JsonDocument jsondoc;
  DeserializationError error = deserializeJson(jsondoc, response);
  if (error) return false; //error de deserializacion
  strlcpy(amanecer, jsondoc["CivTwilightStart"] | "NO TIME", 8);
  strlcpy(anochecer, jsondoc["CivTwilightEnd"] | "NO TIME", 8);
  LOG_DEBUG("amanece ", amanecer, "anochece ", anochecer);
  return true;
}

/**-------------------------------------------------------------------------------
 * devuelve campo de configuracion general del Domoticz
 */
String getDomoticzSettingsInfo(const char *campo)
{
    String response = cmdtoSCD(GETSETTINGS);
    if (response.startsWith("Err")) {
        LOG_ERROR(" ** [ERROR]  [HTTP] GET... failed");
        return response; 
    }
    return parseResponse(response, campo, TOP_LEVEL);
}

/**---------------------------------------------------------------
 * lee datos de temperatura y humedad del sensor remoto con el idx pasado
 * devuelve 999 si no hay sensor asignado (idx=0) o si hay error
 */
float getRemoteTemperature(void)
{
  int idx = config.tempRemoteIdx;
  LOG_TRACE("sensor temp IDX: ", idx);
  // si el IDX es 0 devolvemos 999 sin procesarlo (sensor no asignado)
  if(idx == 0) return 999;
  if(!checkWifi()) return 999; //si no hay conexion devolvemos 999 y no damos error
  String response = deviceInfo(idx, "Data");  //campo Data devuelve temperatura como caracteres (ej. "9.4 C")
  //String response = deviceInfo(idx, "Temp");  //campo Temp devuelve temperatura como numero (ej. 9.4)
  LOG_INFO("Temperatura recibida del Domoticz: ", response);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    LOG_WARN("IDX: ", idx, " respuesta recibida: ", response.c_str());
    return 999;  //devolvemos 999 para indicar temperatura no valida
  }
  //devolvemos la temperatura del sensor en Domoticz del json (campo Data)
  float temp = strtof(response.c_str(), NULL); //strtof convierte a float (ej. 9.4 C -> 9.4)
  LOG_INFO("devuelve temperatura = ",temp);
  return temp;
}

/**---------------------------------------------------------------
 * lee y actualiza descripcion de zona en config desde Domoticz
 */
void updateZoneDescription(int i) {
      String response = deviceInfo(config.zona[i].idx, "Name");
      if (response.startsWith("Err") || strlen(response.c_str()) == 0) {
        LOG_WARN("Sin descripcion de zona", i+1, "idx=", config.zona[i].idx, "response:", response.c_str());
        return; //error en la lectura de la descripcion, no actualizamos nada y pasamos al siguiente idx
      }
      LOG_INFO("\t descripcion ZONA", i+1, "actualizada en config");
      strlcpy(config.zona[i].desc, response.c_str(), sizeof(config.zona[i].desc));
}

/**---------------------------------------------------------------
 * verifica status de la zona coincide con el pasado, devolviendo true en ese caso
 */
bool queryStatus(uint8_t zona, const char *status)
{
  uint16_t idx = getSCD_ID(zona);
  LOG_DEBUG("idx:", idx, "status:", status, "allSimFlags:", simular.all_simFlags);

  if(simular.ErrorVerifyON) {   // simulamos EV no esta ON en Domoticz
    if(strcmp(status, "On") == 0) return false; else return true; 
  } 
  if(simular.ErrorVerifyOFF) {   // simulamos EV no esta OFF en Domoticz
    if(strcmp(status, "Off") == 0) return false; else return true; 
  } 
  if(!Estado.connected) {
    if(Estado.modoDEMO) return true; //si estamos en modoDEMO devolvemos true y no damos error
    else {
      Estado.error = E1;
      return false;
    }
  }
  String response = deviceInfo(idx, "Status");
  LOG_DEBUG("response:", response);
  //procesamos la respuesta para ver si se ha producido error:
  if (response.startsWith("Err")) {
    if (Estado.modoDEMO) return true;  //si estamos en modoDEMO devolvemos true y no damos error
    if (response == "Err2") Estado.error = E2;
    else Estado.error = E3;
    LOG_WARN("queryStatus devuelve FALSE, error ", response.c_str());
    return false;
  }
  #ifdef EXTRADEBUG
    Serial.printf( "queryStatus verificando, status=%s / actual=%s \n" , status, response);
    Serial.printf( "                status_size=%d / actual_size=%d \n" , strlen(status), response.length());
  #endif
  if(strcmp(response.c_str(), status) == 0) return true; //si coinciden devolvemos true
  else{
    if(Estado.modoDEMO) return true; //siempre devolvemos ok en modo simulacion
    LOG_WARN("queryStatus devuelve FALSE, status / actual =",status,"/",response.c_str());
    return false;
  }  
} //fin queryStatus

  //==================================================================================================//
 //============================ FIN COMUNICACION CON SCD/DOMOTICZ ===================================//
//==================================================================================================//

#endif  //DOMOTICZ