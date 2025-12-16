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
  };

  enum m_estados {
    STANDBY       ,
    REGANDO       ,
    CONFIGURANDO  ,
    TERMINANDO    ,
    PAUSE         ,
    STOP          ,
    ERROR         ,
  };
  
  enum _opciones_varias {
    ZONA = 1,
    GRUPO,
  };

  enum error_tipos {
    NOERROR       = 0,
    E0            = 10,
    E1            = 1,
    E2            = 2,
    E3            = 3,
    E4            = 4,
    E5            = 5,
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
    APAGA   = -2, // Detiene y apaga el LED
    PARAR   = -1, // Solo detiene el Ticker (mantiene estado actual)
    FIJO    = 0,  // Detiene y deja encendido fijo
    RAPIDO  = 2,
    NORMAL  = 4,
    LENTO   = 8,     
  };

#endif