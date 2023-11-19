#include <RH_ASK.h>

#define D0  16
#define D1  5
#define D2  4

RH_ASK driver(2000, D0, D1, 0);
bool On = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  
  if(!driver.init())
  {
    Serial.println("Init failed");
  }
  else{
    Serial.println("Init success");
  }

}

void loop() {
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    char charBuf[4];
    charBuf[4] = '\0';
    uint8_t buflen = sizeof(buf);

    if (driver.recv(buf, &buflen)) // Non-blocking
    {
    // Message with a good checksum received, dump it.
    charBuf[2] = buf[2];
    charBuf[1] = buf [1];
    charBuf[0] = buf [0];
    driver.printBuffer("Got:", buf, buflen);

    if (buf[0] == 'O')
    {
      if (buf[1] == 'n')
      {
        digitalWrite(LED_BUILTIN, LOW);
      }
      else if (buf[1] == 'f')
      {
        if(buf[2] == 'f')
        {
          digitalWrite(LED_BUILTIN, HIGH);
        } 
      }
    }

    
    }
}
