#include <stdio.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Mhz19.h>
#include "DataTypes.h"

#define RX_CO2      2  // D2
#define TX_CO2      3  // D3
#define RX_HC12     5  // D5
#define TX_HC12     4   // D4
#define SET_PIN     6   // D6
#define METEO_DATA_IN    7   // D7
#define METEO_DATA_OUT    11  // D11
#define METEO_CLK_IN   9   // D9
#define METEO_CLK_OUT   8  // D8
#define METEO_RDY   10   // D10

#define BME_SEND_FREQUENCY_MILLIS   30000
#define CO2_SEND_FREQUENCY_MILLIS   15000

Adafruit_BME280 bme;
Mhz19 MHZ;
SoftwareSerial CO2Serial(RX_CO2, TX_CO2);
//MHZ CO2(RX_CO2, TX_CO2, MHZ19C);
SoftwareSerial HC12(RX_HC12, TX_HC12);

meteoRawBuffer rawMeteoPacket;
meteoConvertedBuffer convertedMeteoPacket = {'M','T','E','O'};
bmeBuf intBmePacket = {'I','B','M','E'};
co2Buf co2Packet = {'C','O','2','.'};

bool preHeatingFinished = false;
unsigned long lastBmeSend = 0;
unsigned long lastCo2Send = 0;
uint8_t dataTypeCounter;
char readVal;

int watchdogReset = 0;

// Meteo definitions
bool meteoDecoderReady = false;
uint32_t bitPattern;
uint32_t meteoPattern;
int meteobit;

void setup()
{
    char rxArray[9] = {0};
    pinMode(SET_PIN, INPUT);
    CO2Serial.begin(9600);
    MHZ.begin(&CO2Serial);
    MHZ.setMeasuringRange(Mhz19MeasuringRange::Ppm_5000);
    MHZ.enableAutoBaseCalibration();
    Serial.begin(115200);
    HC12.begin(9600);

    Serial.println("Ping");
    // Define pins for meteo decoder
    pinMode(METEO_DATA_IN, OUTPUT);
    pinMode(METEO_DATA_OUT, INPUT);
    pinMode(METEO_CLK_IN, OUTPUT);
    pinMode(METEO_CLK_OUT, INPUT);
    pinMode(METEO_RDY, INPUT);
    digitalWrite(METEO_DATA_IN, LOW);
    digitalWrite(METEO_CLK_IN, LOW);
    Serial.println("Pong");

    // Start i2c communication
    if(!bme.begin(0x76))
    {
        Serial.println("No sensor found.");
    }

    // Enable Setting Mode in HC-12
    pinMode(SET_PIN, OUTPUT);
    digitalWrite(SET_PIN, LOW);

    // Set Baudrate to 9600
    delay(100);
    HC12.print("AT+B9600");
    // while(HC12.available() < 8);
    for(int i = 0; i < 8; i++)
    {
        rxArray[i] = HC12.read();
    }
    rxArray[8] = '\0';
    Serial.println(rxArray);

    // Set Channel to 1
    delay(100);
    HC12.print("AT+C001");
    while(HC12.available() < 8);
    for(int i = 0; i < 8; i++)
    {
        rxArray[i] = HC12.read();
    }
    rxArray[8] = '\0';
    Serial.println(rxArray);

    // Set transmission mode to 3
    delay(100);
    HC12.print("AT+FU3");
    while(HC12.available() < 5);
    for(int i = 0; i < 5; i++)
    {
        rxArray[i] = HC12.read();
    }
    rxArray[5] = '\0';
    Serial.println(rxArray);

    // Set Power to 8dBm
    delay(100);
    HC12.print("AT+P4");
    while(HC12.available() < 5);
    for(int i = 0; i < 5; i++)
    {
        rxArray[i] = HC12.read();
    }
    rxArray[5] = '\0';
    Serial.println(rxArray);
    delay(100);

    // Set data transmission to 8 bits + odd parity + 1 stop bit
    HC12.print("AT+U8N1");
    //while(HC12.available() < 7);
    for(int i = 0; i < 7; i++)
    {
        rxArray[i] = HC12.read();
    }
    rxArray[7] = '\0';
    Serial.println(rxArray);

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);
}

