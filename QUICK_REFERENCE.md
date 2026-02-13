# Quick Reference Guide - RespiroSync Production Features

## Building and Testing

### Quick Build
```bash
# Build everything
make

# Run tests
make test

# Clean build
make clean

# Strict mode (extra warnings)
make strict
```

### Manual Build
```bash
# Core library
g++ -c core/respirosync_core.cpp -std=c++17 -O2 -Wall -o core/respirosync_core.o
ar rcs librespirosync.a core/respirosync_core.o

# Tests
g++ -o tests/test_core tests/test_core.cpp core/respirosync_core.cpp -std=c++17 -O2 -I.
./tests/test_core
```

## New API Features

### Signal Quality Assessment
```c
SleepMetrics metrics;
respiro_get_metrics(handle, timestamp, &metrics);

switch (metrics.signal_quality) {
    case SIGNAL_QUALITY_EXCELLENT:
        // High confidence - trust all metrics
        break;
    case SIGNAL_QUALITY_GOOD:
        // Acceptable - use normally
        break;
    case SIGNAL_QUALITY_FAIR:
        // Marginal - show warning to user
        break;
    case SIGNAL_QUALITY_POOR:
        // Poor - recommend repositioning device
        break;
    case SIGNAL_QUALITY_UNKNOWN:
        // Not enough data - collect more
        break;
}
```

### Signal-to-Noise Ratio
```c
SleepMetrics metrics;
respiro_get_metrics(handle, timestamp, &metrics);

if (metrics.signal_noise_ratio > 5.0f) {
    printf("Excellent signal quality\n");
} else if (metrics.signal_noise_ratio > 3.0f) {
    printf("Good signal quality\n");
} else if (metrics.signal_noise_ratio > 1.5f) {
    printf("Fair signal quality - may need adjustment\n");
} else {
    printf("Poor signal - reposition device\n");
}
```

### Version Checking
```c
const char* version = respiro_get_version();
printf("RespiroSync version: %s\n", version);

// For compatibility checks
if (strcmp(version, "1.0.0") >= 0) {
    // Features from v1.0.0+ available
}
```

## Error Handling Best Practices

### NULL Handle Safety
All API functions are NULL-safe:
```c
// These are all safe - no crashes
respiro_start_session(NULL, timestamp);
respiro_feed_gyro(NULL, x, y, z, timestamp);
respiro_feed_accel(NULL, x, y, z, timestamp);

SleepMetrics metrics;
respiro_get_metrics(NULL, timestamp, &metrics);
// metrics will be zero-filled with current_stage = UNKNOWN
```

### Invalid Input Handling
NaN and infinity are automatically rejected:
```c
// These will be silently rejected - no crashes or corruption
respiro_feed_gyro(handle, NAN, 0, 0, timestamp);
respiro_feed_accel(handle, INFINITY, 0, 0, timestamp);

// Engine continues to function normally
```

## Android JNI Integration

### Updated SleepMetrics Class
```kotlin
data class SleepMetrics(
    val currentStage: Int,           // 0=AWAKE, 1=LIGHT, 2=DEEP, 3=REM, 4=UNKNOWN
    val confidence: Float,           // 0.0-1.0
    val breathingRateBpm: Float,     // breaths per minute
    val breathingRegularity: Float,  // 0.0-1.0
    val movementIntensity: Float,    // 0.0-1.0
    val breathCyclesDetected: Int,   // count
    val possibleApnea: Int,          // 0=no, 1=yes
    val signalQuality: Int,          // 0=EXCELLENT, 1=GOOD, 2=FAIR, 3=POOR, 4=UNKNOWN
    val signalNoiseRatio: Float      // 0.0-10.0+
)
```

### JNI Constructor Signature
```cpp
// 9 parameters: (IFFFFFIIIF)V
// I=int, F=float, V=void
jmethodID constructor = env->GetMethodID(
    metricsClass, 
    "<init>", 
    "(IFFFFFIIIF)V"
);
```

## Metrics Interpretation Guide

