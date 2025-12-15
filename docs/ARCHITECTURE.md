# RespiroSync Architecture

## Overview

RespiroSync is a cross-platform respiratory and sleep monitoring engine designed to infer breathing patterns from chest-mounted smartphone motion sensors. The system is built around a portable C++ core that performs signal processing and state estimation, with thin platform-specific bindings for Android and iOS.

The project prioritizes architectural clarity, portability, and deterministic behavior over feature breadth or clinical claims.

## High-Level Design

RespiroSync is structured into three layers:

```
┌─────────────────────────┐
│ Mobile Platform Layer   │  (Android / iOS)
│ - Sensor access         │
│ - JNI / ObjC / Swift    │
└───────────▲─────────────┘
            │
┌───────────┴─────────────┐
│ C API Boundary          │
│ - Stable ABI            │
│ - Opaque engine handle  │
└───────────▲─────────────┘
            │
┌───────────┴─────────────┐
│ Core Engine (C++)       │
│ - Signal processing     │
│ - Breath detection      │
│ - Metric estimation     │
└─────────────────────────┘
```

The C API boundary ensures the core engine can be safely consumed by multiple platforms and languages without exposing implementation details.

## Core Engine Responsibilities

The C++ core engine is responsible for:

- Sensor fusion of accelerometer and gyroscope data
- Isolation of respiratory motion from gross movement
- Detection of breath cycles
- Estimation of respiratory rate and regularity
- Heuristic sleep-stage classification
- Detection of prolonged breathing pauses (possible apnea)

The engine is stateful and operates continuously on timestamped sensor samples.

## Sensor Inputs

### Accelerometer

- Used to capture chest wall motion
- Gravity component is removed via a simple high-pass filter
- Motion magnitude is used for both breathing detection and movement analysis

### Gyroscope

- Used to capture rotational motion of the chest
- Contributes to respiratory motion estimation
- Weighted lightly relative to accelerometer data

Both sensors are expected to be sampled at typical mobile device rates (≈50 Hz). Exact units are not enforced by the core and are assumed to be consistent within a session.

## Signal Processing Pipeline

The core processing pipeline follows these steps:

### Gravity Removal

- A smoothed gravity estimate is subtracted from accelerometer magnitude
- Produces a motion signal dominated by chest expansion/contraction

### Sensor Fusion

- Gyroscope magnitude is blended into the motion signal
- Improves robustness against posture changes

### Bandpass Filtering

- A 2nd-order Butterworth bandpass filter isolates breathing frequencies
- Typical passband: ~0.1–0.5 Hz (≈6–30 breaths/min)

### Peak Detection

- Breathing peaks are detected using a dynamic threshold
- Threshold adapts based on recent signal variance
- Hysteresis prevents double-counting

### Breath Cycle Tracking

- Valid breath durations are recorded
- History is truncated to a rolling time window

## Metric Estimation

### Respiratory Rate

- Computed from recent breath cycle durations
- Averaged over a short rolling window
- Reported in breaths per minute (BPM)

### Breathing Regularity

- Based on variability of recent breath durations
- Expressed as a normalized 0.0–1.0 score
- Higher values indicate more consistent breathing

### Movement Intensity

- Derived from variance of recent accelerometer magnitude
- Normalized to a 0.0–1.0 heuristic scale
- Used for sleep staging and confidence estimation

## Sleep Stage Classification

Sleep staging is performed using a rule-based heuristic, not a machine-learning model.

Classification considers:

- Movement intensity
- Breathing regularity

Current stages:

- AWAKE
- LIGHT_SLEEP
- DEEP_SLEEP
- REM_SLEEP
- UNKNOWN

The logic is intentionally simple and deterministic, serving as a foundation for future refinement.

## Apnea Detection

A possible apnea event is flagged when:

- No valid breath is detected for more than 10 seconds

This flag is heuristic and intended for informational use only.

## Confidence Estimation

A confidence score (0.0–1.0) is computed based on:

- Number of detected breath cycles
- Signal continuity

Confidence reflects data sufficiency, not clinical certainty.

## Platform Integration

### Android

- JNI bindings forward sensor data into the core engine
- Kotlin layer manages lifecycle and threading
- No signal processing occurs in Java/Kotlin

### iOS

- Objective-C++ bridge connects Swift code to the C API
- SwiftUI views consume only the exposed metrics
- Core logic remains entirely in C++

## What RespiroSync Is Not

- Not a medical device
- Not clinically validated
- Not a diagnostic tool
- Not a machine-learning system (yet)

These constraints are intentional and preserve clarity and trust at this stage.

## Stability Guarantees

As of v1.0.0:

- The C API is considered stable
- Internal C++ implementation may evolve
- Platform bindings should rely only on the public header

## Future Direction (Non-Binding)

Potential future work includes:

- Formal API for generic respiratory signals
- Improved filtering and peak detection
- Machine-learning-based sleep staging
- Extended metrics and validation tooling

These are not part of the current contract.
