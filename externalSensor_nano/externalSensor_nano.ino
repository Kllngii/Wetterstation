/*
 *  Created by faschmali
 */

#include <TimeLib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <CRC.h>
#include <CRC8.h>
#include "DCF77.h"
#include "DataTypes.h"
#include "Meteo.h"

#define DCF_DATA_PIN 2      // D2
#define DCF_INTERRUPT_PIN 2 // D2
#define SET_PIN 4          // D4

// Send bme data every 30 seconds to reduce collision probability with time or meteo packets
#define SENSOR_SEND_FREQUENCY_MILLIS 30000

#define NUMBER_OF_RETRANSMISSIONS   5       // Resend a packet max. 5 times
// A retransmission command has to be sent max. 3 seconds after a packet has been sent. 
#define PACKET_RETRANSMISSION_TIME  3000

Adafruit_BME280 bme;
DCF77 DCF = DCF77(DCF_DATA_PIN, DCF_INTERRUPT_PIN, false);

TimeSendBuf timeBuffer = {'T', 'I', 'M', 'E'};
BMESendBuf sensorBuffer = {'E', 'B', 'M', 'E'};

MeteoDecodedSendBuf meteoBuffer;
Meteo meteo;

bool bmeAvailable = false;

unsigned long lastSensorSend = 0;
unsigned long lastTimeSend = 0;

bool timePacketValid = true;
bool meteoPacketValid = true;
bool bmePacketValid = true;

bool timePacketPending = false;
bool meteoPacketPending = false;
bool bmePacketPending = false;

int timeResendCounter = 0;
int meteoResendCounter = 0;
int bmeResendCounter = 0;

int timeRetransTime = 0;
int meteoRetransTime = 0;
int bmeRetransTime = 0;

CRC8 crc;

void setup()
{
    Serial.begin(9600);

    // Start I2C communication
    bmeAvailable = true;
    if (!bme.begin(0x76))
    {
        bmeAvailable = false;
        //Serial.println("No sensor found");
    }
    else
    {
        //Serial.println("Found BME280");
    }

    // Initialize Pins for Meteodecoder
    pinMode(METEO_DATA_IN, OUTPUT);
    pinMode(METEO_DATA_OUT, INPUT);
    pinMode(METEO_CLK_IN, OUTPUT);
    pinMode(METEO_CLK_OUT, INPUT);
    pinMode(METEO_RDY, INPUT);
    digitalWrite(METEO_DATA_IN, LOW);
    digitalWrite(METEO_CLK_IN, LOW);

    // Enable Setting Mode in HC-12
    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, LOW);

    // Set Baudrate to 9600
    delay(100);
    Serial.print("AT+B9600");
    delay(100);
    while(Serial.available())
    {
        Serial.read();
        delay(10);
    }

    // Set Channel to 1
    delay(100);
    Serial.print("AT+C001");
    delay(100);
    while(Serial.available())
    {
        Serial.read();
        delay(10);
    }

    // Set transmission mode to 3
    delay(100);
    Serial.print("AT+FU3");
    delay(100);
    while(Serial.available())
    {
        Serial.read();
        delay(10);
    }

    // Set Power to 14dBm
    delay(100);
    Serial.print("AT+P6");
    delay(100);
    while(Serial.available())
    {
        Serial.read();
        delay(10);
    }

    // Set data transmission to 8 bits + no parity + 1 stop bit
    Serial.print("AT+U8N1");
    delay(100);
    while(Serial.available())
    {
        Serial.read();
        delay(10);
    }
    //Serial.println("");

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);

    // Reset timestamps for timer
    lastSensorSend = millis();
    lastTimeSend = millis();
    // Start DCF Time detection
    DCF.Start();
}

