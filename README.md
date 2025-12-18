# RespiroSync™

MIT License.

**Cross-Platform Respiratory & Sleep Monitoring Using Smartphone Motion Sensors**

RespiroSync is a portable, on-device engine for estimating respiratory patterns and sleep metrics using only a smartphone's built-in accelerometer and gyroscope. By leveraging chest-mounted motion signals, RespiroSync provides low-cost, hardware-free access to respiratory insights traditionally requiring specialized equipment.

## What Is RespiroSync?

RespiroSync transforms a chest-mounted smartphone into a passive respiratory monitoring system capable of estimating:

🫁 Breathing rate (BPM)

😴 Sleep stage heuristics (Awake, Light, Deep, REM)

⚠️ Breathing irregularities (e.g., prolonged pauses)

📊 Sleep quality indicators (regularity, movement, confidence)

**No wristbands.**
**No rings.**
**No external sensors.**

Just a smartphone securely positioned on the chest (e.g., vest, compression garment, band).

## Why This Works

Most consumer sleep trackers rely on indirect proxies such as wrist-based heart rate or expensive chest hardware. RespiroSync takes a different approach:

**Chest-mounted motion directly encodes respiratory mechanics.**

The core engine performs:

```
Accelerometer + Gyroscope (chest-mounted)
        ↓
Gravity removal & sensor fusion
        ↓
Breathing-frequency bandpass filtering (≈0.1–0.5 Hz)
        ↓
Dynamic peak detection
        ↓
Breath cycle estimation
        ↓
Respiratory + sleep metrics
```

This repository contains the core algorithm and bindings that make this pipeline portable and efficient.

## Project Status

**Version:** 1.0.0  
**Stability:** Stable public C API  
**Scope:** Respiratory & sleep inference (heuristic, non-diagnostic)

RespiroSync prioritizes architectural clarity and correctness over feature breadth. All signal processing occurs on-device, with no cloud dependency.

## Quick Start

### iOS (Swift)

```swift
let respiro = RespiroSync()
respiro.startSession()

Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
    let metrics = respiro.getCurrentMetrics()
    print("Breathing: \(metrics.breathingRateBPM) BPM")
    print("Stage: \(metrics.sleepStage)")
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
            Log.d("RespiroSync", "Stage: ${it.sleepStage}")
        }
        handler.postDelayed(this, 1000)
    }
}, 1000)
```

## Core Capabilities

✅ Real-time respiratory rate estimation

✅ Breath cycle detection and regularity analysis

✅ Heuristic sleep stage classification

✅ Detection of prolonged breathing pauses

✅ Movement and restlessness tracking

✅ Confidence scoring based on signal quality

## Technical Characteristics

⚡ Low overhead: typically <2% CPU, <10 MB RAM

🔋 Battery-efficient: <5% overnight drain (typical devices)

🔒 Privacy-first: 100% on-device processing

📱 Cross-platform: Android & iOS via a shared C++ core

🧩 No dependencies: portable, self-contained engine

**Metrics are heuristic and informational. RespiroSync is not a medical device.**

## Architecture

```
respirosync_core.cpp   ← Core signal processing (C++)
        ▲
        │  Stable C API
        ▼
respirosync_ios.mm     ← iOS Core Motion bridge
respirosync_android.cpp← Android SensorManager bridge
        ▼
Swift / Kotlin API     ← Application layer
```

**One core engine.**  
**Thin platform bindings.**  
**Deterministic behavior across platforms.**

See:

- `docs/ARCHITECTURE.md`
- `docs/SIGNALS.md`
- `docs/PLATFORMS.md`

## Building

### iOS (Static Library)

```bash
clang++ -c respirosync_core.cpp -std=c++17 -O3 -o core.o
clang++ -c respirosync_ios.mm -framework CoreMotion -o ios.o
ar rcs librespirosync.a core.o ios.o
```

### Android (CMake)

```cmake
add_library(
    respirosync
    SHARED
    respirosync_core.cpp
    respirosync_android.cpp
)
```

See project documentation for full build instructions.

## Use Cases

### Consumer Applications

- Sleep tracking
- Meditation & breathwork apps
- Fitness & recovery monitoring
- Infant or passive breathing observation

### Research & Clinical Exploration

- Sleep studies
- Respiratory pattern analysis
- Remote monitoring research
- Low-cost alternatives to traditional instrumentation

### Hardware & Embedded Systems

- Smart garments
- Athletic performance wearables
- Experimental medical devices
- Veterinary monitoring

## Performance Snapshot

Tested on representative devices:

- **iPhone 12 Pro:** ~1.2% CPU, ~8 MB RAM, ~3% battery / 8 hrs
- **Pixel 6:** ~1.8% CPU, ~9 MB RAM, ~4% battery / 8 hrs
- **Galaxy S21:** ~1.5% CPU, ~9 MB RAM, ~4% battery / 8 hrs

Respiratory rate estimation shows strong correlation with reference sensors in controlled tests.

## Contributing

Contributions are welcome.

Areas of interest:

🧪 Validation and benchmarking

🤖 Advanced sleep classification models

📱 Platform optimizations

🐛 Real-world edge cases

🌍 Documentation improvements

See `CONTRIBUTING.md` for guidelines.

## Citation

If you use RespiroSync in academic or technical work, please cite:

```bibtex
@software{respirosync2025,
  author = {Feeney, Don Michael Jr.},
  title  = {RespiroSync: Chest-Mounted Respiratory Monitoring via Smartphone Sensors},
  year   = {2025},
  url    = {https://github.com/dfeen87/RespiroSync-Chest-Based-Respiratory-Monitoring-System}
}
```

## License

MIT License with attribution requirement.  
Commercial use is permitted.

See `LICENSE` for details.

## Recognition

If you build a product using RespiroSync:

- Include "Powered by RespiroSync™"
- Link back to this repository

That's it.

## Origin

RespiroSync began with a simple realization:  
**smartphones already contain the sensors needed for meaningful respiratory monitoring.**

This project exists to make that capability accessible, inspectable, and reusable.

## Contact

📧 **Email:** dfeen87@gmail.com

💼 **LinkedIn:** https://www.linkedin.com/in/don-michael-feeney-jr-908a96351

If this project helped you, consider starring the repo — it genuinely helps independent developers.

---

*"The best way to predict the future is to invent it — and then give others the tools to build on it."*
