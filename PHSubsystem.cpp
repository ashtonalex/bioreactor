#include "PHSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> // For parsing/creating JSON

// --- Pin Definitions ---
const byte Pump1Pin = 4;  // Acid Pump
const byte Pump2Pin = 5;  // Base Pump
const byte pHPin = 36;    // pH Sensor Analog Pin (ADC1_CH0)

// --- Autonomous Control State ---
bool Pump1, Pump2, prevPump1, prevPump2;
int pH_raw; // The raw ADC reading
int pHHi = 600; // ADC value when pH is too high (needs acid)
int pHLo = 500; // ADC value when pH is too low (needs base)
int pHMid = (pHHi + pHLo) / 2;

// --- ThingsBoard MQTT Topics ---
const char* telemetry_topic = "v1/devices/me/telemetry"; // Topic to publish telemetry TO

// --- Helper Function ---
/**
 * @brief Manually pulses a pump for a given duration.
 * This is a blocking function.
 * @param pin The pump pin to activate.
 * @param duration The duration in milliseconds.
 */
void pulsePump(int pin, int duration) {
  if (pin == Pump1Pin) Serial.print("Manual Pulse: ACID");
  if (pin == Pump2Pin) Serial.print("Manual Pulse: BASE");
  Serial.printf(" for %d ms\n", duration);

  digitalWrite(pin, HIGH);
  delay(duration); // This will block the loop, which is acceptable for a short, manual override
  digitalWrite(pin, LOW);

  // We must update the 'prev' state here, or the autonomous
  // logic might not see the change.
  if (pin == Pump1Pin) prevPump1 = LOW;
  if (pin == Pump2Pin) prevPump2 = LOW;
}


// --- Interface Functions (defined in .hpp) ---

void setupPH() {
  pinMode(Pump1Pin, OUTPUT);
  pinMode(Pump2Pin, OUTPUT);
  digitalWrite(Pump1Pin, LOW); // Ensure pumps are off
  digitalWrite(Pump2Pin, LOW);
  pinMode(pHPin, INPUT); // Pins are inputs by default
}

void executePH() {
  // Read the raw ADC value from the pH sensor
  pH_raw = analogRead(pHPin);

  // --- Autonomous Control Logic (from original file) ---
  if (pH_raw > pHHi) {
    Pump1 = 1; // Need Acid
    Pump2 = 0;
  } // operate Pump1, to reduce pH, if pH > pHHi

  if (pH_raw < pHLo) {
    Pump1 = 0;
    Pump2 = 1; // Need Base
  } // operate Pump2, to increase pH, if pH < pHLo

  if (Pump1 == 1 && pH_raw <= pHMid) {
    Pump1 = 0;
  } // Turn-off Pump1 when pH <= pHMid

  if (Pump2 == 1 && pH_raw >= pHMid) {
    Pump2 = 0;
  } // Turn-off Pump2 when pH >= pHMid

  // --- Update Pump Hardware ---
  if (Pump1 != prevPump1) {
    digitalWrite(Pump1Pin, Pump1);
    prevPump1 = Pump1;
  } 

  if (Pump2 != prevPump2) {
    digitalWrite(Pump2Pin, Pump2);
    prevPump2 = Pump2;
  }
}

void publishStatus(PubSubClient& client) {
  // Create a JSON object with the current status
  StaticJsonDocument<200> doc; // Small JSON doc

  // These keys (e.g., "ph_raw") are what will appear in your
  // ThingsBoard "Latest Telemetry" dashboard.
  doc["ph_raw"] = pH_raw;
  doc["pump_acid_on"] = Pump1; // Current state from autonomous logic
  doc["pump_base_on"] = Pump2; // Current state from autonomous logic

  // Serialize JSON to a buffer
  char buffer[200];
  serializeJson(doc, buffer);

  // Publish to the ThingsBoard telemetry topic
  client.publish(telemetry_topic, buffer);
  
  // Serial.print("Published Telemetry: ");
  // Serial.println(buffer);
}

void handleCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length) {
  // 1. Get the RPC Request ID from the topic
  // Topic is "v1/devices/me/rpc/request/12345"
  // We need the "12345" part
  String topicStr = String(topic);
  String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);

  // 2. Deserialize the incoming JSON command
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);

  // 3. Parse the expected RPC format: {"method": "setPump", "params": {"pump": "acid", "duration": 500}}
  const char* method = doc["method"];
  
  if (method && strcmp(method, "setPump") == 0) { // Check if method is "setPump"
    const char* pump = doc["params"]["pump"];
    int duration = doc["params"]["duration"] | 750; // 750ms default

    if (pump) {
      if (strcmp(pump, "acid") == 0) {
        pulsePump(Pump1Pin, duration);
      } else if (strcmp(pump, "base") == 0) {
        pulsePump(Pump2Pin, duration);
      }
    } else {
      Serial.println("RPC Error: 'pump' parameter missing.");
    }

  } else {
    Serial.print("Unknown RPC method: ");
    Serial.println(method);
  }

  // 4. Send a response back to ThingsBoard to clear the command
  char responseTopic[100];
  sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
  Serial.print("Sending RPC response to: ");
  Serial.println(responseTopic);
  
  // You can send back any JSON, e.g., {"status": "ok"}
  client.publish(responseTopic, "{\"status\": \"ok\"}");
}