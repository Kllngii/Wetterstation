
typedef struct _data {
    // DCF77
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    // Meteotime
    uint8_t meteo_day_weather;
    uint8_t meteo_night_weather;
    uint8_t meteo_extrema;
    uint8_t meteo_rainfall;
    uint8_t meteo_anomaly;
    uint8_t meteo_temperature
    // Co2 internal
    char co2[4];
    // BMP280 internal
    char int_temp[5];
    char int_humidity[4];
    // BME280 external
    char ext_temp[5];
    char ext_humidity[4];
    char ext_pressure[8];
} data;

typedef union _sendBuf{
    char sendBuf[sizeof(data)];
    data d;
} sendBuf;