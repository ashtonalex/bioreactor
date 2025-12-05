#include "PHSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h> 

// --- Pin Definitions (from PHCHANGES.md) ---
#define SENSOR_PIN A4
#define ACID_PIN 8
#define ALKALI_PIN 9

// --- State Variables (from PHCHANGES2.md) ---
float targetPH = 0.0; // Start with no target (pumps off until set)
float tolerance = 0.4; // Not const, to allow updates via attributes

const int ARRAY_LENGTH = 10;
float pHArray[ARRAY_LENGTH];
int pHArrayIndex = 0; // NOTE: newPH.cpp uses -1 but that causes undefined behavior

long timeMS, t1;
long timeAfterCalibration; // Added from newPH.cpp

float linearCoefficients[2] = {1.38, 0.76}; // slope, offset - pre-calibrated defaults

int doneCalibrating = 0;

// Telemetry State
float currentPH = 0.0; 
bool acid_on = false;
bool alkali_on = false;

// --- Helper Functions (from PHCHANGES.md) ---

// code from: https://jwbrooks.blogspot.com/2014/02/arduino-linear-regression-function.html?m=1
// code from: https://jwbrooks.blogspot.com/2014/02/arduino-linear-regression-function.html?m=1
// Updated formula from newPH.cpp for consistency with new pH calculation
void simpLinReg(float* x, float* y, float* lrCoef, int n){
  // pass x and y arrays (pointers), lrCoef pointer, and n.  The lrCoef array is comprised of the slope=lrCoef[0] and intercept=lrCoef[1].  n is length of the x and y arrays.
  // http://en.wikipedia.org/wiki/Simple_linear_regression

  // initialize variables
  float xbar=0;
  float ybar=0;
  float xybar=0;
  float xsqbar=0;
  
  // calculations required for linear regression
  for (int i=0; i<n; i++){
    xbar=xbar+x[i];
    ybar=ybar+y[i];
    xybar=xybar+x[i]*y[i];
    xsqbar=xsqbar+x[i]*x[i];
  }
  // Note: Using summed values directly (not normalized) per newPH.cpp formula
  
  // simple linear regression algorithm (updated from newPH.cpp)
  lrCoef[0]=(n*xybar-xbar*ybar)/(n*xsqbar-xbar*xbar);
  lrCoef[1]=(ybar/n)-(lrCoef[0]*(xbar/n));
}

// altered code from: https://wiki.dfrobot.com/PH_meter_SKU__SEN0161_
float get_average(float* arr, int length) {
  int i;
  float max, min;
  float avg;
  long amount = 0;
  if (length <= 0) {
    Serial.println("Error length for the array to averaging!/n");
    return 0;
  }
  if (length < 5) {
    for (i = 0; i < length; i++) {
      amount += arr[i];
    }
    avg = amount / length;
    return avg;
  } else {
    if (arr[0] < arr[1]) {
      min = arr[0];
      max = arr[1];
    } else {
      min = arr[1];
      max = arr[0];
    }
    for (i = 2; i < length; i++) {
      if (arr[i] < min) {
        amount += min;
        min = arr[i];
      } else {
        if (arr[i] > max) {
          amount += max;
          max = arr[i];
        } else {
          amount += arr[i];
        }
      }
    }
    avg = (double)amount / (length - 2);
  }
  return avg;
}

// Calibration routine updated from newPH.cpp
// Note: xArray/yArray swapped to match new formula (pH = slope*voltage + offset)
void calibrate(float* lrCoef) {
  float yArray[3] = {4, 7, 10}; // Known pH values
  float xArray[3]; // Measured voltages
  
  int numOfReadings = 50;
  for (int i = 0; i < 3; i++) {
    int doneRinsing = 0;
    Serial.println("Rinse the probe and then enter y");
    
    while (doneRinsing == 0) {
      if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        userInput.trim();

        if (userInput == "y") {
          Serial.println("Wait 1 minute for values to stabilise");
          delay(60000);
          Serial.println("Taking average now");
          doneRinsing = 1;
        }
        else {
          Serial.println("Ignored... please type y, after rinsing.");
        }
      }
    }

    float voltageSum = 0;
    for (int j = 0; j < numOfReadings+1; j++) { // Changed loop var to 'j' to avoid shadowing
      voltageSum = voltageSum + analogRead(SENSOR_PIN) * 3.3 / 1024.0;
      delay(100);
    }

    float averageVoltage = voltageSum / numOfReadings;

    xArray[i] = averageVoltage; // Store voltage in xArray (swapped from original)
    Serial.println("Done, rinse now.");
  }
  simpLinReg(xArray, yArray, linearCoefficients, 3);
  
  // Output calibration results (from newPH.cpp)
  Serial.print("Slope: ");
  Serial.print(linearCoefficients[0]);
  Serial.print(" Y-intercept: ");
  Serial.print(linearCoefficients[1]);
  Serial.println();
}

