//
// HKW581.ino
//
// Demo of HKW581 decoder chip showing a successfull decode and a failed decode
// Algorithm to correct a 1 bit corrupted signal by repeatedly changing one
// bit in the telegram and looking for the correct result
// It is claimed that this happens in de decoder chip but experiments show that this is not the case.
// The parity bits and comparing the time to previous minute can correct the time key part of the DES.
// Practically this will be used to correct errors in the weather data. This takes 12 seconds to complete.
// This is easily in the three minute window before the next decode is required so that this is a practical proposition.
// Bas Kasteel, 2014
// This example code is in the public domain.
// You may use this software as you see fit as long as this header portion is included.
// Since this program may involve some hardware please note I'm not responsible if you destroy anything while tinkering with this software.
// The program uses a modified version of the library from Thijs Elenbaas, 2012
// You can contact me via www.arduino-projects4u.com
// Date 25:7:2014
// Chanelog:
// 0.1 innitial release

const int ledPin = 3;    // the number of the LED pin
int ledState = LOW;      // ledState used to set the LED
long previousMillis = 0; // will store last time LED was updated
long interval = 1000;    // interval at which to blink (milliseconds)
int DataIn = 4;
int DataOut = 6;
int ClockIn = 7;
int QOut = 3;
int BFR = 5;
bool flag = false;
bool debug = true;
String meteodata;
int meteobit;
int h;
unsigned long bitpattern;
unsigned long meteopattern;
//             1234567890123456789012345678901234567890123456789012345678901234567890123456789012
String test = ("0100001010001101101010100111100110000110001010000001001000110010000010000101100000");
String test2 = ("0100001010001101101010100111100110000110011010000001001000110010000010000101100000");
//                                                      * This bit is changed
byte meteoweather[] = {0x00, 0x00, 0x00};
int invertbit = 0;
void setup()
{
  // initialize the digital pin as an output.
  pinMode(ledPin, OUTPUT);
  pinMode(DataIn, OUTPUT);
  pinMode(DataOut, INPUT);
  pinMode(ClockIn, OUTPUT);
  pinMode(QOut, INPUT);
  pinMode(BFR, INPUT);
  Serial.begin(115200);
  digitalWrite(DataIn, LOW);
  digitalWrite(ClockIn, LOW);
  Serial.println("Decoder powered on and port set up");
  delay(5000);
  Serial.println("Waiting for decoder warm up max 40sec");
  h = 0;
  do
  {
    delay(1000);
    Serial.print(h);
    Serial.print(".");
    Serial.print("BFR=");
    Serial.print(digitalRead(BFR));
    Serial.print("  ");
    h++;
    if (digitalRead(BFR) == 0)
    {
      flag = true;
    }
    delay(10);
    if ((digitalRead(BFR) == 0) && (flag == true))
    {
      break;
    }
    else
    {
      flag = false;
    }
  } while (1);

  Serial.println();
  Serial.println("Warm up complete");
  //              12345678901234 12345678901234 12345678901234 12345678 12345678 12345678 12345 678 12345678
  Serial.println("Datapackage 1  Datapackage 2  Datapackage 3  Time     Hour     Date     month dow year");
  invertbit = 83;
  decode();
  if (meteodata.substring(22, 24) == "10")
  {
    Serial.println("Decoder Result OK");
  }
  else
  {
    Serial.println("Decoder Result NOT OK");
  }
  Serial.println("Change one bit (bit 42) in telegram ");
  test = test2;
  Serial.println("Datapackage 1  Datapackage 2  Datapackage 3  Time     Hour     Date     month dow year");
  invertbit = 83;
  decode();
  if (meteodata.substring(22, 24) == "10")
  {
    Serial.println("Decoder Result OK");
  }
  else
  {
    Serial.println("Decoder Result NOT OK");
  }
  Serial.println("Gives NOT OK so now change 1 bit at a time until found");
  debug = false;
  // Here routine for single bit correction. Look for correct output inverting each bit.
  // If correct then break
  unsigned long previousMillis = millis();
  for (invertbit = 0; invertbit < 82; invertbit++)
  {
    decode();
    if (meteodata.substring(22, 24) == "10")
    {
      Serial.print(invertbit);
      Serial.println(" OK");
    }
    else
    {
      Serial.print(invertbit);
      Serial.print(" ");
    }
    if (meteodata.substring(22, 24) == "10")
    {
      break;
    }
  }
  unsigned long currentMillis = millis();
  Serial.print("Time taken to find correct telegram = ");
  Serial.print((currentMillis - previousMillis) / 1000);
  Serial.println(" Seconds");
  Serial.print("Corrupted telegram = ");
  for (int k = 0; k < 82; k++)
  {
    if (test.substring(k, k + 1) == "1")
    {
      meteobit = 1;
    }
    else
    {
      meteobit = 0;
    };
    if (k == invertbit)
    {
      Serial.print("|");
    }
    Serial.print(meteobit);
    if (k == invertbit)
    {
      Serial.print("|");
    }
    if (k == 13 | k == 27 | k == 41 | k == 49 | k == 57 | k == 65 | k == 70 | k == 73)
    {
      Serial.print(" ");
    }
  }
  Serial.println();
  Serial.print("Corrected telegram = ");
  for (int k = 0; k < 82; k++)
  {
    if (test.substring(k, k + 1) == "1")
    {
      meteobit = 1;
    }
    else
    {
      meteobit = 0;
    };
    if (k == invertbit)
    {
      if (meteobit == 1)
      {
        meteobit = 0;
      }
      else
      {
        meteobit = 1;
      };
    }
    if (k == invertbit)
    {
      Serial.print("|");
    }
    Serial.print(meteobit);
    if (k == invertbit)
    {
      Serial.print("|");
    }
    if (k == 13 | k == 27 | k == 41 | k == 49 | k == 57 | k == 65 | k == 70 | k == 73)
    {
      Serial.print(" ");
    }
  }
}

