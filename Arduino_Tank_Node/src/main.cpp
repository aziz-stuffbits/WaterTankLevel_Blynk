#include <Arduino.h>
#include <ModbusRTU.h>

#define TRIG_PIN 8
#define ECHO_PIN 9
#define DE_RE_PIN 2
#define STATUS_LED_PIN 13

#define MODBUS_SLAVE_ID 1
#define MODBUS_BAUD 9600

#define REG_DISTANCE_MM 0
#define REG_LEVEL_PCT 1
#define REG_VOLUME_L 2
#define REG_SENSOR_STATUS 3
#define REG_RESERVED 4
#define REG_TANK_HEIGHT_MM 10
#define REG_TANK_CAPACITY_L 11
#define REG_COUNT 12

#define STATUS_OK 0
#define STATUS_TIMEOUT 1
#define STATUS_INVALID 2

/* Placeholder tank constants. Replace during calibration. */
#define TANK_HEIGHT_MM_DEFAULT 1500
#define TANK_CAPACITY_L_DEFAULT 1000

#define MEASURE_INTERVAL_MS 1000
#define ECHO_TIMEOUT_US 20000
#define DISTANCE_MM_MIN 20
#define DISTANCE_MM_MAX 4000

ModbusRTU mb;

static uint16_t last_distance_mm = 0;
static bool have_last_good = false;
static unsigned long last_measure_ms = 0;

static void update_tank_registers(uint16_t distance_mm, uint16_t status)
{
    uint16_t height_mm = mb.Hreg(REG_TANK_HEIGHT_MM);
    uint16_t capacity_l = mb.Hreg(REG_TANK_CAPACITY_L);

    if (height_mm == 0)
    {
        height_mm = TANK_HEIGHT_MM_DEFAULT;
        mb.Hreg(REG_TANK_HEIGHT_MM, height_mm);
    }

    if (capacity_l == 0)
    {
        capacity_l = TANK_CAPACITY_L_DEFAULT;
        mb.Hreg(REG_TANK_CAPACITY_L, capacity_l);
    }

    uint16_t water_mm = 0;
    uint16_t level_pct = 0;
    uint16_t volume_l = 0;

    if (status == STATUS_OK)
    {
        if (distance_mm > height_mm)
        {
            status = STATUS_INVALID;
        }
        else
        {
            water_mm = height_mm - distance_mm;
            level_pct = (uint16_t)(((uint32_t)water_mm * 100UL) / height_mm);
            if (level_pct > 100)
            {
                level_pct = 100;
            }
            volume_l = (uint16_t)(((uint32_t)level_pct * capacity_l) / 100UL);
        }
    }

    if ((status != STATUS_OK) && have_last_good)
    {
        mb.Hreg(REG_SENSOR_STATUS, status);
        return;
    }

    mb.Hreg(REG_DISTANCE_MM, distance_mm);
    mb.Hreg(REG_LEVEL_PCT, level_pct);
    mb.Hreg(REG_VOLUME_L, volume_l);
    mb.Hreg(REG_SENSOR_STATUS, status);
    mb.Hreg(REG_RESERVED, 0);
}

static void measure_and_publish(void)
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long echo_us = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);

    if (echo_us == 0)
    {
        digitalWrite(STATUS_LED_PIN, LOW);
        update_tank_registers(last_distance_mm, STATUS_TIMEOUT);
        return;
    }

    float distance_cm = (echo_us * 0.0343f) / 2.0f;
    int distance_mm = (int)((distance_cm * 10.0f) + 0.5f);

    if ((distance_mm < DISTANCE_MM_MIN) ||
        (distance_mm > DISTANCE_MM_MAX))
    {
        digitalWrite(STATUS_LED_PIN, LOW);
        update_tank_registers(last_distance_mm, STATUS_INVALID);
        return;
    }

    last_distance_mm = (uint16_t)distance_mm;
    have_last_good = true;
    digitalWrite(STATUS_LED_PIN, HIGH);
    update_tank_registers(last_distance_mm, STATUS_OK);
}

void setup()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(DE_RE_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);

    digitalWrite(TRIG_PIN, LOW);
    digitalWrite(DE_RE_PIN, LOW);
    digitalWrite(STATUS_LED_PIN, LOW);

    Serial.begin(MODBUS_BAUD, SERIAL_8N1);
    mb.begin(&Serial, DE_RE_PIN);
    mb.setBaudrate(MODBUS_BAUD);
    mb.slave(MODBUS_SLAVE_ID);

    mb.addHreg(REG_DISTANCE_MM, 0, REG_COUNT);
    mb.Hreg(REG_TANK_HEIGHT_MM, TANK_HEIGHT_MM_DEFAULT);
    mb.Hreg(REG_TANK_CAPACITY_L, TANK_CAPACITY_L_DEFAULT);
}

void loop()
{
    mb.task();

    unsigned long now = millis();
    if ((now - last_measure_ms) >= MEASURE_INTERVAL_MS)
    {
        last_measure_ms = now;
        measure_and_publish();
    }

    yield();
}
