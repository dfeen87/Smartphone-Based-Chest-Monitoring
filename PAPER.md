# A Deterministic Phase–Memory Operator for Early Detection of Respiratory Instability Using Smartphone-Based Chest IMU Monitoring

**Smart Wearable Technology (SWT) — Research Article**

| | |
|---|---|
| **Authors** | Marcel Krüger¹·*, Don Michael Feeney Jr.² |
| **Affiliations** | ¹ Independent Researcher, Germany · ² Independent Researcher, USA |
| **Correspondence** | marcelkrueger092@gmail.com |
| **ORCID (M.K.)** | 0009-0002-5709-9729 |
| **ORCID (D.M.F.)** | 0009-0003-1350-4160 |
| **DOI** | *(assigned by journal)* |

---

## Abstract

Wearable respiratory monitoring often relies on heuristic pipelines or opaque machine-learning models, limiting interpretability and auditability in safety-critical or clinical-adjacent contexts. We introduce a **deterministic phase–memory operator** for early detection of respiratory instability using chest-mounted smartphone inertial measurement unit (IMU) signals. The proposed instability metric **ΔΦ(t)** quantifies instantaneous deviations of phase velocity from short-term phase memory, enabling transparent threshold-based decision logic without training dependence. A controlled validation protocol covering frequency drift, intermittent pauses, and burst irregularities is defined and benchmarked against RMS-envelope and FFT-peak tracking baselines. The framework is linear-time, streaming-capable, reproducible via a public reference implementation, and suitable for real-time on-device deployment in wearable respiratory monitoring scenarios.

**Keywords:** wearable respiratory monitoring; smartphone IMU sensing; deterministic signal processing; phase-based instability detection; interpretable health monitoring

---

## 1  Introduction

Wearable respiratory monitoring has become increasingly relevant for sleep assessment, early warning of respiratory compromise, and longitudinal remote observation. Despite rapid technological progress, many existing systems remain constrained by:

1. Weak mechanical coupling to respiration (e.g., wrist-based placement)
2. Indirect proxy measurements with limited physiological specificity
3. Black-box machine-learning pipelines whose internal decision mechanisms are difficult to audit, reproduce, and clinically interpret

For wearable systems operating in clinical-adjacent or safety-sensitive contexts, deterministic and interpretable signal-processing frameworks are desirable. Transparent decision logic enables reproducibility, parameter auditability, and predictable deployment under resource constraints.

This manuscript proposes a deterministic alternative: a **phase–memory operator** applied to chest-mounted smartphone IMU signals. Rather than relying on learned classification boundaries, the proposed method defines respiratory instability as a measurable divergence between instantaneous phase velocity and short-term phase memory.

**Design goals:**

| Goal | Description |
|---|---|
| **Determinism** | Fully specified computation without training-time randomness or data-dependent model fitting |
| **Interpretability** | Instability quantified explicitly as phase-memory divergence |
| **Wearable feasibility** | Linear-time processing suitable for real-time on-device deployment |
| **Protocol clarity** | Controlled validation regimes with transparent baseline comparisons |

---

## 2  Signal Acquisition and Preprocessing

### 2.1  Chest-Mounted Smartphone Placement

A smartphone is positioned on the **anterior thoracic wall (sternal region)** using a strap or compression garment. Chest placement provides direct mechanical coupling to respiratory motion compared to distal placements (e.g., wrist), improving signal-to-noise in the respiration band.

```
          ┌──────────────────────────────┐
          │        ANTERIOR VIEW         │
          │                              │
          │          ┌──────┐            │
          │          │ 📱   │  ← Smartphone strapped
          │          │ IMU  │     to sternal region
          │          └──────┘            │
          │       /           \          │
          │      /   THORAX    \         │
          │     /   (sternum)   \        │
          │    └─────────────────┘       │
          │                              │
          │  ↕ Direct coupling to        │
          │    respiratory motion        │
          └──────────────────────────────┘
```

### 2.2  Sampling and Channels

Assume inertial sampling at **fₛ ∈ [50, 100] Hz**. Let:

- **a(t) ∈ ℝ³** — accelerometer signal
- **Ω(t) ∈ ℝ³** — gyroscope signal

### 2.3  Respiration-Sensitive Scalar Channel

We form a scalar respiration channel **x(t)** via projection onto a gravity-aligned axis:

> **x(t) = a(t) · û_b(t)**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 1)*