void loop()
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval)
  {
    previousMillis = currentMillis;
    if (ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW;
    digitalWrite(ledPin, ledState);
  }
}

void decode()
{

  for (int k = 0; k < 82; k++)
  {
    if (test.substring(k, k + 1) == "1")
    {
      meteobit = 1;
    }
    else
    {
      meteobit = 0;
    };
    if (k == invertbit)
    {
      if (meteobit == 1)
      {
        meteobit = 0;
      }
      else
      {
        meteobit = 1;
      };
    }
    if (invertbit == 83)
    {
      Serial.print(meteobit);
    }
    digitalWrite(DataIn, meteobit);
    digitalWrite(ClockIn, HIGH);
    do
    {
    } while (digitalRead(QOut) == LOW);
    digitalWrite(ClockIn, LOW);
    do
    {
    } while (digitalRead(QOut) == HIGH);
    if (invertbit == 83)
    {
      if (k == 13 | k == 27 | k == 41 | k == 49 | k == 57 | k == 65 | k == 70 | k == 73)
      {
        Serial.print(" ");
      };
    }
  }
  if (debug == true)
  {
    Serial.println();
    Serial.println("All bits clocked in to decoder");
    // do {}
    // while (digitalRead (QOut)==LOW);
    Serial.println("Decoding now complete");
  }
  // Collect output data from decoder
  bitpattern = 0;
  meteopattern = 0;
  for (int k = 0; k < 24; k++)
  {
    do
    {
    } while (digitalRead(QOut) == LOW);
    meteobit = digitalRead(DataOut);
    bitpattern += meteobit;
    bitpattern <<= 1;
    digitalWrite(ClockIn, HIGH);
    do
    {
    } while (digitalRead(QOut) == HIGH);
    digitalWrite(ClockIn, LOW);
  }
  for (int k = 1; k < 25; k++)
  {
    meteobit = (bitpattern >> k) & 0x01;
    meteopattern += meteobit;
    meteopattern <<= 1;
  }
  if (debug == true)
  {
    Serial.print("Decoder result inv = ");
  }
  meteopattern >>= 1;
  meteodata = "";
  for (int j = 0; j < 24; j++)
  {
    bool p = (meteopattern >> j) & 1;
    if (debug == true)
    {
      Serial.print(p, DEC);
    }
    meteodata += p;
    if (debug == true)
    {
      if ((j + 1) % 8 == 0)
      {
        Serial.print(" ");
      };
    }
  }
  if (debug == true)
  {
    Serial.println();
    Serial.println("Now must reverse bit string");
    Serial.print("Decoder result     = ");
    for (int j = 23; j > -1; j--)
    {
      bool p = (meteopattern >> j) & 1;
      Serial.print(p, DEC);
      meteodata += p;
      if ((j) % 8 == 0)
      {
        Serial.print(" ");
      }
    }

    Serial.println();
    Serial.print("Actual result   = 0x");
    meteoweather[0] = meteopattern >> 16;
    meteoweather[1] = meteopattern >> 8;
    meteoweather[2] = meteopattern & 0xFF;
    if (meteoweather[0] < 0x10)
    {
      Serial.print("0");
    };
    Serial.print(meteoweather[0], HEX);
    Serial.print(" 0x");
    if (meteoweather[1] < 0x10)
    {
      Serial.print("0");
    };
    Serial.print(meteoweather[1], HEX);
    Serial.print(" 0x");
    if (meteoweather[2] < 0x10)
    {
      Serial.print("0");
    };
    Serial.println(meteoweather[2], HEX);
    Serial.println("Expected result = 0x7F 0xA9 0x3A");
  }
}