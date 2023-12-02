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

typedef union _timeSendBuf {
    char timeBuf[sizeof(timeStruct)];
    timeStruct dcfTime;
} timeSendBuf;

typedef struct _meteoStruct {
    const char dataType[4];
    meteoDataBuffer packetData;
} meteoStruct;

typedef union _meteoSendBuf {
    char meteoBuf[sizeof(meteoStruct)];
    meteoStruct data;
} meteoSendBuf;

typedef struct _sensorStruct {
    const char dataType[4];
    float temp;
    float humidity;
    float pressure;
} sensorStruct;

typedef union _sensorSendBuf {
    char sensorBuf[sizeof(sensorStruct)];
    sensorStruct sensor;
} sensorSendBuf;

#endif /* DATATYPES_H */
