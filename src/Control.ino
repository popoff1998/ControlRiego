
#define __MAIN__
#include <Control.h>
#include <SPI.h>
#include <Ethernet.h>

S_BOTON *boton;
S_BOTON *ultimoBoton;

void timerIsr()
{
  Encoder->service();
}

void initEeprom() {
  int i,address;
  Serial.println(EEPROM.read(0));
  if(EEPROM.read(0) == 0 || FORCEINITEEPROM == 1) {
    Serial.println("Inicializando la EEPROM");
    //Inicializo los idx
    for(i=0,address=1;i<16;i++,address+=sizeof(Boton[i].idx)) {
      EEPROM.put(address,Boton[i].idx);
    }
  }
  else {
    Serial.println("Leyendo valores de la EEPROM");
    //Leemos los idx
    for(i=0,address=1;i<16;i++,address+=sizeof(Boton[i].idx)) {
      EEPROM.get(address,Boton[i].idx);
    }
  }
}

void setup()
{
  Serial.begin(9600);
  //Para el encoder
  Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  //Para el display
  display = new Display(DISPCLK,DISPDIO);
  display->printTime(minutes, seconds);
  //Para el Configure le paso encoder y display porque lo usara.
  configure = new Configure(Encoder,display);
  //Para el BUZZER
  pinMode(BUZZER, OUTPUT);
  //Para el CD4021B
  initCD4021B();
  //Para el 74HC595
  initHC595();
  //Para la red
  delay(1000);
  #ifdef NET_MQTTCLIENT
    MqttClient.setClient(client);
    MqttClient.setServer(MQTT_SERVER,1883);
  #endif
    Ethernet.begin(mac,ip,gateway,subnet);
    delay(1500);
  //Estado inicial
  Estado.estado = STANDBY;
  //Inicializo la EEPROM
  initEeprom();
}

bool unavez = true;

void loop()
{
  procesaBotones();
  if(unavez) {
   bip(1);
   unavez=false;
  }
  procesaEstados();
}

void procesaBotones()
{
  //Procesamos los botones
  if (!multiSemaforo) {
    //Nos tenemos que asegurar de no leer botones al menos una vez si venimos de un multiriego
    boton = NULL;
    boton = parseInputs();
  }
  else {
    multiSemaforo = false;
    //Serial.println(boton->id);
  }

  if(boton != NULL) {
    //Si estamos en reposo solo nos saca de ese estado
    if (reposo) {
      Serial.println("Salimos de reposo");
      reposo = false;
      StaticTimeUpdate();
    }
    else if (boton->flags.action) {
      //Serial.println("***Antes del switch de boton->id");
      switch (boton->id) {
        //Primero procesamos los botones singulares, el resto van por default
        case bPAUSE:
          if (Estado.estado != STOP) {
            //No procesamos los release del boton salvo en STOP
            if(!boton->estado) break;

            switch (Estado.estado) {
              case REGANDO:
                bip(1);
                Estado.estado = PAUSE;
                if(ultimoBoton) {
                  stopRiego(ultimoBoton->id);
                }
                T.PauseTimer();
                break;
              case PAUSE:
                bip(2);
                if(ultimoBoton) {
                  initRiego(ultimoBoton->id);
                }
                T.ResumeTimer();
                Estado.estado = REGANDO;
                break;
            }
          }
          else {
            //Si lo tenemos pulsado
            if(boton->estado) {
              if(!holdPause) {
                //Serial.println("EN !HOLDPAUSE");
                //activamos el contador
                countHoldPause = millis();
                holdPause = true;
              }
              else {
                //Comprobamos si hemos llegado a holdtime
                //Serial.println("EN HOLDPAUSE");
                if((millis() - countHoldPause) > HOLDTIME) {
                  configure->start();
                  Estado.estado = CONFIGURANDO;
                  holdPause = false;
                }
              }
            }
            //Si lo hemos soltado quitamos holdPause
            else {
              Serial.println("EN RELEASEPAUSE");
              holdPause = false;
            }
          }
          break;
        case bSTOP:
          if (boton->estado) {
            if (Estado.estado == REGANDO || Estado.estado == MULTIREGANDO || Estado.estado == PAUSE) {
              //printStopDisplay();
              display->print("stop");
              stopAllRiego();
              T.StopTimer();
              bip(3);
              display->blink(DEFAULTBLINK);
              Estado.estado = STOP;
            }
            else {
              //Lo hemos pulsado en standby, no paramos riegos
              bip(3);
              //printStopDisplay();
              display->print("stop");
              Estado.estado = STOP;
            }
          }

          if (!boton->estado && (Estado.estado == STOP || Estado.estado == CONFIGURANDO)
        ) {
            minutes = DEFAULTMINUTES;
            seconds = DEFAULTSECONDS;
            StaticTimeUpdate();
            //Borramos el led de config en su caso
            led(Boton[bId2bIndex(bCONFIG)].led,OFF);
            Estado.estado = STANDBY;
          }

          standbyTime = millis();
          break;
        case bMULTIRIEGO:
          if (Estado.estado == STANDBY && !multiriego) {
            bip(4);
            uint16_t multiStatus = getMultiStatus();
            multiriego = true;
            multi.actual = 0;
            switch(multiStatus) {
              case bCOMPLETO:
                multi.serie = COMPLETO;
                multi.size = sizeof(COMPLETO)/2;
                multi.id = bCOMPLETO;
                strcpy((char *)"COMPLETO",multi.desc);
                break;
              case bCESPED:
                multi.serie = CESPED;
                multi.size = sizeof(CESPED)/2;
                multi.id = bCESPED;
                strcpy((char *)"CESPED",multi.desc);
                break;
              case bGOTEOS:
                multi.serie = GOTEOS;
                multi.size = sizeof(GOTEOS)/2;
                multi.id = bGOTEOS;
                strcpy((char *)"GOTEOS",multi.desc);
                break;
            }
            //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
            Serial.print("MULTISIZE: ");Serial.println(multi.size);
            led(Boton[bId2bIndex(multi.id)].led,ON);
            boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
          }
          //Aqui no hay break para que riegue
        default:
          //Serial.println("***Estamos en default");
          if (Estado.estado == STANDBY) {
            bip(2);
            T.SetTimer(0,minutes,seconds);
            T.StartTimer();
            ultimoBoton = boton;
            initRiego(boton->id);
            //Estado.estado = REGANDO;
          }
          //Configuramos el idx
          if (Estado.estado == CONFIGURANDO) {
            configure->idx(&Boton[bId2bIndex(boton->id)]);
          }
      }
    }
  }
}