void loop()
{
    // Try to get updated DCF time
    time_t DCFtime = DCF.getTime();
    if (DCFtime != 0)
    {
        timeBuffer.data.hour = hour(DCFtime);
        timeBuffer.data.minute = minute(DCFtime);
        timeBuffer.data.second = 0;
        timeBuffer.data.year = year(DCFtime);
        timeBuffer.data.month = month(DCFtime);
        timeBuffer.data.day = day(DCFtime);
        if (weekday(DCFtime) == 1)
        {
            timeBuffer.data.weekDay = 7;
        }
        else
        {
            timeBuffer.data.weekDay = weekday(DCFtime) - 1;
        }
        // Calculate Checksum and store it in the sending struct
        crc.restart();
        crc.add((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer) - 1);
        timeBuffer.data.checksum = crc.calc();
        // Send data
        Serial.write((const uint8_t *)timeBuffer.buf, sizeof(timeBuffer));
        timeResendCounter = 0;
        timePacketPending = true;
        timePacketValid = true;
        timeRetransTime = millis();
        lastSensorSend = millis() - 15000;
    }
    // Resend time data if packet has been sent and an error message was received
    if (!timePacketValid)
    {
        Serial.write((const uint8_t *)timeBuffer.buf, sizeof(timeBuffer));
        timePacketValid = true;
        timeResendCounter++;
    }

    // Send meteodata if available
    if (DCF.isMeteoReady())
    {
        meteo.getNewData(DCF.getMeteoBuffer());
    }
    if (meteo.isNewMeteo() && !digitalRead(METEO_RDY))
    {
        meteo.decode();
        if (meteo.isMeteoValid())
        {
            meteoBuffer = meteo.getConvertedBuffer();
            crc.restart();
            crc.add((const uint8_t*)meteoBuffer.buf, sizeof(meteoBuffer) - 1);
            meteoBuffer.data.checksum = crc.calc();
            Serial.write((const uint8_t *)meteoBuffer.buf, sizeof(meteoBuffer));
            meteoResendCounter = 0;
            meteoPacketPending = true;
            meteoPacketValid = true;
            meteoRetransTime = millis();
        }
    }
    // Resend meteopacket if checksum was incorrect at receiver
    if (!meteoPacketValid)
    {
        Serial.write((const uint8_t *)meteoBuffer.buf, sizeof(meteoBuffer));
        meteoPacketValid = true;
        meteoResendCounter++;
    }

    // Send BME Data if sensor is available
    if (((millis() - lastSensorSend) > SENSOR_SEND_FREQUENCY_MILLIS) && bmeAvailable)
    {
        // Send sensor data
        sensorBuffer.data.temp = bme.readTemperature();
        sensorBuffer.data.humidity = bme.readHumidity();
        sensorBuffer.data.pressure = bme.readPressure() / 100.0;
        crc.restart();
        crc.add((const uint8_t*)sensorBuffer.buf, sizeof(sensorBuffer) - 1);
        sensorBuffer.data.checksum = crc.calc();
        Serial.write((const uint8_t *)sensorBuffer.buf, sizeof(sensorBuffer));
        //Serial.println("Sent BME Data");
        bmeResendCounter = 0;
        bmePacketPending = true;
        bmePacketValid = true;
        bmeRetransTime = millis();
        lastSensorSend = millis();
    }
    // Resend bme data if they were invalid
    if (!bmePacketValid)
    {
        Serial.write((const uint8_t *)sensorBuffer.buf, sizeof(sensorBuffer));
        bmePacketValid = true;
        bmeResendCounter++;
    }

    // Declare sent packet as received valid after no retransmission command has been received
    if (bmePacketPending)
    {
        if ((millis() - bmeRetransTime) > PACKET_RETRANSMISSION_TIME)
        {
            bmePacketPending = false;
            bmePacketValid = true;
        }
    }
    if (timePacketPending)
    {
        if ((millis() - timeRetransTime) > PACKET_RETRANSMISSION_TIME)
        {
            timePacketPending = false;
            timePacketValid = true;
        }
    }
    if (meteoPacketPending)
    {
        if ((millis() - meteoRetransTime) > PACKET_RETRANSMISSION_TIME)
        {
            meteoPacketPending = false;
            meteoPacketValid = true;
        }
    }

    // Internal sensor sends 15 bytes, make sure external sensor didn't just receive
    // random data
    if (Serial.available() > 5)
    {
        // Clear buffer
        while(Serial.available())
        {
            Serial.read();
            delay(20);
        }
        // Resend a packet max. 5 times
        if (bmePacketPending && (bmeResendCounter < NUMBER_OF_RETRANSMISSIONS))
        {
            bmePacketValid = false;
            bmeRetransTime = millis();
        }
        else if (timePacketPending && (timeResendCounter < NUMBER_OF_RETRANSMISSIONS))
        {
            timePacketValid = false;
            timeRetransTime = millis();
        }
        else if (meteoPacketPending && (meteoResendCounter < NUMBER_OF_RETRANSMISSIONS))
        {
            meteoPacketValid = false;
            meteoRetransTime = millis();
        }
    }
}
