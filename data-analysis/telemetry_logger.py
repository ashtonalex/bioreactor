import paho.mqtt.client as mqtt
import json
import time
import os
import sys

# Configuration
# Update these to match your actual MQTT broker settings
BROKER = "mqtt.eu.thingsboard.cloud" 
PORT = 1883
ACCESS_TOKEN = "ujp5e0v81hgvbkr2e4el"  # Replace with your device access token

TOPIC = "v1/devices/me/telemetry" 
OUTPUT_FILE = "logs/bioreactor_data.csv"

def on_connect(client, userdata, flags, rc, properties=None):
    print(f"Connected with result code {rc}")
    client.subscribe(TOPIC)
    print(f"Subscribed to {TOPIC}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        # Transform data to match anomaly_analysis.py expectations
        # Expected columns: timestamp,temp_mean,ph_mean,rpm_mean,heater_pwm,motor_pwm,acid_pwm,base_pwm,faults
        
        timestamp = time.time()
        
        # Mapping logic
        temp_mean = data.get("temperature", 0.0)
        ph_mean = data.get("pH", 0.0)
        rpm_mean = data.get("rpm_measured", 0.0)
        
        # Map booleans/states to PWM (0 or 100) if actual PWM not available
        heater_pwm = 100 if data.get("heater_state", False) else 0
        motor_pwm = 100 if data.get("rpm_set", 0) > 0 else 0 # Simplified assumption
        acid_pwm = 100 if data.get("acid_pump", False) else 0
        base_pwm = 100 if data.get("base_pump", False) else 0
        
        faults = "None" # Placeholder as faults might not be in standard telemetry
        
        row = f"{timestamp},{temp_mean},{ph_mean},{rpm_mean},{heater_pwm},{motor_pwm},{acid_pwm},{base_pwm},{faults}\n"
        
        with open(OUTPUT_FILE, "a") as f:
            f.write(row)
            
        print(f"Logged: {row.strip()}")
        
    except Exception as e:
        print(f"Error processing message: {e}")

def setup_csv():
    if not os.path.exists(OUTPUT_FILE):
        with open(OUTPUT_FILE, "w") as f:
            f.write("timestamp,temp_mean,ph_mean,rpm_mean,heater_pwm,motor_pwm,acid_pwm,base_pwm,faults\n")

if __name__ == "__main__":
    setup_csv()
    
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    
    # Authenticate with ThingsBoard using access token
    client.username_pw_set(ACCESS_TOKEN)
    
    print(f"Connecting to {BROKER}...")
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("Logging stopped.")
    except Exception as e:
        print(f"Connection failed: {e}")