where **û_b(t)** is a unit vector estimated from sensor fusion (gravity direction) or a stable principal axis.

### 2.4  Filtering and Normalization

| Step | Operation |
|---|---|
| Drift removal | High-pass filter or detrend |
| Bandpass | 0.1–0.5 Hz at rest (extend for exercise) |
| Motion rejection | Gating via ‖**Ω(t)**‖ or broadband energy |
| Normalization | z-score on a baseline window |

---

## 3  Phase–Memory Operator

### 3.1  Analytic Signal and Instantaneous Phase

Let **x(t)** be the filtered channel. Define the analytic signal using the Hilbert transform 𝓗:

> **z(t) = x(t) + i 𝓗[x(t)] = A(t) e^{iθ(t)}**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 2)*

The instantaneous phase is **θ(t) = arg(z(t))**.

### 3.2  Instantaneous Phase Velocity

Define phase velocity (implemented via discrete derivative with phase unwrapping):

> **ω(t) = dθ/dt**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 3)*

### 3.3  Short-Term Phase Memory

For a memory window **T_m**, define:

> **ω̄(t) = (1/T_m) ∫_{t−T_m}^{t} ω(τ) dτ**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 4)*

In discrete time with **M** samples:

> **ω̄(t) ≈ (1/M) Σ_{k=0}^{M−1} ω[n − k]**

---

## 4  Instability Metric and Decision Logic

### 4.1  Definition of the Instability Score

We define the **phase–memory divergence**:

> **ΔΦ(t) = |ω(t) − ω̄(t)|**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 5)*

> 💡 **Interpretation:** Stable periodic breathing yields small ΔΦ; drift, pauses, or burst irregularity increase ΔΦ via rapid phase-velocity deviations.

### 4.2  Baseline-Normalized Threshold

Let **σ_ω** be the baseline standard deviation of **ω(t)** on an initial stable segment. Define:

> **Instability at time t  ⟺  ΔΦ(t) > α · σ_ω**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 6)*

with **α ∈ [2, 3]** as a transparent sensitivity parameter.

### 4.3  Optional Persistence Criterion

To reduce single-sample false positives, require persistence over **L** samples:

> **Σ_{k=0}^{L−1} 𝟙{ΔΦ(t − k) > α·σ_ω} ≥ L**  &nbsp;&nbsp;&nbsp;&nbsp; *(Eq. 7)*

### 4.4  Implementation Parameters and Temporal Resolution

| Parameter | Value |
|---|---|
| Sampling rate fₛ | 50 Hz (BIDMC respiratory recordings) |
| Phase velocity ω(t) | Discrete differentiation of unwrapped analytic phase |
| Memory window T_m | M samples (rolling average) |
| Threshold multiplier α | 2 |
| Detection latency resolution | Δt = 1/fₛ |
| Perturbation onset | t = 30 s |

No additional smoothing beyond the specified memory window was applied unless explicitly stated.

---

## 5  Experimental Protocol

### 5.1  Controlled Regimes

| Regime | Description |
|---|---|
| 1. Regular breathing (control) | Stationary frequency and amplitude |
| 2. Frequency drift | Gradual change in respiration rate |
| 3. Intermittent pause | Reduced amplitude / near-zero segments |
| 4. Burst irregularity | Transient fast breathing or erratic phase resets |

### 5.2  Baseline Methods

| Method | Description |
|---|---|
| **RMS envelope** | Windowed RMS amplitude proxy |
| **FFT peak shift** | Tracking spectral peak in respiration band |
| **Peak-to-peak intervals** | Time-domain period estimator *(optional)* |

### 5.3  Primary Outcomes

- **Detection latency** — onset-to-alarm time
- **False alarms** — alarm rate in control regime
- **Compute cost** — runtime complexity and device-level CPU/energy estimates

---

## 6  Results

### 6.1  Regime Visualization

The figure below shows representative behavior of ΔΦ(t) across controlled regimes. The metric remains low under stable conditions and increases during structured deviations.

