#include <stdio.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "CO2.h"
#include "Meteo.h"
#include "DataTypes.h"

// Pindefs running on core0
#define UART0_TX        0
#define UART0_RX        1

#define HC12_TX         8
#define HC12_RX         9
#define HC12_SET_PIN    3

#define CO2_PWM_PIN     15

#define BME_SEND_FREQUENCY_MILLIS   30000
#define CO2_SEND_FREQUENCY_MILLIS   15000

Adafruit_BME280 bme;    // BME280, read by core0
CO2 co2(CO2_PWM_PIN, MHZ19C, CO2::RANGE_5K);
Meteo meteo;
CO2SendBuf co2Buffer = {'C','O','2','.'};
BMESendBuf iBmeBuffer = {'I','B','M','E'};

volatile bool co2Ready = false;
volatile int co2Concentration = 0;

unsigned long lastBmeSend = 0;
unsigned long lastCo2Send = 0;

void setup()
{
    char hcAnswerArray[9] = {0};
    pinMode(HC12_SET_PIN, INPUT);
    // Start usb debug output
    Serial.begin(115200);
    // Start COM with other pico
    Serial1.setRX(UART0_RX);
    Serial1.setTX(UART0_TX);
    Serial1.begin(115200);
    // Start COM with HC12
    Serial2.setRX(HC12_RX);
    Serial2.setTX(HC12_TX);
    Serial2.begin(9600);

    if (!bme.begin(0x76))
    {
        Serial.println("No sensor found.");
    }
    else
    {
        Serial.println("Found bme.");
    }

    digitalWrite(HC12_SET_PIN, OUTPUT);

    // Set Baudrate to 9600
    delay(100);
    Serial2.print("AT+B9600");
    if (Serial2.available())
    {
        Serial2.readBytes(hcAnswerArray, 8);
    }
    hcAnswerArray[8] = '\0';
    Serial.println(hcAnswerArray);

    // Set Channel to 1
    delay(100);
    Serial2.print("AT+C001");
    if (Serial2.available())
    {
        Serial2.readBytes(hcAnswerArray, 8);
    }
    hcAnswerArray[8] = '\0';
    Serial.println(hcAnswerArray);

    // Set transmission mode to 3
    delay(100);
    Serial2.print("AT+FU3");
    if (Serial2.available())
    {
        Serial2.readBytes(hcAnswerArray, 8);
    }
    hcAnswerArray[5] = '\0';
    Serial.println(hcAnswerArray);

    // Set Power to 8dBm
    delay(100);
    Serial2.print("AT+P4");
    if (Serial2.available())
    {
        Serial2.readBytes(hcAnswerArray, 8);
    }
    hcAnswerArray[5] = '\0';
    Serial.println(hcAnswerArray);
    delay(100);

    // Set data transmission to 8 bits + odd parity + 1 stop bit
    Serial2.print("AT+U8N1");
    if (Serial2.available())
    {
        Serial2.readBytes(hcAnswerArray, 8);
    }
    hcAnswerArray[5] = '\0';
    Serial.println(hcAnswerArray);

    // Disable Setting Mode in HC-12
    digitalWrite(HC12_SET_PIN, INPUT);
}

void setup1()
{
    // Define pins for meteo decoder
    pinMode(METEO_DATA_IN, OUTPUT);
    pinMode(METEO_DATA_OUT, INPUT);
    pinMode(METEO_CLK_IN, OUTPUT);
    pinMode(METEO_CLK_OUT, INPUT);
    pinMode(METEO_RDY, INPUT);
    digitalWrite(METEO_DATA_IN, LOW);
    digitalWrite(METEO_CLK_IN, LOW);
}

