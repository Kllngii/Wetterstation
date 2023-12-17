// Aktuelle Zeit
typedef struct {
    const char dataType[4];
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t year;
    uint8_t month;
    uint8_t day;
} TimeStruct;

typedef union {
    char buf[sizeof(TimeStruct)];
    TimeStruct data;
} TimeSendBuf;

// Kodierte Wetterdaten mit Zeitinformationen zum Dekodieren
typedef struct {
    const char dataType[4];
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

// Dekodierte Wetterdaten
typedef struct {
    const char dataType[4];
    uint32_t meteoData; 
} MeteoDecodedStruct;

typedef union {
    char buf[sizeof(MeteoDecodedStruct)];
    MeteoDecodedStruct data;
} MeteoDecodedSendBuf;

// Sensordaten vom BME280
typedef struct {
    const char dataType[4];
    float temperature;
    float humidity;
    float pressure;
} BMEStruct;

typedef union {
    char buf[sizeof(BMEStruct)];
    BMEStruct data;
} BMESendBuf;

// CO2 Konzentration
typedef struct {
    const char dataType[4];
    int concentration;
} CO2Struct;

typedef union {
    char buf[sizeof(CO2Struct)];
    CO2Struct data;
} CO2SendBuf;