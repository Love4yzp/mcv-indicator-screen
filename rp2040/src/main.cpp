/*
 * SenseCAP Indicator RP2040 — Sensor collector firmware.
 *
 * Reads SEN54 (PM1.0/PM2.5/PM4.0/PM10/Temp/Humidity/VOC/NOx) and sends
 * data to ESP32-S3 via UART + COBS (PacketSerial).
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <Wire.h>
#include <PacketSerial.h>
#include <SensirionI2CSen5x.h>

#include "protocol.h"

/* ---------- Hardware pins (SenseCAP Indicator) ---------- */
#define PIN_SENSOR_POWER    18
#define PIN_BUZZER          19
#define PIN_I2C_SDA         20
#define PIN_I2C_SCL         21
#define PIN_UART_TX         16
#define PIN_UART_RX         17

/* ---------- Config ---------- */
#ifndef PKT_SEND_INTERVAL_MS
#define PKT_SEND_INTERVAL_MS 5000
#endif

/* ---------- Globals ---------- */
static PacketSerial pktSerial;
static SensirionI2CSen5x sen5x;

/* ---------- Sensor power ---------- */
static void sensor_power_on(void)  { pinMode(PIN_SENSOR_POWER, OUTPUT); digitalWrite(PIN_SENSOR_POWER, HIGH); }
static void sensor_power_off(void) { pinMode(PIN_SENSOR_POWER, OUTPUT); digitalWrite(PIN_SENSOR_POWER, LOW); }

/* ---------- Buzzer ---------- */
static void buzzer_init(void) { pinMode(PIN_BUZZER, OUTPUT); }
static void buzzer_beep(void) { analogWrite(PIN_BUZZER, 127); delay(50); analogWrite(PIN_BUZZER, 0); }
static void buzzer_off(void)  { digitalWrite(PIN_BUZZER, LOW); }

/* ---------- Send packet to ESP32 ---------- */
static void send_sensor_data(uint8_t type, float value)
{
    uint8_t buf[5];
    buf[0] = type;
    memcpy(&buf[1], &value, sizeof(float));
    pktSerial.send(buf, sizeof(buf));
}

/* ---------- Handle commands from ESP32 ---------- */
static void on_packet_received(const uint8_t *buf, size_t size)
{
    if (size < 1) return;
    switch (buf[0]) {
        case PKT_TYPE_CMD_SHUTDOWN:  sensor_power_off(); Serial.println("CMD: shutdown"); break;
        case PKT_TYPE_CMD_POWER_ON:  sensor_power_on();  Serial.println("CMD: power on"); break;
        case PKT_TYPE_CMD_BEEP_ON:   buzzer_beep();      Serial.println("CMD: beep");     break;
        case PKT_TYPE_CMD_BEEP_OFF:  buzzer_off();       Serial.println("CMD: beep off"); break;
        default: break;
    }
}

/* ---------- Arduino entry points ---------- */
void setup()
{
    Serial.begin(115200);

    /* UART to ESP32 */
    Serial1.setTX(PIN_UART_TX);
    Serial1.setRX(PIN_UART_RX);
    Serial1.begin(115200);
    pktSerial.setStream(&Serial1);
    pktSerial.setPacketHandler(&on_packet_received);

    /* Sensors */
    sensor_power_on();

    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();

    sen5x.begin(Wire);
    uint16_t err = sen5x.startMeasurement();
    if (err) {
        Serial.printf("SEN54 startMeasurement failed: %u\n", err);
    }

    buzzer_init();
    delay(500);
    buzzer_beep();

    Serial.println("SenseCAP Indicator RP2040 ready (SEN54)");
}

static uint32_t last_measure_ms = 0;

void loop()
{
    pktSerial.update();

    uint32_t now = millis();
    if (now - last_measure_ms >= PKT_SEND_INTERVAL_MS) {
        last_measure_ms = now;

        float pm1, pm2_5, pm4, pm10, humidity, temperature, vocIndex, noxIndex;
        uint16_t err = sen5x.readMeasuredValues(
            pm1, pm2_5, pm4, pm10,
            humidity, temperature, vocIndex, noxIndex);
        (void)noxIndex;  /* SEN54 has no NOx sensor — value is a sentinel */

        if (!err) {
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_PM1_0,     pm1);
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_PM2_5,     pm2_5);
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_PM4_0,     pm4);
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_PM10,      pm10);
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_TEMP,       temperature);
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_HUMIDITY,   humidity);
            send_sensor_data(PKT_TYPE_SENSOR_SEN54_VOC_INDEX,  vocIndex);
            send_sensor_data(PKT_TYPE_SENSOR_BATCH_END,        0.0f);

            Serial.printf("SEN54  PM2.5=%.1f T=%.1f H=%.1f VOC=%.0f\n",
                          pm2_5, temperature, humidity, vocIndex);
        } else {
            Serial.printf("SEN54 read failed: %u\n", err);
        }
    }
}
