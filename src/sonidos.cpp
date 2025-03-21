// ---------------------------------------------------------------------------
// tonos con control de volumen variando el duty cycle de la onda cuadrada
//
// 
// Basado en TimerFreeTone, created by Tim Eckel and modified by Paul Stoffregen
// ---------------------------------------------------------------------------

#include "control.h"
#include "pitches.h"

// definiciones melodias y duraciones:

// bipOK, notes in the melody:
int bipOK_melody[] = { NOTE_C6, NOTE_D6, NOTE_E6, NOTE_F6, NOTE_G6, NOTE_A6, NOTE_B6 };
const int bipOK_num = ELEMENTCOUNT(bipOK_melody); // numero de notas en la melodia
int bipOK_duration = 75;  // duracion de cada tono en mseg.

// bipKO, notes in the melody:
int bipKO_melody[] = { NOTE_C5, NOTE_B4, NOTE_A3 };
const int bipKO_num = ELEMENTCOUNT(bipKO_melody); // numero de notas en la melodia
int bipKO_duration[] = { DUR_SC, DUR_CO, DUR_NG }; // en corcheas -> especificar tempo

// bipMimi, notes in the melody:
int bipMimi_melody[] = { NOTE_C6, NOTE_E6, NOTE_G6, NOTE_F6, NOTE_G6, NOTE_B6, NOTE_C7 };
const int bipMimi_num = ELEMENTCOUNT(bipMimi_melody); // numero de notas en la melodia
int bipMimi_duration = DUR_SC;  // en corcheas -> especificar tempo

// Tarari Melody (liberated from the toneMelody Arduino example sketch by Tom Igoe).
int Tarari_melody[] = { NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4 };
const int Tarari_num = ELEMENTCOUNT(Tarari_melody); // numero de notas en la melodia
int Tarari_duration[] = { DUR_SC, DUR_FU, DUR_FU, DUR_SC, DUR_SC, DUR_SC, DUR_SC, DUR_SC }; // en corcheas -> especificar tempo

const uint8_t _tft_volume[] = { 67, 50, 40, 33, 29, 22, 15, 11, 6, 2 }; // Duty for linear volume control.
// const uint8_t _tft_volume[] = { 200, 100, 67, 50, 40, 33, 29, 22, 11, 2 }; // Duty for linear volume control.


void mitone(int pin, unsigned long frequency, unsigned int duration, int volume) {
	if (frequency == 0 || volume == 0 || config_mute == 1) { // If frequency or volume are zero, just wait duration and exit.
		delay(duration);
		return;
	} 
	unsigned long period = 1000000 / frequency;       // Calculate the square wave length (period in microseconds).
	uint32_t duty = period / _tft_volume[volume - 1]; // Calculate the duty cycle (volume).

	uint32_t startTime = millis();           // Starting time of note.
	while(millis() - startTime < duration) { // Loop for the duration.
		digitalWrite(pin,HIGH);  // Set pin high.
		delayMicroseconds(duty); // Square wave duration (how long to leave pin high).
		digitalWrite(pin,LOW);   // Set pin low.
		delayMicroseconds(period - duty); // Square wave duration (how long to leave pin low).
	}	
}	

void playNote(int note, int duration){
  // NO -> (we only play the note for 90% of the duration, leaving 10% as a pause)
  mitone(BUZZER, note, duration, config_volume);
  //delay(30); // separacion entre notas fija
  delay(duration*0.3); // separacion entre notas 30% duracion
}

void bip(int veces)
{
  LOG_TRACE("BIP ", veces, " volume: ", config_volume);
  for (int i=0; i<veces;i++) {
    playNote(NOTE_A6, 50);  
    delay(50);
  }  
}  

void longbip(int veces)
{
  LOG_TRACE("LONGBIP ", veces, " volume: ", config_volume);
  for (int i=0; i<veces;i++) {
    playNote(NOTE_A5, 750);  
    delay(100);
  }  
}  

void lowbip(int veces)
{
  LOG_TRACE("LOWBIP ", veces, " volume: ", config_volume);
  for (int i=0; i<veces;i++) {
    playNote(NOTE_A5, 200);  
    delay(100);
  }  
}  

void bipOK() {
  for (int thisNote = 0; thisNote < bipOK_num; thisNote++) {
    playNote(bipOK_melody[thisNote], bipOK_duration);
  }
}

void bipKO() {
    int tempo = TEMPO_120; // Set the tempo.
    for (int thisNote = 0; thisNote < bipKO_num; thisNote++) {
      int noteDuration = tempo / bipKO_duration[thisNote]; // Calculate note duration.
      playNote(bipKO_melody[thisNote], noteDuration);
    }
  }
  
void bipTarari() {
  int tempo = TEMPO_80; // Set the tempo.
	for (int thisNote = 0; thisNote < Tarari_num; thisNote++) { // Loop through the notes in the array.
    int noteDuration = tempo / Tarari_duration[thisNote]; // Calculate note duration.
		playNote(Tarari_melody[thisNote], noteDuration); // Play melody[thisNote] for duration[thisNote].
  }
}

void bipMimi(int veces) {
  int tempo = TEMPO_250; // Set the tempo.
  for (int i=0; i<veces;i++) {
    for (int thisNote = 0; thisNote < bipMimi_num; thisNote++) {
        int noteDuration = tempo / bipMimi_duration; // Calculate note duration.
        playNote(bipMimi_melody[thisNote], noteDuration);
      }
  }  
}

void bipFIN() {
  LOG_TRACE("BIPFIN melody: ", config_finMelody);
  switch(config_finMelody) {
    case LONGx3: longbip(3);  break;
    case MIMI:   bipMimi(2);  break;
    case TARARI: bipTarari(); break;
  }
}

