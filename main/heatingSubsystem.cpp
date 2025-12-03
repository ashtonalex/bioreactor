#include "heatingSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> // Required for JsonObject, StaticJsonDocument
#include <PubSubClient.h> // Required for PubSubClient

// Resistor R from Vcc (5 V?) to A0, thermistor from A0 to ground //

const byte thermistorpin = A0;
const byte heaterpin = 4;
static float Tset = 35; // R=22k for Vcc = 6V
static float deltaT = 0.5;
const float Vcc = 5;
const float R = 18000;
const float Ro = 10000;
const float To = 25;
const float beta = 4220; // Renamed from ebeta to beta to match formula usage
const float Kadc = 3.3 / 4095;

static float Vadc, T, Rth;
static unsigned long currtime, prevtime, T1, T2; // Changed to unsigned long
static bool heater = false;
static bool prevheater = false;

void setupHeating() 
{
  pinMode(thermistorpin, INPUT);
  pinMode(heaterpin, OUTPUT);
  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  #endif

  ledcAttach(heaterpin, 20000, 10); // Set PWM freq and resolution, on Channel 1, for ESP32
}

void executeHeating()
{
  Vadc = Kadc * analogRead(thermistorpin);

  // Avoid division by zero if Vadc is Vcc (unlikely but possible)
  if (abs(Vcc - Vadc) > 0.01) {
      Rth = R * Vadc / (Vcc - Vadc); // Calculate thermistor resistance from ADC voltage
      T = (To + 273) * beta / (beta + (To + 273) * log(Rth / Ro)) - 273; // Calculate temperature from thermistor resistance
  } else {
      // Handle error or saturation
      T = 999.0; 
  }

  if(T < Tset - deltaT) { heater = true; } // Switch on heater if temperature falls below lower threshold
  if(T > Tset + deltaT) { heater = false; } // Switch off heater if temperature rises above upper threshold

  if(heater != prevheater) { // Only write to the heater pin if its status has changed 
    ledcWrite(heaterpin, heater ? 639 : 0); // Limit heater power to 30 W (approx 639/1023 duty cycle?) 10 bit is 1023. 
    
    #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, heater); 
    #endif
    prevheater = heater;
  }
}

void getHeatingStatus(JsonObject& doc) {
    doc["temperature"] = T;
    doc["heater_state"] = heater;
    doc["target_temperature"] = Tset;
}

void handleHeatingAttributes(JsonObject& doc) {
  if (doc.containsKey("target_temperature")) {
    Tset = doc["target_temperature"];
    Serial.print("Updated target temperature: ");
    Serial.println(Tset);
  }
  if (doc.containsKey("temp_tolerance")) {
    deltaT = doc["temp_tolerance"];
    Serial.print("Updated temp tolerance: ");
    Serial.println(deltaT);
  }
}

void handleHeatingCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length) {
  // 1. Get the RPC Request ID
  String topicStr = String(topic);
  String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);

  // 2. Deserialize
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);
  
  // Example: {"method": "setTemperature", "params": 37.0}
  
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