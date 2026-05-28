#ifndef TIPOS_GLOBALES_H
#define TIPOS_GLOBALES_H

  //Enums

  enum sonido_bips {
    LONGBIP = 1,
    LOWBIP,
    BIP,
    BIPOK,
    BIPKO,
    BIPFIN,
  };

  enum bip_melody {
    LONGx3 = 1,
    MIMI,
    TARARI,
    MARIO,
    finMelodynum // numero de melodias definidas en el enum +1
  };

  enum m_estados {
    STANDBY       ,
    REGANDO       ,
    CONFIGURANDO  ,
    TERMINANDO    ,
    PAUSE         ,
    STOP          ,
    ERROR         ,
    NUM_ESTADOS // numero de estados definidos en el enum
  };
  
  enum error_tipos {
    NOERROR       = 0,
    E1            = 1,
    E2            = 2,
    E3            = 3,
    E4            = 4,
    E5            = 5,
    E0            = 10,
  };

  enum estado_tipos {
    LOCAL       = 1,
    REMOTO      = 2,
  };

  enum boton_flags {
    ENABLED      = 0x01,
    disabled     = 0x02,  // DISABLED en mayusculas daba error al compilar por ya definido en una libreria
    ONLYSTATUS   = 0x04,
    ACTION       = 0x08,
    DUAL         = 0x10,
    HOLD         = 0x20,
  };

  // frecuencia de parpadeo del led en decimas de segundo (para RAPIDO, NORMAL Y LENTO)  
  enum velocidad_parpadeo {
    NULO    = 0, // No se especifica parpadeo o se deja el estado actual
    PARAR   = 0, // Solo detiene el Ticker (mantiene estado actual segun le haya pillado)
    RAPIDO  = 2,
    NORMAL  = 4,
    LENTO   = 8,     
  };

  // apager y encender leds (deteniendo posible parpadeo)  
  enum estado_led {   
    APAGA    = 0,  // Detiene y apaga el LED
    ENCIENDE = 1,  // Detiene y deja encendido fijo
  };

  enum display_modo {
    FULL = 1,  // muestra todas las zonas del grupo (multi.w_zserie)
    RESTO,     // muestra las zonas restantes por regar (a partir de multi.actual+1)
    WORKING,   // muestra las zonas añadidas mientras se configura un grupo (a partir de multi.w_size)
  };

#endif