```cpp
#include "heatingSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> // Required for JsonObject, StaticJsonDocument
#include <PubSubClient.h> // Required for PubSubClient

// Resistor R from Vcc (5 V?) to A0, thermistor from A0 to ground //

const byte thermistorpin = A0;
const byte heaterpin = 4;
const float Tset = 35; // R=22k for Vcc = 6V
const float deltaT = 0.5;
const float Vcc = 5;
const float R = 18000;
const float Ro = 10000;
const float To = 25;
const float beta = 4220; // Renamed from ebeta to beta to match formula usage
const float Kadc = 3.3 / 4095;

float Vadc, T, Rth;
unsigned long currtime, prevtime, T1, T2; // Changed to unsigned long
bool heater = false;
bool prevheater = false;

void setupHeating() 
{
  pinMode(thermistorpin, INPUT);
  pinMode(heaterpin, OUTPUT);
  // pinMode(LED_BUILTIN, OUTPUT); // LED_BUILTIN might not be defined on all boards, check if needed. Keeping it if it was there.
  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  #endif

  ledcSetup(1, 20000, 10); // Set PWM freq and resolution, on Channel 1, for ESP32
  ledcAttachPin(heaterpin, 1);
  // Serial.begin(2000000); // Serial is handled in main.ino
  
  T1 = millis();
  T2 = T1;
}

void executeHeating() {
 currtime = millis();
 
 // 100 ms update period
 if (currtime - T1 > 100) { 
  T1 = currtime; 
  
  Vadc = Kadc * analogRead(thermistorpin);

  // Avoid division by zero if Vadc is Vcc (unlikely but possible)
  if (abs(Vcc - Vadc) > 0.01) {
      Rth = R * Vadc / (Vcc - Vadc); // Calculate thermistor resistance from ADC voltage
      T = (To + 273) * beta / (beta + (To + 273) * log(Rth / Ro)) - 273; // Calculate temperature from thermistor resistance
  } else {
      // Handle error or saturation
      T = 999.0; // Error value
  }

  if(T < Tset - deltaT) { heater = true; } // Switch on heater if temperature falls below lower threshold
  if(T > Tset + deltaT) { heater = false; } // Switch off heater if temperature rises above upper threshold

  if(heater != prevheater) { // Only write to the heater pin if its status has changed 
    ledcWrite(1, heater ? 639 : 0); // Limit heater power to 30 W (approx 639/1023 duty cycle?) 10 bit is 1023. 
    // Note: Resolution is 10 bits (0-1023). 639 is ~62%.
    
    #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, heater); // Write also to the on-board LED, for testing
    #endif
    prevheater = heater;
  }
 }
  
 // Print temperature etc once per second, for testing only - moved to telemetry
 /*
 if(currtime - T2 > 1000) {
  T2 = currtime; 
  Serial.println(Vadc); Serial.println(Rth); Serial.println(T); Serial.println(' ');
 }
 */
}

void getHeatingStatus(JsonObject& doc) {
    doc["temperature"] = T;
    doc["heater_state"] = heater;
    doc["target_temperature"] = Tset;
}

void handleHeatingCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length) {
  // 1. Get the RPC Request ID
  String topicStr = String(topic);
  String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);

  // 2. Deserialize
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);
  
  // Example: {"method": "setTemperature", "params": 37.0}
  // Currently just logging as placeholder, as Tset is const in the original code.
  // If we want to make Tset variable, we need to remove 'const' from Tset declaration.
  // For now, I will just acknowledge the command.
  
  const char* method = doc["method"];
  
  if (strcmp(method, "setTemperature") == 0) {
      // TODO: Make Tset non-const to allow changing it.
      // float newTemp = doc["params"];
      // Tset = newTemp; 
      
      char responseTopic[100];
      sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
      client.publish(responseTopic, "{\"status\": \"error\", \"message\": \"Temperature setting not implemented yet (const Tset)\"}");
  } else {
      // Unknown method
  }
}
```