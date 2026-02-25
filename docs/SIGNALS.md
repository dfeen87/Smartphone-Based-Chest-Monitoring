# SIGNALS.md

## Overview

This document defines how signals are interpreted by the RespiroSync core engine.
It describes assumptions, expected characteristics, and limitations of the sensor
data used for respiratory and sleep inference.

The core algorithm is the **deterministic phase–memory operator** described in the
companion manuscript:

> "A Deterministic Phase–Memory Operator for Early Respiratory Instability
> Detection Using Smartphone-Based Chest Monitoring" — see **PAPER.md**.

All variable names below follow the notation used in that manuscript.

---

## Sensor Types

### Accelerometer

The accelerometer is the primary source of respiratory motion information.

**Usage:**
* Captures chest wall expansion and contraction during breathing
* Used to form the scalar respiration channel x(t) and to measure movement intensity

**Assumptions:**
* Sensor is rigidly coupled to the anterior thoracic wall (sternal region)
* Orientation may change over time; gravity must be removed
* Sampling rate fₛ ∈ [50, 100] Hz (PAPER.md §2.2)

**Processing Notes:**
* The scalar channel x(t) is formed by projecting a(t) onto the gravity-aligned
  axis û_b(t) (PAPER.md Eq. 1).  In the current implementation, the magnitude of
  the 3-axis vector is used as an approximation when a full sensor-fusion
  orientation estimate is unavailable.
* Gravity is estimated via exponential smoothing and subtracted

### Gyroscope

The gyroscope provides secondary information about chest rotation.

**Usage:**
* Improves robustness against posture changes (motion-rejection gating, PAPER.md §2.4)
* Angular velocity magnitude ‖Ω(t)‖ can gate or supplement the breathing signal

**Assumptions:**
* Angular velocity values are proportional to chest rotation
* Units are device-native but consistent within a session

---

## Sampling Characteristics

RespiroSync is designed for mobile-typical sampling rates.

| Parameter     | Expected Range                       |
|---------------|--------------------------------------|
| Sample rate   | 50–100 Hz (fₛ, PAPER.md §2.2)       |
| Timestamp     | Monotonic, milliseconds              |
| Latency       | Not time-critical (streaming capable)|

Exact sample rate is not enforced, but irregular sampling may reduce metric quality.

---

## Signal Pipeline

The complete pipeline follows PAPER.md §7.1:

```
Chest IMU            Preprocess              Analytic Signal    Phase     Memory    Decision
(accel / gyro)  →  (detrend + bandpass)  →  (Hilbert approx)  →  θ(t)  →  ω̄(t)  →  ΔΦ(t) threshold
```

### Step 1 – Scalar Respiration Channel x(t)  (PAPER.md Eq. 1)

```
x(t) = a(t) · û_b(t)
```

û_b(t) is the gravity-aligned unit vector (device orientation).  The current
implementation approximates this with the gravity-removed accelerometer
magnitude.

### Step 2 – Preprocessing  (PAPER.md §2.4)

Applied to x(t) before phase analysis:

* **Drift removal** — exponential-smoothing high-pass (gravity subtraction)
* **Bandpass filter** — 2nd-order Butterworth, passband ≈ 0.1–0.5 Hz
* **Motion-rejection gating** *(optional)* — using ‖Ω(t)‖ from gyroscope

### Step 3 – Analytic Signal and Instantaneous Phase  (PAPER.md §3.1, Eq. 2)

```
z(t) = x(t) + i · H[x(t)] = A(t) · e^{iθ(t)}
```

The Hilbert transform H[x] is approximated via the derivative method (valid for
the narrow breathing band):

```
H[x](t) ≈ −(1/ω₀) · dx/dt
```

Instantaneous phase:

```
θ(t) = atan2(H[x](t), x(t))
```

### Step 4 – Phase Velocity  (PAPER.md §3.2, Eq. 3)

```
ω(t) = dθ/dt   (discrete derivative with 2π-unwrap)
```

### Step 5 – Short-Term Phase Memory  (PAPER.md §3.3, Eq. 4)

Rolling mean of ω over a window of M samples (Tₘ ≈ 3 s at 50 Hz):

```
ω̄(t) ≈ (1/M) Σ_{k=0}^{M−1} ω[n − k]
```

### Step 6 – Instability Metric ΔΦ(t)  (PAPER.md §4.1, Eq. 5)

```
ΔΦ(t) = |ω(t) − ω̄(t)|
```

Near zero during stable periodic breathing; elevated during frequency drift,
intermittent pauses, or burst irregularities.

### Step 7 – Threshold Decision  (PAPER.md §4.2, Eq. 6)

```
Instability at time t  ⟺  ΔΦ(t) > α · σ_ω
```

* **σ_ω** — baseline std-dev of ω estimated on the initial stable segment
* **α** — sensitivity parameter ∈ [2, 3] (default 2.0)

An optional persistence criterion over L samples reduces single-sample false
positives (PAPER.md Eq. 7).

---

## Legacy Peak-Detection Branch

In addition to the phase-memory operator, the engine runs a parallel
peak-detection stage to estimate respiratory rate in breaths per minute (BPM).
This provides the `breathing_rate_bpm` and `breath_cycles_detected` fields.
See PAPER.md §5.2 for baseline method context.

---

## Breathing Frequency Assumptions

* Typical adult breathing frequency: 6–30 breaths per minute
* Corresponding signal frequency: 0.1–0.5 Hz

The bandpass filter is tuned to isolate this range and suppress higher-frequency
motion artefacts.

---

## Derived Metrics

### instability_score  (ΔΦ)

Phase–memory divergence in rad/s.  The primary output of the deterministic
phase–memory operator (PAPER.md Eq. 5).  Stable breathing → low ΔΦ.
Frequency drift, pauses, or burst irregularities → elevated ΔΦ.

### instability_detected

Boolean flag: 1 when ΔΦ(t) > α · σ_ω (PAPER.md Eq. 6), else 0.
Becomes meaningful after the baseline calibration window (~5 s at 50 Hz).

### Respiratory Rate

Computed from average breath cycle duration via peak detection.
Expressed in breaths per minute (BPM).

### Breathing Regularity

Derived from variability of peak-to-peak breath durations.
Normalized to 0.0–1.0 scale (higher = more consistent).

### Movement Intensity

Variance of recent accelerometer magnitude.
Normalized heuristically to 0.0–1.0 for sleep staging.

---

## Operator Parameters  (PAPER.md §8)

The phase–memory operator is tunable via three transparent parameters:

| Parameter | Symbol | Default       | Description                               |
|-----------|--------|---------------|-------------------------------------------|
| Memory window | Tₘ / M | 150 samples (≈3 s at 50 Hz) | Rolling mean window for ω̄(t) |
| Sensitivity | α | 2.0 | Threshold multiplier α · σ_ω (Eq. 6)     |
| Persistence | L | — | Optional: sustain L samples before alarm (Eq. 7) |

---

## Limitations

* Sensitive to loose device placement or chest-strap slippage
* Large body movements can temporarily dominate x(t)
* Phase estimate degrades when signal amplitude approaches noise floor
* Derivative-based Hilbert approximation assumes narrow-band signal

These limitations are inherent to passive motion-based monitoring.  Mitigation
strategies include gyroscope-based motion gating and multi-axis fusion
(PAPER.md §8).

---

## Interpretation Guidance

All metrics are heuristic and intended for:
* Trend observation
* Exploratory analysis
* Research and system development

They are **not diagnostic**.  See PAPER.md for full validation protocol.

