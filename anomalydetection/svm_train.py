"""
SVM Training Script for Bioreactor Anomaly Detection

Subscribes to MQTT broker, collects normal (no-fault) data,
and trains a One-Class SVM model. Exports model parameters
for use in Python testing and ESP32 deployment.

Usage:
    python svm_train.py --samples 500
"""

import paho.mqtt.client as mqtt
import json
import numpy as np
import pickle
import time
import argparse
from sklearn.svm import OneClassSVM
from sklearn.preprocessing import StandardScaler

# MQTT Configuration
BROKER = "engf0001.cs.ucl.ac.uk"
PORT = 1883
STREAM = "nofaults"
TOPIC = f"bioreactor_sim/{STREAM}/telemetry/summary"

# Feature configuration
FEATURES = ["temp_mean", "ph_mean", "rpm_mean"]


class SVMTrainer:
    def __init__(self, target_samples=500, nu=0.02, gamma=0.002):
        self.target_samples = target_samples
        self.nu = nu
        self.gamma = gamma

        self.training_data = []
        self.sample_count = 0
        self.is_complete = False

        # Model components
        self.scaler = StandardScaler()
        self.svm = OneClassSVM(kernel='rbf', nu=nu, gamma=gamma)

    def add_sample(self, features):
        """Add a sample to training data."""
        self.training_data.append(features)
        self.sample_count += 1

        if self.sample_count >= self.target_samples:
            self.train()
            self.is_complete = True

    def train(self):
        """Train the SVM model."""
        print(f"\n{'='*60}")
        print("🎓 Training SVM Model")
        print(f"{'='*60}")

        data = np.array(self.training_data)
        print(f"  Training samples: {len(data)}")
        print(f"  Features: {FEATURES}")
        print(f"  Nu: {self.nu}, Gamma: {self.gamma}")

        # Fit scaler
        self.scaler.fit(data)
        scaled_data = self.scaler.transform(data)

        # Train SVM
        self.svm.fit(scaled_data)

        # Get support vectors and coefficients
        n_sv = len(self.svm.support_vectors_)
        print(f"  Support vectors: {n_sv}")
        print("✓ Training complete!")

    def save_python_model(self, filepath="svm_model.pkl"):
        """Save model for Python testing."""
        model_data = {
            'scaler_mean': self.scaler.mean_.tolist(),
            'scaler_scale': self.scaler.scale_.tolist(),
            'support_vectors': self.svm.support_vectors_.tolist(),
            'dual_coef': self.svm.dual_coef_.tolist(),
            'intercept': self.svm.intercept_.tolist(),
            'gamma': self.gamma,
            'nu': self.nu,
            'features': FEATURES,
        }

        with open(filepath, 'wb') as f:
            pickle.dump(model_data, f)

        print(f"💾 Python model saved to: {filepath}")

    def save_esp32_header(self, filepath="svm_model.h"):
        """Export model as C header file for ESP32."""
        sv = self.svm.support_vectors_
        dual_coef = self.svm.dual_coef_[0]
        intercept = self.svm.intercept_[0]

        lines = [
            "// SVM Model for ESP32 - Auto-generated",
            f"// Trained on {self.sample_count} samples",
            f"// Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "#ifndef SVM_MODEL_H",
            "#define SVM_MODEL_H",
            "",
            f"#define N_FEATURES {len(FEATURES)}",
            f"#define N_SUPPORT_VECTORS {len(sv)}",
            f"#define SVM_GAMMA {self.gamma}f",
            f"#define SVM_INTERCEPT {intercept}f",
            "",
            "// Feature names (for reference)",
            f"// {FEATURES}",
            "",
            "// Scaler parameters (mean)",
            "const float SCALER_MEAN[N_FEATURES] = {",
            "    " + ", ".join(f"{v}f" for v in self.scaler.mean_),
            "};",
            "",
            "// Scaler parameters (scale/std)",
            "const float SCALER_SCALE[N_FEATURES] = {",
            "    " + ", ".join(f"{v}f" for v in self.scaler.scale_),
            "};",
            "",
            "// Support vectors (flattened: N_SUPPORT_VECTORS x N_FEATURES)",
            "const float SUPPORT_VECTORS[N_SUPPORT_VECTORS * N_FEATURES] = {",
        ]

        # Add support vectors
        for i, sv_row in enumerate(sv):
            line = "    " + ", ".join(f"{v}f" for v in sv_row)
            if i < len(sv) - 1:
                line += ","
            lines.append(line)

        lines.extend([
            "};",
            "",
            "// Dual coefficients (alpha * y)",
            "const float DUAL_COEF[N_SUPPORT_VECTORS] = {",
            "    " + ", ".join(f"{v}f" for v in dual_coef),
            "};",
            "",
            "#endif // SVM_MODEL_H",
        ])

        with open(filepath, 'w') as f:
            f.write('\n'.join(lines))

        print(f"💾 ESP32 header saved to: {filepath}")
        print(f"   Support vectors: {len(sv)}")
        print(f"   Memory estimate: ~{len(sv) * len(FEATURES) * 4 + len(sv) * 4:.0f} bytes")


