#include "Control.h"
#include "MCP23017.h"

//Globales a este modulo
unsigned long lastMillis;
#define DEBOUNCEMILLIS 20
volatile uint16_t ledStatus = 0;

//MCP23017 mcp(I2C_SDA,I2C_SCL);    //create mcp instance

#define mcpO_ADDR 0x20    // MCP de salidas (LEDs)
#define mcpI_ADDR 0x21    // MCP de entradas (BOTONES)


MCP23017 mcpI = MCP23017(mcpI_ADDR, Wire1);  // usamos segundo bus I2C para no interferir con el display lcd
MCP23017 mcpO = MCP23017(mcpO_ADDR, Wire1);  // usamos segundo bus I2C para no interferir con el display lcd

void initWire() {

  Wire.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_SPEED);     // primer bus I2C para la pantalla lcd
  Wire1.begin(I2C_SDA1, I2C_SCL1, I2C_CLOCK_SPEED);  // segundo bus I2C para los MCPs
}

void mcpOinit() {

    mcpO.init();
    
	/*set i/o pin direction as OUTPUT for both ports A and B en MCP de salidas (leds))*/
	/* void portMode(port, directions, pullups, inverted); */

    mcpO.portMode(MCP23017Port::A, 0x00);  // output
    mcpO.portMode(MCP23017Port::B, 0x00);  // output

}

void mcpIinit() {

    mcpI.init();
    
	/*set i/o pin direction as input for both ports A and B en MCP de entradas (botones)*/
	/* void portMode(port, directions, pullups, inverted); */

    mcpI.portMode(MCP23017Port::A, 0b01111111, 0b01111111, 0b01111111);  // input salvo A7, pull-up, polaridad invertida
    mcpI.portMode(MCP23017Port::B, 0b01111111, 0b01111111, 0b01111111);  // input salvo AB, pull-up, polaridad invertida)

}


void apagaLeds()
{
  analogWrite(LEDR, 0);
  analogWrite(LEDG, 0);
  analogWrite(LEDB, 0);
  mcpO.writePort(MCP23017Port::A, 0x00);
  mcpO.writePort(MCP23017Port::B, 0x00);
  ledStatus = 0;
  delay(200);
}

void enciendeLeds()
{
  analogWrite(LEDR, MAXledLEVEL);
  analogWrite(LEDG, MAXledLEVEL);
  analogWrite(LEDB, MAXledLEVEL);
  mcpO.writePort(MCP23017Port::A, 0xFF);
  mcpO.writePort(MCP23017Port::B, 0xFF);
  ledStatus = 0xFFFF;
  delay(200);
}

void loadDefaultSignal(uint veces)
{
  LOG_TRACE("in loadDefaultSignal");
  uint i;
  for(i=0;i<veces;i++) {
    ledPWM(LEDR,ON);
    ledPWM(LEDG,ON);
    delay(300);
    ledPWM(LEDR,OFF);
    ledPWM(LEDG,OFF);
    delay(300);
  }
}

void wifiClearSignal(uint veces)
{
  LOG_TRACE("in wifiClearSignal");
  uint i;
  for(i=0;i<veces;i++) {
    ledPWM(LEDR,ON);
    ledPWM(LEDB,ON);
    delay(300);
    ledPWM(LEDR,OFF);
    ledPWM(LEDB,OFF);
    delay(300);
  }
}

void actLedError(void) {
  ledPWM(LEDR,ON);
  ledPWM(LEDG,OFF);
  ledPWM(LEDB,OFF);
}

void initLeds()
{
  int i;
  uint ledOrder[] = { lGRUPO1 , lGRUPO2 , lGRUPO3 ,
                      lZONA1 , lZONA2 , lZONA3 , lZONA4 , lZONA5 , lZONA6 , lZONA7 , lZONA8 };
  size_t numLeds = sizeof(ledOrder)/sizeof(ledOrder[0]);
  apagaLeds();
  delay(200);
  for(i=0;i<numLeds;i++) {
    led(ledOrder[i],ON);
    delay(300);
    led(ledOrder[i],OFF);
  }
  delay(200);
  enciendeLeds();
  delay(200);
  apagaLeds();
  delay(200);
  //ledPWM(LEDR,ON);
}


