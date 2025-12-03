"""
SVM Testing Script for Bioreactor Anomaly Detection

Loads trained SVM model and tests it on live MQTT data or CSV files.
Outputs predictions and evaluation metrics.

Usage:
    # Live MQTT testing
    python svm_test.py --model svm_model.pkl --stream faults

    # CSV file testing
    python svm_test.py --model svm_model.pkl --csv test_data.csv
"""

import paho.mqtt.client as mqtt
import json
import numpy as np
import pickle
import time
import argparse
import pandas as pd

# MQTT Configuration
BROKER = "engf0001.cs.ucl.ac.uk"
PORT = 1883

FEATURES = ["temp_mean", "ph_mean", "rpm_mean"]


class SVMDetector:
    """SVM-based anomaly detector using pre-trained model."""

    def __init__(self, model_path):
        self.load_model(model_path)
        self.predictions = []
        self.scores = []

    def load_model(self, filepath):
        """Load model from pickle file."""
        with open(filepath, 'rb') as f:
            model = pickle.load(f)

        self.scaler_mean = np.array(model['scaler_mean'])
        self.scaler_scale = np.array(model['scaler_scale'])
        self.support_vectors = np.array(model['support_vectors'])
        self.dual_coef = np.array(model['dual_coef']).flatten()
        self.intercept = model['intercept'][0]
        self.gamma = model['gamma']

        print(f"✓ Model loaded: {len(self.support_vectors)} support vectors")

    def scale(self, x):
        """Standardize features."""
        return (x - self.scaler_mean) / self.scaler_scale

    def rbf_kernel(self, x, sv):
        """Compute RBF kernel between x and support vector."""
        diff = x - sv
        return np.exp(-self.gamma * np.dot(diff, diff))

    def predict(self, features):
        """
        Predict if sample is anomaly.

        Returns:
            (is_anomaly, decision_score)
            Negative score = anomaly
        """
        x = np.array(features)
        x_scaled = self.scale(x)

        # Compute decision function
        decision = self.intercept
        for i, sv in enumerate(self.support_vectors):
            decision += self.dual_coef[i] * self.rbf_kernel(x_scaled, sv)

        is_anomaly = decision < 0

        self.predictions.append(is_anomaly)
        self.scores.append(decision)

        return is_anomaly, float(decision)

    def get_metrics(self, ground_truth):
        """Compute evaluation metrics."""
        y_true = np.array(ground_truth)
        y_pred = np.array(self.predictions)

        tp = np.sum((y_true == True) & (y_pred == True))
        fp = np.sum((y_true == False) & (y_pred == True))
        tn = np.sum((y_true == False) & (y_pred == False))
        fn = np.sum((y_true == True) & (y_pred == False))

        precision = tp / (tp + fp) if (tp + fp) > 0 else 0
        recall = tp / (tp + fn) if (tp + fn) > 0 else 0
        f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0
        accuracy = (tp + tn) / len(y_true) if len(y_true) > 0 else 0

        return {
            'TP': int(tp), 'FP': int(fp), 'TN': int(tn), 'FN': int(fn),
            'Precision': round(precision, 4),
            'Recall': round(recall, 4),
            'F1': round(f1, 4),
            'Accuracy': round(accuracy, 4),
        }


# Global detector
detector = None
results_log = []
sample_count = 0


def on_connect(client, userdata, flags, rc):
    stream = userdata['stream']
    topic = f"bioreactor_sim/{stream}/telemetry/summary"
    print(f"✓ Connected to broker with result code: {rc}")
    client.subscribe(topic)
    print(f"✓ Subscribed to topic: {topic}")
    print(f"\n{'='*60}")
    print("📡 Listening for data... (Ctrl+C to stop and see results)")
    print(f"{'='*60}\n")


