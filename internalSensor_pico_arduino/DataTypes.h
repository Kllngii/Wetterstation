#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>
#include "Meteo.h"

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

typedef struct {
    const char dataType[4];
    int concentration;
} CO2Struct;

typedef union _co2Buf {
    char buf[sizeof(CO2Struct)];
    CO2Struct data;
} CO2SendBuf;

#endif /* DATATYPES_H */
