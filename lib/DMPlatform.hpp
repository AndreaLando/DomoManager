#pragma once

#include <Arduino.h>
#include <time.h>

#if defined(ARDUINO_ARCH_MBED)

    #include <mbed_rtc_time.h>

#elif defined(ARDUINO_ARCH_ESP32)

    #include <sys/time.h>
    #include <Adafruit_NeoPixel.h>

#else

    #error "Unsupported DomoManager platform"

#endif

namespace DMPlatform
{
    // ========================================================================
    // COMMON
    // ========================================================================

    static constexpr int PIN_NOT_CONNECTED = -1;

    inline bool IsPinAvailable(int pin)
    {
        return pin != PIN_NOT_CONNECTED;
    }

    #if defined(ARDUINO_ARCH_ESP32)

        // ============================================================
        // ESP32-S3-ETH
        // ============================================================

        static constexpr int ESP32_LED_D0 = 21;

        // Forward declaration
        inline Adafruit_NeoPixel& StatusLed();

    #endif

    // ============================================================
    // NETWORK RESOURCES
    // ============================================================

    #if defined(ARDUINO_ARCH_MBED)

        static constexpr uint8_t MAX_NETWORK_SOCKETS = 4;

    #elif defined(ARDUINO_ARCH_ESP32)

        static constexpr uint8_t MAX_NETWORK_SOCKETS = 6;

    #else

        #error "Unsupported DomoManager platform"

    #endif

    // ============================================================
    // INPUT / OUTPUT
    // ============================================================

    inline void SetupInputPin(int pin)
    {
        if (IsPinAvailable(pin))
            pinMode(pin, INPUT);
    }

    inline void SetupOutputPin(int pin)
    {
        if (!IsPinAvailable(pin))
            return;

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
    #if defined(ARDUINO_ARCH_ESP32)

        // --------------------------------------------------------------------
        // ESP32-S3-ETH onboard WS2812
        // --------------------------------------------------------------------

        if (pin == ESP32_LED_D0)
        {
            auto& led = StatusLed();

            if (state)
            {
                led.setPixelColor(
                    0,
                    led.Color(
                        0,
                        255,
                        0
                    )
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

        if (!IsPinAvailable(pin))
            return;

        digitalWrite(
            pin,
            state ? HIGH : LOW
        );
    }

    inline bool ReadInputPin(int pin)
    {
        if (!IsPinAvailable(pin))
            return false;

        return digitalRead(pin);
    }

    // ========================================================================
    // PLATFORM ABSTRACTION
    // ========================================================================

    inline void Restart()
    {
        #if defined(ARDUINO_ARCH_MBED)

                NVIC_SystemReset();

        #elif defined(ARDUINO_ARCH_ESP32)

                ESP.restart();

        #else

                #error "DMPlatform::Restart(): unsupported platform"

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
    // OPTA
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

    static constexpr int UserButton = PIN_NOT_CONNECTED;

    static constexpr int LedUser = PIN_NOT_CONNECTED;
    static constexpr int LedD0   = ESP32_LED_D0;
    static constexpr int LedD1   = PIN_NOT_CONNECTED;
    static constexpr int LedD2   = PIN_NOT_CONNECTED;
    static constexpr int LedD3   = PIN_NOT_CONNECTED;

    static constexpr int I1 = PIN_NOT_CONNECTED;
    static constexpr int I2 = PIN_NOT_CONNECTED;
    static constexpr int I3 = PIN_NOT_CONNECTED;
    static constexpr int I4 = PIN_NOT_CONNECTED;

    static constexpr int SIREN = PIN_NOT_CONNECTED;
    static constexpr int ALARM = PIN_NOT_CONNECTED;

    // ========================================================================
    // ONBOARD WS2812 RGB LEd
    // ========================================================================

    inline Adafruit_NeoPixel& StatusLed()
    {
        static Adafruit_NeoPixel led(
            1,
            ESP32_LED_D0,
            NEO_GRB + NEO_KHZ800
        );

        return led;
    }


    inline void SetupPlatformIO()
    {
        auto& led = StatusLed();

        led.begin();

        led.setBrightness(50);

        led.clear();

        led.show();
    }


#else

    #error "Unsupported DomoManager platform!"

#endif

} // namespace DMPlatform