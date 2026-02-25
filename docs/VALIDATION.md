# Validation Protocol

This document describes the controlled validation protocol for the deterministic
phase–memory operator implemented in this repository.  It mirrors Section 5 of:

> "A Deterministic Phase–Memory Operator for Early Respiratory Instability
> Detection Using Smartphone-Based Chest Monitoring" — see **PAPER.md**.

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

---

## Baseline Methods  (PAPER.md §5.2)

The phase–memory operator is benchmarked against these low-overhead baselines:

| Method             | Description                                         | Complexity     |
|--------------------|-----------------------------------------------------|----------------|
| **RMS envelope**   | Windowed RMS amplitude proxy for signal power        | 𝒪(N)          |
| **FFT peak shift** | Tracking spectral peak in the respiration band       | 𝒪(N log N)    |
| **ΔΦ (proposed)**  | Phase–memory divergence (PAPER.md Eq. 5)             | 𝒪(N)          |

---

## Primary Outcomes  (PAPER.md §5.3)

| Outcome                | Definition                                                |
|------------------------|-----------------------------------------------------------|
| **Detection latency**  | Time from instability onset to ΔΦ(t) > α · σ_ω alarm    |
| **False alarm rate**   | Alarm rate in the control regime (regular breathing)      |
| **Compute cost**       | Runtime complexity and CPU/energy estimate on target device|

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
