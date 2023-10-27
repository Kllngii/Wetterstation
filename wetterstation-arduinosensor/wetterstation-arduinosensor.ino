#include <dhtnew.h>

DHTNEW dht22Sensor(5);

void setup() {
  Serial.begin(115200);
  Serial.println("DHT22");
}

void loop() {
  dht22Sensor.read();
  Serial.print("{\"temperature\":");
  Serial.print(dht22Sensor.getTemperature(), 1);
  Serial.print(",\"humidity\":");
  Serial.print(dht22Sensor.getHumidity(), 1);
  Serial.println("}");

  delay(2000);
}
