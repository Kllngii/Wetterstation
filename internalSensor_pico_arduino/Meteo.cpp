#include "Meteo.h"
#include <Arduino.h>

Meteo::Meteo()
{
    newMeteoData = false;
    meteoDataReady = false;
}

Meteo::~Meteo()
{

}

bool Meteo::getNewData(MeteoRawBuffer buffer)
{
    if (!meteoDataReady)
    {
        rawBuffer = buffer;
        newMeteoData = true;
        return true;
    }
    return false;
}

void Meteo::decode()
{
    int meteoBit = 0;
    uint32_t bitPattern;
    uint32_t meteoPattern;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 14; j++)
        {
            if (i == 0)
            {
                writeToMeteo((rawBuffer.data.packet1 & (1 << j)) >> j);
            }
            else if (i == 1)
            {
                writeToMeteo((rawBuffer.data.packet2 & (1 << j)) >> j);
            }
            else if (i == 2)
            {
                writeToMeteo((rawBuffer.data.packet3 & (1 << j)) >> j);
            }
        }
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawBuffer.data.minute & (1 << i)) >> i);
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawBuffer.data.hour & (1 << i)) >> i);
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawBuffer.data.date & (1 << i)) >> i);
    }
    for (int i = 0; i < 5; i++)
    {
        writeToMeteo((rawBuffer.data.month & (1 << i)) >> i);
    }
    for (int i = 0; i < 3; i++)
    {
        writeToMeteo((rawBuffer.data.dayInWeek & (1 << i)) >> i);
    }
    for (int i = 0; i < 8; i++)
    {
        writeToMeteo((rawBuffer.data.year & (1 << i)) >> i);
    }

    while(!digitalRead(METEO_CLK_OUT));
    bitPattern = 0;
    meteoPattern = 0;

    for (int i = 0; i < 24; i++)
    {
        while (!digitalRead(METEO_CLK_OUT));
        meteoBit = digitalRead(METEO_DATA_OUT);
        bitPattern += meteoBit;
        bitPattern <<= 1;
        digitalWrite(METEO_CLK_IN, HIGH);
        while (digitalRead(METEO_CLK_OUT));
        digitalWrite(METEO_CLK_IN, LOW);
    }
    for (int i = 0; i < 25; i++)
    {
        meteoBit = (bitPattern >> i) & 0x01;
        meteoPattern += meteoBit;
        meteoPattern <<= 1;
    }
    meteoPattern >>= 1;
    Serial.print("Decoded weather: ");
    Serial.println(meteoPattern, BIN);



    newMeteoData = false;
    meteoDataReady = true;
}

bool Meteo::isMeteoReady()
{
    return meteoDataReady;
}

bool Meteo::isNewMeteo()
{
    return newMeteoData;
}

MeteoConvertedBuffer Meteo::getConvertedBuffer()
{
    static MeteoConvertedBuffer emptyBuf = {'E','M','T','Y'};
    if (meteoDataReady)
    {
        meteoDataReady = false;
        return convertedBuffer;
    }
    else
    {
        return emptyBuf;
    }
}

void Meteo::writeToMeteo(uint8_t bit)
{
    digitalWrite(METEO_DATA_IN, bit & 0x01);
    digitalWrite(METEO_CLK_IN, HIGH);
    while (!digitalRead(METEO_CLK_OUT))
    {
        delay(1);   // To reset watchdog timer
    }
    digitalWrite(METEO_CLK_IN, LOW);
    while(digitalRead(METEO_CLK_OUT))
    {
        delay(1);
    }
}
