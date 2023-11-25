#include <RH_ASK.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define D0  16
#define D1  5
#define D2  4

Adafruit_BME280 bme;
RH_ASK driver(2000, D1, D0, D2, false);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if(!bme.begin(0x76))
  {
    Serial.println("No sensor found");
  }
  
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
  String tempString = "Temp: " + (String)bme.readTemperature() + " °C";
  String pressString = "Pressure: " + (String)(bme.readPressure() / 100.0f) + " hPa";
  String humString = "Humidity: " + (String)bme.readHumidity() + " %";
  const char *msgTemp = tempString.c_str();
  const char *msgPress = pressString.c_str();
  const char *msgHum = humString.c_str();

  driver.send((uint8_t *)msgTemp, strlen(msgTemp));
  driver.waitPacketSent();
  driver.send((uint8_t *)msgPress, strlen(msgPress));
  driver.waitPacketSent();
  driver.send((uint8_t *)msgHum, strlen(msgHum));
  driver.waitPacketSent();
  delay(500);

}