void loop()
{
    if (!meteoDecoderReady && !digitalRead(METEO_RDY))
    {
        meteoDecoderReady = true;
    }
    while(HC12.available())
    {
        readVal = HC12.read();
        if (readVal == 'M' && dataTypeCounter == 0)
        {
            dataTypeCounter++;
        }
        else if (readVal == 'T' && dataTypeCounter == 1)
        {
            dataTypeCounter++;
        }
        else if (readVal == 'E' && dataTypeCounter == 3)
        {
            dataTypeCounter++;
        }
        else if (readVal == 'O' && dataTypeCounter == 4)
        {
            for (int i = 0; i < sizeof(meteoRawBuffer); i++)
            {
                rawMeteoPacket.rawBuffer[i] = HC12.read();
            }
            dataTypeCounter = 0;
            decodeMeteo();
        }
        else 
        {
            dataTypeCounter = 0;
            Serial.write(readVal);
        }
    }
    if (!preHeatingFinished)
    {
      preHeatingFinished = MHZ.isReady();
    }
    if ((millis() - lastBmeSend) > BME_SEND_FREQUENCY_MILLIS)
    {
        intBmePacket.sendStruct.temp = bme.readTemperature();
        intBmePacket.sendStruct.humidity = bme.readHumidity();
        intBmePacket.sendStruct.pressure = bme.readPressure() / 100.0;
        Serial.write((const uint8_t*)intBmePacket.sendArr, sizeof(intBmePacket));
        lastBmeSend = millis();
    }
    if (((millis() - lastCo2Send) > CO2_SEND_FREQUENCY_MILLIS) && preHeatingFinished)
    {
        co2Packet.sendStruct.concentration = MHZ.getCarbonDioxide();
        Serial.write((const uint8_t*)co2Packet.sendArr, sizeof(co2Packet));
        lastCo2Send = millis();
    }
}

void decodeMeteo()
{

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 14; j++)
        {
            if (i == 0)
            {
                writeToMeteo((rawMeteoPacket.data.packet1 & (1 << j)) >> j);
            }
            else if (i == 1)
            {
                writeToMeteo((rawMeteoPacket.data.packet2 & (1 << j)) >> j);
            }
            else if (i == 2)
            {
                writeToMeteo((rawMeteoPacket.data.packet3 & (1 << j)) >> j);
            }
        }
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawMeteoPacket.data.minute & (1 << i)) >> i);
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawMeteoPacket.data.hour & (1 << i)) >> i);
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawMeteoPacket.data.date & (1 << i)) >> i);
    }
    for (int i = 0; i < 5; i++)
    {
        writeToMeteo((rawMeteoPacket.data.month & (1 << i)) >> i);
    }
    for (int i = 0; i < 3; i++)
    {
        writeToMeteo((rawMeteoPacket.data.dayInWeek & (1 << i)) >> i);
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawMeteoPacket.data.year & (1 << i)) >> i);
    }

    while(!digitalRead(METEO_CLK_OUT))
    {
        yield();
    }
    bitPattern = 0;
    meteoPattern = 0;

    for (int i = 0; i < 24; i++)
    {
        while (!digitalRead(METEO_CLK_OUT))
        {
            watchdogReset++;
        }
        meteobit = digitalRead(METEO_DATA_OUT);
        bitPattern += meteobit;
        bitPattern <<= 1;
        digitalWrite(METEO_CLK_IN, HIGH);
        while (digitalRead(METEO_CLK_OUT))
        {
            yield();
        }
        digitalWrite(METEO_CLK_IN, LOW);
    }
    for (int i = 0; i < 25; i++)
    {
        meteobit = (bitPattern >> i) & 0x01;
        meteoPattern += meteobit;
        meteoPattern <<= 1;
    }
    meteoPattern >>= 1;
    Serial.print("Decoded weather: ");
    Serial.println(meteoPattern, BIN);
}

void writeToMeteo(byte bit)
{
    digitalWrite(METEO_DATA_IN, bit & 0x01);
    digitalWrite(METEO_CLK_IN, HIGH);
    while (!digitalRead(METEO_CLK_OUT))
    {
        yield();   // To reset watchdog timer
    }
    digitalWrite(METEO_CLK_IN, LOW);
    while(digitalRead(METEO_CLK_OUT))
    {
        yield();
    }
}