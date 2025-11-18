// -------------------------------------------------------------
// PI MOTOR SPEED CONTROLLER (Arduino Nano ESP32, PWM on D10)
// -------------------------------------------------------------

// VERY IMPORTANT - CHECK POWER SUPPLY AND ADJUST HERE
float MotorSupplyVoltage = 5.0;   // CHANGE HERE if you switch supply voltage

// Motor (plant) and sensor parameters
const float Kv = 250;
const float T = 0.15;
const float Npulses = 70;
const int   RPMmax = 1500;
const int   Tmin   = 6e7 / RPMmax / Npulses;

// Desired control parameters
const float wn   = 1 / T;   // Fast design
const float zeta = 1;

// PI gains (from lecture notes)
const float wo = 1 / T;
const float Kp = (2 * zeta * wn / wo - 1) / Kv;
const float KI = (wn * wn) / (Kv * wo);

const float freqtoRPM = 60.0 / Npulses;
float pwmScale = 1023.0 / MotorSupplyVoltage;

// Pins (Nano ESP32)
const byte encoderpin = 2;   // D2 → Hall sensor
const byte motorpin   = 10;  // D10 → MOSFET gate (PWM)

// Variables
float measspeed      = 0;
float meanmeasspeed  = 0;
float error          = 0;
float KIinterror     = 0;
float deltaT         = 0;
float setspeed       = 0;    // RPM setpoint

long currtime, prevtime, T1;
long pulseTime;

// Pulse timestamp buffer (8 samples = 7 intervals)
const byte pulseBufferSize = 8;
long pulseT[pulseBufferSize];

int  Vmotor = 0;
int  count  = 0;
bool blinkk = false;

// ISR prototype
void IRAM_ATTR Tsense();

// -------------------------------------------------------------
void setup() {

  pinMode(encoderpin, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);

  // PWM on D10
  ledcSetup(0, 20000, 10);
  ledcAttachPin(motorpin, 0);

  // Hall sensor interrupt
  attachInterrupt(digitalPinToInterrupt(encoderpin), Tsense, RISING);

  Serial.begin(2000000);

  long t = micros();
  for (int i = 0; i < pulseBufferSize; i++) {
    pulseT[i] = t;
  }
  prevtime = t;
  T1       = t;
}

// -------------------------------------------------------------
void loop() {

  // 1. Serial command → set RPM
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() > 0) {
      int val = cmd.toInt();
      if ((val >= 500 && val <= 1500) || val == 0) {
        setspeed = val;
        Serial.print("Set speed updated to: ");
        Serial.println(setspeed);
      } else {
        Serial.println("Ignored: setpoint must be 0 or 500-1500 RPM");
      }
    }
  }

  // 2. Control loop (10 ms)
  currtime = micros();
  deltaT   = (currtime - prevtime) * 1e-6;

  if (currtime - T1 > 0) {

    prevtime = currtime;
    T1      += 10000;  // 10 ms

    long Tsens = pulseT[0] - pulseT[7];
    if (Tsens <= 0) Tsens = 1;

    measspeed = 7 * freqtoRPM * 1e6 / float(Tsens);

    error = setspeed - measspeed;

    // PI controller
    KIinterror += KI * error * deltaT;
    KIinterror  = constrain(KIinterror, 0, MotorSupplyVoltage);

    Vmotor = round(pwmScale * (Kp * error + KIinterror));

    if (currtime - pulseTime > 100000) {
      measspeed = 0;
    }

    Vmotor = constrain(Vmotor, 0, 1023);
    ledcWrite(0, Vmotor);

    meanmeasspeed = 0.1 * measspeed + 0.9 * meanmeasspeed;

    // 3. Serial Plotter logging
    unsigned long ms = currtime / 1000ul;

    Serial.print("time:");
    Serial.print(ms);
    Serial.print("  set:");
    Serial.print(setspeed);
    Serial.print("  rpm:");
    Serial.println(meanmeasspeed);
  }
}

// -------------------------------------------------------------
// INTERRUPT: Measure Hall pulses (integer math only)
// -------------------------------------------------------------
void IRAM_ATTR Tsense() {

  pulseTime = micros();

  if (abs(pulseTime - pulseT[0]) > Tmin) {

    // Shift buffer
    for (int i = pulseBufferSize - 1; i > 0; i--) {
      pulseT[i] = pulseT[i - 1];
    }

    pulseT[0] = pulseTime;

    // LED flash once per revolution
    count += 2;
    if (count > int(Npulses)) {
      count -= int(Npulses);
      digitalWrite(LED_RED, blinkk);
      blinkk = !blinkk;
    }
  }
}