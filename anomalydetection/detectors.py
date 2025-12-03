import numpy as np
from collections import deque


class ZScoreDetector:
    """
    Detects anomalies when a value deviates significantly from baseline.
    Baseline mean and std are learned from nofaults data.
    """

    def __init__(self, threshold=3.0):
        self.threshold = threshold
        self.baseline_mean = None
        self.baseline_std = None
        self.is_trained = False

    def train(self, training_data):
        """Train on fault-free data to establish baseline statistics."""
        self.baseline_mean = np.mean(training_data)
        self.baseline_std = np.std(training_data)
        self.is_trained = True
        print(f"      mean={self.baseline_mean:.4f}, std={self.baseline_std:.4f}, threshold=±{self.threshold}σ")

    def update(self, value):
        """
        Returns (is_anomaly, z_score)
        """
        if not self.is_trained:
            return False, 0.0

        if self.baseline_std == 0:
            return False, 0.0

        z = abs((value - self.baseline_mean) / self.baseline_std)
        is_anomaly = z > self.threshold

        return is_anomaly, float(z)


class HysteresisDetector:
    """
    Detects when values go outside normal range [low, high].
    Uses hysteresis to prevent rapid toggling at boundaries.

    Thresholds are set from training data: mean ± k*std
    """

    def __init__(self, k=3.0, hysteresis_factor=0.5):
        """
        k: number of standard deviations for threshold
        hysteresis_factor: fraction of std for hysteresis margin
        """
        self.k = k
        self.hysteresis_factor = hysteresis_factor
        self.low = None
        self.high = None
        self.margin = 0
        self.state = False  # False = normal, True = anomaly
        self.is_trained = False

    def train(self, training_data):
        """Set thresholds from training data: mean ± k*std"""
        mean = np.mean(training_data)
        std = np.std(training_data)

        self.low = mean - self.k * std
        self.high = mean + self.k * std
        self.margin = self.hysteresis_factor * std
        self.is_trained = True

        print(f"      range=[{self.low:.4f}, {self.high:.4f}], margin={self.margin:.4f}")

    def update(self, value):
        """
        Returns True if currently in anomaly state.
        """
        if not self.is_trained:
            return False

        if not self.state:
            # Currently normal - check if we should trigger anomaly
            if value < (self.low - self.margin) or value > (self.high + self.margin):
                self.state = True
        else:
            # Currently anomaly - check if we should return to normal
            if self.low < value < self.high:
                self.state = False

        return self.state

    def reset(self):
        """Reset state to normal."""
        self.state = False


class SlidingWindowDetector:
    """
    Detects drift by comparing rolling average against baseline mean.
    Triggers when rolling average deviates more than threshold from baseline.
    """

    def __init__(self, window_size=30, k=2.0):
        """
        window_size: number of samples for rolling average
        k: number of standard deviations for threshold
        """
        self.window_size = window_size
        self.k = k
        self.baseline_mean = None
        self.threshold = None
        self.values = deque(maxlen=window_size)
        self.is_trained = False

    def train(self, training_data):
        """Set baseline mean and threshold from training data."""
        self.baseline_mean = np.mean(training_data)
        std = np.std(training_data)
        self.threshold = self.k * std
        self.is_trained = True

        print(f"      baseline={self.baseline_mean:.4f}, threshold=±{self.threshold:.4f}")

    def update(self, value):
        """
        Returns (is_anomaly, deviation_from_baseline)
        """
        if not self.is_trained:
            return False, 0.0

        self.values.append(value)

        # Need at least half window to make decision
        if len(self.values) < self.window_size // 2:
            return False, 0.0

        rolling_avg = np.mean(self.values)
        deviation = rolling_avg - self.baseline_mean

        is_anomaly = abs(deviation) > self.threshold

        return is_anomaly, float(deviation)

    def reset(self):
        """Reset window."""
        self.values.clear()


class ConfusionMatrix:
    """
    Tracks True Positives, True Negatives, False Positives, False Negatives.
    """

    def __init__(self, name="Overall"):
        self.name = name
        self.tp = 0  # Correctly detected fault
        self.tn = 0  # Correctly detected no fault
        self.fp = 0  # False alarm (detected fault when none exists)
        self.fn = 0  # Missed fault (failed to detect actual fault)

    def update(self, predicted_anomaly: bool, actual_fault: bool):
        """
        predicted_anomaly: True if detector flagged anomaly
        actual_fault: True if fault was actually present

        Returns: result string (TP, TN, FP, FN)
        """
        if actual_fault and predicted_anomaly:
            self.tp += 1
            return "TP"
        elif not actual_fault and not predicted_anomaly:
            self.tn += 1
            return "TN"
        elif not actual_fault and predicted_anomaly:
            self.fp += 1
            return "FP"
        else:  # actual_fault and not predicted_anomaly
            self.fn += 1
            return "FN"

    def get_metrics(self):
        """Calculate precision, recall, F1, accuracy."""
        total = self.tp + self.tn + self.fp + self.fn

        metrics = {
            "TP": self.tp,
            "TN": self.tn,
            "FP": self.fp,
            "FN": self.fn,
            "total": total,
        }

        # Precision: Of all predicted positives, how many were correct?
        if self.tp + self.fp > 0:
            metrics["precision"] = self.tp / (self.tp + self.fp)
        else:
            metrics["precision"] = 0.0

        # Recall: Of all actual positives, how many did we catch?
        if self.tp + self.fn > 0:
            metrics["recall"] = self.tp / (self.tp + self.fn)
        else:
            metrics["recall"] = 0.0

        # F1 Score
        if metrics["precision"] + metrics["recall"] > 0:
            metrics["f1"] = 2 * (metrics["precision"] * metrics["recall"]) / (metrics["precision"] + metrics["recall"])
        else:
            metrics["f1"] = 0.0

        # Accuracy
        if total > 0:
            metrics["accuracy"] = (self.tp + self.tn) / total
        else:
            metrics["accuracy"] = 0.0

        return metrics

    def print_summary(self):
        """Print formatted confusion matrix and metrics."""
        metrics = self.get_metrics()

        print(f"\n{'=' * 50}")
        print(f"CONFUSION MATRIX: {self.name}")
        print("=" * 50)
        print(f"                  Actual Fault    No Fault")
        print(f"  Predicted Fault     TP={self.tp:<6}    FP={self.fp:<6}")
        print(f"  Predicted Normal    FN={self.fn:<6}    TN={self.tn:<6}")
        print("-" * 50)
        print(f"  Total samples: {metrics['total']}")
        if metrics['total'] > 0:
            print(f"  Accuracy:  {metrics['accuracy']:.2%}")
            print(f"  Precision: {metrics['precision']:.2%}")
            print(f"  Recall:    {metrics['recall']:.2%}")
            print(f"  F1 Score:  {metrics['f1']:.2%}")
        print("=" * 50)

        return metrics

    def reset(self):
        """Reset all counters."""
        self.tp = 0
        self.tn = 0
        self.fp = 0
        self.fn = 0