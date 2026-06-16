/*
  Inicializacion de E/S y gestion de botones y leds 
*/

#include "Control.h"

//Globales a este modulo
unsigned long lastMillis;
#define DEBOUNCEMILLIS 20
volatile uint16_t estadoLeds = 0;  // necesita ser volatile porque se usa en interrupcion (ticker parpadeo leds de zonas)


#define mcpO_ADDR 0x20    // MCP de salidas (LEDs)
#define mcpI_ADDR 0x21    // MCP de entradas (BOTONES)


MCP23017 mcpI = MCP23017(mcpI_ADDR, Wire1);  // usamos segundo bus I2C para no interferir con el display lcd
MCP23017 mcpO = MCP23017(mcpO_ADDR, Wire1);  // usamos segundo bus I2C para no interferir con el display lcd

bool sLEDR = LOW;
bool sLEDG = LOW;
bool sLEDB = LOW;


void initWire() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_SPEED);     // primer bus I2C para la pantalla lcd
  Wire1.begin(I2C_SDA1, I2C_SCL1, I2C_CLOCK_SPEED);  // segundo bus I2C para los MCPs
}

void mcpOinit() {
    mcpO.init();
	/*set i/o pin direction as OUTPUT for both ports A and B en MCP de salidas (leds))*/
	/* void portMode(port, directions, pullups, inverted); */
    mcpO.portMode(MCP23017Port::A, 0x00);  // output
    mcpO.portMode(MCP23017Port::B, 0b00001100, 0b00001100, 0b00001100);  // output (salvo B2-B3: input, pull-up, polaridad invertida)
}

void mcpIinit() {
    mcpI.init();
	/*set i/o pin direction as input for both ports A and B en MCP de entradas (botones)*/
	/* void portMode(port, directions, pullups, inverted); */
    mcpI.portMode(MCP23017Port::A, 0b01111111, 0b01111111, 0b01111111);  // input (salvo A7), pull-up, polaridad invertida
    mcpI.portMode(MCP23017Port::B, 0b01111111, 0b01111111, 0b01111111);  // input (salvo B7), pull-up, polaridad invertida
}


void initLeds()
{
  int i;
  uint ledOrder[] = { lGRUPO1 , lGRUPO2 , lGRUPO3 , lGRUPO4 ,
                      lZONA1 , lZONA2 , lZONA3 , lZONA4 , lZONA5 , lZONA6 , lZONA7 , lZONA8 , lZONA9 };
  size_t numLeds = ELEMENTCOUNT(ledOrder);
  apagaLeds();
  for(i=0;i<numLeds;i++) {
    led(ledOrder[i],ON);
    delay(300);
    led(ledOrder[i],OFF);
  }
  delay(200);
  enciendeLeds();
  delay(500);
  apagaLeds();
}


void initGPIOs()
{
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(ENCBOTON, INPUT);
  // LED inicial de estado/error
  digitalWrite(LEDR, ON);
}

void apagaLeds()
{
  analogWrite(LEDR, 0);
  analogWrite(LEDG, 0);
  analogWrite(LEDB, 0);
  mcpO.writePort(MCP23017Port::A, 0x00);
  mcpO.writePort(MCP23017Port::B, 0x00);
  estadoLeds = 0;
}

void enciendeLeds()
{
  analogWrite(LEDR, ledlevel());
  analogWrite(LEDG, ledlevel());
  analogWrite(LEDB, ledlevel());
  mcpO.writePort(MCP23017Port::A, 0xFF);
  mcpO.writePort(MCP23017Port::B, 0xFF);
  estadoLeds = 0xFFFF;
}

void ledRGB(int  R, int G, int B)
{
  ledPWM(LEDR,R);
  ledPWM(LEDG,G);
  ledPWM(LEDB,B);
}

void deleteParmSignal(uint veces)
{
  LOG_TRACE("in deleteParmSignal");
  uint i;
  for(i=0;i<veces;i++) {
    ledRGB(ON,OFF,OFF);
    delay(300);
    ledRGB(OFF,ON,OFF);
    delay(300);
  }
}

