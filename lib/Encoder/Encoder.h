// Encoder.h
#ifndef _ENCODER_h
#define _ENCODER_h

#include "Arduino.h"

// Valores por defecto del hardware
#define ENCODER_DEFAULT_A_PIN 25
#define ENCODER_DEFAULT_B_PIN 26
#define ENCODER_DEFAULT_VCC_PIN -1
#define ENCODER_DEFAULT_STEPS 4

class Encoder
{
private:
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    // Contadores de hardware puros (Modificados únicamente en la ISR)
    volatile long rawEncoderPos = 0; 

    // Variables internas de control para el cálculo del menú e hilo principal
    long logicalEncoderPos = 0;
    long residualTicks = 0; // Acumulador local inmune a la inversión de marcha 
    int8_t lastMovementDirection = 0; 
    unsigned long lastMovementAtMicros = 0; 
    unsigned long rotaryAccelerationCoef = 150;

    bool _circleValues = false;
    bool isEnabled = true;

    uint8_t encoderAPin = ENCODER_DEFAULT_A_PIN;
    uint8_t encoderBPin = ENCODER_DEFAULT_B_PIN;
    int encoderVccPin = ENCODER_DEFAULT_VCC_PIN;
    long encoderSteps = ENCODER_DEFAULT_STEPS;

    long _minEncoderValue = -2147483648; 
    long _maxEncoderValue = 2147483647;  

    int8_t old_AB;
    long lastReadEncoderPos;

    int8_t enc_states[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    void (*ISR_callback)();

    // Método interno de procesamiento
    void syncAndProcessMovement();

public:
    Encoder(
        uint8_t encoderAPin = ENCODER_DEFAULT_A_PIN,
        uint8_t encoderBPin = ENCODER_DEFAULT_B_PIN,
        int encoderVccPin = ENCODER_DEFAULT_VCC_PIN,
        uint8_t encoderSteps = ENCODER_DEFAULT_STEPS,
        bool areEncoderPinsPulldown_forEsp32 = true);

    void setBoundaries(long minEncoderValue = -100, long maxEncoderValue = 100, bool circleValues = false);

    bool areEncoderPinsPulldownforEsp32 = true;

    void IRAM_ATTR readEncoder_ISR();

    void setup(void (*ISR_callback)(void));
    void begin();
    void enable();
    void disable();
    long readEncoder();
    void setEncoderValue(long newValue);
    long encoderChanged();
    
    unsigned long getAcceleration() { return this->rotaryAccelerationCoef; }
    void setAcceleration(unsigned long acceleration) { this->rotaryAccelerationCoef = acceleration; }
    void disableAcceleration() { setAcceleration(0); }
};

#endif