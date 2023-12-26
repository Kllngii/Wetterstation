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

#define READ_TIMEOUT    100 // Timeout value after which the uC stops polling for Serial data from HC12

Adafruit_BME280 bme;    // BME280, read by core0
CO2 co2(CO2_PWM_PIN, MHZ19C, CO2::RANGE_5K);
Meteo meteo;
CO2SendBuf co2Buffer = {'C','O','2','.'};
BMESendBuf iBmeBuffer = {'I','B','M','E'};

BMESendBuf eBmeBuffer;
MeteoRawReceiveBuf meteoReceiveBuffer;
TimeSendBuf timeBuffer;

bool bmeAvailable = true;

volatile bool co2Ready = false;
volatile int co2Concentration = 0;

unsigned long lastBmeSend = 0;
unsigned long lastCo2Send = 0;

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

    // Init BME280
    if (!bme.begin(0x76))
    {
        bmeAvailable = false;
        Serial.println("No sensor found.");
    }
    else
    {
        Serial.println("Found bme.");
    }

    pinMode(HC12_SET_PIN, OUTPUT);

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

    // Set Power to 14dBm
    delay(100);
    Serial2.print("AT+P6");
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
    pinMode(HC12_SET_PIN, INPUT);
}

void setup1()
{
    // Initialize Pins for Meteodecoder
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
    // Decode testpacket with meteodata
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
        bool packetValid = true;
        char receiveBuffer[100] = {'\0'};
        char header[5] = {'\0'};
        int readIndex = 0;
        int readingSize;

        // Isolate header of packet
        for(int i = 0; i < 4; i++)
        {
            receiveBuffer[readIndex++] = (char)Serial2.read();
            header[i] = receiveBuffer[readIndex - 1];
            delay(5);
        }
        String headerString(header);

        // Set datalength to be read
        if (headerString.equals("EBME"))
        {
            readingSize = sizeof(eBmeBuffer) - 4;
        }
        else if (headerString.equals("TIME"))
        {
            readingSize = sizeof(timeBuffer) - 4;
        }
        else if (headerString.equals("MTEO"))
        {
            readingSize = sizeof(meteoReceiveBuffer) - 4;
        }
        else
        {
            Serial.println("Received invalid message");
            // Clear buffer and print data if there were invalid ones
            do
            {
                Serial.print((char)Serial2.read());
                delay(1);
            } while (Serial2.available());
        }

        bool readTimeout = false;
        // Read rest of datapacket
        for(int i = 0; i < readingSize; i++)
        {
            // Read byte
            if (Serial2.available())
            {
                receiveBuffer[readIndex++] = (char)Serial2.read();
                delay(2);
            }
            else
            {
                // Wait until data are ready or Timeout triggered
                int readTime = millis();
                while(!Serial2.available())
                {
                    if ((millis() - readTime) > READ_TIMEOUT)
                    {
                        readTimeout = true;
                        break;
                    }
                    delay(10);
                }
            }

            // Stop reading if timeout appeared
            if (readTimeout)
            {
                break;
            }
        }
        crc.restart();

        if (headerString.equals("EBME"))
        {
            // Copy data
            memcpy(eBmeBuffer.buf, receiveBuffer, sizeof(eBmeBuffer));  
            // Check if checksums match        
            crc.add((const uint8_t*)eBmeBuffer.buf, sizeof(eBmeBuffer));
            if (crc.calc() != 0x00)
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
            // Copy data
            memcpy(timeBuffer.buf, receiveBuffer, sizeof(timeBuffer));
            // Check if checksums match
            crc.add((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer));
            if (crc.calc() != 0x00)
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
                Serial1.write((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer) - 1);
            }
        }
        else if (headerString.equals("MTEO"))
        {
            // Copy data
            memcpy(meteoReceiveBuffer.buf, receiveBuffer, sizeof(meteoReceiveBuffer));
            // Check if checksums match
            crc.add((const uint8_t*)meteoReceiveBuffer.buf, sizeof(meteoReceiveBuffer));
            if (crc.calc() != 0x00)
            {
                Serial.println("Checksum invalud on Meteopacket. Initiate retransmission");
                packetValid = false;
            }
            else
            {
                MeteoRawSendBuf rawBuffer;
                memcpy(rawBuffer.buf, meteoReceiveBuffer.buf + 4, sizeof(rawBuffer));
                Serial.println("Meteodata: ");
                Serial.print("Packet1: ");
                Serial.println(rawBuffer.data.packet1, BIN);
                Serial.print("Packet2: ");
                Serial.println(rawBuffer.data.packet2, BIN);
                Serial.print("Packet3: ");
                Serial.println(rawBuffer.data.packet3, BIN);
                Serial.print("Minute: ");
                Serial.println(rawBuffer.data.minute, BIN);
                Serial.print("Hour: ");
                Serial.println(rawBuffer.data.hour, BIN);
                Serial.print("Day: ");
                Serial.println(rawBuffer.data.date, BIN);
                Serial.print("Month: ");
                Serial.println(rawBuffer.data.month, BIN);
                Serial.print("Weekday: ");
                Serial.println(rawBuffer.data.dayInWeek, BIN);
                Serial.print("Year: ");
                Serial.println(rawBuffer.data.year, BIN);
                meteo.getNewData(rawBuffer);
            }
        }
        else
        {
            packetValid = false;
        }

        // Send errormessage to outdoor module to initate retransmission
        if (!packetValid)
        {
            Serial2.print("ERRORERRORERROR");
        }
    }

    // Send meteodata if there are any new
    if (meteo.isMeteoReady())
    {
        MeteoDecodedSendBuf convertedBuffer = meteo.getConvertedBuffer();
        Serial1.write((const uint8_t*)convertedBuffer.buf, sizeof(convertedBuffer));
        Serial.println("Transmitted decoded meteo data");
    }
    // Send CO2 data if sensor is ready
    if (((millis() - lastCo2Send) > CO2_SEND_FREQUENCY_MILLIS) && co2Ready)
    {
        co2Buffer.data.concentration = co2Concentration;
        Serial1.write((const uint8_t*)co2Buffer.buf, sizeof(co2Buffer));
        lastCo2Send = millis();
        Serial.print("Sent Co2 data. Concentration is: ");
        Serial.println(co2Buffer.data.concentration);
    }
    // Send BME data if sensor is available
    if (((millis() - lastBmeSend) > BME_SEND_FREQUENCY_MILLIS) && bmeAvailable)
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
    // Wait until sensor is heated up
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
        // Read CO2 concentration using PWM
        co2Concentration = co2.readCO2PWM();
        //Serial.print("Got new CO2 value: ");
        //Serial.println(co2Concentration);
    }

    if (meteo.isNewMeteo())
    {
        // Decode data if HKW581 is heated up and ready to decode
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