void wifiClearSignal(uint veces)
{
  LOG_TRACE("in wifiClearSignal");
  uint i;
  for(i=0;i<veces;i++) {
    ledRGB(ON,OFF,OFF);
    delay(300);
    ledRGB(OFF,OFF,ON);
    delay(300);
  }
}

// Hace parpadear un led PWM (RGB) segun su estado actual (al ser llamada desde el Ticker)
void parpadeoLedPWM(int id) {
  bool estadoActual;
  if (id == LEDR) estadoActual = sLEDR;
  else if (id == LEDG) estadoActual = sLEDG;
  else if (id == LEDB) estadoActual = sLEDB;
  else return; // Por seguridad, si el ID no es válido
  ledPWM(id, !estadoActual);
}

// Hace parpadear un led de zona segun su estado actual
void parpadeoLedZona(int ledid)
{
  led(ledid,!estadoLedId(ledid));
}

// Hace parpadear los leds de zona pasados en el array
void parpadeoLedZonas(S_ledsParpadeo* datos)
{
    for(uint8_t i = 0; i < datos->cantidad; i++) { 
        int ledid = datos->leds[i];
        led(ledid, !estadoLedId(ledid));
    }
}


// Versión para parpadeo RAPIDO, NORMAL y LENTO de leds PWM (RGB) o de zonas
void setParpadeo(Ticker &t, velocidad_parpadeo vel, void (*f)(int), int id) {
    t.detach();
    if (vel <= 0) return;
    t.attach(vel / 10.0, f, id);
}

// Versión solo para PARAR temporizador de parpadeo
void setParpadeo(Ticker &t, velocidad_parpadeo vel) {
    t.detach();
}

// Enciende o apaga un led deteniendo su posible parpadeo (led zona 1-16, led PWM 25-26-27)
void setLed(Ticker &t, estado_led estado, int ledid) {
    t.detach();
    ledid > 16? ledPWM(ledid,estado) : led(ledid, estado);
}

//activa o desactiva el led RGB con color amarillo (R+G=Y)
void ledYellow(int estado)
{
  if(estado == ON)  ledRGB(ON,ON,OFF);     //  LED AMARILLO
  if(estado == OFF) ledRGB(OFF,OFF,OFF);  //  los apaga para parpadeo
}

// deja led RGB segun estado wifi y modoDEMO, o estados error y configurando
void setLedStatus()
{
  extern Ticker tic_LedError;
  extern Ticker tic_APLed;
  extern Ticker tic_WifiLed;
  setParpadeo(tic_APLed, PARAR);
  setParpadeo(tic_WifiLed, PARAR);
  setParpadeo(tic_LedError, PARAR);
  if (Estado.estado == ERROR) ledRGB(ON,OFF,OFF);            // rojo fijo
  else if (Estado.estado == CONFIGURANDO) ledRGB(ON,ON,OFF); // amarillo fijo
  else ledRGB(OFF,Estado.connected,Estado.modoDEMO);         // verde si wifi + azul si demo                
}  


// enciende o apaga un led controlado por PWM
void ledPWM(uint8_t id,int estado)
{
  estado ? analogWrite(id, ledlevel()) : analogWrite(id, 0);
  switch (id) {
    case LEDR: sLEDR = estado; break;
    case LEDG: sLEDG = estado; break;
    case LEDB: sLEDB = estado; break;
  }  
  #ifdef EXTRADEBUG2
    Serial.printf("[led R/G/B : %d/%d/%d]\n", sLEDR, sLEDG, sLEDB);
  #endif
}

// enciende o apaga un led controlado por el expansor MCP
void led(uint8_t id,int estado)
{
    #ifdef EXTRADEBUG2
    Serial.print(F("[TRACE: en funcion led]"));
    Serial.print(F("estadoLeds : "));Serial.println(estadoLeds,BIN);
    Serial.print(F("ledID : "));Serial.println(id,DEC);
    #endif

    //Por seguridad no hacemos nada si id=0
    if(id==0) return;
    if(estado == ON) estadoLeds |= (1 << (id-1));
    else estadoLeds &= ~(1 << (id-1));
    //convertimos a la parte baja y alta
    uint8_t bajo = (uint8_t)((estadoLeds & 0x00FF));
    uint8_t alto = (uint8_t)((estadoLeds & 0xFF00) >> 8);
    mcpO.writePort(MCP23017Port::A, bajo);
    mcpO.writePort(MCP23017Port::B, alto);
}

