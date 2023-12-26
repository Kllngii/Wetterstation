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

// Send bme data every 53 seconds to reduce collision probability with time or meteo packets
#define SENSOR_SEND_FREQUENCY_MILLIS 53000

#define NUMBER_OF_RETRANSMISSIONS   5       // Resend a packet max. 5 times
// A retransmission command has to be sent max. 3 seconds after a packet has been sent. 
#define PACKET_RETRANSMISSION_TIME  3000

Adafruit_BME280 bme;
DCF77 DCF = DCF77(DCF_DATA_PIN, DCF_INTERRUPT_PIN);
SoftwareSerial HC12(HC_RX, HC_TX);

TimeSendBuf timeBuffer = {'T', 'I', 'M', 'E'};
BMESendBuf sensorBuffer = {'E', 'B', 'M', 'E'};

MeteoRawSendBuf sendBuffer;

bool bmeAvailable = false;

bool timeValid = false;
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
    Serial.begin(115200);
    HC12.begin(9600);
    WiFi.mode(WIFI_OFF);

    Serial.println("");

    // Start I2C communication
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
    delay(100);
    while(HC12.available())
    {
        Serial.print((char)HC12.read());
        delay(10);
    }
    Serial.println("");

    // Set Channel to 1
    delay(100);
    HC12.print("AT+C001");
    delay(100);
    while(HC12.available())
    {
        Serial.print((char)HC12.read());
        delay(10);
    }
    Serial.println("");

    // Set transmission mode to 3
    delay(100);
    HC12.print("AT+FU3");
    delay(100);
    while(HC12.available())
    {
        Serial.print((char)HC12.read());
        delay(10);
    }
    Serial.println("");

    // Set Power to 14dBm
    delay(100);
    HC12.print("AT+P6");
    delay(100);
    while(HC12.available())
    {
        Serial.print((char)HC12.read());
        delay(10);
    }
    Serial.println("");

    // Set data transmission to 8 bits + no parity + 1 stop bit
    HC12.print("AT+U8N1");
    delay(100);
    while(HC12.available())
    {
        Serial.print((char)HC12.read());
        delay(10);
    }
    Serial.println("");

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);

    // Reset timestamps for timer
    lastSensorSend = millis();
    lastTimeSend = millis();
    Serial.println("Initialization finished.");
    // Start DCF Time detection
    DCF.Start();
}

void loop()
{
    // Try to get updated DCF time
    time_t DCFtime = DCF.getTime();
    if (DCFtime != 0)
    {
        // Set internal time to dcf time if valid
        setTime(DCFtime);
        if (!timeValid)
        {
            // Reset timestamps because millis probably changed after first time update
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

        // Calculate Checksum and store it in the sending struct
        crc.restart();
        crc.add((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer) - 1);
        timeBuffer.data.checksum = crc.calc();
        // Send data
        HC12.write((const uint8_t *)timeBuffer.buf, sizeof(timeBuffer));
        Serial.println("Sent DCF Time");
        timeResendCounter = 0;
        timePacketPending = true;
        timePacketValid = true;
        timeRetransTime = millis();
    }
    // Resend time data if packet has been sent and an error message was received
    if (!timePacketValid)
    {
        HC12.write((const uint8_t *)timeBuffer.buf, sizeof(timeBuffer));
        Serial.println("Resent DCF Time");
        timePacketPending = true;
        timePacketValid = true;
        timeResendCounter++;
    }

    // Send meteodata if available
    if (DCF.isMeteoReady())
    {
        sendBuffer = DCF.getMeteoBuffer();
        crc.restart();
        crc.add((const uint8_t*)sendBuffer.buf, sizeof(sendBuffer) - 1);
        sendBuffer.data.checksum = crc.calc();
        HC12.write((const uint8_t *)sendBuffer.buf, sizeof(sendBuffer));
        Serial.println("Sent Meteodata");
        meteoResendCounter = 0;
        meteoPacketPending = true;
        meteoPacketValid = true;
        meteoRetransTime = millis();
    }
    // Resend meteopacket if checksum was incorrect at receiver
    if (!meteoPacketValid)
    {
        HC12.write((const uint8_t *)sendBuffer.buf, sizeof(sendBuffer));
        Serial.println("Resent Meteodata");
        meteoPacketPending = true;
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
        HC12.write((const uint8_t *)sensorBuffer.buf, sizeof(sensorBuffer));
        Serial.println("Sent BME Data");
        bmeResendCounter = 0;
        bmePacketPending = true;
        bmePacketValid = true;
        bmeRetransTime = millis();
        lastSensorSend = millis();
    }
    // Resend bme data if they were invalid
    if (!bmePacketValid)
    {
        HC12.write((const uint8_t *)sensorBuffer.buf, sizeof(sensorBuffer));
        Serial.println("Resent BME Data");
        bmePacketPending = true;
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
    if (HC12.available() > 5)
    {
        // Clear buffer
        while(HC12.available())
        {
            Serial.print((char)HC12.read());
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
