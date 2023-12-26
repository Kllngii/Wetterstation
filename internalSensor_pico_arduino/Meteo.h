/* 
 *  Created by faschmali
 *  
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

typedef struct __attribute__ ((__packed__)){
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

typedef struct __attribute__ ((__packed__)){
    char dataType[4];
    MeteoRawStruct data;
} MeteoRawReceiveStruct;

typedef union {
    char buf[sizeof(MeteoRawReceiveStruct)];
    MeteoRawReceiveStruct data;
} MeteoRawReceiveBuf;

typedef struct __attribute__ ((__packed__)){
    char dataType[4];
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

        /*
         *  Copies new meteodata to the class member and
         *  allows processing of the data (done by other core)
         */
        bool getNewData(MeteoRawSendBuf rawBuffer);

        /*
         *  Decode the meteotime data. Done by core1
         */
        void decode();

        /*
         *  Returns true if data have been decoded and are ready
         *  to be read. Reading data is possible only once per packet. 
         */
        bool isMeteoReady();

        /*
         *  Returns true if a new packet of meteodata has been stored in class
         *  by using getNewData()
         */
        bool isNewMeteo();

        /*
         *  Returns databuffer with freshly decoded meteodata if there are any. 
         *  Else it returns an empty buffer. Shall be only used after checking with
         *  isMeteoReady() before. 
         */
        MeteoDecodedSendBuf getConvertedBuffer();

    private:
        MeteoRawSendBuf rawBuffer;
        MeteoDecodedSendBuf convertedBuffer;
        volatile bool newMeteoData;
        volatile bool meteoDataReady;

        /* 
         *  Write a bit to the HKW581
         */
        void writeToMeteo(uint8_t bit);
};

#endif /* _METEO_H_ */
