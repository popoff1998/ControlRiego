#ifndef SONIDOS_H
#define SONIDOS_H

#include "pitches.h"

class Sonidos {
    
    private:
    
        const uint8_t _tft_volume[10] = { 67, 50, 40, 33, 29, 22, 15, 11, 6, 2 }; // Duty for linear volume control.
        // const uint8_t _tft_volume[10] = { 200, 100, 67, 50, 40, 33, 29, 22, 11, 2 }; // Duty for linear volume control.
        
        // definiciones melodias (notas y duraciones):
        
        const int bipOK_melody[7] = { NOTE_C6, NOTE_D6, NOTE_E6, NOTE_F6, NOTE_G6, NOTE_A6, NOTE_B6 };
        const int bipOK_duration = 75;
        
        const int bipKO_melody[3] = { NOTE_C5, NOTE_B4, NOTE_A3 };
        const int bipKO_figure[3] = { FIG_SC, FIG_CO, FIG_NG };
        
        const int bipMimi_melody[7] = { NOTE_C5, NOTE_E5, NOTE_G5, NOTE_F5, NOTE_G5, NOTE_B5, NOTE_C6 };
        const int bipMimi_figure = FIG_SC;
        
        const int Tarari_melody[8] = { NOTE_C5, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, 0, NOTE_B4, NOTE_C5 };
        const int Tarari_figure[8] = { FIG_SC, FIG_FU, FIG_FU, FIG_SC, FIG_SC, FIG_SC, FIG_SC, FIG_SC };

        const int Mario_melody[10] = { NOTE_E5, NOTE_E5, 0, NOTE_E5, 0, NOTE_C5, NOTE_E5, 0, NOTE_G5, 0};
        const int Mario_figure[10] = { FIG_SC, FIG_SC, FIG_SC, FIG_SC, FIG_SC, FIG_SC, FIG_SC, FIG_SC, FIG_NG, FIG_NG};        

        #define PIRATES_SIZE 64
        const int Pirates_melody[PIRATES_SIZE] = {
            NOTE_A4, NOTE_C5, NOTE_D5, NOTE_D5, 0,        // 1-5
            NOTE_D5, NOTE_E5, NOTE_F5, NOTE_F5, 0,        // 6-10
            NOTE_F5, NOTE_G5, NOTE_E5, NOTE_E5, 0,        // 11-15
            NOTE_D5, NOTE_C5, NOTE_C5, NOTE_D5, 0,        // 16-20
            
            NOTE_A4, NOTE_C5, NOTE_D5, NOTE_D5, 0,        // 21-25
            NOTE_D5, NOTE_E5, NOTE_F5, NOTE_F5, 0,        // 26-30
            NOTE_F5, NOTE_G5, NOTE_E5, NOTE_E5, 0,        // 31-35
            NOTE_D5, NOTE_C5, NOTE_D5, 0,                 // 36-39

            NOTE_A4, NOTE_C5, NOTE_D5, NOTE_D5,           // 40-43
            NOTE_D5, NOTE_F5, NOTE_G5, NOTE_G5,           // 44-47
            NOTE_G5, NOTE_A5, NOTE_AS5, NOTE_AS5,         // 48-51
            NOTE_A5, NOTE_G5, NOTE_A5, NOTE_D5, 0,        // 52-56

            NOTE_D5, NOTE_E5, NOTE_F5, NOTE_F5, 0,        // 57-61 (do-re-MI-MI...)
            NOTE_G5, NOTE_E5, NOTE_D5                     // 62-64 (...sol-mi-RE!)
        };
        const int Pirates_figure[PIRATES_SIZE] = {
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 1-5
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 6-10
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 11-15
            FIG_SC, FIG_SC, FIG_SC, FIG_CO, FIG_SC,       // 20
            
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 25
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 30
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 35
            FIG_SC, FIG_SC, FIG_NG, FIG_SC,               // 39

            FIG_SC, FIG_SC, FIG_CO, FIG_CO,               // 43
            FIG_SC, FIG_SC, FIG_CO, FIG_CO,               // 47
            FIG_SC, FIG_SC, FIG_CO, FIG_CO,               // 51
            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 56

            FIG_SC, FIG_SC, FIG_CO, FIG_CO, FIG_SC,       // 57-61
            FIG_CO, FIG_CO, FIG_BL                        // 62-64 (Cierre fuerte en blanca)
        };        
        
