#pragma once

// ============================================================================
// BOARD DEFINITIONS
// ============================================================================

#ifndef DM_BOARD_OPTA
    #define DM_BOARD_OPTA 1
#endif

#ifndef DM_BOARD_ESP32_S3_ETH
    #define DM_BOARD_ESP32_S3_ETH 2
#endif

#ifndef DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO
    #define DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO 3
#endif

#ifndef DM_BOARD

    #if defined(ARDUINO_ARCH_MBED)

        #define DM_BOARD DM_BOARD_OPTA

    #elif defined(ARDUINO_ARCH_ESP32)

        #define DM_BOARD DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO  //Oppure DM_BOARD_ESP32_S3_ETH se si usa la scheda Waveshare semplice

    #endif

#endif

#ifndef DM_BOARD
    #error "DM_BOARD not defined"
#endif


// ============================================================================
// INCLUDES
// ============================================================================

#include <Arduino.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(ARDUINO_ARCH_MBED)

    #include <mbed_rtc_time.h>

#elif defined(ARDUINO_ARCH_ESP32)

    #include <sys/time.h>
    #include <Adafruit_NeoPixel.h>

    #if DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO
        #include <Wire.h>
    #endif

#else

    #error "Unsupported DomoManager platform"

#endif


namespace DMPlatform
{

    // ============================================================================
    // PORTABLE SERIAL PRINTF
    //
    // Serial.printf() è disponibile su ESP32, ma non su tutte le architetture
    // Arduino. In particolare, arduino::UART del core mbed Opta espone print()
    // e println(), ma non printf().
    //
    // La funzione formatta il testo in un buffer locale e lo invia tramite
    // Serial.print(), disponibile sia su ESP32 sia su Opta.
    // ============================================================================

    static inline void SerialPrintf(
        const char* format,
        ...)
    {
        if (
            !format ||
            format[0] == '\0'
        )
        {
            return;
        }

        /*
        * Buffer sufficiente per le righe diagnostiche correnti.
        *
        * Se in futuro vengono aggiunti log più lunghi, aumentare
        * questa dimensione oppure usare direttamente Logger::logf().
        */
        char buffer[384];

        va_list arguments;

        va_start(
            arguments,
            format
        );

        const int result =
            vsnprintf(
                buffer,
                sizeof(buffer),
                format,
                arguments
            );

        va_end(
            arguments
        );

        if (result <= 0)
        {
            return;
        }

        /*
        * vsnprintf() garantisce il terminatore quando la dimensione
        * del buffer è maggiore di zero.
        *
        * Se il testo supera il buffer viene stampata la parte
        * disponibile, senza uscire dai limiti.
        */
        Serial.print(
            buffer
        );
    }

    // ========================================================================
    // COMMON
    // ========================================================================

    static constexpr int PIN_NOT_CONNECTED = -1;

    inline bool IsPinAvailable(int pin)
    {
        return pin != PIN_NOT_CONNECTED;
    }


    // ========================================================================
    // OP T A
    // ========================================================================

    #if defined(ARDUINO_ARCH_MBED)

        static constexpr int DO1 = D0;
        static constexpr int DO2 = D1;

        static constexpr int DO3 = PIN_NOT_CONNECTED;
        static constexpr int DO4 = PIN_NOT_CONNECTED;
        static constexpr int DO5 = PIN_NOT_CONNECTED;
        static constexpr int DO6 = PIN_NOT_CONNECTED;
        static constexpr int DO7 = PIN_NOT_CONNECTED;
        static constexpr int DO8 = PIN_NOT_CONNECTED;


    // ========================================================================
    // ESP32
    // ========================================================================

    #elif defined(ARDUINO_ARCH_ESP32)

        // --------------------------------------------------------------------
        // Virtual LED identifiers.
        //
        // Non sono GPIO fisici.
        // Permettono di mantenere la stessa interfaccia LedD0...LedD3
        // utilizzata dal codice di DomoManager su Opta.
        // --------------------------------------------------------------------

        static constexpr int ESP32_LOGICAL_LED_D0 = 2001;
        static constexpr int ESP32_LOGICAL_LED_D1 = 2002;
        static constexpr int ESP32_LOGICAL_LED_D2 = 2003;
        static constexpr int ESP32_LOGICAL_LED_D3 = 2004;


        inline bool IsVirtualLedPin(int pin)
        {
            return pin >= ESP32_LOGICAL_LED_D0 &&
                   pin <= ESP32_LOGICAL_LED_D3;
        }


