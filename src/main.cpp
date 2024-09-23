#define ERA_DEBUG
#define DEFAULT_MQTT_HOST "mqtt1.eoh.io"
#define ERA_AUTH_TOKEN "e07c3b86-7627-4df0-a88a-bb151c5c8bfd"

#include <ERa.hpp>
#include <ERa/ERaTimer.hpp>
#include <Arduino.h>
#include "60ghzbreathheart.h"

// Define pins for hardware serial
#define RX_Pin 16 // Replace with your chosen RX pin
#define TX_Pin 17 // Replace with your chosen TX pin

const char ssid[] = "eoh.io";
const char pass[] = "Eoh@2020";

// Initialize hardware serial with chosen pins
HardwareSerial mySerial(2); // Use UART2 on ESP32

// Initialize the radar module with the custom hardware serial
BreathHeart_60GHz radar_breath_heart = BreathHeart_60GHz(&mySerial);

unsigned long presenceCheckTimer = 0;
const unsigned long presenceCheckInterval = 100; // Check presence every 100ms for quicker response

unsigned long startMillis;
const unsigned long period = 30000; // Reduced to 30 seconds for faster averaging

unsigned long heartRateTimer = 0;
unsigned long breathRateTimer = 0;
const unsigned long heartRateInterval = 500; // 500ms for heart rate updates
const unsigned long breathRateInterval = 500; // 500ms for breath rate updates

// Add variables to track zero values
unsigned long zeroHeartRateStartTime = 0;
unsigned long zeroBreathRateStartTime = 0;
const unsigned long zeroValueCheckDuration = 5000; // 5 seconds duration for checking zero values
bool heartRateIsZero = false;
bool breathRateIsZero = false;

int heartRateSum = 0;
int breathRateSum = 0;
int heartRateCount = 0;
int breathRateCount = 0;
float avgHeartRate;
float avgBreathRate;
bool someoneHere = false;
bool firstCheck = true; // Variable to handle the initial presence check

int presenceCount = 0;
const int presenceThreshold = 2; // Lowered threshold for faster presence detection
int absenceCount = 0;
const int absenceThreshold = 2; // Lowered threshold for faster absence detection

int checkoutHuman;
int checkoutInterval = 3000;

void setup() {
  // Initialize serial communication
  ERa.begin(ssid, pass);
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, RX_Pin, TX_Pin);
  while (!Serial); // Wait for the serial port to be ready
  Serial.println("Ready");
  // Optionally set radar mode
  radar_breath_heart.ModeSelect_fuc(1);  // 1: real-time transmission mode, 2: sleep state mode
  startMillis = millis(); // Initialize the timer
}

void checkNoPresenceTimeout() {    
  static unsigned long noPresenceStartMillis = millis();
  if (!someoneHere) {
    if (millis() - noPresenceStartMillis >= period) {
      Serial.println("No one detected for 30 seconds, skipping measurements.");
      noPresenceStartMillis = millis(); // Reset timer after skipping
    }
  } else {
    noPresenceStartMillis = millis(); // Reset timer when someone is detected
  }
}

void loop() {
  ERa.run();

  // More frequent human presence check for better responsiveness
  if (millis() - presenceCheckTimer >= presenceCheckInterval) {
    radar_breath_heart.HumanExis_Func();
    if (radar_breath_heart.sensor_report != 0x00) {
      switch (radar_breath_heart.sensor_report) {
        case NOONE_HERE:
        case NOONE:
          absenceCount++;
          presenceCount = 0;
          if (absenceCount >= absenceThreshold) {
            someoneHere = false;
            absenceCount = 0; // Reset absence count after confirming no one is present
            Serial.println("No one is here.");
            Serial.println("----------------------------");
          }
          break;
        case SOMEONE:
        case NONEPSE:
        case STATION:
        case MOVE:
        case DIREVAL:
          presenceCount++;
          absenceCount = 0;
          if (presenceCount >= presenceThreshold) {
            someoneHere = true;
            presenceCount = 0; // Reset presence count after confirming presence
            Serial.println("Someone is here.");
            Serial.println("----------------------------");
            firstCheck = false; // Disable first check after initial presence is confirmed
          }
          break;
      }
    }
    presenceCheckTimer = millis(); // Reset presence check timer
  }

  if (someoneHere) {
    // Get breath and heartbeat information
    radar_breath_heart.Breath_Heart();

    // Handle heart rate measurements
    if (millis() - heartRateTimer >= heartRateInterval) {
      if (radar_breath_heart.sensor_report == HEARTRATEVAL) {
        if (radar_breath_heart.heart_rate == 0) {
          if (!heartRateIsZero) {
            // Start tracking the zero value
            zeroHeartRateStartTime = millis();
            heartRateIsZero = true;
          } else if (millis() - zeroHeartRateStartTime >= zeroValueCheckDuration) {
            // If the heart rate remains zero for 5 seconds, push it
            ERa.virtualWrite(V0, 0);
            heartRateIsZero = false; // Reset zero tracking
          }
        } else {
          // If a non-zero value is detected, push it immediately
          heartRateSum += radar_breath_heart.heart_rate;
          heartRateCount++;
          ERa.virtualWrite(V0, radar_breath_heart.heart_rate);
          heartRateIsZero = false; // Reset zero tracking
        }
        heartRateTimer = millis(); // Reset timer
      }
    }

    // Handle breath rate measurements
    if (millis() - breathRateTimer >= breathRateInterval) {
      if (radar_breath_heart.sensor_report == BREATHVAL) {
        if (radar_breath_heart.breath_rate == 0) {
          if (!breathRateIsZero) {
            // Start tracking the zero value
            zeroBreathRateStartTime = millis();
            breathRateIsZero = true;
          } else if (millis() - zeroBreathRateStartTime >= zeroValueCheckDuration) {
            // If the breath rate remains zero for 5 seconds, push it
            ERa.virtualWrite(V1, 0);
            breathRateIsZero = false; // Reset zero tracking
          }
        } else {
          // If a non-zero value is detected, push it immediately
          breathRateSum += radar_breath_heart.breath_rate;
          breathRateCount++;
          ERa.virtualWrite(V1, radar_breath_heart.breath_rate);
          breathRateIsZero = false; // Reset zero tracking
        }
        breathRateTimer = millis(); // Reset timer
      }
    }

    // Check if 30 seconds have passed for averaging
    if (millis() - startMillis >= period) {
      // Calculate and print the average heart rate and breath rate
      if (heartRateCount > 0 && breathRateCount > 0) {
        avgHeartRate = (float)heartRateSum / heartRateCount;
        avgBreathRate = (float)breathRateSum / breathRateCount;
        Serial.print(">Average heart rate over last 30 seconds: ");
        Serial.print(avgHeartRate);
        Serial.println(" bpm");
        Serial.print(">Average breath rate over last 30 seconds: ");
        Serial.print(avgBreathRate);
        Serial.println(" rpm");
      } else {
        Serial.println("___________________________________________________");
        Serial.println("No valid heart rate or breath rate data collected.");
      }
      // Reset the counters and sums for the next 30 seconds
      heartRateSum = 0;
      heartRateCount = 0;
      breathRateSum = 0;
      breathRateCount = 0;
      startMillis = millis(); // Reset the timer
    }
  } else if (heartRateSum == 0 && breathRateSum == 0) {
    if (millis() - checkoutHuman > checkoutInterval) {
      Serial.println("No one is here, skipping breath and heart rate measurement.");
      checkoutHuman = millis();
    }
  }
}





