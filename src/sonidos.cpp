// --------------------------------------------------------------------------------------------
// tonos con control de volumen variando el duty cycle de la onda cuadrada
//
// funcion mitone basada en TimerFreeTone, created by Tim Eckel and modified by Paul Stoffregen
// --------------------------------------------------------------------------------------------

#include "control.h"

Sonidos::Sonidos() {
    // Constructor
}

void Sonidos::bip(int veces) {
  LOG_TRACE("BIP ", veces, " volume: ", config.volume);
  for (int i = 0; i < veces; i++) {
        playNote(NOTE_A6, 50);  
        delay(50);
    }  
}  

void Sonidos::longbip(int veces) {
  LOG_TRACE("LONGBIP ", veces, " volume: ", config.volume);
  for (int i = 0; i < veces; i++) {
    playNote(NOTE_A5, 750);  
    delay(100);
  }  
}  

void Sonidos::lowbip(int veces) {
  LOG_TRACE("LOWBIP ", veces, " volume: ", config.volume);
  for (int i = 0; i < veces; i++) {
    playNote(NOTE_A5, 200);  
    delay(100);
  }  
}  

void Sonidos::bipOK() {
  for (int thisNote = 0; thisNote < ELEMENTCOUNT(bipOK_melody); thisNote++) {
    playNote(bipOK_melody[thisNote], bipOK_duration);
  }
}

void Sonidos::bipKO() {
    int tempo = TEMPO_120; // Set the tempo.
    for (int thisNote = 0; thisNote < ELEMENTCOUNT(bipKO_melody); thisNote++) {
        int noteDuration = tempo / bipKO_figure[thisNote]; // Calculate note duration.
        playNote(bipKO_melody[thisNote], noteDuration);
      }
    }
    
void Sonidos::bipTarari() {
  int tempo = TEMPO_80; // Set the tempo.
  for (int thisNote = 0; thisNote < ELEMENTCOUNT(Tarari_melody); thisNote++) { // Loop through the notes in the array.
    int noteDuration = tempo / Tarari_figure[thisNote]; // Calculate note duration.
    playNote(Tarari_melody[thisNote], noteDuration); // Play melody[thisNote] for duration[thisNote].
  }
}

void Sonidos::bipMimi(int veces) {
  int tempo = TEMPO_250; // Set the tempo.
  for (int i = 0; i < veces; i++) {
    for (int thisNote = 0; thisNote < ELEMENTCOUNT(bipMimi_melody); thisNote++) {
      int noteDuration = tempo / bipMimi_figure; // Calculate note duration.
        playNote(bipMimi_melody[thisNote], noteDuration);
      }
  }  
}

void Sonidos::bipFIN() {
  LOG_TRACE("BIPFIN melody: ", config.finMelody);
  switch(config.finMelody) {
        case LONGx3: longbip(3);  break;
        case MIMI:   bipMimi(2);  break;
        case TARARI: bipTarari(); break;
      }
}
    
void Sonidos::mitone(int pin, unsigned long frequency, unsigned int duration, int volume) {
    if (frequency == 0 || volume == 0 || config.mute == 1) { // If frequency or volume are zero, just wait duration and exit.
        delay(duration);
        return;
    } 
    unsigned long period = 1000000 / frequency;       // Calculate the square wave length (period in microseconds).
    uint32_t duty = period / _tft_volume[volume - 1]; // Calculate the duty cycle (volume).

    uint32_t startTime = millis();           // Starting time of note.
    while(millis() - startTime < duration) { // Loop for the duration.
        digitalWrite(pin, HIGH);  // Set pin high.
        delayMicroseconds(duty); // Square wave duration (how long to leave pin high).
        digitalWrite(pin, LOW);   // Set pin low.
        delayMicroseconds(period - duty); // Square wave duration (how long to leave pin low).
    }    
}    

void Sonidos::playNote(int note, int duration) {
    mitone(BUZZER, note, duration, config.volume);
    delay(duration * 0.3); // separacion entre notas 30% duracion
}
    
