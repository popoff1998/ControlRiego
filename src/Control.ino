
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


void setup()
{
  Serial.begin(9600);
  //Para el display
  tm1637.set(BRIGHT_TYPICAL);//BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
  tm1637.init();
  tm1637.point(POINT_ON);
  StaticTimeUpdate();
  //Para el encoder
  Encoder = new ClickEncoder(ENCCLK,ENCDT,ENCSW);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  //Para el BUZZER
  pinMode(BUZZER, OUTPUT);
  //Para el CD4021B
  initCD4021B();
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
}

void loop()
{
  procesaBotones();
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
            switch (Estado.estado) {
              case REGANDO:
                bip(1);
                T.PauseTimer();
                Estado.estado = PAUSE;
                break;
              case PAUSE:
                bip(2);
                T.ResumeTimer();
                Estado.estado = REGANDO;
                break;
            }
          }
          break;
        case bSTOP:
          if (boton->estado && (Estado.estado == REGANDO || Estado.estado == MULTIREGANDO || Estado.estado == PAUSE) ) {
            Serial.println("PARANDO EL TIEMPO");
            T.StopTimer();
            bip(3);
            blinkStopDisplay(DEFAULTBLINK);
            Estado.estado = STOP;
          }
          if (!boton->estado && Estado.estado == STOP) {
            minutes = DEFAULTMINUTES;
            seconds = DEFAULTSECONDS;
            StaticTimeUpdate();
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
                strcpy((char *)"COMPLETO",multi.desc);
                break;
              case bCESPED:
                multi.serie = CESPED;
                multi.size = sizeof(CESPED)/2;
                strcpy((char *)"CESPED",multi.desc);
                break;
              case bGOTEOS:
                multi.serie = GOTEOS;
                multi.size = sizeof(GOTEOS)/2;
                strcpy((char *)"GOTEOS",multi.desc);
                break;
            }
            //Iniciamos el primer riego del MULTIRIEGO machacando la variable boton
            Serial.print("MULTISIZE: ");Serial.println(multi.size);
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
            Estado.estado = REGANDO;
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
  domoticzSwitch(Boton[index].idx,(char *)"On");
}

void stopRiego(uint16_t id) {
  //Esta funcion mandara el mensaje a domoticz de desactivar el boton
  int index = bId2bIndex(id);
  Serial.print("Parando riego: ");
  Serial.println(Boton[bId2bIndex(id)].desc);
  domoticzSwitch(Boton[index].idx,(char *)"Off");
}

void procesaEstados()
{
  //Procesamos los estados
  switch (Estado.estado){
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
          clearDisplay();
        }
      }
      procesaEncoder();
      break;
    case TERMINANDO:
      //Hacemos un blink del display 5 veces
      bip(5);
      blinkDisplay(DEFAULTBLINK);
      StaticTimeUpdate();
      stopRiego(ultimoBoton->id);
      Estado.estado = STANDBY;

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
          Serial.println("MULTIRIEGO TERMINADO");
        }
      }
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

void blinkPause()
{

  //Hacemos blink en el caso de estar en PAUSE
  if (!displayOff) {
    if (millis() > lastBlinkPause + DEFAULTBLINKMILLIS) {
      clearDisplay();
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

void blinkDisplay(int veces)
{
  for (int i=0; i<veces; i++) {
    clearDisplay();
    delay(500);
    refreshDisplay();
    delay(500);
  }
}

void blinkStopDisplay(int veces)
{
  for (int i=0; i<veces; i++) {
    clearDisplay();
    delay(500);
    printStopDisplay();
    delay(500);
  }
}

void printStopDisplay()
{
  tm1637.point(POINT_OFF);
  tm1637.display(StopDisp);
  tm1637.point(POINT_ON);
}

void clearDisplay()
{
  tm1637.point(POINT_OFF);
  tm1637.clearDisplay();
  tm1637.point(POINT_ON);
}

void TimeUpdate(void)
{
  TimeDisp[2] = T.ShowSeconds() / 10;
  TimeDisp[3] = T.ShowSeconds() % 10;
  TimeDisp[0] = T.ShowMinutes() / 10;
  TimeDisp[1] = T.ShowMinutes() % 10;
}

void StaticTimeUpdate(void)
{
  if (minutes < MINMINUTES) minutes = MINMINUTES;
  if (minutes > MAXMINUTES) minutes = MAXMINUTES;

  TimeDisp[2] = seconds / 10;
  TimeDisp[3] = seconds % 10;
  TimeDisp[0] = minutes / 10;
  TimeDisp[1] = minutes % 10;
  tm1637.display(TimeDisp);
}

void refreshDisplay()
{
  TimeUpdate();
  tm1637.display(TimeDisp);
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
  #if defined(NET_HTTPCLIENT) || defined(NET_DIRECT)
  #endif

  #ifdef NET_DIRECT
    char JSONMSG[200]="GET /json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s HTTP/1.0\r\n\r\n";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    Serial.println(message);
    if (!client.available())
    {
      clientConnect();
    }
    Serial.println(message);
    client.println(message);
    client.stop();
  #endif

  #ifdef NET_HTTPCLIENT
    char JSONMSG[200]="/json.htm?type=command&param=switchlight&idx=%d&switchcmd=%s";
    char message[250];
    sprintf(message,JSONMSG,idx,msg);
    Serial.println(message);
    String response;
    int statusCode = 0;
    httpclient.get(message);
    statusCode = httpclient.responseStatusCode();
    response = httpclient.responseBody();
    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
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
