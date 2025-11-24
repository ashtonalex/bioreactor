#define SENSOR_PIN A0
#define ACID_PIN 5
#define ALKALI_PIN 6

const float SLOPE = 0.54; // need to redo calibration - eugene was so scuffed bro
const float OFFSET = 0.58;

float targetPH = 5.0;
const float tolerance = 0.4;

const int ARRAY_LENGTH = 10;
float pHArray[ARRAY_LENGTH];
int pHArrayIndex = 0;

void setup() {
    Serial.begin(9600);
    
    pinMode(ACID_PIN, OUTPUT);
    pinMode(ALKALI_PIN, OUTPUT);
    pinMode(SENSOR_PIN, INPUT);
    
    digitalWrite(ACID_PIN, LOW);
    digitalWrite(ALKALI_PIN, LOW);
}

void loop() {
    // read voltage and convert to pH
    float voltage = analogRead(SENSOR_PIN) * 5.0 / 1024.0; 
    float pHValue = (SLOPE * voltage) + OFFSET;
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
        } 
        else if (currentPH < targetPH - tolerance) {
            // pH too low, add alkali
            digitalWrite(ACID_PIN, LOW);
            digitalWrite(ALKALI_PIN, HIGH);
            alkali_on = true;
        } 
        else {
            // pH within tolerance, turn off both pumps
            digitalWrite(ACID_PIN, LOW);
            digitalWrite(ALKALI_PIN, LOW);
        }
        
        // print data
        print_readable(currentPH, acid_on, alkali_on);
    }
    
    delay(100);  // Small delay between samples
}

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

// altered code from: https://wiki.dfrobot.com/PH_meter_SKU__SEN0161_
float avergearray(float* arr, int length){
  float i;
  float max,min;
  float avg;
  long amount=0;
  if(length<=0){
    Serial.println("Error length for the array to avraging!/n");
    return 0;
  }
  if(length<5){
    for(i=0;i<length;i++){
      amount+=arr[i];
    }
    avg = amount/length;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];
      max=arr[1];
    }
    else{
      min=arr[1];
      max=arr[0];
    }
    for(i=2;i<length;i++){
      if(arr[i]<min){
        amount+=min;
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;
          max=arr[i];
        }else{
          amount+=arr[i];
        }
      }
    }
    avg = (double)amount/(length-2);
  }
  return avg;
}

void print_data(float pH, bool acid, bool alkali) {
    Serial.print("pH: ");
    Serial.print(pH, 2);
    Serial.print(" | Target: ");
    Serial.print(targetPH, 2);
    Serial.print(" | Acid: ");
    Serial.print(acid ? "ON " : "OFF");
    Serial.print(" | Alkali: ");
    Serial.println(alkali ? "ON " : "OFF");
}
