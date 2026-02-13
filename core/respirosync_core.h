#ifndef RESPIROSYNC_CORE_H
#define RESPIROSYNC_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================================
 * RespiroSync™ Core Engine
 * Stable C API for cross-platform respiratory monitoring
 *
 * Version: 1.0.0
 * Status:  Stable public interface
 *
 * This header defines the binding contract for the RespiroSync core engine.
 * The implementation is provided by a C++ backend and exposed via an opaque
 * handle for ABI stability across platforms and languages.
 * ============================================================================
 */

/* -----------------------------
 * Versioning
 * ----------------------------- */
#define RESPIROSYNC_VERSION_MAJOR 1
#define RESPIROSYNC_VERSION_MINOR 0
#define RESPIROSYNC_VERSION_PATCH 0

#define RESPIROSYNC_VERSION_STRING "1.0.0"

/* -----------------------------
 * Opaque Engine Handle
 * ----------------------------- */
/* The concrete type is RespiroSync::RespiroEngine (C++),
 * intentionally hidden from consumers.
 */
typedef void* RespiroHandle;

/* -----------------------------
 * Sleep Stage Classification
 * ----------------------------- */
typedef enum {
    AWAKE = 0,
    LIGHT_SLEEP = 1,
    DEEP_SLEEP = 2,
    REM_SLEEP = 3,
    UNKNOWN = 4
} SleepStage;

/* -----------------------------
 * Error Codes
 * ----------------------------- */
typedef enum {
    RESPIRO_OK = 0,
    RESPIRO_ERROR_INVALID_HANDLE = -1,
    RESPIRO_ERROR_INVALID_PARAMETER = -2,
    RESPIRO_ERROR_OUT_OF_MEMORY = -3,
    RESPIRO_ERROR_INTERNAL = -4
} RespiroStatus;

/* -----------------------------
 * Signal Quality Assessment
 * ----------------------------- */
typedef enum {
    SIGNAL_QUALITY_EXCELLENT = 0,  /* High confidence, stable readings */
    SIGNAL_QUALITY_GOOD = 1,       /* Acceptable quality */
    SIGNAL_QUALITY_FAIR = 2,       /* Marginal quality, use with caution */
    SIGNAL_QUALITY_POOR = 3,       /* Insufficient data or too noisy */
    SIGNAL_QUALITY_UNKNOWN = 4     /* Not enough data to assess */
} SignalQuality;

/* -----------------------------
 * Sleep / Respiratory Metrics
 * ----------------------------- */
typedef struct {
    SleepStage current_stage;

    float confidence;               /* 0.0 – 1.0 heuristic confidence */
    float breathing_rate_bpm;       /* breaths per minute */
    float breathing_regularity;     /* 0.0 – 1.0 (higher = more consistent) */
    float movement_intensity;       /* 0.0 – 1.0 (higher = more movement) */

    int   breath_cycles_detected;
    int   possible_apnea;           /* boolean (0 = false, 1 = true) */
    
    /* Advanced metrics (v1.1+) */
    SignalQuality signal_quality;   /* Overall signal quality assessment */
    float signal_noise_ratio;       /* Estimated SNR (0.0 - 10.0+) */
} SleepMetrics;

/* -----------------------------
 * Lifecycle Management
 * ----------------------------- */

/* Create a new RespiroSync engine instance 
 * Returns: Handle to engine instance, or NULL on allocation failure */
RespiroHandle respiro_create(void);

/* Destroy an engine instance and release all resources 
 * Parameters:
 *   handle - Engine handle to destroy (NULL-safe) */
void respiro_destroy(RespiroHandle handle);

/* Reset internal state and begin a new monitoring session 
 * Parameters:
 *   handle - Engine handle
 *   timestamp_ms - Session start time in milliseconds since epoch */
void respiro_start_session(
    RespiroHandle handle,
    uint64_t timestamp_ms
);

/* -----------------------------
 * Sensor Data Ingestion
 * ----------------------------- */

/* Feed a gyroscope sample (rad/s or device-native units)
 * Parameters:
 *   handle - Engine handle
 *   x, y, z - Gyroscope readings (NaN/infinity rejected)
 *   timestamp_ms - Sample timestamp in milliseconds
 * Note: Invalid sensor data (NaN/infinity) is silently rejected */
void respiro_feed_gyro(
    RespiroHandle handle,
    float x, float y, float z,
    uint64_t timestamp_ms
);

/* Feed an accelerometer sample (m/s^2 or device-native units) 
 * Parameters:
 *   handle - Engine handle
 *   x, y, z - Accelerometer readings (NaN/infinity rejected)
 *   timestamp_ms - Sample timestamp in milliseconds
 * Note: Invalid sensor data (NaN/infinity) is silently rejected */
void respiro_feed_accel(
    RespiroHandle handle,
    float x, float y, float z,
    uint64_t timestamp_ms
);

/* -----------------------------
 * Metrics Retrieval
 * ----------------------------- */

/* Retrieve the latest computed sleep and respiratory metrics 
 * Parameters:
 *   handle - Engine handle
 *   timestamp_ms - Current time in milliseconds
 *   out_metrics - Pointer to receive metrics (must not be NULL)
 * Returns: Void, but out_metrics populated with current state
 * Note: On error, out_metrics is zero-filled with stage=UNKNOWN */
void respiro_get_metrics(
    RespiroHandle handle,
    uint64_t timestamp_ms,
    SleepMetrics* out_metrics
);

/* Get version string
 * Returns: Static string with version information */
const char* respiro_get_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RESPIROSYNC_CORE_H */
