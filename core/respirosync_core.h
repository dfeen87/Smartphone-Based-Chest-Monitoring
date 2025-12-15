#ifndef RESPIROSYNC_CORE_H
#define RESPIROSYNC_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RespiroSync Core Engine
 * Stable public interface for respiratory signal processing
 * v1.0.0
 * ============================================================================
 */

/* -----------------------------
 * Versioning
 * ----------------------------- */
#define RESPIROSYNC_VERSION_MAJOR 1
#define RESPIROSYNC_VERSION_MINOR 0
#define RESPIROSYNC_VERSION_PATCH 0

/* -----------------------------
 * Forward Declarations
 * ----------------------------- */
typedef struct RespiroSyncEngine RespiroSyncEngine;

/* -----------------------------
 * Status Codes
 * ----------------------------- */
typedef enum {
    RS_OK = 0,
    RS_ERROR_INVALID_ARGUMENT,
    RS_ERROR_NOT_INITIALIZED,
    RS_ERROR_BUFFER_FULL,
    RS_ERROR_INTERNAL
} RS_Status;

/* -----------------------------
 * Signal Configuration
 * ----------------------------- */
typedef struct {
    double sample_rate_hz;     /* Expected input sampling rate */
    double calibration_offset; /* Optional sensor baseline offset */
} RS_Config;

/* -----------------------------
 * Input Sample
 * ----------------------------- */
typedef struct {
    double value;      /* Raw or preprocessed respiratory signal value */
    double timestamp;  /* Seconds since start or monotonic clock */
} RS_Sample;

/* -----------------------------
 * Output Metrics
 * ----------------------------- */
typedef struct {
    double respiratory_rate_bpm;   /* Estimated breaths per minute */
    double signal_quality;         /* [0.0 – 1.0] heuristic quality score */
    double stability_index;        /* Relative temporal stability */
} RS_Metrics;

/* -----------------------------
 * Engine Lifecycle
 * ----------------------------- */

/* Create a new engine instance */
RespiroSyncEngine* rs_create(const RS_Config* config);

/* Destroy an engine instance */
void rs_destroy(RespiroSyncEngine* engine);

/* Reset internal state without destroying the engine */
RS_Status rs_reset(RespiroSyncEngine* engine);

/* -----------------------------
 * Data Ingestion
 * ----------------------------- */

/* Feed a single respiratory sample into the engine */
RS_Status rs_feed_sample(
    RespiroSyncEngine* engine,
    const RS_Sample* sample
);

/* -----------------------------
 * Metrics Retrieval
 * ----------------------------- */

/* Retrieve the latest computed metrics */
RS_Status rs_get_metrics(
    RespiroSyncEngine* engine,
    RS_Metrics* out_metrics
);

/* -----------------------------
 * Diagnostics
 * ----------------------------- */

/* Retrieve a short human-readable status string */
const char* rs_last_error(RespiroSyncEngine* engine);

#ifdef __cplusplus
}
#endif

#endif /* RESPIROSYNC_CORE_H */
