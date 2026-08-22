#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("Arduino Tank Node Starting");
}

void loop()
{
    Serial.println("Tank Node Alive");
    delay(1000);
}