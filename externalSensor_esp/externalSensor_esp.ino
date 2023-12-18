#include <TimeLib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "DCF77.h"
#include "DataTypes.h"

#define DCF_DATA_PIN        14  // D5
#define DCF_INTERRUPT_PIN   14  // D5
#define SET_PIN             2   // D4

#define SENSOR_SEND_FREQUENCY_MILLIS    60000
#define TIME_SEND_FREQUENCY_MILLIS  120000

Adafruit_BME280 bme;
DCF77 DCF = DCF77(DCF_DATA_PIN, DCF_INTERRUPT_PIN);

timeSendBuf timeBuffer = {'T','I','M','E'};
sensorSendBuf sensorBuffer = {'E','B','M','E'};  
meteoSendBuf meteoBuffer = {'M','T','E','O'};

bool timeValid = false;
unsigned long lastSensorSend = 0;
unsigned long lastTimeSend = 0;

void setup() {
    char rxArray[9] = {0};
    pinMode(SET_PIN, INPUT);
    Serial.begin(9600);

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
    while(Serial.available() < 8);
    for(int i = 0; i < 8; i++)
    {
        rxArray[i] = Serial.read();
    }
    rxArray[8] = '\0';
    Serial.println(rxArray);

    // Set Channel to 1
    delay(100);
    Serial.print("AT+C001");
    while(Serial.available() < 8);
    for(int i = 0; i < 8; i++)
    {
        rxArray[i] = Serial.read();
    }
    rxArray[8] = '\0';
    Serial.println(rxArray);


    // Set transmission mode to 3
    delay(100);
    Serial.print("AT+FU3");
    while(Serial.available() < 5);
    for(int i = 0; i < 5; i++)
    {
        rxArray[i] = Serial.read();
    }
    rxArray[5] = '\0';
    Serial.println(rxArray);

    // Set Power to 8dBm
    delay(100);
    Serial.print("AT+P6");
    while(Serial.available() < 5);
    for(int i = 0; i < 5; i++)
    {
        rxArray[i] = Serial.read();
    }
    rxArray[5] = '\0';
    Serial.println(rxArray);
    delay(100);

    // Set data transmission to 8 bits + odd parity + 1 stop bit
    Serial.print("AT+U8N1");
    while(Serial.available() < 7);
    for(int i = 0; i < 7; i++)
    {
        rxArray[i] = Serial.read();
    }
    rxArray[7] = '\0';
    Serial.println(rxArray);

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);

    // Reset timestamps for timer
    lastSensorSend = millis();
    lastTimeSend = millis();
    Serial.println("Ping");
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
        timeBuffer.dcfTime.hour = hour();
        timeBuffer.dcfTime.minute = minute();
        timeBuffer.dcfTime.second = second();
        timeBuffer.dcfTime.year = year();
        timeBuffer.dcfTime.month = month();
        timeBuffer.dcfTime.day = day();
        Serial.write((const uint8_t*)timeBuffer.timeBuf, sizeof(timeBuffer));
    }

    if (DCF.isMeteoReady())
    {
        meteoBuffer.data.packetData = DCF.getMeteoData();
        Serial.write((const uint8_t*)meteoBuffer.meteoBuf, sizeof(meteoBuffer));
    }
    
    // Send BME Data
    if ((millis() - lastSensorSend) > SENSOR_SEND_FREQUENCY_MILLIS)
    {
        // Send sensor data
        sensorBuffer.sensor.temp = bme.readTemperature();
        sensorBuffer.sensor.humidity = bme.readHumidity();
        sensorBuffer.sensor.pressure = bme.readPressure();
        Serial.write((const uint8_t*)sensorBuffer.sensorBuf, sizeof(sensorBuffer));
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
