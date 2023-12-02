#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>

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

typedef struct _co2Struct {
    const char dataType[4];
    int concentration;
} co2Struct;

typedef union _co2Buf {
    char sendArr[sizeof(co2Struct)];
    co2Struct sendStruct;
} co2Buf;

#endif /* DATATYPES_H */
