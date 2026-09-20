// DMPlatform.hpp

#pragma once

#if defined(ARDUINO_ARCH_MBED)
  //OPTA
  //#include "opta/DMPlatformOpta.hpp"

#elif defined(ARDUINO_ARCH_ESP32)
  
    // --- POWER SUPERVISOR ---
    //Per OPTA sono gia definiti
    //Questi sono di esempio
    #define I1 32
    #define I2 33
    #define I3 25
    #define I4 26

#else

    #error "Unsupported DomoManager platform!"

#endif