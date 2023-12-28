/*
 *  Created by: faschmali
 *
 *  Here are all relevant definitions of structs used for communication
 *  between the microcontrollers. The structs are used as part of a union
 *  to be able to send all elements bytewise. The additional attribute is needed 
 *  to avoid padding bytes that increase the size of the structs and make a checksum
 *  calculation more complicated. I was not able to use a simple #pragma as in the
 *  external Sensor program because the Raspi Pico was crashing...
 */

#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>

typedef struct __attribute__ ((__packed__)){
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

typedef struct __attribute__ ((__packed__)){
    char dataType[4];
    uint32_t meteoData;
    uint8_t checksum;
} MeteoDecodedStruct;

typedef union {
    char buf[sizeof(MeteoDecodedStruct)];
    MeteoDecodedStruct data;
} MeteoDecodedSendBuf;

typedef struct __attribute__ ((__packed__)){
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

typedef struct __attribute__ ((__packed__)){
    const char dataType[4];
    int concentration;
} CO2Struct;

typedef union _co2Buf {
    char buf[sizeof(CO2Struct)];
    CO2Struct data;
} CO2SendBuf;

#endif /* DATATYPES_H */
