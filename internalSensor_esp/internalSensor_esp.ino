#include <stdio.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Mhz19.h>
#include "DataTypes.h"

#define RX_CO2      12  // D6
#define TX_CO2      13  // D7
#define RX_HC12     14  // D5
#define TX_HC12     2   // D4
#define SET_PIN     0   // D3
#define RX_METEO    9   // SDD2
#define TX_METEO    10  // SDD3

#define BME_SEND_FREQUENCY_MILLIS   30000
#define CO2_SEND_FREQUENCY_MILLIS   15000

Adafruit_BME280 bme;
Mhz19 MHZ;
SoftwareSerial CO2Serial(RX_CO2, TX_CO2);
//MHZ CO2(RX_CO2, TX_CO2, MHZ19C);
SoftwareSerial HC12(RX_HC12, TX_HC12);
SoftwareSerial MeteoDecoder(RX_METEO, TX_METEO);

timeBuf timePacket = {'T','I','M','E'};
meteoRawBuffer rawMeteoPacket = {'M','T','E','O'};
meteoConvertedBuffer convertedMeteoPacket = {'M','T','E','O'};
bmeBuf intBmePacket = {'I','B','M','E'};
bmeBuf extBmePacket = {'E','B','M','E'};
co2Buf co2Packet = {'C','O','2','.'};

bool preHeatingFinished = false;
unsigned long lastBmeSend = 0;
unsigned long lastCo2Send = 0;
char dataType[4];
uint8_t dataTypeCounter;

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
    while(HC12.available() < 8);
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
    while(HC12.available() < 7);
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
    if (HC12.available()){
        while(HC12.available())
        {
            Serial.print(HC12.read());
        }
        Serial.println("");
    }
    if (!preHeatingFinished)
    {
      preHeatingFinished = MHZ.isReady();
    }
    if ((millis() - lastBmeSend) > BME_SEND_FREQUENCY_MILLIS)
    {
        intBmePacket.sendStruct.temp = bme.readTemperature();
        intBmePacket.sendStruct.humidity = bme.readHumidity();
        intBmePacket.sendStruct.pressure = bme.readPressure();
        Serial.println(intBmePacket.sendArr);
        lastBmeSend = millis();
    }
    if (((millis() - lastCo2Send) > CO2_SEND_FREQUENCY_MILLIS) && preHeatingFinished)
    {
        co2Packet.sendStruct.concentration = MHZ.getCarbonDioxide();
        Serial.println(co2Packet.sendArr);
        lastCo2Send = millis();
    }
}
