import os
import pandas as pd
import time
import sys

# Ensure we can import the modules
sys.path.append(os.getcwd())

def verify_pipeline():
    print("## Starting Pipeline Verification\n")
    
    # 1. Simulate Logger Output
    # Create a dummy CSV with normal and anomalous data
    csv_file = "logs/test_bioreactor_data.csv"
    print(f"Creating test data: {csv_file}")
    
    data = [
        # Normal data
        {"timestamp": time.time(), "temp_mean": 30.0, "ph_mean": 5.0, "rpm_mean": 1000, "heater_pwm": 0, "motor_pwm": 100, "acid_pwm": 0, "base_pwm": 0, "faults": "None"},
        {"timestamp": time.time()+1, "temp_mean": 30.01, "ph_mean": 5.01, "rpm_mean": 1005, "heater_pwm": 0, "motor_pwm": 100, "acid_pwm": 0, "base_pwm": 0, "faults": "None"},
        # Anomaly: High Temp (Z-Score & Hysteresis)
        {"timestamp": time.time()+2, "temp_mean": 35.0, "ph_mean": 5.0, "rpm_mean": 1000, "heater_pwm": 100, "motor_pwm": 100, "acid_pwm": 0, "base_pwm": 0, "faults": "None"},
        # Anomaly: Low pH
        {"timestamp": time.time()+3, "temp_mean": 30.0, "ph_mean": 2.0, "rpm_mean": 1000, "heater_pwm": 0, "motor_pwm": 100, "acid_pwm": 100, "base_pwm": 0, "faults": "None"},
    ]
    
    df = pd.DataFrame(data)
    df.to_csv(csv_file, index=False)
    
    # 2. Run Analysis
    print("Running anomaly_analysis.py...")
    exit_code = os.system(f"{sys.executable} anomaly_analysis.py --csv {csv_file}")
    
    if exit_code != 0:
        print("Analysis script failed!")
        return
        
    # 3. Check Results
    if os.path.exists("logs/anomalies.csv"):
        print("\nanomalies.csv created.")
        anomalies = pd.read_csv("logs/anomalies.csv")
        print(f"Found {len(anomalies)} anomalies.\n")
        print(anomalies[["signal", "detector", "value"]].to_markdown(index=False))
        
        # Simple validation
        if len(anomalies) > 0:
            print("\nVerification PASSED: Anomalies detected.")
        else:
            print("\nVerification FAILED: No anomalies detected in bad data.")
    else:
        print("\nVerification FAILED: logs/anomalies.csv not found.")

    # Cleanup
    # os.remove(csv_file)
    # if os.path.exists("logs/anomalies.csv"): os.remove("logs/anomalies.csv")

if __name__ == "__main__":
    verify_pipeline()