### Breathing Rate
- **Normal Range**: 12-20 BPM (adult at rest)
- **Sleep**: 12-16 BPM typical
- **Deep Sleep**: May drop to 10-14 BPM
- **Alert**: <8 or >30 BPM

### Breathing Regularity
- **1.0**: Perfectly regular (machine-like)
- **0.8-0.9**: Very regular (deep sleep)
- **0.6-0.7**: Moderately regular (light sleep)
- **0.4-0.5**: Irregular (awake/REM)
- **<0.4**: Very irregular (active movement)

### Movement Intensity
- **0.0-0.05**: Minimal movement (deep sleep)
- **0.05-0.15**: Low movement (light sleep)
- **0.15-0.35**: Moderate movement (REM sleep)
- **>0.4**: High movement (awake)

### Confidence Score
- **0.9-1.0**: High confidence (20+ breath cycles)
- **0.7-0.9**: Good confidence (14-19 cycles)
- **0.5-0.7**: Fair confidence (10-13 cycles)
- **<0.5**: Low confidence (<10 cycles)

### Sleep Stage Classification
```
UNKNOWN (4)    -> <5 breath cycles detected
AWAKE (0)      -> movement_intensity > 0.4
DEEP_SLEEP (2) -> movement < 0.05 AND regularity > 0.85
REM_SLEEP (3)  -> movement 0.15-0.35 (rapid micro-movements)
LIGHT_SLEEP (1)-> Default for everything else
```

## Common Issues and Solutions

### Issue: Low Confidence Score
**Causes**:
- Insufficient data collection time
- Device not properly positioned
- Excessive movement

**Solutions**:
1. Collect data for at least 30 seconds
2. Ensure device is securely mounted on chest
3. User should be still (not walking/moving)

### Issue: Poor Signal Quality
**Causes**:
- Device not tight against chest
- User in motion
- Clothing interference

**Solutions**:
1. Use tighter garment/band
2. Ensure direct contact with chest
3. Remove bulky clothing between device and chest
4. Wait for user to settle down

### Issue: No Breaths Detected
**Causes**:
- Device positioned incorrectly
- Movement overwhelming breathing signal
- Insufficient sensitivity

**Solutions**:
1. Position device at sternum (center chest)
2. Ensure device orientation consistent
3. Keep user still for initial calibration (10-15 seconds)

## Performance Tips

### Optimal Sample Rates
- **Accelerometer**: 50-100 Hz recommended
- **Gyroscope**: 50-100 Hz recommended
- **Metrics Query**: Once per second sufficient

### Battery Optimization
```c
// Query metrics at reasonable intervals
// No need for high frequency
Timer timer(1000ms, []() {  // 1 second interval
    SleepMetrics metrics;
    respiro_get_metrics(handle, getCurrentTime(), &metrics);
    updateUI(metrics);
});
```

### Memory Usage
- **Peak RAM**: ~10 MB typical
- **Buffers**: 256 samples × 5 seconds = 1280 samples max
- **Breath History**: Last 60 seconds retained

## Testing Your Integration

### Basic Smoke Test
```c
RespiroHandle handle = respiro_create();
assert(handle != NULL);

respiro_start_session(handle, 0);

// Feed 30 seconds of simulated breathing
for (int i = 0; i < 1500; i++) {
    float t = i / 50.0f;
    float breathing = 0.1f * sin(2 * M_PI * 0.25f * t);
    respiro_feed_accel(handle, 0, 0, 9.81f + breathing, i * 20);
}

SleepMetrics metrics;
respiro_get_metrics(handle, 30000, &metrics);

assert(metrics.breath_cycles_detected > 0);
assert(metrics.breathing_rate_bpm > 0);
assert(metrics.signal_quality != SIGNAL_QUALITY_UNKNOWN);

respiro_destroy(handle);
```

## Support

For detailed information, see:
- `docs/PRODUCTION_IMPROVEMENTS.md` - Complete changelog
- `PRODUCTION_READY.md` - Deployment readiness summary
- `tests/test_core.cpp` - Usage examples and test cases

**Ready to ship!** ✅