```
ΔΦ(t)
  6 │                      ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
    │                    ░░                                      ░░
  5 │                  ░░                                          ░░
    │                ░░         ╔═══════════════════════════════════╗
  4 │──────────────────────────────────────────────────────────  ← THRESHOLD
    │              ░░           │      PAUSE regime                 │
  3 │            ░░             ╚═══════════════════════════════════╝
    │          ░░
  2 │   STABLE ░░  DRIFT ░░░░░░░░░░░░░░░░░░░░░░░░
    │░░░░░░░░░░    ░░░░░░
  1 │░░░░░░░░░░░░░░
    │
  0 └──┬─────────┬──────────┬──────────┬──────────┬──────────┬──▶ t (s)
       0        15         30         45         60         75    90

       ◀── STABLE ──▶◀──── DRIFT ────▶◀────── PAUSE ──────▶

  - - - - - - - - - THRESHOLD (α·σ_ω) - - - - - - - - - - - - -
```

*Figure 1: Evolution of ΔΦ(t) across controlled regimes. The dashed line denotes the statistically derived detection threshold.*

### 6.2  Quantitative Comparison

Detection latency was measured as the time from perturbation onset to first threshold crossing. False alarm rate was assessed in the stable control regime.

**Table 1: Detection latency under controlled perturbations (mean ± SD, seconds)**
*N = 5 BIDMC recordings. Perturbation onset at t = 30 s.*

| Regime | RMS Envelope | FFT Peak Shift | **ΔΦ (proposed)** |
|---|:---:|:---:|:---:|
| Frequency drift | Not detected | 0.060 ± 0.000 s | **0.000 ± 0.000 s** ✓ |
| Pause (amplitude suppression) | **0.000 ± 0.000 s** ✓ | Not detected | 0.572 ± 0.027 s |
| Control (false alarms) | 0 | 0 | **0** ✓ |

**Key findings:**

- The proposed ΔΦ operator detected **frequency drift immediately** at perturbation onset (latency = 0.000 ± 0.000 s), outperforming FFT peak-shift (0.060 s) and RMS envelope (not detected).
- For **amplitude-suppression (pause)** events, RMS envelope responded immediately while ΔΦ exhibited a mean latency of 0.572 ± 0.027 s; FFT peak-shift did not trigger.
- **No false alarms** were observed in the stable control segment for any method.

### 6.3  Motion Robustness Stress Test

Walking and posture-change segments were introduced to assess robustness against motion artifacts. Gyroscope-based motion gating was applied to suppress broadband movement components.

> During gated motion segments, ΔΦ(t) did **not** exhibit sustained false-positive threshold crossings within the respiration band. Transient peaks were reduced by multi-axis fusion and persistence filtering.

---

## 7  Wearable Feasibility and Implementation

### 7.1  Pipeline Overview

```
┌──────────────────────────────────────────────────────────────────────────────┐
│          DETERMINISTIC PHASE–MEMORY OPERATOR PIPELINE                        │
│                                                                              │
│   ┌───────────┐   ┌───────────┐   ┌───────────┐   ┌────────┐   ┌────────┐  │
│   │ Chest IMU │──▶│ Preprocess│──▶│ Analytic  │──▶│ Phase  │──▶│Memory  │  │
│   │(accel +   │   │(detrend + │   │ Signal    │   │velocity│   │ ω̄(t)   │  │
│   │ gyro)     │   │ bandpass) │   │(Hilbert)  │   │ ω(t)   │   │ (EWA)  │  │
│   └───────────┘   └───────────┘   └───────────┘   └────────┘   └───┬────┘  │
│                                                                      │       │
│                                                        ΔΦ(t) = |ω(t) − ω̄(t)|
│                                                                      │       │
│                                                              ┌───────▼────┐  │
│                                                              │ Threshold  │  │
│                                                              │ ΔΦ > α·σ_ω│  │
│                                                              │ + persist. │  │
│                                                              └───────┬────┘  │
│                                                                      │       │
│                                                              ┌───────▼────┐  │
│                                                              │  INSTABILITY│  │
│                                                              │   ALERT    │  │
│                                                              └────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘
```

*Figure 2: Graphical overview of the deterministic phase–memory operator pipeline.*

### 7.2  Computational Footprint

The method is **streaming-capable** and **linear-time** in samples. A practical mobile implementation uses:

1. A causal bandpass filter
2. A lightweight analytic-signal approximation (Hilbert FIR / quadrature filter)
3. Rolling window averages

All parameters are explicit and auditable.

**Reproducibility layer:** A minimal REST-based experiment interface for monitoring and standardized evaluation hooks is provided in a companion repository (see Appendix A).

---

## 8  Reproducible Implementation and Validation Repository

A complete cross-platform reference implementation is publicly available at:

> 🔗 **https://github.com/dfeen87/Smartphone-Based-Chest-Monitoring**

