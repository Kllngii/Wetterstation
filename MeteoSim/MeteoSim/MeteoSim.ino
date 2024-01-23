#define UART0_TX        0
#define UART0_RX        1

#define METEO_SEND_FREQUENCY_MILLIS   15000

typedef struct __attribute__ ((__packed__)){
    char dataType[4];
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekDay;
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

TimeSendBuf timeBuffer = {'T','I','M','E'};
MeteoDecodedSendBuf meteoBuffer = {'M','T','E','O'};
unsigned long lastMeteoSend = 0;
void setup() {
  // Start usb debug output
  Serial.begin(115200);
  // Start COM with other pico
  Serial1.setRX(UART0_RX);
  Serial1.setTX(UART0_TX);
  Serial1.begin(115200);

  /* 14:59 Reg. 19 (Tiefstwert, 3.Tag, Bremerhaven)
   * 6°C; T:bedeckt; N:Starkregen
   */
  timeBuffer.data.hour = 14;
  timeBuffer.data.minute = 59;
  timeBuffer.data.second = 0;

  timeBuffer.data.weekDay = 2;
  timeBuffer.data.day = 23;
  timeBuffer.data.month = 1;
  timeBuffer.data.year = 2024;

  meteoBuffer.data.meteoData = 0b0010011010101100001110; //von DCFLogs.de Bit 0-21
  meteoBuffer.data.checksum = 0b10; //Die letzen beiden Bit
}

void loop() {
  if ((millis() - lastMeteoSend) > METEO_SEND_FREQUENCY_MILLIS) {
    lastMeteoSend = millis();
    Serial.println("DCF-Signal und Meteo-Daten werden übertragen!");
    Serial1.write((const uint8_t*)timeBuffer.buf, sizeof(timeBuffer));
    delay(100);
    Serial1.write((const uint8_t*)meteoBuffer.buf, sizeof(meteoBuffer));
  }

}
