#pragma once
#include <WT_debug.h>

#define pinEcho D1 // brown
#define pinTrig D2 // black


namespace WT_ultraSonic {

    int getImmediateRead();
    int getAvgRead();
    void setup();

    uint32_t duration {0};
    uint16_t echoRead {0};
    byte accuracy {1};
    bool logData {false};

    void setup() {
        pinMode(pinTrig, OUTPUT);
        pinMode(pinEcho, INPUT);
        // while (!getImmediateRead()) {
        //     DebugCode::sensorError();
        // }
        echoRead = 0; 
    }

    int getImmediateRead() {
        digitalWrite(pinTrig, LOW);
        delayMicroseconds(2);
        digitalWrite(pinTrig, HIGH);
        delayMicroseconds(10);
        digitalWrite(pinTrig, LOW);
        duration = pulseIn(pinEcho, HIGH, 25000); //29.2 microseconds to travel 1 cm
        echoRead = duration / 58.2;
        return echoRead;
    }

    int getAvgRead() {
        int avg {0};
        for (byte count = 0; count < accuracy; count++)
        {
            avg += getImmediateRead();
            delay(20);
        }
        if (!avg) DebugCode::sensorError();
        echoRead = avg / accuracy;
        // Serial.println("Read From Sensor: " + String(echoRead));
        return echoRead;
    }
}
