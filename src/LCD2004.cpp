#include "Control.h"
#include "LiquidCrystal_I2C.h"
//#include <WiFi.h>
//#include <time.h>

#define B 0x20
#define FF 0xFF
#define C 0xA5

#define LCD2004_address 0x27  // direccion bus I2C

#define clock_star_line 1  // lineas 0 a 3
#define clock_star_col  0  // posicion 0 a 19
#define timer_star_line 2  // lineas 0 a 3
#define timer_star_col  7  // posicion 0 a 19

LiquidCrystal_I2C lcd(LCD2004_address, 20, 4);  // 20 caracteres x 4 lineas

int msecond, dots;

unsigned long lastSecond = millis();
unsigned long currentMillisLCD = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastUpdateData = 0;

//custom characters
byte cc1[8] = {0x07,0x0F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F};//binarycode
byte cc2[8] = {0x1F,0x1F,0x1F,0x00,0x00,0x00,0x00,0x00};
byte cc3[8] = {0x1C,0x1E,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F};
byte cc4[8] = {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x0F,0x07};
byte cc5[8] = {0x00,0x00,0x00,0x00,0x00,0x1F,0x1F,0x1F};
byte cc6[8] = {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1E,0x1C};
byte cc7[8] = {0x1F,0x1F,0x1F,0x00,0x00,0x00,0x1F,0x1F};
byte cc8[8] = {0x1F,0x00,0x00,0x00,0x00,0x1F,0x1F,0x1F};

// send custom characters to the display

void displayWiFi() {
  
  lcd.clear();
  lcd.print("Connecting to wifi");
  lcd.setCursor(0, 1);
  //lcd.print(ssid);
  delay(1000);
  lcd.clear();
  lcd.print("CONNECTED");
  delay(2000);
  }



void DefineLargeChar() {
    lcd.createChar(1, cc1);
    lcd.createChar(2, cc2);
    lcd.createChar(3, cc3);
    lcd.createChar(4, cc4);
    lcd.createChar(5, cc5);
    lcd.createChar(6, cc6);
    lcd.createChar(7, cc7);
    lcd.createChar(8, cc8);
}

// 0 1 2 3 4 5 6 7 8 9
char bn1[] = {
  1, 2, 3,  2, 3, B,  2, 7, 3,  2, 7, 3,  4, 5, FF, FF, 7, 7,  1, 7, 7,  2, 2, 6,  1, 7, 3,  1, 7, 3
};
char bn2[] = {
  4, 5, 6,  B, FF, B,  1, 8, 8,  5, 8, 6,  B, B, FF,  8, 8, 6,  4, 8, 6,  B, 1, B,  4, 8, 6,  5, 5, 6
};


void printTwoNumber(uint8_t number, uint8_t position, uint8_t line)//13
{
  // Print position is hardcoded
  int digit0; // To represent the ones
  int digit1; // To represent the tens
  digit0 = number % 10;
  digit1 = number / 10;

  // Line 1 of the two-digit number
  //lcd.setCursor(position, 0);
  lcd.setCursor(position, line);  //linea superior 1 = segunda linea
  lcd.write(bn1[digit1 * 3]);
  lcd.write(bn1[digit1 * 3 + 1]);
  lcd.write(bn1[digit1 * 3 + 2]);
  //lcd.write(B); // Blank
  lcd.write(bn1[digit0 * 3]);
  lcd.write(bn1[digit0 * 3 + 1]);
  lcd.write(bn1[digit0 * 3 + 2]);

  // Line 2 of the two-digit number
  //lcd.setCursor(position, 1);
  lcd.setCursor(position, line+1);  //linea inferior 2 = tercera linea
  lcd.write(bn2[digit1 * 3]);
  lcd.write(bn2[digit1 * 3 + 1]);
  lcd.write(bn2[digit1 * 3 + 2]);
  //lcd.write(B); // Blank
  lcd.write(bn2[digit0 * 3]);
  lcd.write(bn2[digit0 * 3 + 1]);
  lcd.write(bn2[digit0 * 3 + 2]);
}

void printColons(uint8_t position, uint8_t line)
{
  lcd.setCursor(position, line);
  lcd.write (C);
  lcd.setCursor(position, line+1);
  lcd.write (C);
}

void printNoColons(uint8_t position, uint8_t line)
{
  lcd.setCursor(position, line);
  lcd.write (B);
  lcd.setCursor(position, line+1);
  lcd.write (B);
}

void updateData() {
  lastUpdateData = millis();
}

void blinkLCD(const char *textDisplay,int veces)
{
    for (int i=0; i<veces; i++) {
      lcd.clear();
      delay(500);
      lcd.setCursor(7, 0);
      lcd.print(textDisplay);
      delay(500);
    }
}

void infoLCD(const char *textDisplay, int dnum, int btype, int bnum) {
    Serial.print(F("[infoLCD] Recibido: "));Serial.println(textDisplay);
    lcd.clear();
    lcd.setCursor(7, 0);
    lcd.print(textDisplay);
    if (btype == LONGBIP) longbip(bnum);
    if (btype == BIP) bip(bnum);
    if (btype == BIPOK) bipOK();
    if (btype == BIPKO) bipKO();
    for (int i=0; i<dnum; i++) {
      lcd.clear();
      delay(500);
      lcd.setCursor(7, 0);
      lcd.print(textDisplay);
      delay(500);
    }
}


/* 
void updateTime() {
  lastUpdateTime = millis();
  time_t now = time(nullptr);
  struct tm* newtime = localtime(&now);

  year = String(newtime->tm_year + 1900).toInt();
  month = String(newtime->tm_mon + 1).toInt();
  dayOfMonth = String(newtime->tm_mday).toInt();
  dayOfWeek = String(newtime->tm_wday).toInt();
  hour = String(newtime->tm_hour).toInt();
  minute = String(newtime->tm_min).toInt();
  second = String(newtime->tm_sec).toInt();
}
 */
void initLCD() {

  lcd.init();
  lcd.backlight();
  DefineLargeChar(); // Create the custom characters
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Ardomo Aqua");
  lcd.setCursor(0, 2);
  lcd.print("Inicializando");
  //delay(1000);
}

/* 
void displayClock(uint8_t line) {
  currentMillisLCD = millis();

  if ((currentMillisLCD - lastUpdateTime) > 300000) updateTime();
  if ((currentMillisLCD - lastUpdateData) > 600000) updateData();
  if (year < 2000) updateTime();

  dots = (currentMillisLCD % 1000) < 500;
  if (dots) printColons(clock_star_col+6, line); else printNoColons(clock_star_col+6, line);
  if (dots) printColons(clock_star_col+13, line); else printNoColons(clock_star_col+13, line);

  if ((millis() - lastSecond) > 1000) {
    lastSecond = millis();
    second++;
    if (second > 59) {
      second = 0;
      minute++;
      if (minute > 59) {
        minute = 0;
        hour++;
        if (hour > 23) hour = 0;
      }
    }
    printTwoNumber(hour, clock_star_col, line);
    printTwoNumber(minute, clock_star_col+7, line);
    printTwoNumber(second, clock_star_col+14, line);
  }
}
 */

void displayTimer(uint8_t minute, uint8_t second, uint8_t col, uint8_t line) {
  //currentMillisLCD = millis();
  //lcd.clear();
  //dots = (currentMillisLCD % 1000) < 500;
  //if (dots) printColons(col+6, line); else printNoColons(col+6, line);

  printTwoNumber(minute, col, line);
  printColons(col+6, line);
  printTwoNumber(second, col+7, line);
}

  //displayClock(timer_star_line);
  //displayTimer(timer_star_line, timer_star_col);
