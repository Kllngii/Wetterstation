#include <Wire.h>
#include <math.h>
#include <dhtnew.h> //DHTNew: https://github.com/RobTillaart/DHTNew
#include <BME280I2C.h> //BME280: https://github.com/finitespace/BME280/tree/master 
#include <CCS811-SOLDERED.h> //CCS811-Soldered: https://github.com/SolderedElectronics/Soldered-CCS811-Air-Quality-Sensor-Arduino-Library/

DHTNEW dht22Sensor(5); //Temperatur(°C), Luftfeuchtigkeit(%rel)
BME280I2C bme280Sensor; //Temperatur(°C), Luftfeuchtigkeit(%rel), Luftdruck(hPa)
CCS_811 ccs811Sensor; //CO2 Konzentration(ppm)[400ppm .. 8192ppm], TVOC Konzentration(ppb)[0ppb .. 1187ppb]

float lastValidTemp = 21, lastValidHumi = 46;

void setup() {
  Serial.begin(115200);
  Serial.print("INIT");
  Wire.begin();

  bme280Sensor.begin();
  ccs811Sensor.begin();

  Serial.println(" DONE");
}

void loop() {
  dht22Sensor.read();

  float temp(NAN), hum(NAN), pres(NAN);
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);
  bme280Sensor.read(pres, temp, hum, tempUnit, presUnit);

  if (ccs811Sensor.dataAvailable()) {
    ccs811Sensor.readAlgorithmResults();
    lastValidTemp = isnan(temp) ? lastValidTemp : temp;
    lastValidHumi = isnan(hum) ? lastValidHumi : hum;
    ccs811Sensor.setEnvironmentalData(lastValidHumi, lastValidTemp); //Sensorabgleich für die nächste Messung anpassen
  }

  Serial.print("{\"dht-temperature\":");
  Serial.print(dht22Sensor.getTemperature(), 1);
  Serial.print(",\"dht-humidity\":");
  Serial.print(dht22Sensor.getHumidity(), 1);
  Serial.print(",\"bme-temperature\":");
  Serial.print(temp, 1);
  Serial.print(",\"bme-humidity\":");
  Serial.print(hum, 1);
  Serial.print(",\"bme-pressure\":");
  Serial.print(pres, 1);
  Serial.print(",\"ccs-co2\":");
  Serial.print(ccs811Sensor.getCO2(), 1);
  Serial.print(",\"ccs-tvoc\":");
  Serial.print(ccs811Sensor.getTVOC(), 1);
  Serial.println("}");

  delay(2000);
}
