
#define __MAIN__
#include <Control.h>

S_BOTON *boton;

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
  //Inicializar los botones
  //for (int i=0;i<NUMBOTONES;i++) {
  //  pinMode(Boton[i].pin,INPUT_PULLUP);
  //}
  //Para el BUZZER
  pinMode(BUZZER, OUTPUT);
  //Para el CD4021B
  initCD4021B();
  //Estado inicial
  Estado.estado = STANDBY;
}

void loop()
{
  //Leo los botones
  boton = NULL;
  boton = parseInputs();

  //Procesamos los botones
  if(boton != NULL) {
    //Si estamos en reposo solo nos saca de ese estado
    if (reposo) {
      reposo = false;
      StaticTimeUpdate();
    }
    else {
      switch (boton->id) {
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
          if (boton->estado && Estado.estado == REGANDO) {
            Serial.println("PARANDO EL TIEMPO");
            T.StopTimer();
            bip(3);
            blinkStopDisplay(DEFAULTBLINK);
            Estado.estado = STOP;
          }
          if (!boton->estado && Estado.estado == STOP) {
            minutes = DEFAULTMINUTES;
            StaticTimeUpdate();
            Estado.estado = STANDBY;
          }
          standbyTime = millis();
          break;
        case bTURBINAS:
          if (Estado.estado == STANDBY) {
            bip(2);
            T.SetTimer(0,minutes,0);
            T.StartTimer();
            Estado.estado = REGANDO;
          }
          break;
      }
    }
  }

  switch (Estado.estado){
    case REGANDO:
      tiempoTerminado = T.Timer();
      if (T.TimeHasChanged()) refreshDisplay();
      if (tiempoTerminado == 0)
      {
        Serial.println("SE ACABO EL TIEMPO");
        Estado.estado = TERMINANDO;
      }
      break;
    case STANDBY:
      //Apagamos el display si ha pasado el lapso
      if (reposo) {
        standbyTime = millis();
      }
      else
      {
        if (millis() > standbyTime + (1000 * STANDBYSECS)) {
          reposo = true;
          clearDisplay();
        }
      }
      //Procesamos el encoder
      value += Encoder->getValue();
      if (value > 99) value = MAXMINUTES;
      if (value <  0) value = MINMINUTES;
      //Serial.print("VALUE: ");Serial.print(value);Serial.print("MINUTES: ");Serial.println(minutes);
      if (value != minutes)
      {
        reposo = false;
        minutes = value;
        StaticTimeUpdate();
      }
      break;
    case TERMINANDO:
      //Hacemos un blink del display 5 veces
      bip(10);
      blinkDisplay(DEFAULTBLINK);
      StaticTimeUpdate();
      Estado.estado = STANDBY;
      break;
    case PAUSE:
      blinkPause();
      break;
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

void botonInterrupt()
{
    cli();
    //Si estamos en reposo solo salimos de ese estado
    if (reposo) {
      reposo = false;
      StaticTimeUpdate();
      return;
    }
    //Si no ponemos el timer y comenzamos a regar
    T.SetTimer(0,minutes,0);
    T.StartTimer();
    Estado.estado = REGANDO;
    sei();
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

  TimeDisp[2] = 0;
  TimeDisp[3] = 0;
  TimeDisp[0] = minutes / 10;
  TimeDisp[1] = minutes % 10;
  tm1637.display(TimeDisp);
}

void refreshDisplay()
{
  TimeUpdate();
  tm1637.display(TimeDisp);
}
