#define SENSOR_PIN A4
#define ACID_PIN 8
#define ALKALI_PIN 9

float targetPH = 0.0;
const float tolerance = 0.4;

const int ARRAY_LENGTH = 10;
float pHArray[ARRAY_LENGTH];
int pHArrayIndex = -1; // changed to -1

long timeMS, t1;
long timeAfterCalibration;

float linearCoefficients[2] = {1.38, 0.76}; //slope, offset

int doneCalibrating = 0;

void setup() {
  Serial.begin(115200);
  pinMode(ACID_PIN, OUTPUT);
  pinMode(ALKALI_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  digitalWrite(ACID_PIN, LOW);
  digitalWrite(ALKALI_PIN, LOW);
}

// use interrupt so it only reads if pH changes
// do PID
void loop() {
  if (doneCalibrating == 0) {
    //calibrate(linearCoefficients);
    doneCalibrating = 1;
    timeAfterCalibration = millis();
  }

  if (doneCalibrating == 1) {
    if (Serial.available()) {
      String userInput = Serial.readStringUntil('\n');
      userInput.trim();

      if (userInput.length() > 0) {
        targetPH = userInput.toFloat();
        Serial.println("Input received, changing pH");
      }
    }
      // read voltage and convert to pH
      float voltage = analogRead(SENSOR_PIN) * 3.3 / 1024.0;
      float pHValue = (linearCoefficients[0] * voltage) + linearCoefficients[1];
      pHArray[pHArrayIndex++] = pHValue;

      // once buffer full, calculate average
      if (pHArrayIndex >= ARRAY_LENGTH - 1) { // changed -1
        float currentPH = get_average(pHArray, ARRAY_LENGTH);
        pHArrayIndex = 0;
        bool acid_on = false;
        bool alkali_on = false;
        if (targetPH != 0.0) {
          // bang-bang control

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
        }
        else {
          digitalWrite(ACID_PIN, LOW);
          digitalWrite(ALKALI_PIN, LOW);
        }
        timeMS = millis() - timeAfterCalibration;
        if (timeMS - t1 > 0) {
          t1 = t1 + 1000;
          // Serial.println(currentPH, acid_on, alkali_on)
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
        } else {
        }
      }

      // set a timer for micro seconds
      // assign a variable to microseconds
      delay(1);  // Small delay between samples
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


void calibrate(float* lrCoef) {
  float yArray[3] = {4, 7, 10};
  float xArray[3];
  float knownPH = 1.0;
  
  int numOfReadings = 50;
  for (int i = 0; i < 3; i++) {
    int doneRinsing = 0;
    // float voltage = analogRead(SENSOR_PIN) * 3.3 / 1024.0;
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
    for (int i = 0; i < numOfReadings+1; i++) {
      voltageSum = voltageSum + analogRead(SENSOR_PIN) * 3.3 / 1024.0;
      delay(100);
    }

    float averageVoltage = voltageSum / numOfReadings;

    xArray[i] = averageVoltage;
    Serial.println("Done, rinse now.");
  }
  simpLinReg(xArray, yArray, linearCoefficients, 3);

  Serial.print("Slope: ");
  Serial.print(linearCoefficients[0]);
  Serial.print("Y-intercept: ");
  Serial.print(linearCoefficients[1]);
  Serial.println();
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
  // xbar=xbar/n;
  // ybar=ybar/n;
  // xybar=xybar/n;
  // xsqbar=xsqbar/n;
  
  // simple linear regression algorithm
  lrCoef[0]=(n*xybar-xbar*ybar)/(n*xsqbar-xbar*xbar);
  lrCoef[1]=(ybar/n)-(lrCoef[0]*(xbar/n));
}