        // --------------------------------------------------------------------
        // Board selection
        // --------------------------------------------------------------------

        #if DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO

            // ---------------------------------------------------------------
            // Waveshare ESP32-S3-POE-ETH-8DI-8DO
            // ---------------------------------------------------------------

            static constexpr int ESP32_LED_D0      = 38;
            static constexpr int ESP32_BOOT_BUTTON = 0;
            static constexpr int ESP32_BUZZER      = 46;

            static constexpr int ESP32_I2C_SDA = 42;
            static constexpr int ESP32_I2C_SCL = 41;

            // Digital inputs: GPIO4-GPIO11.

            static constexpr int ESP32_DI1 = 4;
            static constexpr int ESP32_DI2 = 5;
            static constexpr int ESP32_DI3 = 6;
            static constexpr int ESP32_DI4 = 7;
            static constexpr int ESP32_DI5 = 8;
            static constexpr int ESP32_DI6 = 9;
            static constexpr int ESP32_DI7 = 10;
            static constexpr int ESP32_DI8 = 11;

            // Logical outputs managed through TCA9554.

            static constexpr int DO1 = 1001;
            static constexpr int DO2 = 1002;
            static constexpr int DO3 = 1003;
            static constexpr int DO4 = 1004;
            static constexpr int DO5 = 1005;
            static constexpr int DO6 = 1006;
            static constexpr int DO7 = 1007;
            static constexpr int DO8 = 1008;


        #elif DM_BOARD == DM_BOARD_ESP32_S3_ETH

            // ---------------------------------------------------------------
            // Generic ESP32-S3-ETH
            // ---------------------------------------------------------------

            static constexpr int ESP32_LED_D0 = 21;

            static constexpr int DO1 = PIN_NOT_CONNECTED;
            static constexpr int DO2 = PIN_NOT_CONNECTED;
            static constexpr int DO3 = PIN_NOT_CONNECTED;
            static constexpr int DO4 = PIN_NOT_CONNECTED;
            static constexpr int DO5 = PIN_NOT_CONNECTED;
            static constexpr int DO6 = PIN_NOT_CONNECTED;
            static constexpr int DO7 = PIN_NOT_CONNECTED;
            static constexpr int DO8 = PIN_NOT_CONNECTED;

        #else

            #error "Unsupported DM_BOARD for ESP32"

        #endif

    #endif


    // ========================================================================
    // NETWORK RESOURCES
    // ========================================================================

    #if defined(ARDUINO_ARCH_MBED)

        static constexpr uint8_t MAX_NETWORK_SOCKETS = 4;

    #elif defined(ARDUINO_ARCH_ESP32)

        static constexpr uint8_t MAX_NETWORK_SOCKETS = 6;

    #endif


    // ========================================================================
    // ESP32 RGB LED
    // ========================================================================

    #if defined(ARDUINO_ARCH_ESP32)

        inline Adafruit_NeoPixel& StatusLed()
        {
            static Adafruit_NeoPixel led(
                1,
                ESP32_LED_D0,
                NEO_GRB + NEO_KHZ800
            );

            return led;
        }


        // --------------------------------------------------------------------
        // Virtual LED states.
        //
        // Bit 0: LedD0 - green
        // Bit 1: LedD1 - yellow
        // Bit 2: LedD2 - blue
        // Bit 3: LedD3 - red
        // --------------------------------------------------------------------

        inline uint8_t& VirtualLedState()
        {
            static uint8_t state = 0;
            return state;
        }


        // --------------------------------------------------------------------
        // Refresh the physical RGB LED from the four logical LED states.
        //
        // R = LedD1 OR LedD3
        // G = LedD0 OR LedD1
        // B = LedD2
        // --------------------------------------------------------------------

        inline void UpdateVirtualLeds()
        {
            const uint8_t state = VirtualLedState();

            const bool d0 = (state & 0x01) != 0;
            const bool d1 = (state & 0x02) != 0;
            const bool d2 = (state & 0x04) != 0;
            const bool d3 = (state & 0x08) != 0;

            const uint8_t red =
                (d1 || d3) ? 255 : 0;

            const uint8_t green =
                (d0 || d1) ? 255 : 0;

            const uint8_t blue =
                d2 ? 255 : 0;

            auto& led = StatusLed();

            led.setPixelColor(
                0,
                led.Color(red, green, blue)
            );

            led.show();
        }


        // --------------------------------------------------------------------
        // Set one logical LED on or off.
        // --------------------------------------------------------------------

