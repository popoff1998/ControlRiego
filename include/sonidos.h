#ifndef SONIDOS_H
#define SONIDOS_H

#include "pitches.h"

class Sonidos {
    
    private:
    
        struct Config_parm& config; // Referencia a la estructura config
        
        const uint8_t _tft_volume[10] = { 67, 50, 40, 33, 29, 22, 15, 11, 6, 2 }; // Duty for linear volume control.
        // const uint8_t _tft_volume[10] = { 200, 100, 67, 50, 40, 33, 29, 22, 11, 2 }; // Duty for linear volume control.
        
        // definiciones melodias (notas y duraciones):
        
        int bipOK_melody[7] = { NOTE_C6, NOTE_D6, NOTE_E6, NOTE_F6, NOTE_G6, NOTE_A6, NOTE_B6 };
        int bipOK_duration = 75;
        
        int bipKO_melody[3] = { NOTE_C5, NOTE_B4, NOTE_A3 };
        int bipKO_duration[3] = { DUR_SC, DUR_CO, DUR_NG };
        
        int bipMimi_melody[7] = { NOTE_C5, NOTE_E5, NOTE_G5, NOTE_F5, NOTE_G5, NOTE_B5, NOTE_C6 };
        int bipMimi_duration = DUR_SC;
        
        int Tarari_melody[8] = { NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, 0, NOTE_B4, NOTE_C5 };
        int Tarari_duration[8] = { DUR_SC, DUR_FU, DUR_FU, DUR_SC, DUR_SC, DUR_SC, DUR_SC, DUR_SC };
    
        void mitone(int pin, unsigned long frequency, unsigned int duration, int volume);
        void playNote(int note, int duration);

    public:
    
        // Constructor que recibe config por referencia
        Sonidos(struct Config_parm&);
        void bip(int veces);
        void longbip(int veces);
        void lowbip(int veces);
        void bipOK();
        void bipKO();
        void bipTarari();
        void bipMimi(int veces);
        void bipFIN();
};

#endif // SONIDOS_H