# Global trainer instance
trainer = None


def on_connect(client, userdata, flags, rc):
    print(f"✓ Connected to broker with result code: {rc}")
    client.subscribe(TOPIC)
    print(f"✓ Subscribed to topic: {TOPIC}")
    print(f"📊 Collecting {trainer.target_samples} samples for training...\n")


def on_message(client, userdata, msg):
    global trainer

    if trainer.is_complete:
        return

    raw = msg.payload.decode()

    try:
        data = json.loads(raw)

        # Extract features (same structure as working code)
        temp = data["temperature_C"]["mean"]
        ph = data["pH"]["mean"]
        rpm = data["rpm"]["mean"]

        features = [temp, ph, rpm]

        trainer.add_sample(features)

        # Progress update
        if trainer.sample_count % 50 == 0 or trainer.sample_count <= 5:
            print(f"  [{trainer.sample_count}/{trainer.target_samples}] temp={temp:.2f}, pH={ph:.2f}, rpm={rpm:.0f}")

        if trainer.is_complete:
            # Save models
            trainer.save_python_model("svm_model.pkl")
            trainer.save_esp32_header("svm_model.h")

            print("\n✓ Training complete! You can now stop the script (Ctrl+C)")

    except KeyError as e:
        # Skip messages that don't have telemetry data (e.g., 'msg' only messages)
        pass
    except Exception as e:
        print(f"❌ Error: {e}")


def main():
    global trainer

    parser = argparse.ArgumentParser(description="Train SVM on bioreactor data")
    parser.add_argument("--samples", type=int, default=500, help="Number of training samples")
    parser.add_argument("--nu", type=float, default=0.02, help="SVM nu parameter (outlier fraction)")
    parser.add_argument("--gamma", type=float, default=0.002, help="RBF kernel gamma")
    args = parser.parse_args()

    trainer = SVMTrainer(
        target_samples=args.samples,
        nu=args.nu,
        gamma=args.gamma
    )

    print(f"{'='*60}")
    print("🚀 SVM Training Script")
    print(f"{'='*60}")
    print(f"  Broker: {BROKER}:{PORT}")
    print(f"  Topic: {TOPIC}")
    print(f"  Target samples: {args.samples}")
    print(f"  SVM nu: {args.nu}")
    print(f"  SVM gamma: {args.gamma}")
    print(f"{'='*60}\n")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT, 60)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n\n👋 Stopped by user")
        if trainer.sample_count > 100 and not trainer.is_complete:
            print(f"  Training on {trainer.sample_count} collected samples...")
            trainer.train()
            trainer.save_python_model("svm_model.pkl")
            trainer.save_esp32_header("svm_model.h")


if __name__ == "__main__":
    main()