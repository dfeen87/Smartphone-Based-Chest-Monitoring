# Validation Protocol

This document describes the controlled validation protocol for the deterministic
phase–memory operator implemented in this repository.  It mirrors Section 5 of:

> "A Deterministic Phase–Memory Operator for Early Respiratory Instability
> Detection Using Smartphone-Based Chest Monitoring" — see **PAPER.md**.

---

## PhysioNet BIDMC Dataset Integration

The `validation/` directory provides a ready-to-run pipeline for **semi-real
validation** using the PhysioNet BIDMC Respiratory Dataset.

### Dataset

| Property | Value |
|----------|-------|
| Name | BIDMC PPG and Respiration Dataset |
| URL  | https://physionet.org/content/bidmc/1.0.0/ |
| Records | 53 subjects, ~8 min each |
| Native sampling rate | 125 Hz |
| Respiratory channel | `RESP` (impedance pneumography) |
| Target sampling rate | 50 Hz (resampled per PAPER.md §2.2) |

### Quick start

```bash
# Install dependencies
pip install -r validation/requirements.txt

# Run on real BIDMC data (downloads automatically via wfdb)
python validation/validate_bidmc.py --record 1

# Run offline with synthetic fallback (no internet required)
python validation/validate_bidmc.py --synthetic
```

### Figures generated

| Figure | Regime | Expected ΔΦ(t) |
|--------|--------|----------------|
| `regime1_stable.png` | Regular breathing (control) | ΔΦ(t) ≈ 0, no alarms |
| `regime2_drift.png`  | Frequency drift | ΔΦ(t) rises gradually |
| `regime3_pause.png`  | Intermittent pause | ΔΦ(t) spikes at onset |
| `comparison_baselines.png` | All methods vs baselines | ΔΦ(t), RMS, FFT |

### Sampling rate normalisation

BIDMC records are natively sampled at 125 Hz.  The loader resamples to
50 Hz using `scipy.signal.resample_poly` with rational up/down factors
(GCD-reduced).  This preserves the respiratory band (0.1–0.5 Hz) without
aliasing.

```python
# validation/physionet_loader.py
from physionet_loader import load_bidmc_record
data = load_bidmc_record(record_id=1, target_fs=50)
signal, fs = data['signal'], data['fs']   # shape: (N,), fs=50 Hz
```

### Channel selection

The BIDMC `RESP` (impedance pneumography) channel is selected automatically.
It measures chest-wall impedance during breathing, which is mechanically
analogous to the gravity-aligned accelerometer projection `x(t) = a(t)·û_b(t)`
described in PAPER.md §2.3.

### Preprocessing

Applied in `validation/pipeline.run_pipeline()`:

1. **Detrend** — `scipy.signal.detrend` (linear drift removal)
2. **Bandpass** — 2nd-order Butterworth 0.1–0.5 Hz (PAPER.md §2.4)
3. **Phase–memory operator** — analytic signal via `scipy.signal.hilbert`
   (FFT-based, replaces the derivative approximation used in the C++ core for
   efficiency on embedded hardware)

---

## Controlled Regimes  (PAPER.md §5.1)

Validation uses four synthetic or semi-synthetic test regimes:

| # | Regime                      | Description                                                |
|---|-----------------------------|------------------------------------------------------------|
| 1 | **Regular breathing** *(control)* | Stationary frequency and amplitude; ΔΦ(t) should remain near zero |
| 2 | **Frequency drift**          | Gradual change in respiration rate; ΔΦ(t) rises progressively     |
| 3 | **Intermittent pause**       | Reduced amplitude / near-zero segments; ΔΦ(t) spikes at onset     |
| 4 | **Burst irregularity**       | Transient fast breathing or erratic phase resets; ΔΦ(t) elevated  |

A synthetic signal for each regime can be constructed as a bandpass-filtered
sinusoid with the appropriate frequency profile and then fed to the engine via
`respiro_feed_accel`.

For semi-real validation on BIDMC data, Regimes 2 and 3 are produced by
applying controlled perturbations to the real signal after t = 30 s:

- **Regime 2 (drift):** The signal tail is resampled to simulate a rising
  respiratory rate while preserving real morphology in the stable prefix.
- **Regime 3 (pause):** Amplitude is multiplied by 0.03 for 8 s to simulate
  an intermittent breathing pause / apnea event.

---

## Baseline Methods  (PAPER.md §5.2)

The phase–memory operator is benchmarked against these low-overhead baselines:

| Method             | Description                                         | Complexity     |
|--------------------|-----------------------------------------------------|----------------|
| **RMS envelope**   | Windowed RMS amplitude proxy for signal power        | 𝒪(N)          |
| **FFT peak shift** | Tracking spectral peak in the respiration band       | 𝒪(N log N)    |
| **ΔΦ (proposed)**  | Phase–memory divergence (PAPER.md Eq. 5)             | 𝒪(N)          |

Python implementations are in `validation/metrics.py`:

```python
from metrics import rms_envelope, fft_peak_shift
rms = rms_envelope(filtered_signal, window_samples=150)
fft_times, fft_freqs = fft_peak_shift(filtered_signal, fs=50)
```

---

## Primary Outcomes  (PAPER.md §5.3)

| Outcome                | Definition                                                |
|------------------------|-----------------------------------------------------------|
| **Detection latency**  | Time from instability onset to ΔΦ(t) > α · σ_ω alarm    |
| **False alarm rate**   | Alarm rate in the control regime (regular breathing)      |
| **Compute cost**       | Runtime complexity and CPU/energy estimate on target device|

Python implementations are in `validation/metrics.py`:

```python
from metrics import detection_latency, false_alarm_rate
lat = detection_latency(delta_phi, threshold, onset_sample=1500, fs=50)
far = false_alarm_rate(delta_phi[250:-150], threshold, fs=50)
# Note: skip baseline calibration window (first 250 samples) and
#       boundary samples (last 150) when evaluating FAR.
```

---

## Statistical Validation Checklist  (PAPER.md Table 2)

| Item                   | Notes                                                         |
|------------------------|---------------------------------------------------------------|
| Reference sensor       | Belt / airflow / PSG channel (if available)                   |
| Agreement analysis     | Bland–Altman; correlation; MAE                                |
| Motion robustness      | Walking / posture change / speaking segments                  |
| Generalization         | Multiple phones, placements, subjects *(future work)*         |
| Reproducibility        | Code + fixed parameters + versioned release (see below)       |

---

## Reproducibility  (PAPER.md Appendix A)

To reproduce a validation experiment:

1. **Fix all operator parameters** before the run:

   | Parameter | Symbol | Default          |
   |-----------|--------|------------------|
   | Memory window | Tₘ / M | 150 samples (≈3 s at 50 Hz) |
   | Sensitivity | α | 2.0              |
   | Baseline window | — | 250 samples (≈5 s) |
   | Bandpass | — | 0.1–0.5 Hz       |
   | Sample rate | fₛ | 50 Hz            |

2. **Record or generate** the test signal at fₛ Hz.

3. **Feed samples** sequentially via `respiro_feed_accel` / `respiro_feed_gyro`.

4. **Read metrics** via `respiro_get_metrics` at each step; record
   `instability_score` (ΔΦ) and `instability_detected`.

5. **Compare** detection latency and false alarm rate across all four regimes.

A minimal REST-based experiment interface is described in PAPER.md Appendix A.

---

## Important Disclaimer

This validation protocol is designed for research and exploratory evaluation.
The engine is **not a medical device** and all outputs are informational only.
See PAPER.md §9 for application perspectives and limitations.
