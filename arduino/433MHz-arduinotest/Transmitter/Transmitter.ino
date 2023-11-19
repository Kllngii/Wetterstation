#include <RH_ASK.h>

#define D0  16
#define D1  5
#define D2  4

RH_ASK driver(2000, D1, D0, D2, false);
bool On = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  if(!driver.init())
  {
    Serial.println("Init failed");
  }
  else{
    Serial.println("Init success");
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  const char *msgOn = "On";
  const char *msgOff = "Off";

  if (On)
  {
    driver.send((uint8_t *)msgOff, strlen(msgOff));
    driver.waitPacketSent();
    On = false;
  }
  else
  {
    driver.send((uint8_t *)msgOn, strlen(msgOn));
    driver.waitPacketSent();
    On = true;
  }
  delay(2000);

}