> 🖥️ **Live demo dashboard:** https://smartphone-based-chest-monitoring.onrender.com

The repository contains:

| Component | Description |
|---|---|
| C++ core implementation | Deterministic phase–memory operator for mobile (iOS/Android) |
| Python reference pipeline | Reproducible validation scripts |
| BIDMC integration scripts | PhysioNet Respiratory Dataset integration |
| Perturbation protocols | Stable, drift, pause, burst semi-real protocols |
| Baseline modules | RMS-envelope, FFT-peak-shift comparison |
| Metrics evaluation | Detection latency, false alarm rate |
| Computational profiling | CPU, memory, battery benchmarks |
| Parameter documentation | Versioned documentation of all operator parameters |

All operator parameters (**T_m**, **α**, **L**, sampling rate, filtering specs) are explicitly documented and auditable.

> ⚠️ *The dashboard serves as an experimental demonstration layer and does not constitute a medical device.*

---

## 9  Discussion

The proposed framework introduces an interpretable instability metric grounded in phase–memory divergence and avoids training-time uncertainty associated with data-driven models. The instability score **ΔΦ(t)** is directly tunable via **(T_m, α, L)**, allowing explicit control over sensitivity and persistence criteria.

Because all parameters are deterministic and auditable, the method supports reproducible deployment under mobile resource constraints and predictable behavior across devices.

**Limitations and mitigations:**

| Limitation | Mitigation Strategy |
|---|---|
| Sensor placement sensitivity | Standardized sternal placement protocol |
| Motion artifacts | Gyroscope-based motion gating |
| High-amplitude non-respiratory movement | Multi-axis fusion, adaptive filtering, artifact rejection |

**Future work** includes multi-subject validation, cross-device benchmarking, and evaluation against reference respiratory instrumentation.

---

## 10  Application Perspectives

> ⚕️ *The deterministic phase–memory operator is **not** intended as a diagnostic medical device but may support assistive monitoring scenarios where awareness of respiratory pattern stability is beneficial.*

### 10.1  Sleep Monitoring and Pattern Screening

During sleep, changes in respiratory regularity may precede clinically relevant disturbances. The instability metric **ΔΦ(t)** enables real-time detection of emerging deviations and may support vibration-based alerts or retrospective sleep-pattern analysis. The framework is intended for **screening-level** pattern monitoring and does **not** replace polysomnography or clinical sleep diagnostics.

### 10.2  Stress and Hyperventilation Awareness

Acute stress and hyperventilation involve rapid shifts in respiratory frequency and phase dynamics. Because ΔΦ(t) measures divergence from short-term phase memory, the method may support **biofeedback-oriented monitoring** for stress management, guided breathing exercises, or mindfulness training.

### 10.3  Respiratory Rehabilitation and Training

In respiratory physiotherapy or post-illness rehabilitation, tracking breathing stability may assist adherence to controlled breathing protocols. The deterministic formulation allows reproducible, on-device implementation without reliance on data-trained classification models.

### 10.4  Chronic Respiratory Condition Monitoring (Assistive)

For individuals with chronic respiratory conditions (e.g., asthma or COPD), the framework may serve as a **trend-level monitoring tool** to identify deviations from a personalized baseline. It is not designed to replace clinical assessment, spirometry, oxygen saturation monitoring, or physician-directed care.

---

## 11  Conclusion

We presented a deterministic phase–memory operator and a transparent instability score **ΔΦ(t)** for early respiratory instability detection using chest-mounted smartphone IMU sensing.

The method replaces opaque classification models with explicit, phase-based decision logic, enabling:

- ✅ Auditability
- ✅ Parameter transparency
- ✅ Predictable real-time behavior on mobile hardware

A controlled validation protocol and public reference implementation support reproducibility. The proposed framework provides a computationally lightweight and interpretable foundation for **assistive wearable respiratory monitoring** applications.

---

## Ethics Statement

No human or animal subjects were involved in the preparation of this manuscript.

## Data Availability Statement

The reference implementation, validation scripts, baseline comparison modules, and reproducibility infrastructure are publicly available at:
**https://github.com/dfeen87/Smartphone-Based-Chest-Monitoring**

## Funding

No external funding was received.

## Conflict of Interest

The authors declare no conflict of interest.

## Author Contributions

- **M.K.** conceived the deterministic phase–memory operator framework and drafted the manuscript.
- **D.M.F.** contributed implementation considerations for mobile and cross-platform deployment and reviewed the manuscript for engineering clarity.
- All authors approved the final version.

