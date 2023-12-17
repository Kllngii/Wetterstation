#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>
#include "Meteo.h"

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
