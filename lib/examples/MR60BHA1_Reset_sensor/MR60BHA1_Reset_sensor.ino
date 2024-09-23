#include "Arduino.h"
#include <60ghzbreathheart.h>

#define RX_Pin 16 // Replace with your chosen RX pin
#define TX_Pin 17 // Replace with your chosen TX pin

// Initialize hardware serial with chosen pins
HardwareSerial mySerial(2); // Use UART2 on ESP32

// Initialize the radar module with the custom hardware serial
BreathHeart_60GHz radar = BreathHeart_60GHz(&mySerial);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial1.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, RX_Pin, TX_Pin);
  //  mySerial.begin(115200);

  while(!Serial);   //When the serial port is opened, the program starts to execute.

  Serial.println("Ready");

  radar.reset_func();
}

void loop()
{
  // put your main code here, to run repeatedly:
}