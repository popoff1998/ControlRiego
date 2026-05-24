// Encoder.cpp
// Rediseñada a partir de https://github.com/igorantolic/ai-esp32-rotary-encoder

#include "Encoder.h"

// --------------------------------------------------------------------------
// 1. LA INTERRUPCIÓN (ISR)
// --------------------------------------------------------------------------
void IRAM_ATTR Encoder::readEncoder_ISR()
{
    if (!this->isEnabled) return;

    portENTER_CRITICAL_ISR(&(this->mux));
    
    this->old_AB = (this->old_AB << 2) & 0x0C; // Desplazamos el estado anterior a la izquierda y limpiamos el resto de bits 
    int8_t ENC_PORT = ((digitalRead(this->encoderBPin)) ? (1 << 1) : 0) | ((digitalRead(this->encoderAPin)) ? (1 << 0) : 0);
    this->old_AB |= (ENC_PORT & 0x03); // Combinamos el nuevo estado con el anterior para obtener un valor entre 0 y 15 que representa la transición 
    // Suma la transición calculada según la tabla de estados
    this->rawEncoderPos += (this->enc_states[(this->old_AB & 0x0f)]);
    
    portEXIT_CRITICAL_ISR(&(this->mux));
}

// --------------------------------------------------------------------------
// 2. LÓGICA INTERNA DE PROCESAMIENTO
// --------------------------------------------------------------------------
void Encoder::syncAndProcessMovement()
{
    // Si el encoder está deshabilitado, cortamos la ejecución inmediatamente.
    if (!this->isEnabled) return;

    long rawCopiados = 0;

    // 1. Volcado atómico inmediato del HW y puesta a cero
    portENTER_CRITICAL(&(this->mux));
    rawCopiados = this->rawEncoderPos;
    this->rawEncoderPos = 0; 
    portEXIT_CRITICAL(&(this->mux));

    // Si no hay movimiento de hardware salimos, nada que procesar
    if (rawCopiados == 0) return;

    // 2. FILTRO DE INVERSIÓN: Destruye los ticks residuales al cambiar de sentido
    if (rawCopiados > 0 && this->residualTicks < 0) this->residualTicks = 0;
    if (rawCopiados < 0 && this->residualTicks > 0) this->residualTicks = 0;

    // Acumulamos las transiciones físicas en el residuo local y
    // calculamos los pasos completos disponibles (leidos+residuo),pueden ser positivos o negativos.
    this->residualTicks += rawCopiados;
    long pasosCompletosEfectuados = this->residualTicks / this->encoderSteps;

    // ----------------------------------------------------------------------
    // DIAGNÓSTICO FILTRADO: Solo imprime si hay actividad real
    // ----------------------------------------------------------------------
    #ifdef DEBUGENCODER
    if (rawCopiados != 0 || pasosCompletosEfectuados != 0) {
        Serial.print("[ENC] Ticks_Nativos: ");
        Serial.print(rawCopiados);
        Serial.print(" | Residuo: ");
        Serial.print(this->residualTicks);
        Serial.print(" | Pasos_Calc: ");
        Serial.print(pasosCompletosEfectuados);
        Serial.print(" | Pos_Logica previa: ");
        Serial.println(this->logicalEncoderPos);
    }
    #endif    

    // Si no hay pasos enteros que aplicar a la lógica, salimos
    if (pasosCompletosEfectuados == 0) return; 
    // Descontamos del residuo solo los pasos que vamos a procesar
    this->residualTicks -= (pasosCompletosEfectuados * this->encoderSteps);

    // 3. PROCESAMIENTO DE INCREMENTO Y ACELERACIÓN
    int8_t direccionActual = (pasosCompletosEfectuados > 0) ? 1 : -1;
    unsigned long ahoraMicros = micros();
    long incrementoAcelerado = pasosCompletosEfectuados;

    if (this->rotaryAccelerationCoef > 1 && direccionActual == this->lastMovementDirection) {
        unsigned long tiempoTranscurridoMicros = ahoraMicros - this->lastMovementAtMicros;
        
        if (tiempoTranscurridoMicros < 150000 && tiempoTranscurridoMicros > 0) {
            long bonoAceleracion = (this->rotaryAccelerationCoef * 1000) / tiempoTranscurridoMicros;
            if (bonoAceleracion > 100) bonoAceleracion = 100; // Cap de seguridad
            if (bonoAceleracion > 0) {
                incrementoAcelerado += (direccionActual * bonoAceleracion);
            }
        }
    }

    this->lastMovementAtMicros = ahoraMicros;
    this->lastMovementDirection = direccionActual;

    // Aplicamos el movimiento final a la posición lógica
    this->logicalEncoderPos += incrementoAcelerado;

    // 4. CONTROL DE LÍMITES Y CIRCULARIDAD
    if (this->logicalEncoderPos > this->_maxEncoderValue) {
        if (this->_circleValues) {
            long exceso = this->logicalEncoderPos - this->_maxEncoderValue - 1;
            this->logicalEncoderPos = this->_minEncoderValue + exceso;
        } else {
            this->logicalEncoderPos = this->_maxEncoderValue;
        }
    }
    else if (this->logicalEncoderPos < this->_minEncoderValue) {
        if (this->_circleValues) {
            long defecto = this->_minEncoderValue - this->logicalEncoderPos - 1;
            this->logicalEncoderPos = this->_maxEncoderValue - defecto;
        } else {
            this->logicalEncoderPos = this->_minEncoderValue;
        }
    }
}

