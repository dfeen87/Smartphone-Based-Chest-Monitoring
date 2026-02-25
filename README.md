<div align="center">
<a name="top"></a>

# Smartphone Based Chest Monitoring

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/dfeen87/Smartphone-Based-Chest-Monitoring)
[![Platform](https://img.shields.io/badge/platform-iOS%20%7C%20Android-lightgrey.svg)](https://github.com/dfeen87/Smartphone-Based-Chest-Monitoring)
[![C++](https://img.shields.io/badge/C++-17-00599C.svg)](https://isocpp.org/)

**Deterministic Phase–Memory Operator for Early Respiratory Instability Detection**

</div>

---

> **Scientific reference.** The algorithm, equations, validation protocol, and
> baseline comparisons are described in full in **[PAPER.md](PAPER.md)**:
> *"A Deterministic Phase–Memory Operator for Early Respiratory Instability
> Detection Using Smartphone-Based Chest Monitoring."*
> PAPER.md is the canonical scientific description of this repository.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Pipeline Overview](#pipeline-overview)
- [Phase–Memory Operator](#phasememory-operator)
- [Key Features](#key-features)
- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [Building](#building)
- [Render.com Deployment](#rendercom-deployment)
- [Reproducibility Layer](#reproducibility-layer)
- [Validation Protocol](#validation-protocol)
- [Multi-Record Validation & Results Export](#multi-record-validation--results-export)
- [PhysioNet / Real-Data Validation](#physionet--real-data-validation)
- [Data Sources & Citations](#data-sources--citations)
- [Use Cases](#use-cases)
- [Performance](#performance)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Citation](#citation)
- [License](#license)

---

## Overview

RespiroSync is a portable, on-device engine for estimating respiratory patterns
and detecting respiratory instability using only a smartphone's built-in
accelerometer and gyroscope.  By leveraging chest-mounted inertial signals, it
provides low-cost, hardware-free access to respiratory insights through a
**fully deterministic, training-free operator** — the *phase–memory operator*.

### Design Goals  (PAPER.md §1)

| Goal | Description |
|------|-------------|
| **Determinism** | Fully specified computation — no training-time randomness |
| **Interpretability** | Instability measured as phase-memory divergence ΔΦ(t) |
| **Wearable feasibility** | Linear-time, streaming-capable, on-device processing |
| **Protocol clarity** | Controlled regimes and baseline comparisons |

> ⚠️ **Important:** RespiroSync provides heuristic, informational metrics.
> It is **not a medical device** and must not be used for diagnostic purposes.

---

## Pipeline Overview

The complete processing pipeline follows PAPER.md §7.1:

```
Chest IMU           Preprocess              Analytic Signal    Phase     Memory    Decision
(accel / gyro)  →  (detrend + bandpass)  →  (Hilbert approx) → θ(t)  →  ω̄(t)  →  ΔΦ(t) > α·σ_ω
```

### Step-by-step

| Step | Operation | Paper reference |
|------|-----------|----------------|
| **1** | Form scalar respiration channel `x(t) = a(t) · û_b(t)` | Eq. 1 |
| **2** | Detrend + bandpass filter (≈ 0.1–0.5 Hz) | §2.4 |
| **3** | Analytic signal `z(t) = x(t) + i·H[x(t)]` via Hilbert transform | Eq. 2 |
| **4** | Instantaneous phase `θ(t) = arg(z(t))` | §3.1 |
| **5** | Phase velocity `ω(t) = dθ/dt` with 2π-unwrap | Eq. 3 |
| **6** | Short-term phase memory `ω̄(t) = (1/M) Σ ω[n−k]` | Eq. 4 |
| **7** | Instability metric `ΔΦ(t) = \|ω(t) − ω̄(t)\|` | Eq. 5 |
| **8** | Threshold decision `ΔΦ(t) > α · σ_ω` | Eq. 6 |

---

## Phase–Memory Operator

The **instability metric ΔΦ(t)** quantifies the divergence between the
instantaneous phase velocity ω(t) and its short-term memory ω̄(t):

```
ΔΦ(t) = |ω(t) − ω̄(t)|                     (Eq. 5)
```

**Interpretation:**

- **Stable breathing** — ω(t) tracks ω̄(t) closely → ΔΦ ≈ 0
- **Frequency drift** — ω(t) deviates gradually → ΔΦ rises
- **Intermittent pause** — ω(t) drops to ~0 → ΔΦ spikes at onset
- **Burst irregularity** — ω(t) oscillates rapidly → ΔΦ elevated

### Threshold Decision  (PAPER.md §4.2, Eq. 6)

```
Instability ⟺ ΔΦ(t) > α · σ_ω
```

- **σ_ω** — baseline std-dev of ω estimated on the initial stable segment
- **α ∈ [2, 3]** — transparent sensitivity parameter (default: 2.0)

### Tunable Parameters  (PAPER.md §8)

| Parameter | Symbol | Default | Description |
|-----------|--------|---------|-------------|
| Memory window | Tₘ / M | 150 samples ≈ 3 s | Rolling mean window for ω̄(t) |
| Sensitivity | α | 2.0 | Threshold multiplier |
| Persistence | L | — | Optional: sustain L samples before alarm (Eq. 7) |

---

## Key Features

| Feature | Description |
|---------|-------------|
| ✅ **Instability Detection** | ΔΦ(t) — deterministic phase–memory divergence metric |
| ✅ **Real-time Respiratory Rate** | Continuous BPM estimation via peak detection |
| ✅ **Breath Cycle Detection** | Individual breath tracking and regularity analysis |
| ✅ **Sleep Stage Classification** | Heuristic multi-stage sleep analysis |
| ✅ **Pause Detection** | Identification of prolonged breathing pauses |
| ✅ **Confidence Scoring** | Signal quality-based reliability metrics |
| ✅ **Cross-Platform** | Android & iOS via shared C++ core |
| ✅ **No Dependencies** | Self-contained, portable engine |

---

## 📊 Live Monitoring Dashboard

Access the real‑time respiratory monitoring dashboard here:  
**https://smartphone-based-chest-monitoring.onrender.com/**

The dashboard provides:

- **Live system status** with health and readiness indicators  
- **Real‑time logs** streamed directly from the backend  
- **Validation controls** for running multi‑record BIDMC evaluations  
- **Downloadable results** in CSV, PDF, and DOCX formats  
- **Email delivery** for sending validation reports  
- **Mobile‑friendly UI** designed for smartphone‑based monitoring workflows  

This interface turns the full respiratory operator pipeline into an accessible, interactive tool for research, validation, and demonstration.

---

## Quick Start

### iOS (Swift)

```swift
let respiro = RespiroSync()
respiro.startSession()

Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
    let metrics = respiro.getCurrentMetrics()
    print("Breathing: \(metrics.breathingRateBPM) BPM")
    // Phase–memory instability score ΔΦ(t)
    print("ΔΦ instability score: \(metrics.instabilityScore)")
    print("Instability detected: \(metrics.instabilityDetected)")
}
```

### Android (Kotlin)

```kotlin
val respiro = RespiroSyncEngine(context)
respiro.startSession()

handler.postDelayed(object : Runnable {
    override fun run() {
        respiro.getCurrentMetrics()?.let {
            Log.d("RespiroSync", "Breathing: ${it.breathingRateBPM} BPM")
            // Phase–memory instability score ΔΦ(t)
            Log.d("RespiroSync", "ΔΦ score: ${it.instabilityScore}")
            Log.d("RespiroSync", "Instability: ${it.instabilityDetected}")
        }
        handler.postDelayed(this, 1000)
    }
}, 1000)
```

### C API (direct)

```c
RespiroHandle engine = respiro_create();
respiro_start_session(engine, getCurrentTimeMs());

// In your sensor callback loop:
respiro_feed_accel(engine, ax, ay, az, timestamp_ms);
respiro_feed_gyro(engine,  gx, gy, gz, timestamp_ms);

// Read real-time metrics:
SleepMetrics m;
respiro_get_metrics(engine, timestamp_ms, &m);

// Phase–memory operator output (PAPER.md §4):
printf("ΔΦ(t) = %.4f rad/s  |  instability = %d\n",
       m.instability_score, m.instability_detected);
printf("BPM = %.1f  |  stage = %d\n", m.breathing_rate_bpm, m.current_stage);

respiro_destroy(engine);
```

---

## Architecture

```
┌──────────────────────────────────────┐
│   respirosync_core.cpp               │
│   Core signal processing (C++)       │
│   - Phase–memory operator (ΔΦ)       │
│   - Bandpass filter + Hilbert approx │
│   - Breath rate & sleep metrics      │
└──────────────┬───────────────────────┘
               │
               │  Stable C API
               │
       ┌───────┴────────┐
       │                │
       ▼                ▼
┌──────────────┐ ┌──────────────────┐
│ respirosync_ │ │ respirosync_     │
│ ios.mm       │ │ android.cpp      │
│ iOS bridge   │ │ Android bridge   │
└──────┬───────┘ └────────┬─────────┘
       │                  │
       ▼                  ▼
┌──────────────┐ ┌──────────────────┐
│ Swift API    │ │ Kotlin API       │
└──────────────┘ └──────────────────┘
```

For detailed design information see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

---

## Building

### Prerequisites

- **iOS:** Xcode 12+ with Swift 5.3+
- **Android:** Android NDK r21+, CMake 3.10+
- **C++ Compiler:** Clang or GCC with C++17 support

### Quick Build (Makefile)

```bash
# Build library and run tests
make all

# Run tests only
make test

# Clean build artifacts
make clean
```

### iOS (Static Library)

```bash
cd core
clang++ -c respirosync_core.cpp -std=c++17 -O3 -o core.o
clang++ -c ../ios/bridge/respirosync_ios.mm -framework CoreMotion -o ios.o
ar rcs librespirosync.a core.o ios.o
```

### Android (CMake)

```cmake
add_library(respirosync SHARED
    respirosync_core.cpp
    respirosync_android.cpp)
target_link_libraries(respirosync android log)
```

For comprehensive build instructions see [`docs/BUILDING.md`](docs/BUILDING.md).

---

## Render.com Deployment

The `server/` directory provides a lightweight Flask dashboard that exposes the
phase–memory operator (PAPER.md §3–4) via a REST API and a browser UI.

### What the server provides

| Endpoint | Description |
|----------|-------------|
| `GET /` | Dashboard UI — status, metrics, configuration, live logs, validation |
| `GET /api/status` | System status (version, uptime, pipeline name) |
| `GET /api/logs?n=50` | Last *n* structured log entries |
| `GET /api/config` | Current operator parameters (M, α, baseline, fs) |
| `POST /api/config` | Update operator parameters at runtime |
| `POST /api/run` | Run the operator on a synthetic signal and return metrics |
| `POST /api/validate` | Run multi-record BIDMC validation (N records, returns mean ± SD) |
| `GET /api/results/metrics.csv` | Download per-record metrics CSV |
| `GET /api/results/summary.csv` | Download aggregated summary CSV |
| `GET /api/results/pdf` | Download auto-generated PDF report |
| `GET /api/results/docx` | Download auto-generated DOCX report |
| `POST /api/send-results` | Email results (CSV + PDF + DOCX) to a recipient |

### Deploy to Render.com (one-click)

1. Fork or push this repository to GitHub.
2. Sign in at [render.com](https://render.com) → **New → Web Service**.
3. Connect the repository and select **"Use render.yaml"** — Render will read
   [`render.yaml`](render.yaml) automatically.
4. Click **Deploy**. Render will:
   - run `pip install -r server/requirements.txt`
   - start `python server/app.py` bound to the `$PORT` it provides

> The service is defined in [`render.yaml`](render.yaml) with
> `healthCheckPath: /api/status` so Render can verify liveness automatically.

### Environment variables

All variables can be set in the Render dashboard under
**Environment → Environment Variables**.

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | (set by Render) | Port the Flask server listens on |
| `FLASK_ENV` | `production` | Set to `development` for debug mode locally |
| `PYTHON_VERSION` | `3.11.0` | Python runtime version |

No secrets are required for the default synthetic-data mode.

### Accessing the dashboard

After deployment the dashboard is available at your Render service URL, e.g.:

```
https://respirosync-dashboard.onrender.com/
```

- **Configuration** — adjust M, α, baseline window, and fs in the form and
  click **Apply** to update the running operator parameters.
- **Run Operator** — click **▶ Run Operator** to execute the phase–memory
  pipeline on a synthetic respiratory signal and view the output metrics
  (ΔΦ max/mean, σ_ω, instability rate, alarm count).

### Viewing logs

All server output is written to **stdout** using Python's `logging` module with
structured `[LEVEL]` prefixes.  Render captures stdout automatically; logs are
visible in the Render dashboard under **Logs** for the web service.

The dashboard also exposes `GET /api/logs?n=50` which returns the last *n* log
entries as JSON and is polled every 5 seconds by the browser UI.

### Running locally

```bash
pip install -r server/requirements.txt
python server/app.py          # listens on http://localhost:5000
# or with a custom port:
PORT=8080 python server/app.py
```

### Relationship to PAPER.md

The server wraps the Python reference implementation of the phase–memory
operator from `validation/pipeline.py` — the same equations described in
PAPER.md §3–4:

```
detrend → bandpass → Hilbert → θ(t) → ω(t) → ω̄(t) → ΔΦ(t) > α·σ_ω
```

The tunable parameters exposed in the dashboard (M, α, baseline window, fs)
correspond directly to the operator parameters defined in PAPER.md §4.2 and §8.
No scientific logic is modified; the server only adds HTTP routing, structured
logging, and a browser interface.

---

## Reproducibility Layer

Per PAPER.md Appendix A, all operator parameters are explicit and auditable.
To reproduce an experiment:

1. Fix parameters: `M = 150`, `α = 2.0`, `fₛ = 50 Hz`, bandpass 0.1–0.5 Hz
2. Feed a versioned signal (synthetic or recorded) sample-by-sample
3. Record `instability_score` and `instability_detected` at each step
4. Compare against the four controlled regimes (see [Validation Protocol](#validation-protocol))

A minimal REST-based experiment interface is described in PAPER.md Appendix A.

---

## Validation Protocol

Controlled validation covers four regimes (PAPER.md §5.1):

| # | Regime | Expected ΔΦ behaviour |
|---|--------|-----------------------|
| 1 | **Regular breathing** *(control)* | ΔΦ ≈ 0, no alarms |
| 2 | **Frequency drift** | ΔΦ rises gradually |
| 3 | **Intermittent pause** | ΔΦ spikes at pause onset |
| 4 | **Burst irregularity** | ΔΦ elevated throughout burst |

Benchmarked against RMS-envelope and FFT-peak-shift baselines (PAPER.md §5.2).

Primary outcomes: detection latency, false alarm rate, compute cost (PAPER.md §5.3).

See [`docs/VALIDATION.md`](docs/VALIDATION.md) for the full protocol.

---

## Multi-Record Validation & Results Export

The `validation/multi_record_validation.py` module extends the single-record
`validate_bidmc.py` script to process N ≥ 5 BIDMC records automatically and
produce the quantitative Results section metrics defined in PAPER.md §5.3.

### Run from the command line

```bash
# Offline / CI mode — uses synthetic signals, no internet required
python validation/multi_record_validation.py --n-records 5 --synthetic

# With real PhysioNet data (requires internet + wfdb)
pip install wfdb
python validation/multi_record_validation.py --n-records 5
```

### Output files

| File | Description |
|------|-------------|
| `results/metrics.csv` | Per-record metrics — one row per BIDMC record |
| `results/summary.csv` | Aggregated mean ± SD across all records |

Per-record CSV columns:
```
record_id, drift_latency, pause_latency, false_alarms, rms_latency, fft_latency
```

Aggregated summary CSV columns:
```
metric, mean, std
```

### Download results from the dashboard

After running validation from the dashboard or API, results can be downloaded
in multiple formats:

| Endpoint | Format | Description |
|----------|--------|-------------|
| `GET /api/results/metrics.csv` | CSV | Per-record metrics |
| `GET /api/results/summary.csv` | CSV | Aggregated statistics |
| `GET /api/results/pdf`         | PDF | Auto-generated report (Table 1 + 2) |
| `GET /api/results/docx`        | DOCX | Auto-generated report (Table 1 + 2) |

All download endpoints require a JWT bearer token (same authentication as other
`/api/*` endpoints).

### Email delivery

```bash
curl -X POST http://localhost:5000/api/send-results \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"email": "author@example.com"}'
```

SMTP is configured via environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `SMTP_HOST` | `localhost` | SMTP server hostname |
| `SMTP_PORT` | `587` | SMTP server port |
| `SMTP_USER` | — | SMTP username (optional) |
| `SMTP_PASS` | — | SMTP password (optional) |
| `SMTP_FROM` | `respirosync@localhost` | Sender address |

### Dashboard — Validation form

The dashboard (`/`) includes a **Multi-Record Validation** panel with:

- **Records to evaluate** input (default 5)
- **Synthetic fallback** checkbox (no internet required)
- **Run Validation** button
- Aggregated statistics display (mean ± SD per metric)
- Download buttons for CSV, PDF, and DOCX
- Email delivery form

### Methods statement (for the paper)

> *"Results are averaged across N = 5 BIDMC recordings using the
> semi-synthetic perturbation protocol described in Section 5."*

---

The `validation/` directory provides a complete semi-real validation pipeline
using the PhysioNet **BIDMC Respiratory Dataset**
(https://physionet.org/content/bidmc/1.0.0/).

### Prerequisites

```bash
pip install -r validation/requirements.txt
```

### Run on real PhysioNet data

```bash
# Downloads BIDMC record 1 from PhysioNet (requires internet)
python validation/validate_bidmc.py

# Use a specific record (1–53)
python validation/validate_bidmc.py --record 5
```

### Run offline (synthetic fallback — no internet required)

```bash
python validation/validate_bidmc.py --synthetic
```

### What the script produces

| Output | Description |
|--------|-------------|
| `validation/figures/regime1_stable.png` | Stable breathing — ΔΦ(t) ≈ 0 (PAPER.md §5.1 row 1) |
| `validation/figures/regime2_drift.png` | Frequency drift — ΔΦ(t) rises (PAPER.md §5.1 row 2) |
| `validation/figures/regime3_pause.png` | Breathing pause — ΔΦ(t) spikes (PAPER.md §5.1 row 3) |
| `validation/figures/comparison_baselines.png` | ΔΦ(t) vs RMS envelope vs FFT peak shift (PAPER.md §5.2) |
| Console summary | Detection latency, false alarm rate, σ_ω, threshold (PAPER.md Table 1) |

### Pipeline modules

| Module | Description |
|--------|-------------|
| `validation/physionet_loader.py` | BIDMC data loader — downloads, extracts RESP channel, resamples to 50 Hz |
| `validation/pipeline.py` | Python reference implementation of the phase–memory operator (PAPER.md §3–4) |
| `validation/metrics.py` | Detection latency, false alarm rate, RMS envelope, FFT peak shift (§5.2–5.3) |
| `validation/plots.py` | Generates the three regime figures and the baseline comparison figure |
| `validation/validate_bidmc.py` | End-to-end orchestration script |

### Validation protocol (PAPER.md §5)

The script applies **semi-synthetic perturbations** to a real BIDMC respiratory
signal to produce the three controlled regimes:

1. **Stable segment** — first 30 s of the real signal (no perturbation)
2. **Drift segment** — time-axis compression after t = 30 s simulates rising
   respiratory rate (frequency drift)
3. **Pause segment** — amplitude zeroed to ≈ 3 % for 8 s after t = 30 s
   simulates an intermittent breathing pause

This follows the "semi-real validation" path described in PAPER.md §5.1 —
real signal morphology is preserved in the stable portion; controlled
perturbations are applied only to isolate specific instability regimes.

> **Scientific reference.** PAPER.md is the canonical description of the
> operator, validation protocol, and baseline comparisons.

---

## Data Sources & Citations

If you use the PhysioNet BIDMC dataset in work that builds on this repository,
please cite both the dataset paper and the PhysioNet platform:

**BIDMC dataset**

> M. A. F. Pimentel, A. E. W. Johnson, P. H. Charlton, D. Birrenkott,
> P. J. Watkinson, L. Tarassenko, and D. A. Clifton,
> "Towards a Robust Estimation of Respiratory Rate from Pulse Oximeters,"
> *IEEE Trans. Biomed. Eng.*, vol. 64, no. 8, pp. 1914–1923, 2016.
> DOI: [10.1109/TBME.2016.2613124](https://doi.org/10.1109/TBME.2016.2613124)

```bibtex
@article{pimentel2016bidmc,
  author  = {Pimentel, Marco A. F. and Johnson, Alistair E. W. and
             Charlton, Peter H. and Birrenkott, Drew and Watkinson,
             Peter J. and Tarassenko, Lionel and Clifton, David A.},
  title   = {Towards a Robust Estimation of Respiratory Rate from
             Pulse Oximeters},
  journal = {IEEE Transactions on Biomedical Engineering},
  volume  = {64},
  number  = {8},
  pages   = {1914--1923},
  year    = {2016},
  doi     = {10.1109/TBME.2016.2613124}
}
```

**PhysioNet platform**

> A. L. Goldberger, L. A. N. Amaral, L. Glass, J. M. Hausdorff,
> P. Ch. Ivanov, R. G. Mark, J. E. Mietus, G. B. Moody, C.-K. Peng,
> and H. E. Stanley,
> "PhysioBank, PhysioToolkit, and PhysioNet: Components of a New
> Research Resource for Complex Physiologic Signals,"
> *Circulation*, vol. 101, no. 23, pp. e215–e220, 2000.
> DOI: [10.1161/01.CIR.101.23.e215](https://doi.org/10.1161/01.CIR.101.23.e215)

```bibtex
@article{goldberger2000physionet,
  author  = {Goldberger, Ary L. and Amaral, Luis A. N. and Glass, Leon
             and Hausdorff, Jeffrey M. and Ivanov, Plamen Ch. and
             Mark, Roger G. and Mietus, Joseph E. and Moody, George B.
             and Peng, Chung-Kang and Stanley, H. Eugene},
  title   = {{PhysioBank}, {PhysioToolkit}, and {PhysioNet}: Components
             of a New Research Resource for Complex Physiologic Signals},
  journal = {Circulation},
  volume  = {101},
  number  = {23},
  pages   = {e215--e220},
  year    = {2000},
  doi     = {10.1161/01.CIR.101.23.e215}
}
```

The BIDMC dataset is made available under the
[Open Data Commons Attribution License (ODC-By) v1.0](https://physionet.org/content/bidmc/1.0.0/).
Access it at: https://physionet.org/content/bidmc/1.0.0/

---

## Use Cases

> All use cases are for informational and research purposes.
> RespiroSync is not intended for medical diagnosis or treatment.
> See PAPER.md §9 for application perspectives.

### Consumer Applications

- **Sleep Tracking** — Monitor breathing regularity and pattern changes
- **Breathwork** — Guide breathing exercises with real-time phase feedback
- **Fitness & Recovery** — Track respiratory patterns during rest

### Research & Clinical Exploration

- **Respiratory instability research** — Reproducible, deterministic operator
- **Sleep studies** — Longitudinal respiratory pattern collection
- **Baseline evaluation** — Compare against RMS and FFT methods

---

## Performance

| Device | CPU Usage | RAM Usage | Battery Drain (8 hrs) |
|--------|-----------|-----------|----------------------|
| iPhone 12 Pro | ~1.2% | ~8 MB | ~3% |
| Google Pixel 6 | ~1.8% | ~9 MB | ~4% |
| Samsung Galaxy S21 | ~1.5% | ~9 MB | ~4% |

The phase–memory operator is **O(N)** in samples and streaming-capable
(PAPER.md §7.2).  No buffering or cloud synchronisation required.

---

## Documentation

| Document | Description |
|----------|-------------|
| [PAPER.md](PAPER.md) | **Canonical scientific description** of the operator |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design and phase–memory operator details |
| [docs/SIGNALS.md](docs/SIGNALS.md) | Signal processing pipeline and operator equations |
| [docs/VALIDATION.md](docs/VALIDATION.md) | Controlled validation protocol and reproducibility |
| [docs/BUILDING.md](docs/BUILDING.md) | Complete build instructions |
| [docs/QUICKSTART.md](docs/QUICKSTART.md) | Quick integration guide |
| [docs/PLATFORMS.md](docs/PLATFORMS.md) | Platform-specific implementation details |
| [docs/SECURITY.md](docs/SECURITY.md) | Security considerations |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | Version history |

---

## Contributing

Contributions are welcome.  We are particularly interested in:

- 🧪 **Validation** — Real-world testing against the four controlled regimes
- 📊 **Baseline comparisons** — RMS-envelope and FFT-peak-shift implementations
- 📱 **Platform optimisations** — Performance on additional device models
- 🐛 **Edge cases** — Bug reports and fixes for unusual signal conditions
- 🌍 **Documentation** — Improvements to guides and examples

### Getting Started

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-improvement`)
3. Commit your changes
4. Open a Pull Request

---

## Citation

If you use RespiroSync in academic or technical work, please cite:

```bibtex
@article{krueger_feeney_2025,
  author  = {Krüger, Marcel and Feeney, Don Michael Jr.},
  title   = {A Deterministic Phase–Memory Operator for Early Respiratory
             Instability Detection Using Smartphone-Based Chest Monitoring},
  journal = {Smart Wearable Technology},
  year    = {2025}
}
```

```bibtex
@software{respirosync2025,
  author  = {Feeney, Don Michael Jr. and Krüger, Marcel},
  title   = {RespiroSync: Chest-Mounted Respiratory Monitoring via Smartphone Sensors},
  year    = {2025},
  url     = {https://github.com/dfeen87/Smartphone-Based-Chest-Monitoring},
  version = {1.0.0}
}
```

---

## License

RespiroSync is licensed under the **MIT License**.

- ✅ Commercial use permitted
- ✅ Modification and distribution allowed
- ✅ Private use allowed
- ℹ️ Attribution required ("Powered by RespiroSync™")

See [`LICENSE`](LICENSE) for complete details.

---

## Contact

- 📧 **Email:** [dfeen87@gmail.com](mailto:dfeen87@gmail.com) · [marcelkrueger092@gmail.com](mailto:marcelkrueger092@gmail.com)
- 🐛 **Issues:** [GitHub Issues](https://github.com/dfeen87/Smartphone-Based-Chest-Monitoring/issues)

---

<div align="center">

*The instability metric ΔΦ(t) quantifies phase–memory divergence and supports
transparent threshold-based decision logic suitable for real-time on-device
monitoring.*  — PAPER.md §7.1

[⬆ Back to Top](#top)

</div>