        inline void WriteVirtualLed(
            int pin,
            bool state)
        {
            if (!IsVirtualLedPin(pin))
                return;

            const uint8_t index =
                static_cast<uint8_t>(
                    pin - ESP32_LOGICAL_LED_D0
                );

            const uint8_t mask =
                static_cast<uint8_t>(1U << index);

            uint8_t& current = VirtualLedState();

            if (state)
                current |= mask;
            else
                current &= static_cast<uint8_t>(~mask);

            UpdateVirtualLeds();
        }

    #endif


    // ========================================================================
    // TCA9554 - DIGITAL OUTPUT EXPANDER
    // ========================================================================

    #if defined(ARDUINO_ARCH_ESP32) && \
        (DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO)

        static constexpr uint8_t TCA9554_ADDRESS       = 0x20;
        static constexpr uint8_t TCA9554_REG_INPUT     = 0x00;
        static constexpr uint8_t TCA9554_REG_OUTPUT    = 0x01;
        static constexpr uint8_t TCA9554_REG_POLARITY  = 0x02;
        static constexpr uint8_t TCA9554_REG_CONFIG    = 0x03;


        inline bool& I2CBusStarted()
        {
            static bool started = false;
            return started;
        }

        inline bool& OutputExpanderReady()
        {
            static bool ready = false;
            return ready;
        }

        inline uint8_t& OutputExpanderState()
        {
            static uint8_t state = 0;
            return state;
        }


        inline bool EnsureOutputI2C()
        {
            if (I2CBusStarted())
                return true;

            if (!Wire.begin(
                    ESP32_I2C_SDA,
                    ESP32_I2C_SCL
                ))
            {
                return false;
            }

            Wire.setClock(100000);

            I2CBusStarted() = true;

            return true;
        }


        inline bool WriteTCA9554Register(
            uint8_t reg,
            uint8_t value)
        {
            Wire.beginTransmission(TCA9554_ADDRESS);

            Wire.write(reg);
            Wire.write(value);

            return Wire.endTransmission() == 0;
        }


        inline bool InitializeOutputExpander()
        {
            if (OutputExpanderReady())
                return true;

            if (!EnsureOutputI2C())
                return false;

            if (!WriteTCA9554Register(
                    TCA9554_REG_OUTPUT,
                    0x00
                ))
            {
                return false;
            }

            if (!WriteTCA9554Register(
                    TCA9554_REG_POLARITY,
                    0x00
                ))
            {
                return false;
            }

            if (!WriteTCA9554Register(
                    TCA9554_REG_CONFIG,
                    0x00
                ))
            {
                return false;
            }

            OutputExpanderState() = 0;
            OutputExpanderReady() = true;

            return true;
        }


        inline bool IsExpanderOutputPin(int pin)
        {
            return pin >= DO1 && pin <= DO8;
        }


        inline bool WriteExpanderOutput(
            int pin,
            bool state)
        {
            if (!IsExpanderOutputPin(pin))
                return false;

            if (!InitializeOutputExpander())
                return false;

            const uint8_t channel =
                static_cast<uint8_t>(pin - DO1);

            const uint8_t mask =
                static_cast<uint8_t>(1U << channel);

            uint8_t nextState = OutputExpanderState();

            if (state)
                nextState |= mask;
            else
                nextState &= static_cast<uint8_t>(~mask);

            if (!WriteTCA9554Register(
                    TCA9554_REG_OUTPUT,
                    nextState
                ))
            {
                return false;
            }

            OutputExpanderState() = nextState;

            return true;
        }

    #endif


    // ========================================================================
    // INPUT / OUTPUT
    // ========================================================================

    inline void SetupInputPin(int pin)
    {
        if (!IsPinAvailable(pin))
            return;

    #if defined(ARDUINO_ARCH_ESP32) && \
        (DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO)

        // ------------------------------------------------------------
        // Pulsante BOOT interno: attivo LOW.
        // ------------------------------------------------------------

        if (pin == ESP32_BOOT_BUTTON)
        {
            pinMode(pin, INPUT_PULLUP);
            return;
        }

        // ------------------------------------------------------------
        // Waveshare DI1-DI8: GPIO4-GPIO11.
        //
        // Abilita il pull-up interno per stabilizzare il livello
        // del GPIO lato microcontrollore.
        //
        // L'eventuale inversione logica rimane nella configurazione
        // del buffer, non in DMPlatform.
        // ------------------------------------------------------------

        if (pin >= ESP32_DI1 && pin <= ESP32_DI8)
        {
            pinMode(pin, INPUT_PULLUP);
            return;
        }

    #endif

        // Mantiene la configurazione preesistente per Opta
        // e per le altre schede.
        pinMode(pin, INPUT);
    }


