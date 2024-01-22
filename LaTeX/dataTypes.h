// Aktuelle Zeit
typedef struct __attribute__ ((__packed__)){
    const char dataType[4];
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekDay;
    uint8_t checksum;
} TimeStruct;

typedef union {
    char buf[sizeof(TimeStruct)];
    TimeStruct data;
} TimeSendBuf;

// Dekodierte Wetterdaten
typedef struct __attribute__ ((__packed__)){
    const char dataType[4];
    uint32_t meteoData; 
    uint8_t checksum;
} MeteoDecodedStruct;

typedef union {
    char buf[sizeof(MeteoDecodedStruct)];
    MeteoDecodedStruct data;
} MeteoDecodedSendBuf;

// Sensordaten vom BME280
typedef struct __attribute__ ((__packed__)){
    const char dataType[4];
    float temperature;
    float humidity;
    float pressure;
    uint8_t checksum;
} BMEStruct;

typedef union {
    char buf[sizeof(BMEStruct)];
    BMEStruct data;
} BMESendBuf;

// CO2 Konzentration
typedef struct __attribute__ ((__packed__)){
    const char dataType[4];
    int concentration;
} CO2Struct;

typedef union {
    char buf[sizeof(CO2Struct)];
    CO2Struct data;
} CO2SendBuf;