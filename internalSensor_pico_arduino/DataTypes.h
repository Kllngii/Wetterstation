#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>
#include "Meteo.h"

typedef struct {
    char dataType[4];
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t checksum;
} TimeStruct;

typedef union {
    char buf[sizeof(TimeStruct)];
    TimeStruct data;
} TimeSendBuf;

typedef struct {
    char dataType[4];
    float temp;
    float humidity;
    float pressure;
    uint8_t checksum;
} BMEStruct;

typedef union {
    char buf[sizeof(BMEStruct)];
    BMEStruct data;
} BMESendBuf;

typedef struct {
    const char dataType[4];
    int concentration;
} CO2Struct;

typedef union _co2Buf {
    char buf[sizeof(CO2Struct)];
    CO2Struct data;
} CO2SendBuf;

#endif /* DATATYPES_H */