    inline void SetupOutputPin(int pin)
    {
        if (!IsPinAvailable(pin))
            return;

        #if defined(ARDUINO_ARCH_ESP32)

            // Logical LEDs need no pinMode().
            if (IsVirtualLedPin(pin))
                return;

        #endif

        #if defined(ARDUINO_ARCH_ESP32) && \
            (DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO)

            if (IsExpanderOutputPin(pin))
            {
                (void)InitializeOutputExpander();
                return;
            }

        #endif

        #if defined(ARDUINO_ARCH_ESP32)

            if (pin == ESP32_LED_D0)
                return;

        #endif

        pinMode(pin, OUTPUT);
    }


    inline void WriteOutputPin(
        int pin,
        bool state)
    {
        if (!IsPinAvailable(pin))
            return;

        #if defined(ARDUINO_ARCH_ESP32)

            // ------------------------------------------------------------
            // Four logical LEDs mapped onto the physical RGB LED.
            // ------------------------------------------------------------

            if (IsVirtualLedPin(pin))
            {
                WriteVirtualLed(pin, state);
                return;
            }

        #endif

        #if defined(ARDUINO_ARCH_ESP32) && \
            (DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO)

            // ------------------------------------------------------------
            // Digital outputs EXIO1-EXIO8.
            // ------------------------------------------------------------

            if (IsExpanderOutputPin(pin))
            {
                (void)WriteExpanderOutput(pin, state);
                return;
            }

        #endif

        #if defined(ARDUINO_ARCH_ESP32)

            // Legacy direct access to the physical RGB LED GPIO.
            if (pin == ESP32_LED_D0)
            {
                auto& led = StatusLed();

                if (state)
                {
                    led.setPixelColor(
                        0,
                        led.Color(0, 255, 0)
                    );
                }
                else
                {
                    led.clear();
                }

                led.show();
                return;
            }

        #endif

        digitalWrite(
            pin,
            state ? HIGH : LOW
        );
    }


    inline bool ReadInputPin(int pin)
    {
        if (!IsPinAvailable(pin))
            return false;

        return digitalRead(pin) == HIGH;
    }


    // ========================================================================
    // SYSTEM
    // ========================================================================

    inline void Restart()
    {
        #if defined(ARDUINO_ARCH_MBED)

            NVIC_SystemReset();

        #elif defined(ARDUINO_ARCH_ESP32)

            ESP.restart();

        #endif
    }


    inline bool SetSystemTime(time_t epoch)
    {
        if (epoch <= 0)
            return false;

        #if defined(ARDUINO_ARCH_MBED)

            set_time(epoch);
            return true;

        #elif defined(ARDUINO_ARCH_ESP32)

            struct timeval tv;

            tv.tv_sec  = epoch;
            tv.tv_usec = 0;

            return settimeofday(&tv, nullptr) == 0;

        #else

            (void)epoch;
            return false;

        #endif
    }


    // ========================================================================
    // ARDUINO OPTA
    // ========================================================================

    #if defined(ARDUINO_ARCH_MBED)

        static constexpr int UserButton = BTN_USER;

        static constexpr int LedUser = LED_USER;
        static constexpr int LedD0   = LED_D0;
        static constexpr int LedD1   = LED_D1;
        static constexpr int LedD2   = LED_D2;
        static constexpr int LedD3   = LED_D3;

        static constexpr int SIREN = D0;
        static constexpr int ALARM = D1;

        static constexpr int I1 = ::I1;
        static constexpr int I2 = ::I2;
        static constexpr int I3 = ::I3;
        static constexpr int I4 = ::I4;

        static constexpr int I5 = PIN_NOT_CONNECTED;
        static constexpr int I6 = PIN_NOT_CONNECTED;
        static constexpr int I7 = PIN_NOT_CONNECTED;
        static constexpr int I8 = PIN_NOT_CONNECTED;

        static constexpr int BUZZER = PIN_NOT_CONNECTED;


        inline void SetupPlatformIO()
        {
            SetupInputPin(I1);
            SetupInputPin(I2);
            SetupInputPin(I3);
            SetupInputPin(I4);

            SetupOutputPin(SIREN);
            SetupOutputPin(ALARM);

            WriteOutputPin(SIREN, false);
            WriteOutputPin(ALARM, false);
        }


