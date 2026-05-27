#ifndef DEBUGLOG_SETUP_H
  #define DEBUGLOG_SETUP_H  
  /*
  * Uncommenting DEBUGLOG_DISABLE_LOG disables ASSERT and all log (Release Mode)
  * PRINT and PRINTLN are always valid even in Release Mode
  * para cambiarlo posteriormente: LOG_SET_LEVEL(DebugLogLevel::LVL_TRACE);
  *  0: NONE, 1: ERROR, 2: WARN, 3: INFO, 4: DEBUG, 5: TRACE
  */
  #ifdef DEVELOP
    //Comportamiento general para PRUEBAS . DESCOMENTAR LO QUE CORRESPONDA
    // #define DEBUGLOG_DEFAULT_LOG_LEVEL_TRACE
    #define DEBUGLOG_DEFAULT_LOG_LEVEL_DEBUG
    #define VERBOSE  // muestra info adicional en el arranque
    // #define EXTRADEBUG
    // #define EXTRADEBUG2
    // #define EXTRATRACE
  #endif

  #ifdef RELEASE
    //Comportamiento general para uso normal . DESCOMENTAR LO QUE CORRESPONDA
    // #define DEBUGLOG_DISABLE_LOG
    #define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
    // #define DEBUGLOG_DEFAULT_LOG_LEVEL_WARN
    // En RELEASE: [tipo] [timestamp] [función] -> mensaje
    #define LOG_PREAMBLE "[", getTimestamp(), "] [", __func__, "] ->"
    #define VERBOSE  // muestra info adicional en el arranque
  #endif

  #ifdef LOGTOFILE
    // Enable file logging errors to LittleFS
    #define DEBUGLOG_ENABLE_FILE_LOGGER
    #define DEBUGLOG_DEFAULT_FILE_LEVEL_ERROR
  #else
    #define DEBUGLOG_DEFAULT_FILE_LEVEL_NONE    
  #endif  

  #include <DebugLog.h>

#endif // DEBUGLOG_SETUP_H