void initRiego(uint16_t id) {
  //Esta funcion mandara el mensaje a domoticz de activar el boton
  int index = bId2bIndex(id);
  Serial.print("Iniciando riego: ");
  Serial.println(Boton[index].desc);
  led(Boton[index].led,ON);
  domoticzSwitch(Boton[index].idx,(char *)"On");
}

void stopRiego(uint16_t id) {
  //Esta funcion mandara el mensaje a domoticz de desactivar el boton
  int index = bId2bIndex(id);
  //Serial.print("Parando riego: ");
  //Serial.println(Boton[bId2bIndex(id)].desc);
  if(Estado.estado != PAUSE) led(Boton[index].led,OFF);
  domoticzSwitch(Boton[index].idx,(char *)"Off");
}

void stopAllRiego() {
  //Esta funcion pondra a off todos los botones
  for(unsigned int i=0;i<sizeof(COMPLETO)/2;i++)
    stopRiego(COMPLETO[i]);
}

void procesaEstados()
{
  //Procesamos los estados
  switch (Estado.estado){
    case ERROR:
      display->print(" err");
      display->blink(DEFAULTBLINK);
      //blinkErrorDisplay(5);
      break;
    case REGANDO:
      tiempoTerminado = T.Timer();
      if (T.TimeHasChanged()) refreshDisplay();
      if (tiempoTerminado == 0) {
        //Serial.println("SE ACABO EL TIEMPO");
        Estado.estado = TERMINANDO;
      }
      break;
    case STANDBY:
      //Apagamos el display si ha pasado el lapso
      if (reposo) {
        standbyTime = millis();
      }
      else {
        if (millis() > standbyTime + (1000 * STANDBYSECS) ) {
          reposo = true;
          //clearDisplay();
          display->clearDisplay();
        }
      }
      procesaEncoder();
      break;
    case CONFIGURANDO:
      led(Boton[bId2bIndex(bCONFIG)].led,ON);
      break;
    case TERMINANDO:
      //Hacemos un blink del display 5 veces
      bip(5);
      stopRiego(ultimoBoton->id);
      display->blink(DEFAULTBLINK);
      //blinkDisplay(DEFAULTBLINK);
      StaticTimeUpdate();
      standbyTime = millis();
      reposo = false;
      if(Estado.estado != ERROR) Estado.estado = STANDBY;

      //Comprobamos si estamos en un multiriego
      if (multiriego) {
        //Serial.print("Estamos en multiriego, actual: "); Serial.println(multi.actual);
        multi.actual++;
        if (multi.actual < multi.size) {
          //Serial.print("Siguiente multiriego: ");
          boton = &Boton[bId2bIndex(multi.serie[multi.actual])];
          //Serial.println(boton->desc);
          multiSemaforo = true;
        }
        else {
          multiriego = false;
          multiSemaforo = false;
          //Apagamos el led
          Serial.println("MULTIRIEGO TERMINADO");
          //apagaLeds();
          led(Boton[bId2bIndex(multi.id)].led,OFF);
        }
      }
      break;
    case STOP:
      //Aprovechamos el estado STOP para configurar
      break;
    case PAUSE:
      blinkPause();
      break;
  }
}

