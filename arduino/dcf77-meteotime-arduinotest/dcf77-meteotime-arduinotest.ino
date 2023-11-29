#include "DCF77.h"
#include "TimeLib.h"

#define DCF_PIN 4// Connection pin to DCF 77 device
#define DCF_INTERRUPT 4  // Interrupt number associated with pin

//time_t time;
DCF77 DCF = DCF77(DCF_PIN,DCF_INTERRUPT);

void setup() {
  Serial.begin(9600);
  Serial.println("Ping");
  DCF.Start();
  Serial.println("Waiting for DCF77 time ... ");
  Serial.println("It will take at least 2 minutes before a first time update.");
}

void loop() {
  delay(1000);
  time_t DCFtime = DCF.getTime(); // Check if new DCF77 time is available
  if (DCFtime!=0)
  {
Serial.println("Time is updated");
setTime(DCFtime);
  }
  digitalClockDisplay();  
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
    Serial.print(digits);
}