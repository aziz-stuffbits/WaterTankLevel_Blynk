#include <Arduino.h>

#define TRIG_PIN 8
#define ECHO_PIN 9

void setup()
{
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    Serial.println("Arduino Tank Node Starting");
}

void loop()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    unsigned long echoTime =
        pulseIn(ECHO_PIN, HIGH, 30000);

    if (echoTime == 0)
    {
        Serial.println("Sensor timeout");
    }
    else
    {
        float distanceCm =
            (echoTime * 0.0343f) / 2.0f;

        Serial.print("Distance: ");
        Serial.print(distanceCm, 1);
        Serial.println(" cm");
    }

    delay(500);
}