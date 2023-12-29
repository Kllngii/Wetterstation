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

 #include <stdint.h>

// Pindefs running on core1
#define METEO_DATA_IN   6
#define METEO_DATA_OUT  7
#define METEO_CLK_IN    8
#define METEO_CLK_OUT   9
#define METEO_RDY       10

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
} MeteoRawStruct;

typedef struct __attribute__ ((__packed__)){
    char dataType[4];
    uint32_t meteoData;
    uint8_t checksum;
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
        bool getNewData(MeteoRawStruct rawBuffer);

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
         *  Returns true if the decoder answered with valid data
         */
        bool isMeteoValid();

        /*
         *  Returns databuffer with freshly decoded meteodata if there are any. 
         *  Else it returns an empty buffer. Shall be only used after checking with
         *  isMeteoReady() before. 
         */
        MeteoDecodedSendBuf getConvertedBuffer();

    private:
        MeteoRawStruct rawBuffer;
        MeteoDecodedSendBuf convertedBuffer;
        volatile bool newMeteoData;
        volatile bool meteoDataReady;
        const uint32_t meteoErrorValue = (1 << 2) | ((uint32_t)1 << 23);

        /* 
         *  Write a bit to the HKW581
         */
        void writeToMeteo(uint8_t bit);
};

#endif /* _METEO_H_ */