void loop()
{
    static char readVal = 0;
    static int dataTypeCounter = 0;

    if (Serial.available() == 4)
    {
        char buf[5];
        for(int i = 0; i < 4; i++)
        {
            Serial.readBytes(buf, 4);
        }
        buf[4] = '\0';
        Serial.print("Received: ");
        Serial.println(buf);
        MeteoRawSendBuf testBuffer;
        testBuffer.data.packet1 = 0x1d88;
        testBuffer.data.packet2 = 0x3453;
        testBuffer.data.packet3 = 0x32e4;
        testBuffer.data.minute = 0x40;
        testBuffer.data.hour = 0;
        testBuffer.data.date = 0xe8;
        testBuffer.data.month = 9;
        testBuffer.data.dayInWeek = 0x7;
        testBuffer.data.year = 0xc4;
        Serial.println(testBuffer.data.packet1, BIN);
        if (meteo.getNewData(testBuffer))
        {
            Serial.println("Meteo Packet accepted");
        }
        else
        {
            Serial.println("Meteo packet not accepted");
        }
    }
    
    while(Serial2.available())
    {
        readVal = Serial2.read();
        if (readVal == 'M' && dataTypeCounter == 0)
        {
            dataTypeCounter++;
        }
        else if (readVal == 'T' && dataTypeCounter == 1)
        {
            dataTypeCounter++;
        }
        else if (readVal == 'E' && dataTypeCounter == 2)
        {
            dataTypeCounter++;
        }
        else if (readVal == 'O' && dataTypeCounter == 3)
        {
            MeteoRawSendBuf buffer;
            for(int i = 0; i < sizeof(buffer); i++)
            {
                buffer.buf[i] = Serial2.read();
            }
            meteo.getNewData(buffer);
            Serial.println("Got new Meteo Data");
            
            dataTypeCounter = 0;
        }
        else
        {
            if (dataTypeCounter == 1)
            {
                Serial1.write("M");
                Serial.println("Wrote back M");
            }
            else if (dataTypeCounter == 2)
            {
                Serial1.write("MT");
                Serial.println("Wrote back MT");
            }
            else if (dataTypeCounter == 3)
            {
                Serial1.write("MTE");
                Serial.println("Wrote back MTE");
            }
            dataTypeCounter = 0;
            Serial1.write(readVal);
        }
    }
    if (meteo.isMeteoReady())
    {
        MeteoDecodedSendBuf convertedBuffer = meteo.getConvertedBuffer();
        Serial1.write((const uint8_t*)convertedBuffer.buf, sizeof(convertedBuffer));
        Serial.println("Transmitted decoded meteo data");
    }
    if (((millis() - lastCo2Send) > CO2_SEND_FREQUENCY_MILLIS) && co2Ready)
    {
        co2Buffer.data.concentration = co2Concentration;
        Serial1.write((const uint8_t*)co2Buffer.buf, sizeof(co2Buffer));
        lastCo2Send = millis();
        Serial.print("Sent Co2 data. Concentration is: ");
        Serial.println(co2Buffer.data.concentration);
    }
    if ((millis() - lastBmeSend) > BME_SEND_FREQUENCY_MILLIS)
    {
        iBmeBuffer.data.temp = bme.readTemperature();
        iBmeBuffer.data.humidity = bme.readHumidity();
        iBmeBuffer.data.pressure = bme.readPressure() / 100.0;
        Serial1.write((const uint8_t*)iBmeBuffer.buf, sizeof(iBmeBuffer));
        lastBmeSend = millis();
        Serial.println("Set BME Data. Values are: Temp, Hum, Press: ");
        Serial.println(iBmeBuffer.data.temp);
        Serial.println(iBmeBuffer.data.humidity);
        Serial.println(iBmeBuffer.data.pressure);
    }
}

void loop1()
{
    if (!co2Ready)
    {
        co2Ready = co2.isReady();
    }
    else
    {
        co2Concentration = co2.readCO2PWM();
        Serial.print("Got new CO2 value: ");
        Serial.println(co2Concentration);
    }

    if (meteo.isNewMeteo())
    {
        if (!digitalRead(METEO_RDY))
        {
            meteo.decode();
            Serial.println("Just finished decoding");
        }
        else
        {
            Serial.println("Meteo decoder is not ready yet...");
        }
    }
}
