#ifndef RESPIROSYNC_CORE_H
#define RESPIROSYNC_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> /* uint64_t */

/* ============================================================================
 * RespiroSync Core Engine - C API
 * Stable binding surface for iOS/Android (JNI/ObjC/Swift)
 * ============================================================================
 */

/* Opaque engine handle (implementation is C++ RespiroSync::RespiroEngine) */
typedef void* RespiroHandle;

/* -----------------------------
 * Sleep Stage
 * ----------------------------- */
typedef enum SleepStage {
    AWAKE = 0,
    LIGHT_SLEEP = 1,
    DEEP_SLEEP = 2,
    REM_SLEEP = 3,
    UNKNOWN = 4
} SleepStage;

/* -----------------------------
 * Sleep / Respiratory Metrics
 * ----------------------------- */
typedef struct SleepMetrics {
    SleepStage current_stage;
    float confidence;               /* 0.0 - 1.0 */
    float breathing_rate_bpm;       /* breaths/min */
    float breathing_regularity;     /* 0.0 - 1.0 (higher = more consistent) */
    float movement_intensity;       /* 0.0 - 1.0 (higher = more restless) */
    int   breath_cycles_detected;
    int   possible_apnea;           /* 0/1 (C-friendly bool) */
} SleepMetrics;

/* -----------------------------
 * Lifecycle
 * ----------------------------- */
RespiroHandle respiro_create(void);
void         respiro_destroy(RespiroHandle handle);

/* Start a new session (resets internal state) */
void respiro_start_session(RespiroHandle handle, uint64_t timestamp_ms);

/* -----------------------------
 * Sensor Ingestion
 * ----------------------------- */
void respiro_feed_gyro(
    RespiroHandle handle,
    float x, float y, float z,
    uint64_t timestamp_ms
);

void respiro_feed_accel(
    RespiroHandle handle,
    float x, float y, float z,
    uint64_t timestamp_ms
);

/* -----------------------------
 * Metrics Retrieval
 * ----------------------------- */
void respiro_get_metrics(
    RespiroHandle handle,
    uint64_t timestamp_ms,
    SleepMetrics* out_metrics
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RESPIROSYNC_CORE_H */
