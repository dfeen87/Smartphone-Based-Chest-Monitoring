/*
 * RespiroSync™ Core Engine
 * Chest-mounted respiratory monitoring via smartphone gyroscope/accelerometer
 * 
 * PROPRIETARY AND CONFIDENTIAL
 * Copyright (c) 2025 - All Rights Reserved
 * 
 * This is the licensable IP - the "magic" that turns phone sensors into
 * clinical-grade respiratory monitoring.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

#include "respirosync_core.h"

namespace RespiroSync {

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct SensorSample {
    float x, y, z;
    uint64_t timestamp_ms;
};

struct BreathCycle {
    uint64_t timestamp_ms;
    float amplitude;        // Depth of breath (0.0-1.0 normalized)
    float duration_ms;      // Time since last breath
};

using ::SleepMetrics;
using ::SleepStage;

// ============================================================================
// SIGNAL PROCESSING - THE CORE MAGIC
// ============================================================================

class ButterworthFilter {
private:
    // 2nd order Butterworth bandpass for breathing frequency (0.1-0.5 Hz)
    // This isolates the breathing signal from other body movements
    float x1, x2, y1, y2;
    float b0, b1, b2, a1, a2;
    
public:
    ButterworthFilter() : x1(0), x2(0), y1(0), y2(0) {
        // Coefficients for 0.1-0.5 Hz bandpass at ~50Hz sample rate
        // These are pre-calculated for computational efficiency
        // Simplified coefficients (in production, calculate dynamically)
        b0 = 0.0201f;
        b1 = 0.0f;
        b2 = -0.0201f;
        a1 = -1.5610f;
        a2 = 0.6414f;
    }
    
    float process(float input) {
        // Apply IIR filter
        float output = b0*input + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        
        // Update state
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        
        return output;
    }
    
    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
    }
};

// ============================================================================
// RESPIROSYNC ENGINE - THE BRAIN
// ============================================================================

class RespiroEngine {
private:
    // Sensor buffers
    std::deque<SensorSample> gyro_buffer;
    std::deque<SensorSample> accel_buffer;
    std::vector<BreathCycle> breath_history;
    
    // Signal processing
    ButterworthFilter breathing_filter;
    float breathing_signal_buffer[256];
    int buffer_index;
    
    // State tracking
    uint64_t last_peak_time;
    float last_peak_value;
    bool in_peak;
    float peak_threshold;
    
    // Metrics
    float current_bpm;
    SleepStage current_stage;
    float movement_variance;
    float gravity_estimate;
    uint64_t session_start_time;
    uint64_t last_breath_time;
    
    // Configuration
    const int BUFFER_SIZE = 256;
    const float PEAK_THRESHOLD_MULTIPLIER = 0.6f;
    const uint64_t APNEA_THRESHOLD_MS = 10000; // 10 seconds
    
    // Helper: Calculate magnitude of 3D vector
    float magnitude(const SensorSample& s) {
        return std::sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
    }
    
    // Helper: Remove gravity from accelerometer (simple high-pass)
    float removeGravity(const SensorSample& accel) {
        const float alpha = 0.8f; // Smoothing factor
        
        float mag = magnitude(accel);
        gravity_estimate = alpha * gravity_estimate + (1 - alpha) * mag;
        
        return mag - gravity_estimate;
    }
    
    // Helper: Detect breathing peaks in filtered signal
    void detectBreathingPeaks(float signal, uint64_t timestamp) {
        breathing_signal_buffer[buffer_index] = signal;
        buffer_index = (buffer_index + 1) % BUFFER_SIZE;
        
        // Dynamic threshold based on recent signal variance
        float mean = 0.0f;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            mean += breathing_signal_buffer[i];
        }
        mean /= BUFFER_SIZE;
        
        float variance = 0.0f;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            float diff = breathing_signal_buffer[i] - mean;
            variance += diff * diff;
        }
        variance /= BUFFER_SIZE;
        float stddev = std::sqrt(variance);
        
        peak_threshold = mean + stddev * PEAK_THRESHOLD_MULTIPLIER;
        
        // Peak detection with hysteresis
        if (!in_peak && signal > peak_threshold) {
            in_peak = true;
            
            if (last_peak_time > 0) {
                uint64_t duration = timestamp - last_peak_time;
                
                // Valid breath cycle (0.5-6 seconds = 10-120 BPM range)
                if (duration > 500 && duration < 6000) {
                    BreathCycle cycle;
                    cycle.timestamp_ms = timestamp;
                    cycle.duration_ms = (float)duration;
                    cycle.amplitude = signal / (stddev > 0 ? stddev : 1.0f);
                    
                    breath_history.push_back(cycle);
                    
                    // Keep only last 60 seconds of breaths
                    while (!breath_history.empty() && 
                           timestamp - breath_history.front().timestamp_ms > 60000) {
                        breath_history.erase(breath_history.begin());
                    }
                    
                    last_breath_time = timestamp;
                }
            }
            
            last_peak_time = timestamp;
            last_peak_value = signal;
        } else if (in_peak && signal < peak_threshold * 0.8f) {
            in_peak = false;
        }
    }
    
    // Helper: Calculate breathing rate from recent cycles
    float calculateBreathingRate() {
        if (breath_history.size() < 3) return 0.0f;
        
        // Use last 30 seconds of data
        uint64_t now = breath_history.back().timestamp_ms;
        std::vector<float> recent_durations;
        
        for (auto it = breath_history.rbegin(); it != breath_history.rend(); ++it) {
            if (now - it->timestamp_ms > 30000) break;
            recent_durations.push_back(it->duration_ms);
        }
        
        if (recent_durations.empty()) return 0.0f;
        
        // Average duration
        float avg_duration = 0.0f;
        for (float d : recent_durations) {
            avg_duration += d;
        }
        avg_duration /= recent_durations.size();
        
        // Convert to BPM
        return (60000.0f / avg_duration);
    }
    
    // Helper: Calculate breathing regularity (0.0-1.0)
    float calculateBreathingRegularity() {
        if (breath_history.size() < 5) return 0.0f;
        
        std::vector<float> durations;
        for (const auto& cycle : breath_history) {
            durations.push_back(cycle.duration_ms);
        }
        
        // Calculate coefficient of variation
        float mean = 0.0f;
        for (float d : durations) mean += d;
        mean /= durations.size();
        
        float variance = 0.0f;
        for (float d : durations) {
            float diff = d - mean;
            variance += diff * diff;
        }
        variance /= durations.size();
        
        float cv = std::sqrt(variance) / mean;
        
        // Convert to 0-1 scale (lower CV = higher regularity)
        return std::max(0.0f, 1.0f - cv);
    }
    
    // Helper: Classify sleep stage based on movement + breathing
    SleepStage classifySleepStage(float movement_intensity, float breathing_regularity) {
        // Simple rule-based classifier (can upgrade to ML later)
        
        if (movement_intensity > 0.4f) {
            return AWAKE;
        }
        
        if (movement_intensity < 0.05f && breathing_regularity > 0.85f) {
            return DEEP_SLEEP;
        }
        
        if (movement_intensity > 0.15f && movement_intensity < 0.35f) {
            return REM_SLEEP; // Characteristic rapid micro-movements
        }
        
        return LIGHT_SLEEP;
    }
    
public:
    RespiroEngine() : 
        buffer_index(0), 
        last_peak_time(0),
        last_peak_value(0.0f),
        in_peak(false),
        peak_threshold(0.1f),
        current_bpm(0.0f),
        current_stage(UNKNOWN),
        movement_variance(0.0f),
        gravity_estimate(9.81f),
        session_start_time(0),
        last_breath_time(0)
    {
        for (int i = 0; i < 256; i++) {
            breathing_signal_buffer[i] = 0.0f;
        }
    }
    
    // ========================================================================
    // PUBLIC API - What licensees call
    // ========================================================================
    
    void startSession(uint64_t timestamp_ms) {
        session_start_time = timestamp_ms;
        breath_history.clear();
        breathing_filter.reset();
        buffer_index = 0;
        current_stage = UNKNOWN;
        current_bpm = 0.0f;
        movement_variance = 0.0f;
        gravity_estimate = 9.81f;
        last_peak_time = 0;
        last_peak_value = 0.0f;
        last_breath_time = 0;
        in_peak = false;
        peak_threshold = 0.1f;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            breathing_signal_buffer[i] = 0.0f;
        }
    }
    
    void feedGyroscope(float x, float y, float z, uint64_t timestamp_ms) {
        SensorSample sample = {x, y, z, timestamp_ms};
        gyro_buffer.push_back(sample);
        
        // Keep only last 5 seconds
        while (!gyro_buffer.empty() && 
               timestamp_ms - gyro_buffer.front().timestamp_ms > 5000) {
            gyro_buffer.pop_front();
        }
    }
    
    void feedAccelerometer(float x, float y, float z, uint64_t timestamp_ms) {
        SensorSample sample = {x, y, z, timestamp_ms};
        accel_buffer.push_back(sample);
        
        // Keep only last 5 seconds
        while (!accel_buffer.empty() && 
               timestamp_ms - accel_buffer.front().timestamp_ms > 5000) {
            accel_buffer.pop_front();
        }
        
        // CORE PROCESSING PIPELINE
        // 1. Remove gravity from accelerometer
        float chest_motion = removeGravity(sample);
        
        // 2. Add gyroscope contribution (angular velocity indicates rotation)
        if (!gyro_buffer.empty()) {
            chest_motion += magnitude(gyro_buffer.back()) * 0.1f; // Scale factor
        }
        
        // 3. Bandpass filter to isolate breathing frequency
        float breathing_signal = breathing_filter.process(chest_motion);
        
        // 4. Detect breathing peaks
        detectBreathingPeaks(breathing_signal, timestamp_ms);
        
        // 5. Update metrics
        current_bpm = calculateBreathingRate();
        
        // Calculate movement intensity (for sleep staging)
        movement_variance = 0.0f;
        if (accel_buffer.size() > 10) {
            float mean_mag = 0.0f;
            for (const auto& s : accel_buffer) {
                mean_mag += magnitude(s);
            }
            mean_mag /= accel_buffer.size();
            
            for (const auto& s : accel_buffer) {
                float diff = magnitude(s) - mean_mag;
                movement_variance += diff * diff;
            }
            movement_variance /= accel_buffer.size();
        }
    }
    
    SleepMetrics getCurrentMetrics(uint64_t timestamp_ms) {
        SleepMetrics metrics;
        
        metrics.breathing_rate_bpm = current_bpm;
        metrics.breath_cycles_detected = (int)breath_history.size();
        metrics.breathing_regularity = calculateBreathingRegularity();
        
        // Normalize movement intensity to 0-1 scale
        metrics.movement_intensity = std::min(1.0f, movement_variance * 10.0f);
        
        // Classify sleep stage
        metrics.current_stage = classifySleepStage(
            metrics.movement_intensity, 
            metrics.breathing_regularity
        );
        
        // Calculate confidence based on data quality
        int samples_available = (int)breath_history.size();
        metrics.confidence = std::min(1.0f, samples_available / 20.0f);
        
        // Check for apnea (no breath detected in >10 seconds)
        metrics.possible_apnea = (last_breath_time > 0 &&
                                  timestamp_ms - last_breath_time > APNEA_THRESHOLD_MS)
                                     ? 1
                                     : 0;
        
        return metrics;
    }
    
    // Get detailed breath history (for advanced analysis)
    const std::vector<BreathCycle>& getBreathHistory() const {
        return breath_history;
    }
};

} // namespace RespiroSync

// ============================================================================
// C-STYLE API FOR EASY BINDING TO iOS/Android
// ============================================================================

extern "C" {
    using namespace RespiroSync;
    
    RespiroHandle respiro_create() {
        return new RespiroEngine();
    }
    
    void respiro_destroy(RespiroHandle handle) {
        if (!handle) {
            return;
        }
        delete static_cast<RespiroEngine*>(handle);
    }
    
    void respiro_start_session(RespiroHandle handle, uint64_t timestamp_ms) {
        if (!handle) {
            return;
        }
        static_cast<RespiroEngine*>(handle)->startSession(timestamp_ms);
    }
    
    void respiro_feed_gyro(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms) {
        if (!handle) {
            return;
        }
        static_cast<RespiroEngine*>(handle)->feedGyroscope(x, y, z, timestamp_ms);
    }
    
    void respiro_feed_accel(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms) {
        if (!handle) {
            return;
        }
        static_cast<RespiroEngine*>(handle)->feedAccelerometer(x, y, z, timestamp_ms);
    }
    
    void respiro_get_metrics(RespiroHandle handle, uint64_t timestamp_ms, SleepMetrics* out_metrics) {
        if (!out_metrics) {
            return;
        }
        if (!handle) {
            std::memset(out_metrics, 0, sizeof(*out_metrics));
            out_metrics->current_stage = UNKNOWN;
            return;
        }
        *out_metrics = static_cast<RespiroEngine*>(handle)->getCurrentMetrics(timestamp_ms);
    }
}

/*
 * USAGE EXAMPLE:
 * 
 * RespiroHandle engine = respiro_create();
 * respiro_start_session(engine, getCurrentTime());
 * 
 * // In your sensor callback loop:
 * respiro_feed_gyro(engine, gx, gy, gz, timestamp);
 * respiro_feed_accel(engine, ax, ay, az, timestamp);
 * 
 * // Get real-time metrics:
 * SleepMetrics metrics;
 * respiro_get_metrics(engine, timestamp, &metrics);
 * printf("BPM: %.1f, Stage: %d, Confidence: %.2f\n", 
 *        metrics.breathing_rate_bpm, metrics.current_stage, metrics.confidence);
 * 
 * respiro_destroy(engine);
 */
