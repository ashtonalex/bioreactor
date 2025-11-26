import numpy as np

class ZScoreDetector:

    def __init__(self, window_size=100, threshold=3.0):
        self.window_size = window_size
        self.threshold = threshold
        self.values = []

    def update(self, value):
        self.values.append(value)

        if len(self.values) > self.window_size:
            self.values.pop(0)

        if len(self.values) < 10:
            return False, 0.0

        mean = np.mean(self.values)
        std = np.std(self.values)

        if std == 0:
            return False, 0.0

        z = abs((value - mean) / std)

        is_anomaly = z > self.threshold

        return is_anomaly, float(z)

class HysteresisDetector:
    def __init__(self, low_threshold, high_threshold, initial_state=False):

        self.low = low_threshold
        self.high = high_threshold
        self.state = initial_state

    def update(self, value):

        if not self.state and value > self.high:
            self.state = True
        elif self.state and value < self.low:
            self.state = False
        return self.state


class SlidingWindowDetector:

    def __init__(self, window_size=50, threshold=0.0, ideal_value=0.0):

        self.window_size = window_size
        self.threshold = threshold
        self.values = []
        self.ideal_value = ideal_value

    def update(self, value):
        self.values.append(value)

        if len(self.values) > self.window_size:
            self.values.pop(0)

        avg = sum(self.values) / len(self.values)

        if avg > (self.ideal_value + self.threshold):
            return True, float(avg)
        if avg < (self.ideal_value - self.threshold):
            return True, float(avg)

        return False, 0.0