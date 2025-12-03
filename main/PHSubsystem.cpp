#include "PHSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> 

// --- Pin Definitions ---
// const byte Pump1Pin = 4;  // Acid Pump
// const byte Pump2Pin = 5;  // Base Pump
// const byte pHPin = 36;    // pH Sensor Analog Pin (ADC1_CH0)

// --- Constants & Calibration (from SYSTEM.md) ---
const float SLOPE = 0.54; 
const float OFFSET = 0.58;
const int ARRAY_LENGTH = 10;

// --- State Variables ---
float targetPH = 5.0;
float tolerance = 0.4;

float pHArray[ARRAY_LENGTH];
int pHArrayIndex = 0;
float currentPH = 0.0; // Store calculated pH for telemetry

// Pump States
bool acid_on = false;
bool alkali_on = false;
bool prev_acid_on = false;
bool prev_alkali_on = false;

// --- Helper Function ---
float get_average(float* arr, int length) {
    if (length <= 2) {
        return arr[0];
    }
    
    float min = arr[0];
    float max = arr[0];
    float sum = 0;
    
    // Find min and max
    for (int i = 0; i < length; i++) {
        if (arr[i] < min) min = arr[i];
        if (arr[i] > max) max = arr[i];
        sum += arr[i];
    }
    
    // Remove outliers and average the rest
    return (sum - min - max) / (length - 2);
}

/**
 * @brief Manually pulses a pump for a given duration.
 * This is a blocking function.
 */
void pulsePump(int pin, int duration) {
  if (pin == Pump1Pin) Serial.print("Manual Pulse: ACID");
  if (pin == Pump2Pin) Serial.print("Manual Pulse: BASE");
  Serial.printf(" for %d ms\n", duration);

  digitalWrite(pin, HIGH);
  delay(duration); 
  digitalWrite(pin, LOW);
}

// --- Interface Functions ---

void setupPH() {
  pinMode(Pump1Pin, OUTPUT);
  pinMode(Pump2Pin, OUTPUT);
  digitalWrite(Pump1Pin, LOW); 
  digitalWrite(Pump2Pin, LOW);
  
  float voltage = analogRead(pHPin) * 5.0 / 1024.0; 
  float pHValue = (SLOPE * voltage) + OFFSET;
  pHArray[pHArrayIndex++] = pHValue;
  
  // 2. Once buffer full, calculate average and control
  if (pHArrayIndex >= ARRAY_LENGTH) {
      currentPH = get_average(pHArray, ARRAY_LENGTH);
      pHArrayIndex = 0;
      
      // Bang-bang control
      if (currentPH > targetPH + tolerance) {
          // pH too high, add acid
          digitalWrite(Pump1Pin, HIGH);
          digitalWrite(Pump2Pin, LOW);
          acid_on = true;
          alkali_on = false;
      } 
      else if (currentPH < targetPH - tolerance) {
          // pH too low, add alkali
          digitalWrite(Pump1Pin, LOW);
          digitalWrite(Pump2Pin, HIGH);
          acid_on = false;
          alkali_on = true;
      } 
      else {
          // pH within tolerance, turn off both pumps
          digitalWrite(Pump1Pin, LOW);
          digitalWrite(Pump2Pin, LOW);
          acid_on = false;
          alkali_on = false;
      }
  }
  
  delay(100); // Small delay as per SYSTEM.md
}

void getPHStatus(JsonObject& doc) {
  doc["pH"] = currentPH;
  doc["target_pH"] = targetPH;
  doc["acid_pump"] = acid_on;
  doc["base_pump"] = alkali_on;
}

void handlePHCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);

  // Expected RPC: {"method": "setPump", "params": {"pump": "acid", "duration": 500}}
  
  JsonObject params = doc["params"];
  const char* pump = params["pump"];     
  int duration = params["duration"] | 750; 

  // Get request ID from topic
  String topicStr = String(topic);
  String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);

  if (pump) { 
    if (strcmp(pump, "acid") == 0) {
      pulsePump(Pump1Pin, duration);
    } else if (strcmp(pump, "base") == 0) {
      pulsePump(Pump2Pin, duration);
    }
    
    char responseTopic[100];
    sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
    client.publish(responseTopic, "{\"status\": \"ok\", \"pump\": \"" + String(pump) + "\"}");
    
  } else {
    Serial.println("RPC Error: 'pump' parameter missing.");
    char responseTopic[100];
    sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
    client.publish(responseTopic, "{\"error\": \"Invalid parameters\"}");
  }
}

void handlePHAttributes(JsonObject& doc) {
  if (doc.containsKey("target_pH")) {
    targetPH = doc["target_pH"];
    Serial.print("Updated targetPH: ");
    Serial.println(targetPH);
  }
  if (doc.containsKey("pH_tolerance")) {
    tolerance = doc["pH_tolerance"];
    Serial.print("Updated pH tolerance: ");
    Serial.println(tolerance);
  }
}