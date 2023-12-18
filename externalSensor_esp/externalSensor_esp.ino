#include <TimeLib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include "DCF77.h"
#include "DataTypes.h"

#define DCF_DATA_PIN        14  // D5
#define DCF_INTERRUPT_PIN   14  // D5
#define SET_PIN             2   // D4

#define SENSOR_SEND_FREQUENCY_MILLIS    60000
#define TIME_SEND_FREQUENCY_MILLIS  120000

Adafruit_BME280 bme;
DCF77 DCF = DCF77(DCF_DATA_PIN, DCF_INTERRUPT_PIN);

TimeSendBuf timeBuffer = {'T','I','M','E'};
BMESendBuf sensorBuffer = {'E','B','M','E'};  

bool timeValid = false;
unsigned long lastSensorSend = 0;
unsigned long lastTimeSend = 0;

void setup() {
    pinMode(SET_PIN, INPUT);
    Serial.begin(9600);
    WiFi.mode(WIFI_OFF);

    // Start i2c communication
    if(!bme.begin(0x76))
    {
        Serial.println("No sensor found");
    }
    
    // Enable Setting Mode in HC-12
    pinMode(SET_PIN, OUTPUT);

    // Set Baudrate to 9600
    delay(100);
    Serial.print("AT+B9600");

    // Set Channel to 1
    delay(100);
    Serial.print("AT+C001");

    // Set transmission mode to 3
    delay(100);
    Serial.print("AT+FU3");

    // Set Power to 8dBm
    delay(100);
    Serial.print("AT+P6");
    delay(100);

    // Set data transmission to 8 bits + odd parity + 1 stop bit
    Serial.print("AT+U8N1");

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);

    // Reset timestamps for timer
    lastSensorSend = millis();
    lastTimeSend = millis();
    Serial.println("Initialization finished.");
    DCF.Start();
}

void loop() {
    time_t DCFtime = DCF.getTime();
    if(DCFtime != 0)
    {
        // Set internal time to dcf time if valid
        setTime(DCFtime);
        if (!timeValid)
        {
          // Reset timestamps because time changed
          lastSensorSend = millis();
          lastTimeSend = millis();
        }
        timeValid = true;   // Begin transmission of time
        timeBuffer.data.hour = hour();
        timeBuffer.data.minute = minute();
        timeBuffer.data.second = second();
        timeBuffer.data.year = year();
        timeBuffer.data.month = month();
        timeBuffer.data.day = day();
        Serial.write((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer));
    }

    if (DCF.isMeteoReady())
    {
        MeteoRawSendBuf sendBuffer = DCF.getMeteoBuffer();
        Serial.write((const uint8_t*)sendBuffer.buf, sizeof(sendBuffer));
    }
    
    // Send BME Data
    if ((millis() - lastSensorSend) > SENSOR_SEND_FREQUENCY_MILLIS)
    {
        // Send sensor data
        sensorBuffer.data.temp = bme.readTemperature();
        sensorBuffer.data.humidity = bme.readHumidity();
        sensorBuffer.data.pressure = bme.readPressure();
        Serial.write((const uint8_t*)sensorBuffer.buf, sizeof(sensorBuffer));
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