void procesaEncoder()
{
  //Procesamos el encoder
  value += Encoder->getValue();
  if (value > MAXMINUTES) value = MAXMINUTES;
  if (value <  MINMINUTES) value = MINMINUTES;
  //Serial.print("VALUE: ");Serial.print(value);Serial.print("MINUTES: ");Serial.println(minutes);
  if (value != minutes) {
    reposo = false;
    minutes = value;
    StaticTimeUpdate();
  }
}

void bip(int veces)
{
  for (int i=0; i<veces;i++) {
    analogWrite(BUZZER, 255);
    delay(50);
    analogWrite(BUZZER, 0);
    delay(50);
  }
}

void longbip(int veces)
{
  for (int i=0; i<veces;i++) {
    analogWrite(BUZZER, 255);
    delay(750);
    analogWrite(BUZZER, 0);
    delay(100);
  }
}

void blinkPause()
{

  //Hacemos blink en el caso de estar en PAUSE
  if (!displayOff) {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      //clearDisplay();
      display->clearDisplay();
      displayOff = true;
      lastBlinkPause = millis();
    }
  }
  else {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      refreshDisplay();
      displayOff = false;
      lastBlinkPause = millis();
    }
  }
}


void StaticTimeUpdate(void)
{
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;
  display->printTime(minutes, seconds);
}

void refreshDisplay()
{
  display->printTime(T.ShowMinutes(),T.ShowSeconds());
}

void riegaGoteos()
{
  Serial.println("REGANDO GOTEOS");
}

void riegaCesped()
{
  Serial.println("REGANDO CESPED");
}

void riegaCompleto()
{
  Serial.println("REGANDO COMPLETO");
}

void domoticzSwitch(int idx,char *msg) {
  #ifdef NET_DIRECT
    char JSONMSG[200]="GET /json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s HTTP/1.1\r\n\r\n";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    //Serial.println(message);
    if (!client.available())
    {
      clientConnect();
    }
    Serial.println(message);
    client.println(message);
    char c = client.read();
    Serial.println(c)
    client.stop();
  #endif

  #ifdef NET_HTTPCLIENT
    char JSONMSG[200]="/json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    //Serial.println(message);
    String response;
    int statusCode = 0;
    httpclient.get(message);
    statusCode = httpclient.responseStatusCode();
    response = httpclient.responseBody();
    //Si se produce un status diferente de 200 algo ha pasado
    if(statusCode != 200){
      Serial.print("Status code: ");
      Serial.println(statusCode);
      Estado.estado = ERROR;
      httpclient.stop();
    }
    //Serial.print("Response: ");
    //Serial.println(response);
    int pos = response.indexOf("\"status\" : \"ERR\"");
    //Serial.print("POS: ");Serial.println(pos);
    if(pos != -1) {
      Serial.println("SE HA DEVUELTO ERROR");
      Estado.estado = ERROR;
    }
    httpclient.stop();
    if(Estado.estado != ERROR && strcmp(msg,"On") == 0) Estado.estado = REGANDO;
  #endif

  #ifdef NET_MQTTCLIENT
    if (!MqttClient.connected()){
      mqttReconnect();
    }
    MqttClient.publish("out","HOLA");
    //MqttClient.loop();
  #endif
}

#ifdef NET_DIRECT
void clientConnect()
{
  while(1)
  {
    delay(1000);
    Serial.println("Conectando a domoticz ...");
    if (client.connect(server,port)) {
      Serial.println("conectado");
      break;
    }
    else {
      Serial.println("Conexion fallida");
      client.stop();
    }
  }
}
#endif

#ifdef NET_MQTTCLIENT
void mqttReconnect() {
  // Loop until we're reconnected
  while (!MqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MqttClient.connect("arduinoClient")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      MqttClient.publish("outTopic","hello world");
      // ... and resubscribe
      MqttClient.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(MqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
#endif
