import paho.mqtt.client as mqtt
import json
import pandas as pd
import os
import time

from detectors import (
    ZScoreDetector,
    HysteresisDetector,
    SlidingWindowDetector,
)

BROKER = "engf0001.cs.ucl.ac.uk"
PORT = 1883
STREAM = "nofaults"
TOPIC = f"bioreactor_sim/{STREAM}/telemetry/summary"

with open(f"{STREAM}_data.csv", "w") as f:
    f.write("timestamp,temp_mean,ph_mean,rpm_mean,heater_pwm,motor_pwm,acid_pwm,base_pwm,faults\n")

with open("anomalies.csv", "w") as f:
    f.write("timestamp,signal,detector,value,score,anomaly\n")

detectors = {
    "temp_mean": {
        "z": ZScoreDetector(window_size=50, threshold=2),
        "hyst": HysteresisDetector(low_threshold=29.97, high_threshold=30.02),  # Fixed: Better range
        "win": SlidingWindowDetector(window_size=30, threshold=0.05, ideal_value= 30),
    },
    "ph_mean": {
        "z": ZScoreDetector(window_size=50, threshold=2),
        "hyst": HysteresisDetector(low_threshold=4.8, high_threshold=5.2),  # Fixed: Better range
        "win": SlidingWindowDetector(window_size=30, threshold=0.3, ideal_value= 5),
    },
    "rpm_mean": {
        "z": ZScoreDetector(window_size=50, threshold=2),
        "hyst": HysteresisDetector(low_threshold=990, high_threshold=1110),  # Fixed: Better range
        "win": SlidingWindowDetector(window_size=30, threshold=10, ideal_value= 1000),
    }
}

def log_anomaly(signal, detector, value, score):

    with open("anomalies.csv", "a") as f:
        f.write(f"{time.time()},{signal},{detector},{value},{score},1\n")


def on_connect(client, userdata, flags, rc):
    print(f"✓ Connected to broker with result code: {rc}")
    client.subscribe(TOPIC)
    print(f"✓ Subscribed to topic: {TOPIC}")


def on_message(client, userdata, msg):
    raw = msg.payload.decode()
    print("\n" + "=" * 60)
    print("📨 Received message")
    print("=" * 60)

    try:
        data = json.loads(raw)

        row = {
            "timestamp": time.time(),
            "temp_mean": data["temperature_C"]["mean"],
            "ph_mean": data["pH"]["mean"],
            "rpm_mean": data["rpm"]["mean"],
            "heater_pwm": data["actuators_avg"]["heater_pwm"],
            "motor_pwm": data["actuators_avg"]["motor_pwm"],
            "acid_pwm": data["actuators_avg"]["acid_pwm"],
            "base_pwm": data["actuators_avg"]["base_pwm"],
            "faults": ",".join(data["faults"]["last_active"])
        }

        pd.DataFrame([row]).to_csv(f"{STREAM}_data.csv", mode="a", header=False, index=False)
        print(f"💾 Data saved: temp={row['temp_mean']:.2f}, pH={row['ph_mean']:.2f}, rpm={row['rpm_mean']:.2f}")

        for signal in ["temp_mean", "ph_mean", "rpm_mean"]:
            value = row[signal]
            D = detectors[signal]

            is_z_anom, z = D["z"].update(value)
            if is_z_anom:
                print(f"  🚨 [Z-SCORE] {signal} anomaly detected! z={z:.2f}")
                log_anomaly(signal, "zscore", value, z)

            is_hyst = D["hyst"].update(value)  # <-- FIXED (no z-score)
            if is_hyst:
                print(f"  ⚠️  [HYSTERESIS] {signal} sustained anomaly! value={value:.2f}")
                log_anomaly(signal, "hysteresis", value, value)

            is_drift, drift = D["win"].update(value)
            if is_drift:
                print(f"  📈 [DRIFT] {signal} showing drift={drift:.2f}")
                log_anomaly(signal, "sliding_window", value, drift)

    except Exception as e:
        print(f"❌ Error processing message: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    # Create MQTT client and register callbacks
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    # Connect to broker and start listening
    print(f"🔌 Connecting to {BROKER}:{PORT} ...")
    client.connect(BROKER, PORT, 60)

    print("👂 Listening for messages (Ctrl+C to stop)...\n")
    client.loop_forever()