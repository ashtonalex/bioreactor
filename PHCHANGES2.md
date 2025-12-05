# Developer Report: pH_Control. ino

## Overview
This Arduino sketch implements a **pH control system** for a bioreactor.  It reads pH values from an analog sensor, applies calibration, and uses bang-bang control to maintain a target pH by activating acid or alkali pumps. 

---

## I/O Configuration

### Pin Definitions
| Pin | Constant | Mode | Purpose |
|-----|----------|------|---------|
| `A4` | `SENSOR_PIN` | INPUT | Analog pH sensor input |
| `8` | `ACID_PIN` | OUTPUT | Controls acid pump relay |
| `9` | `ALKALI_PIN` | OUTPUT | Controls alkali pump relay |

### Serial Communication
- **Baud Rate:** 115200
- **Input:** Reads target pH value from user via `Serial. readStringUntil('\n')`
- **Output:** Prints timestamped status including current pH, target pH, and pump states

---

## Variables

### Global Configuration
| Variable | Type | Initial Value | Description |
|----------|------|---------------|-------------|
| `targetPH` | `float` | `0.0` | User-defined target pH setpoint |
| `tolerance` | `const float` | `0.4` | Acceptable deviation from target (±0.4 pH units) |
| `linearCoefficients` | `float[2]` | `{1.38, 0.76}` | Calibration coefficients `[slope, offset]` |
| `doneCalibrating` | `int` | `0` | Flag indicating calibration status |

### Data Buffering
| Variable | Type | Initial Value | Description |
|----------|------|---------------|-------------|
| `ARRAY_LENGTH` | `const int` | `10` | Size of pH reading buffer |
| `pHArray` | `float[10]` | uninitialized | Circular buffer for pH readings |
| `pHArrayIndex` | `int` | `-1` | Current index in buffer (⚠️ starts at -1) |

### Timing
| Variable | Type | Description |
|----------|------|-------------|
| `timeMS` | `long` | Elapsed time since calibration |
| `t1` | `long` | Timer for 1-second output intervals |
| `timeAfterCalibration` | `long` | Timestamp when calibration completed |

---

## Methods

### `void setup()`
Initializes serial communication, configures pin modes, and sets pump outputs to LOW (off).

### `void loop()`
Main control loop that:
1. Checks/completes calibration
2. Reads target pH from serial input
3. Samples sensor voltage and converts to pH
4.  Buffers readings and calculates rolling average
5.  Implements bang-bang control logic
6. Outputs status every second

**Control Logic:**
```
if currentPH > (targetPH + 0.4) → Activate ACID pump
if currentPH < (targetPH - 0.4) → Activate ALKALI pump
otherwise                       → Both pumps OFF
```

### `float get_average(float* arr, int length)`
Calculates a **trimmed mean** by excluding min/max outliers (for arrays ≥5 elements).  Returns simple average for smaller arrays.

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `float*` | Pointer to array of values |
| `length` | `int` | Number of elements |
| **Returns** | `float` | Averaged value |

### `void calibrate(float* lrCoef)`
Three-point calibration routine using pH 4, 7, and 10 buffer solutions.

| Parameter | Type | Description |
|-----------|------|-------------|
| `lrCoef` | `float*` | Output array for `[slope, intercept]` |

**Process:**
1.  Prompt user to rinse probe
2. Wait for 'y' confirmation
3. Wait 60 seconds for stabilization
4. Take 50 voltage readings over 5 seconds
5.  Repeat for each buffer solution
6. Calculate linear regression coefficients

### `void simpLinReg(float* x, float* y, float* lrCoef, int n)`
Performs **simple linear regression** to determine calibration slope and intercept. 

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `float*` | Array of measured voltages |
| `y` | `float*` | Array of known pH values |
| `lrCoef` | `float*` | Output `[slope, intercept]` |
| `n` | `int` | Number of data points |

---

## ⚠️ Potential Issues

1. **Array Index Bug:** `pHArrayIndex` starts at `-1`, causing the first write to `pHArray[-1]` (undefined behavior)
2.  **Off-by-one in calibration:** Loop uses `numOfReadings+1` iterations but divides by `numOfReadings`
3. **Type mismatch in `get_average`:** Uses `long amount` to accumulate `float` values, causing precision loss
