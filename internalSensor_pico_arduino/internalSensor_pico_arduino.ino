#include <stdio.h>
#include <cstring>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <CRC.h>
#include <CRC8.h>
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

#define READ_TIMEOUT    500

Adafruit_BME280 bme;    // BME280, read by core0
CO2 co2(CO2_PWM_PIN, MHZ19C, CO2::RANGE_5K);
Meteo meteo;
CO2SendBuf co2Buffer = {'C','O','2','.'};
BMESendBuf iBmeBuffer = {'I','B','M','E'};

BMESendBuf eBmeBuffer;
MeteoRawReceiveBuf meteoReceiveBuffer;
TimeSendBuf timeBuffer;

volatile bool co2Ready = false;
volatile int co2Concentration = 0;

unsigned long lastBmeSend = 0;
unsigned long lastCo2Send = 0;

char receiveBuffer[100] = {0};
int readTime = 0;
int readIndex = 0;

bool packetValid = true;

CRC8 crc;

void setup()
{
    char hcAnswerArray[9] = {0};
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

    if (Serial2.available())
    {
        packetValid = true;
        char header[5] = {'\0'};
        readIndex = 0;
        readTime = millis();
        while ((millis() - readTime) < READ_TIMEOUT)
        {
            if (Serial2.available())
            {
                readTime = millis();
                receiveBuffer[readIndex++] = (char)Serial2.read();
            }
        }
        for(int i = 0; i < 4; i++)
        {
            header[i] = receiveBuffer[i];
        }
        String headerString(header);
        crc.restart();

        if (headerString.equals("EBME"))
        {
            memcpy(eBmeBuffer.buf, receiveBuffer, sizeof(eBmeBuffer));           
            crc.add((const uint8_t*)eBmeBuffer.buf, sizeof(eBmeBuffer) - 1);
            if (crc.calc() != (uint8_t)eBmeBuffer.data.checksum)
            {
                Serial.println("Checksum invalid on EBME Packet. Initiate retransmission");
                packetValid = false;
            }
            else
            {
                Serial.println("Received valid EBME Packet.");
                Serial.print("Temperature: ");
                Serial.println(eBmeBuffer.data.temp);
                Serial.print("Humidity: ");
                Serial.println(eBmeBuffer.data.humidity);
                Serial.print("Pressure: ");
                Serial.println(eBmeBuffer.data.pressure);
                Serial1.write((const uint8_t*)eBmeBuffer.buf, sizeof(eBmeBuffer) - 1);
            }
        }
        else if (headerString.equals("TIME"))
        {
            memcpy(timeBuffer.buf, receiveBuffer, sizeof(timeBuffer));
            crc.add((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer) - 1);
            if (crc.calc() != timeBuffer.data.checksum)
            {
                Serial.println("Checksum invalid on Time Packet. Initiate retransmission");
                packetValid = false;
            }
            else
            {
                Serial.println("Received valid Time packet.");
                Serial.print("Hour: ");
                Serial.println(timeBuffer.data.hour);
                Serial.print("Minute: ");
                Serial.println(timeBuffer.data.minute);
                Serial.print("Second: ");
                Serial.println(timeBuffer.data.second);
                Serial.print("Year: ");
                Serial.println(timeBuffer.data.year);
                Serial.print("Month: ");
                Serial.println(timeBuffer.data.month);
                Serial.print("Day: ");
                Serial.println(timeBuffer.data.day);
                Serial.write((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer) - 1);
            }
        }
        else if (headerString.equals("MTEO"))
        {
            memcpy(meteoReceiveBuffer.buf, receiveBuffer, sizeof(meteoReceiveBuffer));
            crc.add((const uint8_t*)meteoReceiveBuffer.buf, sizeof(meteoReceiveBuffer) - 1);
            if (crc.calc() != meteoReceiveBuffer.data.data.checksum)
            {
                Serial.println("Checksum invalud on Meteopacket. Initiate retransmission");
                packetValid = false;
            }
            else
            {
                MeteoRawSendBuf rawBuffer;
                memcpy(rawBuffer.buf, meteoReceiveBuffer.buf + 4, sizeof(rawBuffer));
                meteo.getNewData(rawBuffer);
            }
        }
        else
        {
            packetValid = false;
        }

        if (!packetValid)
        {
            Serial2.print("ERRORERRORERROR");
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
        if (co2Ready)
        {
            Serial.println("CO2 is ready!");
        }
    }
    else
    {
        co2Concentration = co2.readCO2PWM();
        //Serial.print("Got new CO2 value: ");
        //Serial.println(co2Concentration);
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