// devuelve el estado actual de un led (ON/OFF)
bool estadoLedId(int ledID)
{
  #ifdef EXTRADEBUG2
    Serial.print(F("estadoLeds : "));Serial.println(estadoLeds,BIN);
    Serial.print(F("ledID : "));Serial.println(ledID,DEC);
  #endif
  return((estadoLeds & (1 << (ledID-1))));
}

// ON/OFF atenuacion LEDG y LEDB
void dimmerLeds(bool status)
{
  if(status) {
    LOG_TRACE("leds atenuados ");
    if(Estado.connected) analogWrite(LEDG, config.dimmlevel);
    if(Estado.modoDEMO) analogWrite(LEDB, config.dimmlevel);
  }
  else {
    LOG_TRACE("leds brillo normal ");
    if(Estado.connected) analogWrite(LEDG, config.maxledlevel);
    if(Estado.modoDEMO) analogWrite(LEDB, config.maxledlevel);
  }  
}

// devuelve el nivel de brillo actual para leds RGB
int  ledlevel()
{
  return (Estado.reposo ? config.dimmlevel : config.maxledlevel);
}

// Lee el estado de todas las entradas (botones) y devuelve un bitmask de 16 bits
uint16_t readInputs()
{
  uint8_t    alto, bajo, altoMCPO;
  bajo = mcpI.readPort(MCP23017Port::A);
  alto = mcpI.readPort(MCP23017Port::B);
  //**  los bits 11 y 12 correspondientes a bPAUSE y bSTOP leidos de mcpO  
  // se deben convertir e integrar como bits 15 y 16 con los leidos de mcpI
  altoMCPO = mcpO.readPort(MCP23017Port::B);
  alto = alto | ((altoMCPO << 4) & 0b11000000);
  return bajo | (alto << 8);
}

//testea estado instantaneo del boton(id) pasado
//devolviendo 1 si es igual a state y 0 en caso contrario 
bool testButton(uint16_t id,bool state)
{
  uint16_t buttons = readInputs();
  bool result = ((buttons & id) == 0)?0:1;
  if (result == state) return 1;
   else return 0;
}

// Lee el estado del encoderSW (instantaneo o con antirebote segun modo)
void leerEncoderSW() {
  // NOTA: el encoderSW esta en estado HIGH en reposo y en estado LOW cuando esta pulsado
  // Hace lectura instantanea si no estamos en modo CONFIGURANDO
  //   (encoderSw se comporta como modificador de otro boton)
  if (Estado.estado != CONFIGURANDO) encoderSW = !digitalRead(ENCBOTON);
  // Si estamos en modo CONFIGURANDO, aplicamos debounce
  //   (encoderSW se comporta como boton independiente emulando bPAUSE)
  else {
    static long lastDebounceTime = 0;
    static int lastState = 0;
    int reading = !digitalRead(ENCBOTON);
    if (reading != lastState) lastDebounceTime = millis();
    if ((millis() - lastDebounceTime) > DEBOUNCEMILLIS) {
        if (reading != encoderSW) encoderSW = reading;
    }
    lastState = reading;
  }  
  if (Estado.reposo && encoderSW) reposoOFF(); // pulsar boton del encoder saca del reposo
}

