/*
 * Created by faschmali
 */

#include "Meteo.h"
#include <Arduino.h>

Meteo::Meteo()
{
    newMeteoData = false;
    meteoDataReady = false;
    convertedBuffer.buf[0] = 'M';
    convertedBuffer.buf[1] = 'T';
    convertedBuffer.buf[2] = 'E';
    convertedBuffer.buf[3] = 'O';
}

Meteo::~Meteo()
{

}

bool Meteo::getNewData(MeteoRawStruct rawBuffer)
{
    if (!meteoDataReady)
    {
        this->rawBuffer = rawBuffer;
        newMeteoData = true;
        return true;
    }
    return false;
}

void Meteo::decode()
{
    int meteoBit = 0;
    uint32_t meteoPattern;
    
    for (int i = 0; i < 3; i++)
    {
        for (int j = 13; j >= 0; j--)
        {
            if (i == 0)
            {
                writeToMeteo((rawBuffer.packet1 & (1 << j)) >> j);
            }
            else if (i == 1)
            {
                writeToMeteo((rawBuffer.packet2 & (1 << j)) >> j);
            }
            else if (i == 2)
            {
                writeToMeteo((rawBuffer.packet3 & (1 << j)) >> j);
            }
        }
    }
    for (int i = 7; i >= 0; i--)
    {
        writeToMeteo((rawBuffer.minute & (1 << i)) >> i);
    }
    for (int i = 7; i >= 0; i--)
    {
        writeToMeteo((rawBuffer.hour & (1 << i)) >> i);
    }
    for (int i = 7; i >= 0; i--)
    {
        writeToMeteo((rawBuffer.date & (1 << i)) >> i);
    }
    for (int i = 4; i >= 0; i--)
    {
        writeToMeteo((rawBuffer.month & (1 << i)) >> i);
    }
    for (int i = 2; i >= 0; i--)
    {
        writeToMeteo((rawBuffer.dayInWeek & (1 << i)) >> i);
    }
    for (int i = 7; i >= 0; i--)
    {
        writeToMeteo((rawBuffer.year & (1 << i)) >> i);
    }

    while(!digitalRead(METEO_CLK_OUT));
    meteoPattern = 0;

    for (int i = 0; i < 24; i++)
    {
        while (!digitalRead(METEO_CLK_OUT));
        meteoBit = digitalRead(METEO_DATA_OUT);
        meteoPattern += meteoBit;
        meteoPattern <<= 1;
        digitalWrite(METEO_CLK_IN, HIGH);
        while (digitalRead(METEO_CLK_OUT));
        digitalWrite(METEO_CLK_IN, LOW);
    }
    meteoPattern >>= 1;
    convertedBuffer.data.meteoData = meteoPattern;
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

MeteoDecodedSendBuf Meteo::getConvertedBuffer()
{
    static MeteoDecodedSendBuf emptyBuf = {'E','M','T','Y'};
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
