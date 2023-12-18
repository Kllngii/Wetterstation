#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>

typedef struct {
    const char dataType[4];
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
} TimeStruct;

typedef union {
    char buf[sizeof(TimeStruct)];
    TimeStruct data;
} TimeSendBuf;

typedef struct {
    char dataType[4];
    uint16_t packet1;
    uint16_t packet2;
    uint16_t packet3;
    uint8_t minute;
    uint8_t hour;
    uint8_t date;
    uint8_t month; // Only 5 bits used
    uint8_t dayInWeek;  // Only 3 bits used
    uint8_t year;
} MeteoRawStruct;

typedef union {
    char buf[sizeof(MeteoRawStruct)];
    MeteoRawStruct data;
} MeteoRawSendBuf;

typedef struct {
    const char dataType[4];
    float temp;
    float humidity;
    float pressure;
} BMEStruct;

typedef union {
    char buf[sizeof(BMEStruct)];
    BMEStruct data;
} BMESendBuf;

#endif /* DATATYPES_H */