// Lee el estado de los botones y devuelve un puntero al primer boton que ha cambiado de estado
S_BOTON *parseInputs(bool read)
{
  int i;
  //Para el debounce
  unsigned long currentMillis = millis();
  if(currentMillis < (lastMillis + DEBOUNCEMILLIS)) return nullptr;
  else lastMillis = currentMillis;
  uint16_t inputs = readInputs();
  //analizamos estado de los botones habilitados en la estructura Boton[]
  for (i=0;i<NUM_S_BOTON;i++) {
    //Nos saltamos los disabled
    if (!Boton[i].flags.enabled) continue;
    Boton[i].estado = inputs & Boton[i].bID;
    //Solo si el estado del boton ha cambiado (o tiene habilitado el HOLD) devuelve cual ha sido 
    if ((Boton[i].estado != Boton[i].ultimo_estado) || (Boton[i].estado && Boton[i].flags.hold && !Boton[i].flags.holddisabled))
    {
      Boton[i].ultimo_estado = Boton[i].estado;
      if (Boton[i].estado || Boton[i].flags.dual) {
        #ifdef EXTRADEBUG
          if (!read) Serial.print(F("Cleared: "));
          Serial.printf("Boton: %s  id: %#X  Estado: %d \n", Boton[i].desc, Boton[i].bID, Boton[i].estado);
        #endif
        if (read) return &Boton[i]; //si no clear retorna 1er boton que ha cambiado de estado
      }
    }
  }
  return nullptr;
}

/**---------------------------------------------------------------
 * En modo configuracion, encoderSW simula pulsación de PAUSE
 * (selecciona item menu, valida cambios, etc)
 */
void simulaPauseIfEncoderSW(bool initialize) {
    static bool simulaPausePrev = false;
    // Si nos piden inicializar, actualizamos el estado anterior y salimos
    if (initialize) { simulaPausePrev = encoderSW; return; } 
    S_BOTON* pPAUSE = getBotonPointer(bPAUSE);         
    // 1. TRANSICIÓN: PULSO (De false a true)
    if (encoderSW && !simulaPausePrev) {
        simulaPausePrev = true;
        pPAUSE->estado = true; 
        boton = pPAUSE; 
        LOG_DEBUG("bPAUSE PULSADO simulado.");
        return;
    }
    // 2. TRANSICIÓN: LIBERACIÓN (De true a false)
    if (!encoderSW && simulaPausePrev) {
        simulaPausePrev = false;
    } 
}

void panicNotFound(const char* contexto, uint16_t id) {
    char msg[64];
    snprintf(msg, sizeof(msg), "!!! BUG: bID %04X no encontrado en %s[]", id, contexto);
    stopHW(msg); // El sistema se detiene aquí
}

// devuelve el puntero directo en memoria de la estructura del boton pasado por bID
S_BOTON* getBotonPointer(uint16_t id)
{
  for (int i = 0; i < NUM_S_BOTON; i++) {
    if (Boton[i].bID == id) return &Boton[i]; // Devolvemos la dirección de memoria directa
  }
  panicNotFound("Boton", id); // Si llega aquí, detiene el HW de forma segura
  return nullptr;             // Nunca se alcanzará
}

// devuelve la posicion en array Zonas[] (zona-1) del boton que se le ha pasado (bID)
int getZonaIndex(uint16_t id)
{
  for (int i=0;i<NUMZONAS;i++) {
    if (Zonas[i] == id) return i;
  }
  char msg[64];
  panicNotFound("Zonas", id);
  return -1;   // Nunca se alcanzará
}

// devuelve la posicion en array Grupos[] (grupo-1) del boton que se le ha pasado (bID)
int getGroupIndex(uint16_t id)
{
  #ifdef M3GRP
    // en modo M3GRP el id que se recibe es el del boton multirriego, pero el grupo seleccionado se obtiene de la posicion del selector de multirriego
    id = getMultiStatus(); 
  #endif
  for (int i=0;i<NUMGRUPOS;i++) {
    if (Grupos[i] == id) return i;
  }
  panicNotFound("Grupos", id);
  return -1;   // Nunca se alcanzará
}

// salva apuntador a la zona en curso y precalcula zindex y znumber para no tener que recalcularlo cada vez que se necesite
void setZonaEnCurso(S_BOTON* pBoton) {
    zonaEnCurso.pBoton  = pBoton;
    zonaEnCurso.zindex  = getZonaIndex(pBoton->bID);
    zonaEnCurso.znumber = zonaEnCurso.zindex + 1;
    LOG_DEBUG("Zona apuntada:", zonaEnCurso.znumber, "(" ,config.zona[zonaEnCurso.zindex].desc, ")");
}


// fin de src/botones.cpp