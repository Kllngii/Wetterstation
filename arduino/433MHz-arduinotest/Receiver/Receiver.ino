#include <RH_ASK.h>

#define D0  16
#define D1  5
#define D2  4

RH_ASK driver(2000, D0, D1, 0);
bool On = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
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
    uint8_t buflen = sizeof(buf);

    if (driver.recv(buf, &buflen)) // Non-blocking
    {
      Serial.println((char*)buf);

    
    }
}
