/*
 * RespiroSync™ Core Engine
 * Chest-mounted respiratory monitoring via smartphone gyroscope/accelerometer
 *
 * Scientific basis: "A Deterministic Phase–Memory Operator for Early
 * Respiratory Instability Detection Using Smartphone-Based Chest Monitoring"
 * See PAPER.md for full derivation and validation protocol.
 *
 * Pipeline (PAPER.md §7.1):
 *
 *   Chest IMU  →  Preprocess           →  Analytic Signal  →  Phase  →  Memory  →  Decision
 *   (accel/gyro)  (detrend + bandpass)     (Hilbert approx)    θ(t)     ω̄(t)      ΔΦ(t) > α·σ_ω
 *
 * Key operator variables (equation numbers refer to PAPER.md):
 *   x(t)   – scalar respiration channel formed by gravity-axis projection (Eq. 1)
 *   z(t)   – analytic signal = x(t) + i·H[x(t)]  (Eq. 2)
 *   θ(t)   – instantaneous phase = arg(z(t))
 *   ω(t)   – instantaneous phase velocity = dθ/dt  (Eq. 3)
 *   ω̄(t)  – short-term phase memory = rolling mean of ω over M samples  (Eq. 4)
 *   ΔΦ(t) – instability metric = |ω(t) − ω̄(t)|  (Eq. 5)
 *   σ_ω   – baseline std-dev of ω on initial stable segment  (Eq. 6)
 *   α     – sensitivity parameter ∈ [2, 3]  (Eq. 6)
 *   L     – persistence window (optional, Eq. 7)
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
using ::SignalQuality;

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
// PHASE–MEMORY OPERATOR  (PAPER.md §3–4)
// ============================================================================
//
// Implements the deterministic instability metric ΔΦ(t) described in the
// manuscript.  All variable names and equation numbers refer to PAPER.md.
//
// Step 1 – Analytic signal (Eq. 2):
//   The Hilbert transform H[x] is approximated via the derivative method,
//   which is valid for narrow-band signals (breathing band 0.1–0.5 Hz):
//       H[x](t) ≈ −(1/ω₀) · dx/dt
//   where ω₀ = 2π·f₀ is the angular centre frequency of the passband.
//
// Step 2 – Instantaneous phase (Eq. 2 / §3.1):
//       θ(t) = atan2(H[x](t), x(t))
//
// Step 3 – Phase velocity with unwrapping (Eq. 3):
//       ω(t) = Δθ / Δt   (discrete, after 2π-unwrap)
//
// Step 4 – Short-term phase memory (Eq. 4):
//       ω̄(t) = (1/M) Σ_{k=0}^{M−1} ω[n−k]   (rolling mean, M samples)
//
// Step 5 – Instability metric (Eq. 5):
//       ΔΦ(t) = |ω(t) − ω̄(t)|
//
// Step 6 – Baseline-normalized threshold (Eq. 6):
//       instability iff ΔΦ(t) > α · σ_ω
//   σ_ω is estimated on the first BASELINE_WINDOW samples; α = 2.0 (default).

class PhaseMemoryOperator {
public:
    // Tunable parameters (PAPER.md §4.2 and §8)
    //   alpha – sensitivity parameter α ∈ [2, 3]  (Eq. 6)
    //   memory_samples – phase-memory window M  (Eq. 4)
    //   baseline_samples – number of initial samples used to estimate σ_ω
    static constexpr float DEFAULT_ALPHA       = 2.0f;
    static constexpr int   MEMORY_SAMPLES      = 150;   // Tₘ ≈ 3 s at 50 Hz
    static constexpr int   BASELINE_SAMPLES    = 250;   // ≈ 5 s at 50 Hz
    // Centre angular frequency of the breathing passband (≈0.3 Hz at 50 Hz)
    static constexpr float OMEGA_0             = 2.0f * 3.14159265f * 0.3f;
    static constexpr float DT                  = 1.0f / 50.0f; // nominal sample period (s)

private:
    float prev_x;           // x[n−1] for derivative (dx/dt)
    float prev_theta;       // θ[n−1] for unwrapped phase velocity
    bool  initialized;

    // Rolling buffer for phase velocity ω (M samples)
    float  omega_buf[MEMORY_SAMPLES];
    int    omega_idx;
    float  omega_sum;       // running sum for fast mean
    int    omega_count;     // samples filled so far

    // Baseline estimation for σ_ω (first BASELINE_SAMPLES samples)
    float  baseline_buf[BASELINE_SAMPLES];
    int    baseline_count;
    bool   baseline_ready;
    float  sigma_omega;     // σ_ω (Eq. 6)

    // Output
    float  delta_phi;       // ΔΦ(t) – most recent instability metric  (Eq. 5)

    // Unwrap a phase difference into (−π, π]
    static float unwrapDelta(float d) {
        while (d >  3.14159265f) d -= 2.0f * 3.14159265f;
        while (d < -3.14159265f) d += 2.0f * 3.14159265f;
        return d;
    }

public:
    PhaseMemoryOperator() { reset(); }

    void reset() {
        prev_x        = 0.0f;
        prev_theta    = 0.0f;
        initialized   = false;
        omega_idx     = 0;
        omega_sum     = 0.0f;
        omega_count   = 0;
        baseline_count = 0;
        baseline_ready = false;
        sigma_omega   = 1.0f;  // safe non-zero default until calibrated
        delta_phi     = 0.0f;
        for (int i = 0; i < MEMORY_SAMPLES;   ++i) omega_buf[i]    = 0.0f;
        for (int i = 0; i < BASELINE_SAMPLES; ++i) baseline_buf[i] = 0.0f;
    }

    // Feed one bandpass-filtered sample x[n].
    // Returns the current instability metric ΔΦ(t).
    float update(float x) {
        if (!initialized) {
            prev_x     = x;
            prev_theta = 0.0f;
            initialized = true;
            return 0.0f;
        }

        // --- Step 1: Analytic signal via derivative approximation (Eq. 2) ---
        // H[x](t) ≈ −dx/dt / ω₀   (valid for narrow-band breathing signal)
        float dx     = x - prev_x;
        float h_x    = -dx / (OMEGA_0 * DT);   // approximate Hilbert component
        prev_x = x;

        // --- Step 2: Instantaneous phase θ(t) (§3.1) ---
        float theta  = std::atan2(h_x, x);

        // --- Step 3: Phase velocity ω(t) with 2π-unwrap (Eq. 3) ---
        float d_theta = unwrapDelta(theta - prev_theta);
        float omega   = d_theta / DT;           // rad/s
        prev_theta    = theta;

        // --- Step 4: Phase memory ω̄(t) – rolling mean over M samples (Eq. 4) ---
        // Outgoing sample leaves the window
        float outgoing  = omega_buf[omega_idx];
        omega_buf[omega_idx] = omega;
        omega_sum += omega - outgoing;
        omega_idx = (omega_idx + 1) % MEMORY_SAMPLES;
        if (omega_count < MEMORY_SAMPLES) ++omega_count;

        float omega_mean = (omega_count > 0) ? (omega_sum / omega_count) : omega;

        // --- Step 5: Instability metric ΔΦ(t) = |ω(t) − ω̄(t)| (Eq. 5) ---
        delta_phi = std::fabs(omega - omega_mean);

        // --- Baseline σ_ω estimation (Eq. 6, calibration window) ---
        if (!baseline_ready) {
            baseline_buf[baseline_count++] = omega;
            if (baseline_count >= BASELINE_SAMPLES) {
                // Compute mean and std-dev of ω over the baseline window
                float mean = 0.0f;
                for (int i = 0; i < BASELINE_SAMPLES; ++i) mean += baseline_buf[i];
                mean /= BASELINE_SAMPLES;
                float var = 0.0f;
                for (int i = 0; i < BASELINE_SAMPLES; ++i) {
                    float diff = baseline_buf[i] - mean;
                    var += diff * diff;
                }
                var /= BASELINE_SAMPLES;
                sigma_omega   = std::sqrt(var);
                if (sigma_omega < 1e-4f) sigma_omega = 1e-4f;  // guard against zero
                baseline_ready = true;
            }
        }

        return delta_phi;
    }

    // ΔΦ(t) – most recent instability score  (Eq. 5)
    float instabilityScore() const { return delta_phi; }

    // True when ΔΦ(t) > α · σ_ω  (Eq. 6)
    bool instabilityDetected(float alpha = DEFAULT_ALPHA) const {
        return baseline_ready && (delta_phi > alpha * sigma_omega);
    }

    // σ_ω estimated on the calibration window
    float baselineSigma() const { return sigma_omega; }
};



class RespiroEngine {
private:
    // Sensor buffers
    std::deque<SensorSample> gyro_buffer;
    std::deque<SensorSample> accel_buffer;
    std::deque<BreathCycle> breath_history;
    std::deque<float> accel_magnitude_buffer;
    
    // Signal processing
    ButterworthFilter breathing_filter;
    float breathing_signal_buffer[256];
    float breathing_signal_sum;
    float breathing_signal_sum_squares;
    int buffer_index;

    // Phase–memory operator (PAPER.md §3–4)
    // Computes ΔΦ(t) = |ω(t) − ω̄(t)| and the threshold decision (Eq. 5–6).
    PhaseMemoryOperator phase_memory;
    
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
    float accel_magnitude_sum;
    float accel_magnitude_sum_squares;
    
    // Configuration
    static constexpr int BUFFER_SIZE = 256;
    static constexpr float PEAK_THRESHOLD_MULTIPLIER = 0.6f;
    static constexpr uint64_t APNEA_THRESHOLD_MS = 10000; // 10 seconds
    static constexpr float EPSILON = 1e-6f; // For floating point comparisons
    static constexpr float MIN_STDDEV = 1e-6f; // Minimum standard deviation to prevent singularities
    
    // Helper: Calculate magnitude of 3D vector
    float magnitude(const SensorSample& s) {
        return std::sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
    }
    
    // Helper: Remove gravity from accelerometer (simple high-pass)
    float removeGravity(float magnitude) {
        const float alpha = 0.8f; // Smoothing factor
        
        gravity_estimate = alpha * gravity_estimate + (1 - alpha) * magnitude;
        
        return magnitude - gravity_estimate;
    }
    
    // Helper: Detect breathing peaks in filtered signal
    void detectBreathingPeaks(float signal, uint64_t timestamp) {
        // Validate buffer index (defensive programming)
        if (buffer_index < 0 || buffer_index >= BUFFER_SIZE) {
            buffer_index = 0;
        }
        
        float outgoing = breathing_signal_buffer[buffer_index];
        breathing_signal_buffer[buffer_index] = signal;
        breathing_signal_sum += signal - outgoing;
        breathing_signal_sum_squares += (signal * signal) - (outgoing * outgoing);
        buffer_index = (buffer_index + 1) % BUFFER_SIZE;
        
        // Dynamic threshold based on recent signal variance
        float mean = breathing_signal_sum / BUFFER_SIZE;
        float variance = (breathing_signal_sum_squares / BUFFER_SIZE) - (mean * mean);
        float stddev = std::sqrt(std::max(0.0f, variance));
        stddev = std::max(MIN_STDDEV, stddev); // Prevent singularities
        
        peak_threshold = mean + stddev * PEAK_THRESHOLD_MULTIPLIER;
        
        // Peak detection with hysteresis
        if (!in_peak && signal > peak_threshold) {
            in_peak = true;
            
            // Check for timestamp monotonicity to prevent wraparound issues
            if (last_peak_time > 0 && timestamp >= last_peak_time) {
                uint64_t duration = timestamp - last_peak_time;
                
                // Valid breath cycle (0.5-6 seconds = 10-120 BPM range)
                if (duration > 500 && duration < 6000) {
                    BreathCycle cycle;
                    cycle.timestamp_ms = timestamp;
                    cycle.duration_ms = (float)duration;
                    cycle.amplitude = signal / stddev; // stddev already validated to be >= MIN_STDDEV
                    
                    breath_history.push_back(cycle);
                    
                    // Keep only last 60 seconds of breaths
                    while (!breath_history.empty() && 
                           timestamp - breath_history.front().timestamp_ms > 60000) {
                        breath_history.pop_front();
                    }
                    
                    last_breath_time = timestamp;
                }
            }
            
            last_peak_time = timestamp;
            last_peak_value = signal;
        } else if (in_peak && signal < (peak_threshold * 0.8f - EPSILON)) {
            // Use epsilon-based comparison for floating point reliability
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
            // Check timestamp monotonicity - allow equal timestamps
            if (it->timestamp_ms > now || now - it->timestamp_ms > 30000) break;
            recent_durations.push_back(it->duration_ms);
        }
        
        if (recent_durations.empty()) return 0.0f;
        
        // Average duration
        float avg_duration = 0.0f;
        for (float d : recent_durations) {
            avg_duration += d;
        }
        avg_duration /= recent_durations.size();
        
        // Convert to BPM - protect against division by zero
        if (avg_duration < EPSILON) {
            return 0.0f;
        }
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
        
        // Protect against division by zero
        if (mean < EPSILON) {
            return 0.0f;
        }
        
        float cv = std::sqrt(variance) / mean;
        
        // Convert to 0-1 scale (lower CV = higher regularity)
        // Clamp to ensure result stays in valid range
        return std::max(0.0f, std::min(1.0f, 1.0f - cv));
    }
    
    // Helper: Assess signal quality based on data characteristics
    SignalQuality assessSignalQuality(float snr, size_t sample_count, float regularity) {
        // Require minimum data
        if (sample_count < 5) {
            return SIGNAL_QUALITY_UNKNOWN;
        }
        
        // Excellent: high SNR, good regularity, sufficient data
        if (snr > 5.0f && regularity > 0.7f && sample_count >= 20) {
            return SIGNAL_QUALITY_EXCELLENT;
        }
        
        // Good: acceptable SNR and regularity
        if (snr > 3.0f && regularity > 0.5f && sample_count >= 10) {
            return SIGNAL_QUALITY_GOOD;
        }
        
        // Fair: marginal quality
        if (snr > 1.5f && sample_count >= 5) {
            return SIGNAL_QUALITY_FAIR;
        }
        
        // Poor: insufficient quality
        return SIGNAL_QUALITY_POOR;
    }
    
    // Helper: Calculate signal-to-noise ratio estimate
    float calculateSNR() {
        if (breath_history.size() < 3) {
            return 0.0f;
        }
        
        // Calculate variance in breath amplitudes
        std::vector<float> amplitudes;
        for (const auto& cycle : breath_history) {
            amplitudes.push_back(cycle.amplitude);
        }
        
        float mean_amplitude = 0.0f;
        for (float a : amplitudes) {
            mean_amplitude += a;
        }
        mean_amplitude /= amplitudes.size();
        
        float variance = 0.0f;
        for (float a : amplitudes) {
            float diff = a - mean_amplitude;
            variance += diff * diff;
        }
        variance /= amplitudes.size();
        
        float noise = std::sqrt(variance);
        
        // SNR = signal / noise
        if (noise < EPSILON) {
            return 0.0f;
        }
        
        return mean_amplitude / noise;
    }
    
    // Helper: Classify sleep stage based on movement + breathing
    SleepStage classifySleepStage(float movement_intensity, float breathing_regularity, size_t sample_count) {
        // Need minimum data to classify
        if (sample_count < 5) {
            return UNKNOWN;
        }
        
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
        last_breath_time(0),
        accel_magnitude_sum(0.0f),
        accel_magnitude_sum_squares(0.0f)
    {
        for (int i = 0; i < 256; i++) {
            breathing_signal_buffer[i] = 0.0f;
        }
        breathing_signal_sum = 0.0f;
        breathing_signal_sum_squares = 0.0f;
    }
    
    // ========================================================================
    // PUBLIC API - What licensees call
    // ========================================================================
    
    void startSession(uint64_t timestamp_ms) {
        session_start_time = timestamp_ms;
        breath_history.clear();
        breathing_filter.reset();
        phase_memory.reset();  // reset phase–memory operator (PAPER.md §3)
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
        accel_magnitude_sum = 0.0f;
        accel_magnitude_sum_squares = 0.0f;
        accel_magnitude_buffer.clear();
        for (int i = 0; i < BUFFER_SIZE; i++) {
            breathing_signal_buffer[i] = 0.0f;
        }
        breathing_signal_sum = 0.0f;
        breathing_signal_sum_squares = 0.0f;
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
        float accel_magnitude = magnitude(sample);
        accel_magnitude_buffer.push_back(accel_magnitude);
        accel_magnitude_sum += accel_magnitude;
        accel_magnitude_sum_squares += accel_magnitude * accel_magnitude;
        
        // Keep only last 5 seconds
        while (!accel_buffer.empty() && 
               timestamp_ms - accel_buffer.front().timestamp_ms > 5000) {
            if (!accel_magnitude_buffer.empty()) {
                float outgoing_magnitude = accel_magnitude_buffer.front();
                accel_magnitude_sum -= outgoing_magnitude;
                accel_magnitude_sum_squares -= outgoing_magnitude * outgoing_magnitude;
                accel_magnitude_buffer.pop_front();
            }
            accel_buffer.pop_front();
        }
        
        // CORE PROCESSING PIPELINE  (PAPER.md §7.1)
        //
        // Step 1 – Form scalar respiration channel x(t)  (Eq. 1)
        //   Here we use the gravity-removed accelerometer magnitude as x(t).
        //   A gravity-aligned projection a(t)·û_b(t) would be preferred when
        //   sensor-fusion orientation is available (see PAPER.md §2.3).
        float chest_motion = removeGravity(accel_magnitude);
        
        // Gyroscope contribution – optional motion-rejection gating (§2.4)
        // Angular velocity ‖Ω(t)‖ is blended lightly; in a full implementation
        // it should gate rather than add (PAPER.md §2.4).
        if (!gyro_buffer.empty()) {
            SensorSample gyro_sample = gyro_buffer.back();
            chest_motion += magnitude(gyro_sample) * 0.1f; // Scale factor
        }
        
        // Step 2 – Bandpass filter x(t) to isolate breathing frequency (§2.4)
        //   Removes drift (low-frequency) and motion artefacts (high-frequency).
        //   Passband ≈ 0.1–0.5 Hz corresponds to 6–30 breaths per minute.
        float breathing_signal = breathing_filter.process(chest_motion);
        
        // Step 3 – Phase–memory operator on bandpass-filtered x(t) (§3–4)
        //   Updates the analytic signal approximation, instantaneous phase θ(t),
        //   phase velocity ω(t), phase memory ω̄(t), and instability score ΔΦ(t).
        phase_memory.update(breathing_signal);
        
        // Step 4 – Legacy peak detection (breath-cycle rate estimation)
        detectBreathingPeaks(breathing_signal, timestamp_ms);
        
        // Step 5 – Update respiratory rate estimate
        current_bpm = calculateBreathingRate();
        
        // Calculate movement intensity (for sleep staging)
        movement_variance = 0.0f;
        if (accel_magnitude_buffer.size() > 10) {
            float mean_mag = accel_magnitude_sum / accel_magnitude_buffer.size();
            float variance = (accel_magnitude_sum_squares / accel_magnitude_buffer.size()) -
                             (mean_mag * mean_mag);
            movement_variance = std::max(0.0f, variance);
        }
    }
    
    SleepMetrics getCurrentMetrics(uint64_t timestamp_ms) {
        SleepMetrics metrics;
        std::memset(&metrics, 0, sizeof(metrics));
        
        metrics.breathing_rate_bpm = current_bpm;
        metrics.breath_cycles_detected = (int)breath_history.size();
        metrics.breathing_regularity = calculateBreathingRegularity();
        
        // Normalize movement intensity to 0-1 scale
        metrics.movement_intensity = std::min(1.0f, movement_variance * 10.0f);
        
        // Classify sleep stage
        metrics.current_stage = classifySleepStage(
            metrics.movement_intensity, 
            metrics.breathing_regularity,
            breath_history.size()
        );
        
        // Calculate confidence based on data quality
        int samples_available = (int)breath_history.size();
        metrics.confidence = std::min(1.0f, static_cast<float>(samples_available) / 20.0f);
        
        // Check for apnea (no breath detected in >10 seconds)
        metrics.possible_apnea = (last_breath_time > 0 &&
                                  timestamp_ms - last_breath_time > APNEA_THRESHOLD_MS)
                                     ? 1
                                     : 0;
        
        // Advanced signal quality metrics
        metrics.signal_noise_ratio = calculateSNR();
        metrics.signal_quality = assessSignalQuality(
            metrics.signal_noise_ratio,
            breath_history.size(),
            metrics.breathing_regularity
        );

        // Phase–memory operator output (PAPER.md §4)
        //   instability_score  = ΔΦ(t) = |ω(t) − ω̄(t)|  (Eq. 5)
        //   instability_detected = 1 when ΔΦ(t) > α · σ_ω  (Eq. 6)
        metrics.instability_score    = phase_memory.instabilityScore();
        metrics.instability_detected = phase_memory.instabilityDetected() ? 1 : 0;
        
        return metrics;
    }
    
    // Get detailed breath history (for advanced analysis)
    const std::deque<BreathCycle>& getBreathHistory() const {
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
        try {
            return new RespiroEngine();
        } catch (const std::exception& e) {
            // In case of allocation failure, return null
            return nullptr;
        } catch (...) {
            return nullptr;
        }
    }
    
    void respiro_destroy(RespiroHandle handle) {
        if (!handle) {
            return;
        }
        try {
            delete static_cast<RespiroEngine*>(handle);
        } catch (...) {
            // Suppress any exceptions during destruction
        }
    }
    
    void respiro_start_session(RespiroHandle handle, uint64_t timestamp_ms) {
        if (!handle) {
            return;
        }
        try {
            static_cast<RespiroEngine*>(handle)->startSession(timestamp_ms);
        } catch (...) {
            // Suppress exceptions - best effort
        }
    }
    
    void respiro_feed_gyro(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms) {
        if (!handle) {
            return;
        }
        // Validate input - check for NaN and infinity
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            return; // Reject invalid sensor data
        }
        try {
            static_cast<RespiroEngine*>(handle)->feedGyroscope(x, y, z, timestamp_ms);
        } catch (...) {
            // Suppress exceptions - best effort
        }
    }
    
    void respiro_feed_accel(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms) {
        if (!handle) {
            return;
        }
        // Validate input - check for NaN and infinity
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            return; // Reject invalid sensor data
        }
        try {
            static_cast<RespiroEngine*>(handle)->feedAccelerometer(x, y, z, timestamp_ms);
        } catch (...) {
            // Suppress exceptions - best effort
        }
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
        try {
            *out_metrics = static_cast<RespiroEngine*>(handle)->getCurrentMetrics(timestamp_ms);
        } catch (...) {
            // On error, return safe defaults
            std::memset(out_metrics, 0, sizeof(*out_metrics));
            out_metrics->current_stage = UNKNOWN;
            out_metrics->signal_quality = SIGNAL_QUALITY_UNKNOWN;
        }
    }
    
    const char* respiro_get_version() {
        return "1.0.0"; // RESPIROSYNC_VERSION_STRING from header
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
