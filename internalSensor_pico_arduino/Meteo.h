/* 
 *  Kind of class for decoding the meteo data. 
 *  The intention of using a class is the following:
 *  - If I put in in another file, the "main" looks cleaner
 *  - Its C++ so I use a class...
 */

 #ifndef _METEO_H_
 #define _METEO_H_

 #include <cstdint>

// Pindefs running on core1
#define METEO_DATA_IN   16
#define METEO_DATA_OUT  17
#define METEO_CLK_IN    18
#define METEO_CLK_OUT   19
#define METEO_RDY       20

typedef struct {
    uint16_t packet1;
    uint16_t packet2;
    uint16_t packet3;
    uint8_t minute;
    uint8_t hour;
    uint8_t date;
    uint8_t month; // Only 5 bits used
    uint8_t dayInWeek;  // Only 3 bits used
    uint8_t year;
} MeteoPacketStruct;

typedef union {
    char rawBuffer[sizeof(MeteoPacketStruct)];
    MeteoPacketStruct data;
} MeteoRawBuffer;

typedef struct {
    const char dataType[4];
    uint8_t day_weather;
    uint8_t night_weather;
    uint8_t extrema;
    uint8_t rainfall;
    uint8_t anomaly;
    uint8_t temperature;
} MeteoConvertedStruct;

typedef union {
    char sendArr[sizeof(MeteoConvertedStruct)];
    MeteoConvertedStruct data;
} MeteoConvertedBuffer;

class Meteo {
    public:
        Meteo();
        ~Meteo();
        bool getNewData(MeteoRawBuffer rawBuffer);
        void decode();
        bool isMeteoReady();
        bool isNewMeteo();
        MeteoConvertedBuffer getConvertedBuffer();

    private:
        MeteoRawBuffer rawBuffer;
        MeteoConvertedBuffer convertedBuffer = {'M','T','E','O'};
        volatile bool newMeteoData;
        volatile bool meteoDataReady;

        void writeToMeteo(uint8_t bit);
};

#endif /* _METEO_H_ */
