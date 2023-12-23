#include <SoftwareSerial.h>

#define HC12_1_RX 5
#define HC12_1_TX 4
#define HC12_1_SET  0

#define HC12_2_RX 14
#define HC12_2_TX 12
#define HC12_2_SET  2

SoftwareSerial HC12_1(HC12_1_RX, HC12_1_TX);
SoftwareSerial HC12_2(HC12_2_RX, HC12_2_TX);

void setup() {
  HC12_1.begin(9600);
  HC12_2.begin(9600);
  Serial.begin(115200);
  Serial.print("SETUP Finished");

  pinMode(HC12_1_SET, OUTPUT);
  pinMode(HC12_2_SET, OUTPUT);

  HC12_1.print("AT+DEFAULT");
  Serial.print("12_1");
  while(HC12_1.available() < 10)
  {
    delay(1);
    Serial.println(HC12_1.available());
  }
  for(int i = 0; i < 10; i++)
  {
    Serial.print((char)HC12_1.read());
  }
 
  HC12_2.print("AT+DEFAULT");
  Serial.print("12_2");
  while(HC12_2.available() < 10)
  {
    delay(1);
    Serial.println(HC12_2.available());
  }
  for(int i = 0; i < 10; i++)
  {
    Serial.print((char)HC12_2.read());
  }

  pinMode(HC12_1_SET, INPUT);
  pinMode(HC12_2_SET, INPUT);
}

void loop() {
  if (HC12_2.available())
  {
    Serial.write((char)HC12_2.read());
  }
  if (Serial.available())
  {
    HC12_1.write(Serial.read());
  }

}