// --------------------------------------------------------------------------
// 3. API PÚBLICA
// --------------------------------------------------------------------------
Encoder::Encoder(uint8_t encoder_APin, uint8_t encoder_BPin, int encoder_VccPin, uint8_t encoderSteps, bool areEncoderPinsPulldown_forEsp32)
{
    this->old_AB = 0;
    this->encoderAPin = encoder_APin;
    this->encoderBPin = encoder_BPin;
    this->encoderVccPin = encoder_VccPin;
    this->encoderSteps = encoderSteps;
    this->areEncoderPinsPulldownforEsp32 = areEncoderPinsPulldown_forEsp32;

    // Configurar pines de entrada con la resistencia correspondiente (default PULLDOWN para ESP32)
    pinMode(this->encoderAPin, (areEncoderPinsPulldown_forEsp32 ? INPUT_PULLDOWN : INPUT_PULLUP));
    pinMode(this->encoderBPin, (areEncoderPinsPulldown_forEsp32 ? INPUT_PULLDOWN : INPUT_PULLUP));
}

void Encoder::setBoundaries(long minEncoderValue, long maxEncoderValue, bool circleValues)
{
    this->_minEncoderValue = minEncoderValue;
    this->_maxEncoderValue = maxEncoderValue;
    this->_circleValues = circleValues;

    if (this->logicalEncoderPos > this->_maxEncoderValue) this->logicalEncoderPos = this->_maxEncoderValue;
    if (this->logicalEncoderPos < this->_minEncoderValue) this->logicalEncoderPos = this->_minEncoderValue;
}

long Encoder::readEncoder()
{
    syncAndProcessMovement();
    return this->logicalEncoderPos;
}

void Encoder::setEncoderValue(long newValue)
{
    portENTER_CRITICAL(&(this->mux));
    this->rawEncoderPos = 0; 
    portEXIT_CRITICAL(&(this->mux));

    this->residualTicks = 0; // Limpieza del acumulador local
    this->logicalEncoderPos = newValue;
    
    if (this->logicalEncoderPos > this->_maxEncoderValue) this->logicalEncoderPos = this->_maxEncoderValue;
    if (this->logicalEncoderPos < this->_minEncoderValue) this->logicalEncoderPos = this->_minEncoderValue;
    
    this->lastReadEncoderPos = this->logicalEncoderPos;
}

long Encoder::encoderChanged()
{
    long posicionActual = readEncoder();
    long diferencialPosicion = posicionActual - this->lastReadEncoderPos;
    this->lastReadEncoderPos = posicionActual;
    return diferencialPosicion;
}

void Encoder::setup(void (*ISR_callback)(void))
{
    // Vincular interrupciones a los cambios de estado de los pines
    attachInterrupt(digitalPinToInterrupt(this->encoderAPin), ISR_callback, CHANGE);
    attachInterrupt(digitalPinToInterrupt(this->encoderBPin), ISR_callback, CHANGE);
}

void Encoder::begin()
{
    this->lastReadEncoderPos = 0;
    
    // Configurar y alimentar el pin VCC si está definido (default -1 para no usarlo -> alimentacion directa del pin 5V o 3.3V)
    if (this->encoderVccPin >= 0) {
        pinMode(this->encoderVccPin, OUTPUT);
        digitalWrite(this->encoderVccPin, 1);
        delay(2); // Esperar estabilización de tensión
    }

    // Inicializar el estado anterior duplicando el estado físico inicial
    // Esto previene disparos espurios en el arranque de la máquina o que ignore el primer movimiento real
    int8_t estadoInicial = ((digitalRead(this->encoderBPin)) ? (1 << 1) : 0) | 
                           ((digitalRead(this->encoderAPin)) ? (1 << 0) : 0);
    this->old_AB = (estadoInicial << 2) | estadoInicial;
}

void Encoder::enable()  { this->isEnabled = true; }
void Encoder::disable() { this->isEnabled = false; }