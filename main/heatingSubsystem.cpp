#include "heatingSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> // Required for JsonObject, StaticJsonDocument
#include <PubSubClient.h> // Required for PubSubClient

// Configuration from heating.cpp
// Resistor R from Vcc to thermistor pin, thermistor from pin to ground

const byte thermistorpin = A5;
const byte heaterpin = 6;
static float Tset = 35;
static float deltaT = 0.5;
const float Vcc = 3.3;
const float R = 10000;
const float Kadc = 3.3 / 4095;

static float Vadc, T, Rth;
static unsigned long currtime, T1, T2;
static int heaterPWM = 0;
static int prevHeaterPWM = 0;

void setupHeating() 
{
  pinMode(heaterpin, OUTPUT);
  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  #endif

  T1 = micros();
  T2 = T1;
}

void executeHeating()
{
  // Safety Check: If system is not active, force heater off and exit
  if (!is_system_active) {
    analogWrite(heaterpin, 0);
    prevHeaterPWM = 0;
    return;
  }

  currtime = micros();

  // Execute heating control every 100ms (100000 microseconds)
  if (currtime - T1 >= 100000) {
    T1 = currtime;

    Vadc = Kadc * analogRead(thermistorpin);
    
    // Avoid division by zero if Vadc is Vcc (unlikely but possible)
    if (abs(Vcc - Vadc) > 0.01) {
      Rth = R * Vadc / (Vcc - Vadc); // Calculate thermistor resistance from ADC voltage
      T = -0.00295 * Rth + 50.23; // Linear temperature calculation from heating.cpp
    } else {
      // Handle error or saturation
      T = 999.0; 
    }

    // Hysteresis control logic
    if (T < Tset - deltaT) { heaterPWM = 255; } // Switch on heater if temperature falls below lower threshold
    if (T > Tset - deltaT) { heaterPWM = 0; } // Switch off heater if temperature rises above threshold

    // Only write to the heater pin if its status has changed 
    if (heaterPWM != prevHeaterPWM) {
      analogWrite(heaterpin, heaterPWM);
      
      #ifdef LED_BUILTIN
      digitalWrite(LED_BUILTIN, heaterPWM > 0); 
      #endif
      prevHeaterPWM = heaterPWM;
    }
  }

  // Serial debug output every 1 second (1000000 microseconds)
  if (currtime - T2 >= 1000000) {
    T2 = currtime;
    Serial.print("Rth: "); Serial.print(Rth, 0);
    Serial.print(" | T: "); Serial.print(T, 1);
    Serial.print(" | Heater: "); Serial.println(heaterPWM > 0 ? "ON" : "OFF");
  }
}

void getHeatingStatus(JsonObject& doc) {
    doc["temperature"] = T;
    doc["heater_state"] = heaterPWM > 0;
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
      float newTemp = doc["params"];
      Tset = newTemp; 
      
      char responseTopic[100];
      sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
      client.publish(responseTopic, "{\"status\": \"ok\", \"message\": \"Temperature target updated\"}");
  } else {
      // Unknown method
  }
}