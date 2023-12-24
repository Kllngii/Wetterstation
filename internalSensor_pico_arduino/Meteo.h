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
    uint8_t checksum;
} MeteoRawStruct;

typedef union {
    char buf[sizeof(MeteoRawStruct)];
    MeteoRawStruct data;
} MeteoRawSendBuf;

typedef struct {
    char dataType[4];
    MeteoRawStruct data;
} MeteoRawReceiveStruct;

typedef union {
    char buf[sizeof(MeteoRawReceiveStruct)];
    MeteoRawReceiveStruct data;
} MeteoRawReceiveBuf;

typedef struct {
    const char dataType[4];
    uint32_t meteoData;
} MeteoDecodedStruct;

typedef union {
    char buf[sizeof(MeteoDecodedStruct)];
    MeteoDecodedStruct data;
} MeteoDecodedSendBuf;

class Meteo {
    public:
        Meteo();
        ~Meteo();
        bool getNewData(MeteoRawSendBuf rawBuffer);
        void decode();
        bool isMeteoReady();
        bool isNewMeteo();
        MeteoDecodedSendBuf getConvertedBuffer();

    private:
        MeteoRawSendBuf rawBuffer;
        MeteoDecodedSendBuf convertedBuffer = {'M','T','E','O'};
        volatile bool newMeteoData;
        volatile bool meteoDataReady;

        void writeToMeteo(uint8_t bit);
};

#endif /* _METEO_H_ */