        #define STARWARS_SIZE 21
        const int StarWars_melody[STARWARS_SIZE] = {
            NOTE_D4, NOTE_D4, NOTE_D4, 
            NOTE_G4, NOTE_D5, 
            NOTE_C5, NOTE_B4, NOTE_A4, NOTE_G5, NOTE_D5,
            NOTE_C5, NOTE_B4, NOTE_A4, NOTE_G5, NOTE_D5,
            NOTE_C5, NOTE_B4, NOTE_C5, NOTE_A4, 0,
            NOTE_G4
        };
        const int StarWars_figure[STARWARS_SIZE] = {
            FIG_FU, FIG_FU, FIG_FU, 
            FIG_BL + FIG_NG, FIG_BL + FIG_NG, // G4 y D5 son notas muy largas (Blanca + Negra)
            FIG_SC, FIG_SC, FIG_SC, FIG_BL, FIG_CO,
            FIG_SC, FIG_SC, FIG_SC, FIG_BL, FIG_CO,
            FIG_SC, FIG_SC, FIG_SC, FIG_NG, FIG_CO,
            FIG_RE // Final apoteósico con Redonda
        };

        #define HARRY2_SIZE 9
        const int Harry2_melody[HARRY2_SIZE] = {
            NOTE_B4, 
            NOTE_E5, NOTE_G5, NOTE_FS5,
            NOTE_E5, NOTE_B5, NOTE_A5,      
            NOTE_FS5,                       
            NOTE_E5
        };
        const int Harry2_figure[HARRY2_SIZE] = {
            FIG_CO,                         
            FIG_NG + FIG_CO, FIG_CO, FIG_NG, 
            FIG_BL, FIG_NG,                  
            FIG_BL + FIG_NG,                 
            FIG_BL + FIG_NG,                 
            FIG_BL + FIG_NG                
         };

        #define INDY_SIZE 19
        const int Indy_melody[INDY_SIZE] = {
            // El motivo principal
            NOTE_E4, NOTE_F4, NOTE_G4, NOTE_C5, 0, // ¡Ta-ta-ta-TAAA!
            NOTE_D4, NOTE_E4, NOTE_F4, 0,          // ¡Ta-ta-ta!
            NOTE_G4, NOTE_A4, NOTE_B4, NOTE_F5, 0, // ¡Ta-ta-ta-TAAA!
            NOTE_A4, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5 // ¡Ta-ta-ta-ta-TAA!
        };
        const int Indy_figure[INDY_SIZE] = {
            FIG_CO, FIG_SC, FIG_NG, FIG_BL, FIG_SC, // El 0 es un silencio breve
            FIG_CO, FIG_SC, FIG_BL, FIG_SC,
            FIG_CO, FIG_SC, FIG_NG, FIG_BL, FIG_SC,
            FIG_CO, FIG_SC, FIG_CO, FIG_CO, FIG_BL
        };




        void mitone(int pin, unsigned long frequency, unsigned int duration, int volume);
        void playNote(int note, int duration, int fullvolume=false);

    public:
    
        Sonidos();
        void bip(int veces);
        void longbip(int veces);
        void lowbip(int veces);
        void bipOK();
        void bipKO();
        void bipTarari();
        void bipMimi(int veces);
        void bipFIN();
        void temaPiratas(bool luces=false);
        void bipMario(bool luces=false);
        void temaStarWars(bool luces=false);
        void temaHarry2(bool luces=false);
        void temaIndianaJones(bool luces=false);
};

#endif // SONIDOS_H