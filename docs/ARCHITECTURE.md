# RespiroSync Architecture

## Overview

RespiroSync is a cross-platform respiratory and sleep monitoring engine designed
to infer breathing patterns from chest-mounted smartphone motion sensors.  The
system is built around a portable C++ core that implements the **deterministic
phase–memory operator** described in:

> "A Deterministic Phase–Memory Operator for Early Respiratory Instability
> Detection Using Smartphone-Based Chest Monitoring" — see **PAPER.md**.

The project prioritizes architectural clarity, portability, and deterministic
behaviour over feature breadth or clinical claims.

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
│ - Phase–memory operator │
│ - Breath detection      │
│ - Metric estimation     │
└─────────────────────────┘
```

The C API boundary ensures the core engine can be safely consumed by multiple
platforms and languages without exposing implementation details.

## Core Engine Responsibilities

The C++ core engine is responsible for:

- Sensor fusion of accelerometer and gyroscope data  (PAPER.md §2.2–2.3)
- Forming scalar respiration channel x(t)  (PAPER.md Eq. 1)
- Isolation of respiratory motion via bandpass filtering  (PAPER.md §2.4)
- Computing the analytic signal and instantaneous phase θ(t)  (PAPER.md §3.1)
- Computing instantaneous phase velocity ω(t)  (PAPER.md §3.2)
- Maintaining short-term phase memory ω̄(t)  (PAPER.md §3.3)
- Computing the instability metric ΔΦ(t) = |ω − ω̄|  (PAPER.md §4.1)
- Issuing threshold-based instability alarms ΔΦ > α · σ_ω  (PAPER.md §4.2)
- Legacy peak-detection for respiratory rate estimation
- Heuristic sleep-stage classification
- Detection of prolonged breathing pauses (possible apnea)

The engine is stateful and operates continuously on timestamped sensor samples.

## Sensor Inputs

### Accelerometer

- Primary signal source for respiratory motion
- Gravity component removed via exponential-smoothing high-pass
- Forms scalar respiration channel x(t) (PAPER.md Eq. 1)
- Sampling rate fₛ ∈ [50, 100] Hz (PAPER.md §2.2)

### Gyroscope

- Captures rotational motion of the chest
- Used for optional motion-rejection gating (PAPER.md §2.4)
- Weighted lightly relative to accelerometer data

## Signal Processing Pipeline  (PAPER.md §7.1)

```
Chest IMU          Preprocess              Analytic Signal    Phase     Memory    Decision
(accel / gyro)  →  (detrend + bandpass)  →  (Hilbert approx) → θ(t)  →  ω̄(t)  →  ΔΦ(t) threshold
```

### Preprocessing  (PAPER.md §2.4)

- Gravity removal — exponential-smoothing high-pass on accelerometer magnitude
- Bandpass filter — 2nd-order Butterworth (≈ 0.1–0.5 Hz passband)
- Optional gyroscope-based motion gating

### Phase–Memory Operator  (PAPER.md §3–4)

The deterministic instability operator proceeds as follows:

1. **Analytic signal** (Eq. 2) — Hilbert transform approximated via the
   derivative method for narrow-band signals:
   `H[x](t) ≈ −(1/ω₀) · dx/dt`

2. **Instantaneous phase** (§3.1) — `θ(t) = atan2(H[x], x)`

3. **Phase velocity** (Eq. 3) — `ω(t) = Δθ/Δt` with 2π-unwrapping

4. **Phase memory** (Eq. 4) — `ω̄(t) = (1/M) Σ ω[n−k]`
   (rolling mean, M ≈ 150 samples ≈ 3 s at 50 Hz)

5. **Instability metric** (Eq. 5) — `ΔΦ(t) = |ω(t) − ω̄(t)|`

6. **Threshold decision** (Eq. 6) — alarm when `ΔΦ(t) > α · σ_ω`
   where σ_ω is estimated on the initial stable segment; α ∈ [2, 3].

### Legacy Peak Detection

- Dynamic-threshold peak detection on the bandpass-filtered signal
- Used to estimate `breathing_rate_bpm` and `breath_cycles_detected`
- Runs in parallel with the phase-memory operator

## Metric Estimation

### instability_score  (ΔΦ, PAPER.md Eq. 5)

Phase–memory divergence in rad/s.  The primary output of the deterministic
phase–memory operator.  Exposed via `SleepMetrics.instability_score`.

### instability_detected  (PAPER.md Eq. 6)

Boolean: 1 when `ΔΦ(t) > α · σ_ω`.  Exposed via `SleepMetrics.instability_detected`.

### Respiratory Rate

Computed from recent peak-to-peak breath cycle durations.
Reported in breaths per minute (BPM).

### Breathing Regularity

Coefficient of variation of breath durations, normalised to 0.0–1.0.

### Movement Intensity

Variance of recent accelerometer magnitude, normalised to 0.0–1.0.

## Sleep Stage Classification

Sleep staging uses a rule-based heuristic (not machine learning), classifying:

- AWAKE
- LIGHT_SLEEP
- DEEP_SLEEP
- REM_SLEEP
- UNKNOWN

## Apnea Detection

A `possible_apnea` flag is set when no valid breath is detected for >10 s.
This is heuristic and informational only.

## Operator Parameters  (PAPER.md §8)

All parameters are explicit and auditable:

| Symbol | Default | Description                                      |
|--------|---------|--------------------------------------------------|
| Tₘ / M | 150 samples (≈3 s) | Phase memory window (Eq. 4)        |
| α      | 2.0     | Threshold sensitivity (Eq. 6)                    |
| L      | —       | Optional persistence window (Eq. 7)              |

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
- Not a machine-learning system

These constraints are intentional and preserve clarity and trust at this stage.

## Stability Guarantees

As of v1.0.0:

- The C API is considered stable
- Internal C++ implementation may evolve
- Platform bindings should rely only on the public header

## Scientific Reference

The scientific basis for this implementation is described in full in **PAPER.md**,
which should be treated as the canonical description of the deterministic
phase–memory operator, validation protocol, and baseline comparisons.