    // ========================================================================
    // ESP32
    // ========================================================================

    #elif defined(ARDUINO_ARCH_ESP32)

        static constexpr int LedUser = PIN_NOT_CONNECTED;

        // Identical logical interface on both ESP32 board selections.
        static constexpr int LedD0 = ESP32_LOGICAL_LED_D0;
        static constexpr int LedD1 = ESP32_LOGICAL_LED_D1;
        static constexpr int LedD2 = ESP32_LOGICAL_LED_D2;
        static constexpr int LedD3 = ESP32_LOGICAL_LED_D3;


        #if DM_BOARD == DM_BOARD_ESP32_S3_POE_ETH_8DI_8RO

            // ---------------------------------------------------------------
            // Waveshare ESP32-S3-POE-ETH-8DI-8DO
            // ---------------------------------------------------------------

            static constexpr int UserButton = ESP32_BOOT_BUTTON;

            static constexpr int I1 = ESP32_DI1;
            static constexpr int I2 = ESP32_DI2;
            static constexpr int I3 = ESP32_DI3;
            static constexpr int I4 = ESP32_DI4;
            static constexpr int I5 = ESP32_DI5;
            static constexpr int I6 = ESP32_DI6;
            static constexpr int I7 = ESP32_DI7;
            static constexpr int I8 = ESP32_DI8;

            static constexpr int SIREN = DO1;
            static constexpr int ALARM = DO2;

            static constexpr int BUZZER = ESP32_BUZZER;


            inline void SetupPlatformIO()
            {
                // Digital inputs.
                SetupInputPin(I1);
                SetupInputPin(I2);
                SetupInputPin(I3);
                SetupInputPin(I4);
                SetupInputPin(I5);
                SetupInputPin(I6);
                SetupInputPin(I7);
                SetupInputPin(I8);

                // Internal BOOT button.
                SetupInputPin(UserButton);

                // EXIO1-EXIO8.
                (void)InitializeOutputExpander();

                // Buzzer.
                SetupOutputPin(BUZZER);
                WriteOutputPin(BUZZER, false);

                // RGB LED.
                auto& led = StatusLed();

                led.begin();
                led.setBrightness(50);
                led.clear();
                led.show();

                // Reset all four logical LED states.
                VirtualLedState() = 0;
                UpdateVirtualLeds();
            }


        #elif DM_BOARD == DM_BOARD_ESP32_S3_ETH

            // ---------------------------------------------------------------
            // Generic ESP32-S3-ETH
            // ---------------------------------------------------------------

            static constexpr int UserButton = PIN_NOT_CONNECTED;

            static constexpr int I1 = PIN_NOT_CONNECTED;
            static constexpr int I2 = PIN_NOT_CONNECTED;
            static constexpr int I3 = PIN_NOT_CONNECTED;
            static constexpr int I4 = PIN_NOT_CONNECTED;
            static constexpr int I5 = PIN_NOT_CONNECTED;
            static constexpr int I6 = PIN_NOT_CONNECTED;
            static constexpr int I7 = PIN_NOT_CONNECTED;
            static constexpr int I8 = PIN_NOT_CONNECTED;

            static constexpr int SIREN = PIN_NOT_CONNECTED;
            static constexpr int ALARM = PIN_NOT_CONNECTED;

            static constexpr int BUZZER = PIN_NOT_CONNECTED;


            inline void SetupPlatformIO()
            {
                auto& led = StatusLed();

                led.begin();
                led.setBrightness(50);
                led.clear();
                led.show();

                VirtualLedState() = 0;
                UpdateVirtualLeds();
            }

        #endif

    #endif


    // ========================================================================
    // USER BUTTON
    // ========================================================================

    inline bool IsUserButtonPressed()
    {
        if (!IsPinAvailable(UserButton))
            return false;

        return digitalRead(UserButton) == LOW;
    }


    // One event per stable press; non-blocking 30 ms debounce.
    inline bool WasUserButtonPressed()
    {
        static bool lastReading = false;
        static bool stableState = false;
        static unsigned long lastChangeTime = 0;

        const unsigned long now = millis();
        const bool reading = IsUserButtonPressed();

        if (reading != lastReading)
        {
            lastReading = reading;
            lastChangeTime = now;
        }

        if ((unsigned long)(now - lastChangeTime) >= 30)
        {
            if (stableState != lastReading)
            {
                stableState = lastReading;

                if (stableState)
                    return true;
            }
        }

        return false;
    }

} // namespace DMPlatform