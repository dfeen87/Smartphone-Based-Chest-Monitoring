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
- [Reproducibility Layer](#reproducibility-layer)
- [Validation Protocol](#validation-protocol)
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

