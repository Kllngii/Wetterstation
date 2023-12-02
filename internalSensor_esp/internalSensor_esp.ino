#include <stdint.h>
#include <stdio.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Mhz19.h>

#define RX_CO2      12  // D6
#define TX_CO2      13  // D7
#define RX_HC12     14  // D5
#define TX_HC12     2   // D4
#define SET_PIN     0   // D3
#define RX_METEO    9   // SDD2
#define TX_METEO    10  // SDD3

typedef struct _timeStruct {
    const char dataType[4];
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
} timeStruct;

typedef union _timeBuf {
    char sendArr[sizeof(timeStruct)];
    timeStruct sendStruct;
} timeBuf;

typedef struct _meteoPacketStruct {
    uint16_t packet1;
    uint16_t packet2;
    uint16_t packet3;
} meteoPacketStruct;
typedef struct _meteoRawStruct {
    const char dataType[4];
    meteoPacketStruct packetData;
} meteoRawStruct;

typedef union _meteoRawBuffer {
    char sendArr[sizeof(meteoRawStruct)];
    meteoRawStruct data;
} meteoRawBuffer;

typedef struct _meteoConvertedStruct {
    uint8_t day_weather;
    uint8_t night_weather;
    uint8_t extrema;
    uint8_t rainfall;
    uint8_t anomaly;
    uint8_t temperature;
} meteoConvertedStruct;

typedef union _meteoConvertedBuffer {
    char sendArr[sizeof(meteoConvertedStruct)];
    meteoConvertedStruct data;
} meteoConvertedBuffer;

typedef struct _bmeStruct {
    const char dataType[4];
    float temp;
    float humidity;
    float pressure;
} bmeStruct;

typedef union _bmeBuf {
    char sendArr[sizeof(bmeStruct)];
    bmeStruct sendStruct;
} bmeBuf;

Adafruit_BME280 bme;
Mhz19 MHZ;
SoftwareSerial CO2Serial(RX_CO2, TX_CO2);
//MHZ CO2(RX_CO2, TX_CO2, MHZ19C);
SoftwareSerial HC12(RX_HC12, TX_HC12);
SoftwareSerial MeteoDecoder(RX_METEO, TX_METEO);

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
    HC12.print("AT+U8O1");
    while(HC12.available() < 7);
    for(int i = 0; i < 7; i++)
    {
        rxArray[i] = HC12.read();
    }
    rxArray[7] = '\0';
    Serial.println(rxArray);

    // Disable Setting Mode in HC-12
    pinMode(SET_PIN, INPUT);

    while(!MHZ.isReady())
    {
      Serial.println("Still preheating...");
      delay(5000);
    }

}

void loop()
{
    int co2 = MHZ.getCarbonDioxide();
    Serial.println(co2);
    delay(1000);
}
