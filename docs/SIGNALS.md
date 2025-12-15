# SIGNALS.md

## Overview

This document defines how signals are interpreted by the RespiroSync core engine. It describes assumptions, expected characteristics, and limitations of the sensor data used for respiratory and sleep inference.

RespiroSync does not impose strict unit requirements; instead, it relies on relative consistency and temporal coherence within a monitoring session.

## Sensor Types

### Accelerometer

The accelerometer is the primary source of respiratory motion information.

**Usage:**
* Captures chest wall expansion and contraction
* Used to derive both breathing motion and general movement intensity

**Assumptions:**
* Sensor is rigidly coupled to the chest
* Orientation may change over time
* Gravity is present and must be removed

**Processing Notes:**
* Magnitude of the 3-axis vector is used
* Gravity is estimated via exponential smoothing
* Residual motion is treated as breathing + movement

### Gyroscope

The gyroscope provides secondary information about chest rotation.

**Usage:**
* Improves robustness against posture changes
* Helps distinguish breathing motion from gross movement

**Assumptions:**
* Angular velocity values are proportional to chest rotation
* Units are device-native but consistent within a session

**Processing Notes:**
* Magnitude of angular velocity is computed
* Gyroscope contribution is scaled and blended into the breathing signal

## Sampling Characteristics

RespiroSync is designed for mobile-typical sampling rates.

| Parameter | Expected Range |
|-----------|----------------|
| Sample rate | ~25–100 Hz |
| Timestamp | Monotonic, milliseconds |
| Latency | Not time-critical |

Exact sample rate is not enforced, but irregular sampling may reduce metric quality.

## Signal Pipeline Summary

```
Accelerometer ──┐
                ├─► Motion Magnitude ─► Gravity Removal ─┐
Gyroscope ──────┘                                         │
                                                          ▼
                                              Bandpass Filter (Breathing)
                                                          ▼
                                                 Peak Detection
                                                          ▼
                                              Breath Cycle Metrics
```

## Breathing Frequency Assumptions

* Typical adult breathing frequency:
   * ~6–30 breaths per minute
* Corresponding signal frequency:
   * ~0.1–0.5 Hz

The bandpass filter is tuned to isolate this range and suppress higher-frequency movement artifacts.

## Peak Detection Semantics

A breath is defined as:
* A significant peak in the filtered breathing signal
* Separated by a physiologically plausible duration

**Constraints:**
* Minimum duration: ~0.5 seconds
* Maximum duration: ~6 seconds

Dynamic thresholds adapt to recent signal variance to handle amplitude changes.

## Derived Metrics

### Respiratory Rate

* Computed from average breath cycle duration
* Expressed in breaths per minute (BPM)

### Breathing Regularity

* Derived from variability of breath durations
* Normalized to a 0.0–1.0 scale

### Movement Intensity

* Based on variance of recent accelerometer magnitude
* Normalized heuristically to 0.0–1.0

## Limitations

* Sensitive to loose device placement
* Large body movements can temporarily dominate the signal
* Not validated for all postures or activity states

These limitations are inherent to passive motion-based monitoring.

## Interpretation Guidance

All metrics are heuristic and intended for:
* Trend observation
* Exploratory analysis
* System development

They are **not diagnostic**.