def on_message(client, userdata, msg):
    global sample_count

    raw = msg.payload.decode()

    try:
        data = json.loads(raw)

        # Extract features (same structure as working code)
        temp = data["temperature_C"]["mean"]
        ph = data["pH"]["mean"]
        rpm = data["rpm"]["mean"]

        features = [temp, ph, rpm]

        # Get faults - handle both string list and dict list formats
        faults_raw = data["faults"]["last_active"]
        if faults_raw and isinstance(faults_raw[0], dict):
            # Format: [{"name": "fault_name", ...}, ...]
            faults = [f.get("name", str(f)) for f in faults_raw]
        else:
            # Format: ["fault_name", ...]
            faults = faults_raw if faults_raw else []

        has_fault = len(faults) > 0

        # Run prediction
        is_anomaly, score = detector.predict(features)

        sample_count += 1

        # Log result
        results_log.append({
            'timestamp': time.time(),
            'temp': temp,
            'ph': ph,
            'rpm': rpm,
            'score': score,
            'predicted_anomaly': is_anomaly,
            'actual_fault': has_fault,
            'faults': ','.join(faults),
        })

        # Print detection
        status = "🚨 ANOMALY" if is_anomaly else "✓ Normal"
        fault_str = f" [FAULT: {','.join(faults)}]" if has_fault else ""

        print(f"[{sample_count:4d}] {status} | score={score:+.4f} | "
              f"temp={temp:.2f} pH={ph:.2f} rpm={rpm:.0f}{fault_str}")

    except KeyError as e:
        # Skip messages that don't have telemetry data
        pass
    except Exception as e:
        print(f"❌ Error: {e}")


def test_mqtt(model_path, stream):
    """Test on live MQTT stream."""
    global detector

    detector = SVMDetector(model_path)

    print(f"\n{'='*60}")
    print("🧪 SVM Testing - Live MQTT")
    print(f"{'='*60}")
    print(f"  Model: {model_path}")
    print(f"  Stream: {stream}")

    client = mqtt.Client(userdata={'stream': stream})
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT, 60)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n\n" + "="*60)
        print("📊 RESULTS SUMMARY")
        print("="*60)

        if results_log:
            # Compute metrics
            ground_truth = [r['actual_fault'] for r in results_log]
            metrics = detector.get_metrics(ground_truth)

            print(f"\n  Total samples: {len(results_log)}")
            print(f"  Actual faults: {sum(ground_truth)}")
            print(f"  Detected anomalies: {sum(detector.predictions)}")

            print(f"\n  ╔═══════════════════════════════════════╗")
            print(f"  ║         CONFUSION MATRIX              ║")
            print(f"  ╠═══════════════════════════════════════╣")
            print(f"  ║              PREDICTED                ║")
            print(f"  ║           Normal    Anomaly           ║")
            print(f"  ║  ACTUAL  ┌────────┬────────┐          ║")
            print(f"  ║  Normal  │  {metrics['TN']:4d}  │  {metrics['FP']:4d}  │ (TN,FP) ║")
            print(f"  ║         ├────────┼────────┤          ║")
            print(f"  ║  Fault   │  {metrics['FN']:4d}  │  {metrics['TP']:4d}  │ (FN,TP) ║")
            print(f"  ║          └────────┴────────┘          ║")
            print(f"  ╚═══════════════════════════════════════╝")

            print(f"\n  Metrics:")
            print(f"    Precision: {metrics['Precision']:.4f}")
            print(f"    Recall:    {metrics['Recall']:.4f}")
            print(f"    F1 Score:  {metrics['F1']:.4f}")
            print(f"    Accuracy:  {metrics['Accuracy']:.4f}")

            # Save results
            df = pd.DataFrame(results_log)
            df.to_csv('svm_test_results.csv', index=False)
            print(f"\n💾 Results saved to: svm_test_results.csv")
        else:
            print("  No data collected.")