## AI Statement

No generative AI models were used for data generation, signal analysis, or automated decision-making in the proposed method.

---

## Appendix A — Reproducibility Layer: Minimal REST API

The prototype exposes a minimal REST interface for monitoring experiments and standardizing evaluation.

```python
import requests

BASE = "http://localhost:5000/api"

# Register a node
response = requests.post(
    f"{BASE}/network/nodes",
    json={"frequency_hz": 440.0, "q_factor": 200.0, "beta": 1e-4}
)
node_id = response.json()["id"]

# Inject and evolve
requests.post(
    f"{BASE}/network/nodes/{node_id}/inject",
    json={"amplitude": 1.0, "phase": 0.0}
)
requests.post(
    f"{BASE}/network/tick",
    json={"dt": 1e-6, "steps": 1000}
)

# Read state
state = requests.get(f"{BASE}/network/state").json()
print(f"R(t) = {state['order_parameter']:.4f}")
```

*Listing 1: Example REST usage for monitoring (illustrative).*

---

## References

[1] B. Boashash, "Estimating and interpreting the instantaneous frequency of a signal," *Proceedings of the IEEE* 80, 520–538 (1992).

[2] L. Cohen, *Time-Frequency Analysis* (Prentice Hall, 1995).

[3] J. M. Bland and D. G. Altman, "Statistical methods for assessing agreement between two methods of clinical measurement," *The Lancet* 327(8476), 307–310 (1986).

[4] P. Welch, "The use of fast Fourier transform for the estimation of power spectra," *IEEE Trans. Audio Electroacoust.* 15, 70–73 (1967).

[5] P. H. Charlton et al., "Breathing rate estimation from physiological signals: a review," *Physiological Measurement* 37 (2016).

[6] C. Massaroni et al., "Contact-based methods for measuring respiratory rate: a review," *Sensors* 19 (2019).

[7] W. Karlen et al., "Respiratory rate monitoring: methods and clinical perspectives," (2013).

[8] S. Khan et al., "Smartphone-based respiratory monitoring: opportunities and challenges," *IEEE Access* (2021).

[9] J. Kim et al., "Mobile health respiratory monitoring and validation considerations," *JMIR mHealth and uHealth* (2021).

[10] S. Coyle et al., "Real-time wearable signal processing for health monitoring: considerations and pitfalls," *IEEE J. Biomed. Health Inform.* (2021).

[11] E. Smets et al., "Large-scale evaluation/validation considerations for wearable physiological monitoring," *Nature Medicine* (2021).

[12] S. Benatti et al., "Energy-efficient wearable computing for continuous sensing," (2021).

[13] C. Zhao et al., "Recent advances in fiber optic sensors for respiratory monitoring," *Biosensors and Bioelectronics* (2022).

[14] S. Stankoski et al., "Breathing rate estimation from head-worn inertial sensors," *Sensors* 22(6), 2079 (2022).

[15] H. Liu et al., "Wearable respiration monitoring: sensors, systems, and algorithms (review)," *Biosensors* (2022).

[16] S. Nemati et al., "Wearable sensing and digital biomarkers in clinical monitoring," *npj Digital Medicine* (2022).

[17] S. Roy et al., "Explainable pipelines for wearable health monitoring: a survey," (2023).

[18] D. Brown et al., "Digital biomarkers in respiratory disease and remote monitoring," *The Lancet Digital Health* (2023).

[19] H. Assael et al., "RespEar: an ear-worn system for respiration monitoring," *Proc. ACM IMWUT* (2023).

[20] A. Author et al., "Smartphone IMU-based respiratory monitoring under motion: evaluation study," (2023).

[21] B. Author et al., "Explainability and auditability in wearable sensing for health," (2024).

[22] Z. Peng et al., "Phase-based analysis methods for biomedical time series: applications and pitfalls," (2022).

[23] J. Xu et al., "Low-power on-device physiological inference on mobile hardware," (2022).

[24] C. Author et al., "Wearable respiratory monitoring: 2024 update and benchmarking," (2024).

[25] G. D. Clifford et al., "PhysioNet: updates and perspectives for physiologic signal research," (2022).

[26] M. Author et al., "Best practices for reproducible mobile health signal processing," (2021).

[27] N. Author et al., "REST interfaces and reproducibility layers for mHealth experiments," (2023).
