# Data Analysis & Anomaly Detection

This directory contains scripts for analyzing bioreactor telemetry and detecting anomalies.

## Files

- `anomaly_analysis.py`: Main script for real-time (MQTT) or offline (CSV) anomaly detection.
- `detectors.py`: Implementation of statistical detectors (Z-Score, Hysteresis, Sliding Window).
- `telemetry_logger.py`: **[NEW]** Bridge script to log live bioreactor telemetry to CSV.

## Usage

### 1. Live Data Logging

To capture data from the bioreactor and save it for analysis:

```bash
python telemetry_logger.py
```

This will create `logs/bioreactor_data.csv`.

### 2. Offline Analysis

To run the anomaly detection algorithms on the captured CSV data:

```bash
python anomaly_analysis.py --csv logs/bioreactor_data.csv
```

### 3. Real-time Simulation Analysis

To run against the `bioreactor_sim` MQTT stream (legacy mode):

```bash
python anomaly_analysis.py
```

## Data Pipeline

1. **Source**: Bioreactor publishes JSON telemetry to MQTT.
2. **Logger**: `telemetry_logger.py` subscribes to MQTT, flattens the JSON, maps keys (e.g., `temperature` -> `temp_mean`), and appends to CSV.
3. **Analysis**: `anomaly_analysis.py` reads the CSV, feeds data points into `detectors.py`, and logs any detected faults to `logs/anomalies.csv`.