void initGPIOs()
{
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(ENCBOTON, INPUT);
}

// enciende o apaga un led controlado por PWM
void ledPWM(uint8_t id,int estado)
{
  estado ? analogWrite(id, MAXledLEVEL) : analogWrite(id, 0);
  switch (id) {
    case LEDR: sLEDR = estado;
    case LEDG: sLEDG = estado;
    case LEDB: sLEDB = estado;
  }  

}

// enciende o apaga un led controlado por el expansor MCP
void led(uint8_t id,int estado)
{
    #ifdef EXTRADEBUG
    Serial.print(F("[TRACE: en funcion led]"));
    Serial.print(F("ledStatus : "));Serial.println(ledStatus,BIN);
    Serial.print(F("ledID : "));Serial.println(id,DEC);
    #endif

    //Por seguridad no hacemos nada si id=0
    if(id==0) return;
    if(estado == ON) ledStatus |= (1 << (id-1));
    else ledStatus &= ~(1 << (id-1));
    //convertimos a la parte baja y alta
    uint8_t bajo = (uint8_t)((ledStatus & 0x00FF));
    uint8_t alto = (uint8_t)((ledStatus & 0xFF00) >> 8);
    mcpO.writePort(MCP23017Port::A, bajo);
    mcpO.writePort(MCP23017Port::B, alto);
}

bool ledStatusId(int ledID)
{
  #ifdef EXTRADEBUG
    Serial.print(F("ledStatus : "));Serial.println(ledStatus,BIN);
    Serial.print(F("ledID : "));Serial.println(ledID,DEC);
  #endif
  return((ledStatus & (1 << (ledID-1))));
}

uint16_t readInputs()
{
  uint8_t    alto;
  uint8_t    bajo;
  bajo = mcpI.readPort(MCP23017Port::A);
  alto = mcpI.readPort(MCP23017Port::B);
  return bajo | (alto << 8);
}

bool testButton(uint16_t id,bool state)
{
  //testea estado instantaneo del boton(id) pasado
  //devolviendo 1 si es igual a state y 0 en caso contrario 
  uint16_t buttons = readInputs();
  bool result = ((buttons & id) == 0)?0:1;
  if (result == state) return 1;
   else return 0;
}

S_BOTON *parseInputs(bool read)
{
  int i;
  //Para el debounce
  unsigned long currentMillis = millis();
  if(currentMillis < (lastMillis + DEBOUNCEMILLIS)) return NULL;
  else lastMillis = currentMillis;
  uint16_t inputs = readInputs();
  //analizamos estado de los botones habilitados en la estructura Boton[]
  for (i=0;i<NUM_S_BOTON;i++) {
    //Nos saltamos los disabled
    if (!Boton[i].flags.enabled) continue;
    Boton[i].estado = inputs & Boton[i].id;
    //Solo si el estado del boton ha cambiado devuelve cual ha sido 
    if ((Boton[i].estado != Boton[i].ultimo_estado) || (Boton[i].estado && Boton[i].flags.hold && !Boton[i].flags.holddisabled))
    {
      Boton[i].ultimo_estado = Boton[i].estado;
      if (Boton[i].estado || Boton[i].flags.dual) {
        #ifdef EXTRADEBUG
          bool bEstado = Boton[i].estado;
          if (!read) Serial.print(F("Cleared: "));
          Serial.printf("Boton: %s  idx: %d  id: %#X  Estado: %d \n", Boton[i].desc, Boton[i].idx, Boton[i].id, bEstado);
        #endif
        if (read) return &Boton[i]; //si no clear retorna 1er boton que ha cambiado de estado
      }
    }
  }
  return NULL;
}

int bID2bIndex(uint16_t id)
{
  for (int i=0;i<NUM_S_BOTON;i++) {
    if (Boton[i].id == id) return i;
  }
  return 999;
}

int bID2zIndex(uint16_t id)
{
  for (uint i=0;i<NUMZONAS;i++) {
    if(ZONAS[i] == id) return i;
  }
  return 999;
}

