'''
#define SENSOR_PIN A4
#define ACID_PIN 8
#define ALKALI_PIN 9

// const float SLOPE = 5.56;  // need to redo calibration - eugene was so scuffed bro
// const float OFFSET = 0.778;

float targetPH = 5.0;
const float tolerance = 0.4;

const int ARRAY_LENGTH = 10;
float pHArray[ARRAY_LENGTH];
int pHArrayIndex = 0;

long timeMS, t1;

float linearCoefficients[2] = {0, 0};

int doneCalibrating = 0;

void setup() {
  Serial.begin(200000);

  pinMode(ACID_PIN, OUTPUT);
  pinMode(ALKALI_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  digitalWrite(ACID_PIN, LOW);
  digitalWrite(ALKALI_PIN, LOW);

  calibrate(linearCoefficients);
  doneCalibrating = 1;
}

// void loop() {
//   // digitalWrite(ALKALI_PIN, HIGH);
//   printf("Hi")
// }

// use interrupt so it only reads if pH changes
// do PID
void loop() {
  if (doneCalibrating == 1) {
    // read voltage and convert to pH
    float voltage = analogRead(SENSOR_PIN) * 3.3 / 1024.0;
    float pHValue = (voltage - linearCoefficients[1])/linearCoefficients[0];
    pHArray[pHArrayIndex++] = pHValue;

    // once buffer full, calculate average
    if (pHArrayIndex >= ARRAY_LENGTH) {
      float currentPH = get_average(pHArray, ARRAY_LENGTH);
      pHArrayIndex = 0;

      // bang-bang control
      bool acid_on = false;
      bool alkali_on = false;

      if (currentPH > targetPH + tolerance) {
        // pH too high, add acid
        digitalWrite(ACID_PIN, HIGH);
        digitalWrite(ALKALI_PIN, LOW);
        acid_on = true;
      } else if (currentPH < targetPH - tolerance) {
        // pH too low, add alkali
        digitalWrite(ACID_PIN, LOW);
        digitalWrite(ALKALI_PIN, HIGH);
        alkali_on = true;
      } else {
        // pH within tolerance, turn off both pumps
        digitalWrite(ACID_PIN, LOW);
        digitalWrite(ALKALI_PIN, LOW);
      }

      timeMS = millis();
      if (timeMS - t1 > 0) {
        t1 = t1 + 1000;
        // Serial.println(currentPH, acid_on, alkali_on)
        Serial.print("time: ");
        Serial.print(t1 / 1000);
        Serial.print(" | ");
        Serial.print("current pH: ");
        Serial.print(currentPH);
        Serial.print(" | ");
        Serial.print("alkali: ");
        Serial.print(alkali_on);
        Serial.print(" | ");
        Serial.print("acid: ");
        Serial.print(acid_on);
        Serial.println();
      }
    }

    // set a timer for micro seconds
    // assign a variable to microseconds
    delay(100);  // Small delay between samples
  }
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

// void print_data(float pH, bool acid, bool alkali) {
//     Serial.print("pH: ");
//     Serial.print(pH, 2);
//     Serial.print(" | Target: ");
//     Serial.print(targetPH, 2);
//     Serial.print(" | Acid: ");
//     Serial.print(acid ? "ON " : "OFF");
//     Serial.print(" | Alkali: ");
//     Serial.println(alkali ? "ON " : "OFF");
// }

// make something to calibrate pH probe each time
// each time, the program waits(while rinsing) until after we type something into serial monitor
// then wait 1-2 minutes for values to stabilise
// record 10 values in 1 second, then take the average
// repeat 3 times for each pH solution
// make a line equation

void calibrate(float* lrCoef) {
  float xArray[3] = {4, 7, 10};
  float yArray[3];
  float knownPH = 1.0;
  
  int numOfReadings = 50;
  for (int i = 0; i < 3; i++) {
    int doneRinsing = 0;
    // float voltage = analogRead(SENSOR_PIN) * 3.3 / 1024.0;

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
    for (int i = 0; i < numOfReadings+1; i++) {
      voltageSum = voltageSum + analogRead(SENSOR_PIN) * 3.3 / 1024.0;
      delay(100);
    }

    float averageVoltage = voltageSum / numOfReadings;

    yArray[i] = averageVoltage;
    Serial.println("Done, rinse now.");
  }
  simpLinReg(xArray, yArray, linearCoefficients, 3);
}

// code from: https://jwbrooks.blogspot.com/2014/02/arduino-linear-regression-function.html?m=1
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
  xbar=xbar/n;
  ybar=ybar/n;
  xybar=xybar/n;
  xsqbar=xsqbar/n;
  
  // simple linear regression algorithm
  lrCoef[0]=(xybar-xbar*ybar)/(xsqbar-xbar*xbar);
  lrCoef[1]=ybar-lrCoef[0]*xbar;
}
'''