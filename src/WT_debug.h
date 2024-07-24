#pragma once

namespace DebugCode {

    enum DEBUG_LED {
        SERVER = D4,
        SDCARD = D3,
        SENSOR = D0
    };

    void setup() {
        pinMode(DEBUG_LED::SERVER, OUTPUT);
        pinMode(DEBUG_LED::SDCARD, OUTPUT);
        pinMode(DEBUG_LED::SENSOR, OUTPUT);
        digitalWrite(DEBUG_LED::SERVER, HIGH);
        digitalWrite(DEBUG_LED::SDCARD, LOW);
        digitalWrite(DEBUG_LED::SENSOR, LOW);
    }

    void blink(DEBUG_LED led, uint16_t duration) {
        if (led == DEBUG_LED::SERVER)
        {
            digitalWrite(led, LOW);
            delay(duration);
            digitalWrite(led, HIGH);
            return;
        }
        digitalWrite(led, HIGH);
        delay(duration);
        digitalWrite(led, LOW);
    }
    
    // Debug Codes
    void wifiNotConnected() {
        blink(DEBUG_LED::SERVER, 500);
        delay(500);
    }

    void clientConnected() {
        blink(DEBUG_LED::SERVER, 100);
    }

    void clientDisconnected() {
        blink(DEBUG_LED::SERVER, 50);
        delay(50);
        blink(DEBUG_LED::SERVER, 50);
    }

    void sensorError() {
        blink(DEBUG_LED::SENSOR, 25);
    }

    void sdCardIntializationError() {
        for (byte count = 0; count < 10; count++) {
            blink(DEBUG_LED::SDCARD, 500);
            delay(500);
        }
    }

    void sdCardFileNotFoundError() {
        for (byte count = 0; count < 2; count++)
        {
            blink(DEBUG_LED::SDCARD, 200);
            delay(50);
            blink(DEBUG_LED::SDCARD, 200);
            delay(100);
        }
        
    }
}