/**
 * @brief Manually pulses a pump for a given duration.
 * This is a blocking function.
 */
void pulsePump(int pin, int duration) {
  if (pin == ACID_PIN) Serial.print("Manual Pulse: ACID");
  if (pin == ALKALI_PIN) Serial.print("Manual Pulse: ALKALI");
  Serial.printf(" for %d ms\n", duration);

  digitalWrite(pin, HIGH);
  delay(duration); 
  digitalWrite(pin, LOW);
}

// --- Interface Functions ---

void setupPH() {
  // Serial.begin(200000); // Handled by main.ino

  pinMode(ACID_PIN, OUTPUT);
  pinMode(ALKALI_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  digitalWrite(ACID_PIN, LOW);
  digitalWrite(ALKALI_PIN, LOW);

  // Calibration is now optional - using pre-calibrated defaults
  // Uncomment the line below to enable calibration on startup
  // calibrate(linearCoefficients);
  doneCalibrating = 1;
  timeAfterCalibration = millis(); // Track time from startup
}

void executePH() {
  if (doneCalibrating == 1) {
    // Check for serial input to change target pH (from newPH.cpp)
    if (Serial.available()) {
      String userInput = Serial.readStringUntil('\n');
      userInput.trim();

      if (userInput.length() > 0) {
        targetPH = userInput.toFloat();
        Serial.println("Input received, changing pH");
      }
    }
    
    // read voltage and convert to pH (updated formula from newPH.cpp)
    float voltage = analogRead(SENSOR_PIN) * 3.3 / 1024.0;
    float pHValue = (linearCoefficients[0] * voltage) + linearCoefficients[1];
    pHArray[pHArrayIndex++] = pHValue;

    // once buffer full, calculate average
    if (pHArrayIndex >= ARRAY_LENGTH) {
      currentPH = get_average(pHArray, ARRAY_LENGTH);
      pHArrayIndex = 0;

      // bang-bang control
      acid_on = false;
      alkali_on = false;

      // Safety check: only activate pumps if targetPH is set (from newPH.cpp)
      if (targetPH != 0.0) {
        if (currentPH > (targetPH + tolerance)) {
          // pH too high, add acid
          digitalWrite(ACID_PIN, HIGH);
          digitalWrite(ALKALI_PIN, LOW);
          acid_on = true;
        } else if (currentPH < (targetPH - tolerance)) {
          // pH too low, add alkali
          digitalWrite(ACID_PIN, LOW);
          digitalWrite(ALKALI_PIN, HIGH);
          alkali_on = true;
        } else {
          // pH within tolerance, turn off both pumps
          digitalWrite(ACID_PIN, LOW);
          digitalWrite(ALKALI_PIN, LOW);
        }
      } else {
        // No target set, ensure both pumps are off
        digitalWrite(ACID_PIN, LOW);
        digitalWrite(ALKALI_PIN, LOW);
      }

      // Time tracking relative to calibration (from newPH.cpp)
      timeMS = millis() - timeAfterCalibration;
      if (timeMS - t1 > 0) {
        t1 = t1 + 1000;
        Serial.print("time: ");
        Serial.print(t1 / 1000);
        Serial.print(" | ");
        Serial.print("current pH: ");
        Serial.print(currentPH);
        Serial.print(" | ");
        Serial.print("set pH: ");
        Serial.print(targetPH);
        Serial.print(" | ");
        Serial.print("alkali: ");
        Serial.print(alkali_on);
        Serial.print(" | ");
        Serial.print("acid: ");
        Serial.print(acid_on);
        Serial.println();
      }
    }

    delay(1);  // Reduced delay from newPH.cpp (was 100ms)
  }
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
      pulsePump(ACID_PIN, duration);
    } else if (strcmp(pump, "base") == 0) {
      pulsePump(ALKALI_PIN, duration);
    }
    
    char responseTopic[100];
    sprintf(responseTopic, "v1/devices/me/rpc/response/%s", requestId.c_str());
    String responsePayload = "{\"status\": \"ok\", \"pump\": \"" + String(pump) + "\"}";
    client.publish(responseTopic, responsePayload.c_str());
    
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