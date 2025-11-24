#include "PHSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> 

// --- Pin Definitions ---
const byte Pump1Pin = 4;  // Acid Pump
const byte Pump2Pin = 5;  // Base Pump
const byte pHPin = 36;    // pH Sensor Analog Pin (ADC1_CH0)

// --- Constants & Calibration (from SYSTEM.md) ---
const float SLOPE = 0.54; 
const float OFFSET = 0.58;
const int ARRAY_LENGTH = 10;

// --- State Variables ---
float targetPH = 5.0;
const float tolerance = 0.4;

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
  pinMode(pHPin, INPUT); 
  
  // Initialize array
  for(int i=0; i<ARRAY_LENGTH; i++) pHArray[i] = 0.0;
}

void executePH() {
  // 1. Read voltage and convert to pH
  float voltage = analogRead(pHPin) * 5.0 / 1024.0; // Note: ESP32 ADC is 12-bit (4096), but SYSTEM.md used 1024. 
                                                    // Keeping 1024 if using standard Arduino analogRead on ESP32 without config might be wrong, 
                                                    // but sticking to SYSTEM.md logic for now. 
                                                    // *Correction*: ESP32 analogRead returns 0-4095. 
                                                    // However, if the user provided code for an Arduino Uno (10-bit), 
                                                    // mapping might be needed. 
                                                    // I will use 4096.0 for ESP32 correctness if this is ESP32, 
                                                    // but the file says "Arduino/bioreactor". 
                                                    // Main.ino includes "WiFi.h" which implies ESP32.
                                                    // I will use 4096.0 to be safe for ESP32, or should I stick to the code?
                                                    // The user said "The code in the .md file is the functional code".
                                                    // I will stick to the formula but adjust the denominator if it's clearly ESP32.
                                                    // Actually, let's stick to the user's formula but note the potential issue if values are weird.
                                                    // Wait, `analogRead(pHPin)` on ESP32 is 12-bit. 
                                                    // If I use 1024, the voltage will be 4x higher. 
                                                    // I will assume the user wants the LOGIC, not necessarily the hardware-specific constants if they differ.
                                                    // BUT, `SYSTEM.md` defines `SENSOR_PIN A0`.
                                                    // I'll use the formula as is but with 4095.0 since `main.ino` uses `WiFi.h` (ESP32).
                                                    
  // Re-evaluating: The user said "integrate the functionality... code in .md is the functional code".
  // If I change 1024 to 4095, I change the calibration.
  // I will use the exact formula from SYSTEM.md but comment about the ADC resolution.
  // Actually, `analogRead` resolution depends on the board.
  // I will use `analogRead(pHPin) * 5.0 / 1024.0` as requested, but this is risky on ESP32.
  // Let's look at PHSubsystem.cpp original code: `pH_raw = analogRead(pHPin);`
  // It compared against 500-600.
  // If I use the new logic, I must use the new formula.
  
  voltage = analogRead(pHPin) * 5.0 / 1024.0; 
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
  deserializeJson(doc, payload, length);

  // Expected RPC: {"method": "setPump", "params": {"pump": "acid", "duration": 500}}
  // Or maybe setTargetPH? I'll keep the pump control for now as it was there.
  
  JsonObject params = doc["params"];
  const char* pump = params["pump"];     
  int duration = params["duration"] | 750; 

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
    // Check for setTargetPH maybe?
    // For now, just error if not pump command
    Serial.println("RPC Error: 'pump' parameter missing.");
    char responseTopic[100];
    sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
    client.publish(responseTopic, "{\"error\": \"Invalid parameters\"}");
  }
}