def test_csv(model_path, csv_path, faults_column='faults'):
    """Test on CSV file."""
    global detector

    detector = SVMDetector(model_path)

    print(f"\n{'='*60}")
    print("🧪 SVM Testing - CSV File")
    print(f"{'='*60}")
    print(f"  Model: {model_path}")
    print(f"  Data: {csv_path}")

    df = pd.read_csv(csv_path)
    print(f"  Samples: {len(df)}")

    # Determine ground truth
    if faults_column in df.columns:
        ground_truth = ~(df[faults_column].isna() | (df[faults_column].astype(str).str.strip() == ''))
    else:
        print(f"  ⚠️ No '{faults_column}' column found, assuming all normal")
        ground_truth = pd.Series([False] * len(df))

    print(f"  Faulty samples: {ground_truth.sum()}")
    print(f"\n{'='*60}")
    print("Running predictions...")
    print(f"{'='*60}\n")

    # Run predictions
    predictions = []
    scores = []

    for idx, row in df.iterrows():
        features = [row['temp_mean'], row['ph_mean'], row['rpm_mean']]
        is_anomaly, score = detector.predict(features)
        predictions.append(is_anomaly)
        scores.append(score)

        if is_anomaly:
            fault_str = f" [FAULT: {row[faults_column]}]" if ground_truth.iloc[idx] else ""
            print(f"  🚨 Anomaly at index {idx}: score={score:+.4f}{fault_str}")

    # Compute metrics
    metrics = detector.get_metrics(ground_truth.tolist())

    print(f"\n{'='*60}")
    print("📊 EVALUATION RESULTS")
    print(f"{'='*60}")

    print(f"\n  ╔═══════════════════════════════════════╗")
    print(f"  ║         CONFUSION MATRIX              ║")
    print(f"  ╠═══════════════════════════════════════╣")
    print(f"  ║              PREDICTED                ║")
    print(f"  ║           Normal    Anomaly           ║")
    print(f"  ║  ACTUAL  ┌────────┬────────┐          ║")
    print(f"  ║  Normal  │  {metrics['TN']:4d}  │  {metrics['FP']:4d}  │ (TN,FP) ║")
    print(f"  ║         ├────────┼────────┤          ║")
    print(f"  ║  Fault   │  {metrics['FN']:4d}  │  {metrics['TP']:4d}  │ (FN,TP) ║")
    print(f"  ║          └────────┴────────┘          ║")
    print(f"  ╚═══════════════════════════════════════╝")

    print(f"\n  Metrics:")
    print(f"    Precision: {metrics['Precision']:.4f}")
    print(f"    Recall:    {metrics['Recall']:.4f}")
    print(f"    F1 Score:  {metrics['F1']:.4f}")
    print(f"    Accuracy:  {metrics['Accuracy']:.4f}")

    # Save predictions
    df['svm_score'] = scores
    df['svm_anomaly'] = predictions
    output_path = csv_path.replace('.csv', '_svm_results.csv')
    df.to_csv(output_path, index=False)
    print(f"\n💾 Results saved to: {output_path}")

    return metrics


def main():
    parser = argparse.ArgumentParser(description="Test SVM anomaly detector")
    parser.add_argument("--model", type=str, default="svm_model.pkl", help="Model file path")
    parser.add_argument("--stream", type=str, help="MQTT stream name")
    parser.add_argument("--csv", type=str, help="CSV file path for offline testing")
    args = parser.parse_args()

    if args.csv:
        test_csv(args.model, args.csv)
    elif args.stream:
        test_mqtt(args.model, args.stream)
    else:
        print("Error: Specify either --stream or --csv")
        print("\nAvailable streams:")
        print("  nofaults          - Clean baseline (no faults)")
        print("  single_fault      - Temperature sensor bias only")
        print("  three_faults      - Multiple faults (temp, pH, heater)")
        print("  variable_setpoints - Faults + changing setpoints")
        print("\nExamples:")
        print("  python svm_test.py --model svm_model.pkl --stream single_fault")
        print("  python svm_test.py --model svm_model.pkl --stream three_faults")
        print("  python svm_test.py --model svm_model.pkl --csv test_data.csv")


if __name__ == "__main__":
    main()