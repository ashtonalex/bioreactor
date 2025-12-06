#include "StirringSubsystem.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>

// -------------------------------------------------------------
// PI MOTOR SPEED CONTROLLER IMPLEMENTATION
// -------------------------------------------------------------

// --- Motor and Control Parameters (Constants) ---
float MotorSupplyVoltage = 5.0; // IMPORTANT: Define supply voltage here
const float Kv = 250;           // Motor Velocity Constant
const float T = 0.15;           // Time Constant
const float Npulses = 70;       // Pulses per motor revolution
int RPM_MAX = 1500;             // Max allowed RPM
const int CONTROL_INTERVAL_US = 10000; // 10 ms control loop

// --- Calculated Control Gains ---
const float wn = 1.0 / T;
const float zeta = 1.0;
const float wo = 1.0 / T;
const float Kp = (2.0 * zeta * wn / wo - 1.0) / Kv;
const float KI = (wn * wn) / (Kv * wo);

// --- Conversion Factors and PWM Setup ---
const float freqtoRPM = 60.0 / Npulses;
float pwmScale = 1023.0 / MotorSupplyVoltage;

// --- System Variables (Shared with .hpp) ---
float setspeed = 0;             // RPM setpoint
float meanmeasspeed = 0;        // Filtered measured RPM

// --- Internal Variables ---
volatile long pulseT[8];        
volatile long pulseTime = 0;    
volatile int count = 0;         
volatile bool blinkk = false;   

static long currtime, prevtime, T1;
static float measspeed = 0;
static float error = 0;
static float KIinterror = 0;
static float deltaT = 0;
static int Vmotor = 0;
static int currentPWM = 0;  // Track current PWM for soft-start ramping


// -------------------------------------------------------------
// 1. INTERRUPT SERVICE ROUTINE (ISR)
// -------------------------------------------------------------
void IRAM_ATTR Tsense() {
  const int Tmin = 60000000 / RPM_MAX / Npulses; 

  pulseTime = micros();

  if (abs(pulseTime - pulseT[0]) > Tmin) {
    for (int i = 7; i > 0; i--) {
      pulseT[i] = pulseT[i - 1];
    }

    pulseT[0] = pulseTime;

    count += 2;
    if (count > int(Npulses)) {
      count -= int(Npulses);
      digitalWrite(LED_RED_PIN, blinkk);
      blinkk = !blinkk;
    }
  }
}

// -------------------------------------------------------------
// 2. SETUP FUNCTION
// -------------------------------------------------------------
void setupStirring() {
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  pinMode(LED_RED_PIN, OUTPUT);

  // PWM setup on MOTOR_PIN (D10)
  ledcAttach(MOTOR_PIN, 20000, 10);
  ledcWrite(MOTOR_PIN, 0); 

  // Hall sensor interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), Tsense, RISING);

  // Initialize pulse buffer timestamps
  long t = micros();
  for (int i = 0; i < 8; i++) {
    pulseT[i] = t;
  }
  prevtime = t;
  T1 = t;
}

// -------------------------------------------------------------
// 3. EXECUTION FUNCTION
// -------------------------------------------------------------
void executeStirring() {
  // 1. Safety Check: If system is not active, force off and exit
  if (!is_system_active) {
    ledcWrite(MOTOR_PIN, 0); // Force PWM duty cycle to 0
    return; 
  }

  // --- A. Serial Command Input (Local Test Override) ---
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() > 0) {
      int val = cmd.toInt();
      if ((val >= 500 && val <= RPM_MAX) || val == 0) {
        setspeed = val;
        Serial.print("Set speed updated to: ");
        Serial.println(setspeed);
      } else {
        Serial.println("Ignored: setpoint must be 0 or 500-1500 RPM");
      }
    }
  }

  // --- B. PI Control Loop (Runs every 10 ms) ---
  currtime = micros();
  
  if (currtime - T1 >= 0) {

    deltaT = (currtime - prevtime) * 1e-6; 
    prevtime = currtime;
    T1 += CONTROL_INTERVAL_US; 

    // Disable interrupts while reading ISR-shared variables to prevent race conditions
    noInterrupts();
    long Tsens = pulseT[0] - pulseT[7];
    long localPulseTime = pulseTime;
    interrupts();
    
    if (Tsens <= 0) Tsens = 1;

    measspeed = 7.0 * freqtoRPM * 1e6 / (float)Tsens;

    if (currtime - localPulseTime > 100000) {
      measspeed = 0;
    }

    error = setspeed - measspeed;

    // PI controller implementation
    KIinterror += KI * error * deltaT;
    KIinterror = constrain(KIinterror, 0, MotorSupplyVoltage); 

    Vmotor = round(pwmScale * (Kp * error + KIinterror));

    Vmotor = constrain(Vmotor, 0, 1023); 
    
    // Soft-start: Ramp PWM gradually to prevent power spikes
    // Max change of 50 units per control cycle (~10ms) = ~500ms for full ramp
    if (Vmotor > currentPWM) {
      currentPWM = min(currentPWM + 50, Vmotor);
    } else if (Vmotor < currentPWM) {
      currentPWM = max(currentPWM - 50, Vmotor);
    }
    
    ledcWrite(MOTOR_PIN, currentPWM);

    // Filtered RPM for display
    meanmeasspeed = 0.1 * measspeed + 0.9 * meanmeasspeed;

    // Serial Plotter logging
    unsigned long ms = currtime / 1000ul;

    // Serial.print("time:");
    // Serial.print(ms);
    // Serial.print("  set:");
    // Serial.print(setspeed);
    // Serial.print("  rpm:");
    // Serial.println(meanmeasspeed);
    
  }
}

// -------------------------------------------------------------
// 4. MQTT STATUS PUBLISH 
// -------------------------------------------------------------
void getStirringStatus(JsonObject& doc) {
  doc["rpm_set"] = (int)setspeed; // Include the current setpoint
  doc["rpm_measured"] = (int)meanmeasspeed; 
}


// -------------------------------------------------------------
// 5. MQTT COMMAND HANDLER (RPC)
// -------------------------------------------------------------
/**
 * @brief Handles the 'setRPM' RPC call.
 */
void handleStirringAttributes(JsonObject& doc) {
  if (doc.containsKey("target_rpm")) {
    int new_rpm = doc["target_rpm"];
    if ((new_rpm >= 500 && new_rpm <= RPM_MAX) || new_rpm == 0) {
      setspeed = (float)new_rpm;
      Serial.print("Updated setspeed (RPM): ");
      Serial.println(setspeed);
    } else {
      Serial.println("Attribute Error: target_rpm outside valid range (0 or 500-1500).");
    }
  }
}