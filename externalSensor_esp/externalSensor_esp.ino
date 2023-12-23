#include <TimeLib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <CRC.h>
#include <CRC8.h>
#include "DCF77.h"
#include "DataTypes.h"

#define DCF_DATA_PIN 14      // D5
#define DCF_INTERRUPT_PIN 14 // D5
#define SET_PIN 16           // D0
#define HC_RX 12             // D6
#define HC_TX 13             // D7

#define SENSOR_SEND_FREQUENCY_MILLIS 60000
#define TIME_SEND_FREQUENCY_MILLIS 120000

#define HC12_INIT_DELAY 2000

Adafruit_BME280 bme;
DCF77 DCF = DCF77(DCF_DATA_PIN, DCF_INTERRUPT_PIN);
SoftwareSerial HC12(HC_RX, HC_TX);

TimeSendBuf timeBuffer = {'T', 'I', 'M', 'E'};
BMESendBuf sensorBuffer = {'E', 'B', 'M', 'E'};

bool bmeAvailable = false;

bool timeValid = false;
unsigned long lastSensorSend = 0;
unsigned long lastTimeSend = 0;

CRC8 crc;

void setup()
{
    int hcInitTime;
    pinMode(SET_PIN, INPUT);
    Serial.begin(115200);
    HC12.begin(9600);
    WiFi.mode(WIFI_OFF);

    // Start i2c communication
    bmeAvailable = true;
    if (!bme.begin(0x76))
    {
        bmeAvailable = false;
        Serial.println("No sensor found");
    }
    else
    {
        Serial.println("Found BME280");
    }

    // Enable Setting Mode in HC-12
    pinMode(SET_PIN, OUTPUT);

    // Set Baudrate to 9600
    delay(100);
    HC12.print("AT+B9600");
    hcInitTime = millis();
    while (HC12.available() < 8)
    {
        delay(1); // To reset watchdog
        if ((millis() - hcInitTime) > HC12_INIT_DELAY)
        {
            ESP.restart();
        }
    }
    for (int i = 0; i < 8; i++)
    {
        Serial.print((char)HC12.read());
    }
    Serial.println("");

    // Set Channel to 1
    delay(100);
    HC12.print("AT+C001");
    hcInitTime = millis();
    while (HC12.available() < 7)
    {
        delay(1);
        if ((millis() - hcInitTime) > HC12_INIT_DELAY)
        {
            ESP.restart();
        }
    }
    for (int i = 0; i < 7; i++)
    {
        Serial.print((char)HC12.read());
    }
    Serial.println("");

    // Set transmission mode to 3
    delay(100);
    HC12.print("AT+FU3");
    hcInitTime = millis();
    while (HC12.available() < 6)
    {
        delay(1);
        if ((millis() - hcInitTime) > HC12_INIT_DELAY)
        {
            ESP.restart();
        }
    }
    for (int i = 0; i < 6; i++)
    {
        Serial.print((char)HC12.read());
    }
    Serial.println("");

    // Set Power to 8dBm
    delay(100);
    HC12.print("AT+P4");
    delay(100);
    hcInitTime = millis();
    while (HC12.available() < 5)
    {
        delay(1);
        if ((millis() - hcInitTime) > HC12_INIT_DELAY)
        {
            ESP.restart();
        }
    }
    for (int i = 0; i < 5; i++)
    {
        Serial.print((char)HC12.read());
    }
    Serial.println("");

    // Set data transmission to 8 bits + odd parity + 1 stop bit
    Serial.print("AT+U8N1");
    hcInitTime = millis();
    while (HC12.available() < 7)
    {
        delay(1);
        if ((millis() - hcInitTime) > HC12_INIT_DELAY)
        {
            ESP.restart();
        }
    }
    for (int i = 0; i < 7; i++)
    {
        Serial.print((char)HC12.read());
    }
    Serial.println("");

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);

    // Reset timestamps for timer
    lastSensorSend = millis();
    lastTimeSend = millis();
    Serial.println("Initialization finished.");
    DCF.Start();
}

void loop()
{
    time_t DCFtime = DCF.getTime();
    if (DCFtime != 0)
    {
        // Set internal time to dcf time if valid
        setTime(DCFtime);
        if (!timeValid)
        {
            // Reset timestamps because time changed
            lastSensorSend = millis();
            lastTimeSend = millis();
        }
        timeValid = true; // Begin transmission of time
        timeBuffer.data.hour = hour();
        timeBuffer.data.minute = minute();
        timeBuffer.data.second = second();
        timeBuffer.data.year = year();
        timeBuffer.data.month = month();
        timeBuffer.data.day = day();

        crc.restart();
        crc.add((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer) - 1);
        timeBuffer.data.checksum = crc.calc();
        HC12.write((const uint8_t *)timeBuffer.buf, sizeof(timeBuffer));
        Serial.println("Sent DCF Time");
    }

    if (DCF.isMeteoReady())
    {
        MeteoRawSendBuf sendBuffer = DCF.getMeteoBuffer();
        crc.restart();
        crc.add((const uint8_t*)sendBuffer.buf, sizeof(sendBuffer) - 1);
        sendBuffer.data.checksum = crc.calc();
        HC12.write((const uint8_t *)sendBuffer.buf, sizeof(sendBuffer));
        Serial.println("Sent Meteodata");
    }

    // Send BME Data
    if ((millis() - lastSensorSend) > SENSOR_SEND_FREQUENCY_MILLIS)
    {
        if (bmeAvailable)
        {
            // Send sensor data
            sensorBuffer.data.temp = bme.readTemperature();
            sensorBuffer.data.humidity = bme.readHumidity();
            sensorBuffer.data.pressure = bme.readPressure();
            crc.restart();
            crc.add((const uint8_t*)sensorBuffer.buf, sizeof(sensorBuffer) - 1);
            sensorBuffer.data.checksum = crc.calc();
            HC12.write((const uint8_t *)sensorBuffer.buf, sizeof(sensorBuffer));
            Serial.println("Sent BME Data");
        }
        else
        {
            HC12.print("EBMEERROR");
            Serial.println("Sent BME Error message");
        }
        lastSensorSend = millis();
    }
    // Send DCF Data
    /*
    if (((millis() - lastTimeSend) > TIME_SEND_FREQUENCY_MILLIS) && timeValid)
    {
        Serial.println("Send time");
        timeBuffer.dcfTime.hour = hour();
        timeBuffer.dcfTime.minute = minute();
        timeBuffer.dcfTime.second = second();
        timeBuffer.dcfTime.year = year();
        timeBuffer.dcfTime.month = month();
        timeBuffer.dcfTime.day = day();
        Serial.write((const uint8_t*)timeBuffer.timeBuf, sizeof(timeBuffer));
        lastTimeSend = millis();
    }
    */
}
