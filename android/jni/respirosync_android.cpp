/*
 * RespiroSync Android Middleware (JNI)
 * Bridges Android SensorManager → RespiroSync Core → Kotlin/Java
 */

#include <jni.h>
#include <android/log.h>
#include <string>

// Use the canonical C API header so the SleepMetrics struct layout always
// matches the core engine exactly (avoids out-of-bounds writes when fields
// are added to the struct).
#include "../../core/respirosync_core.h"

#define LOG_TAG "RespiroSync"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================================
// JNI BINDINGS
// ============================================================================

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_respirosync_RespiroSyncEngine_nativeCreate(JNIEnv* env, jobject thiz) {
    RespiroHandle handle = respiro_create();
    LOGD("Native engine created: %p", handle);
    return reinterpret_cast<jlong>(handle);
}

JNIEXPORT void JNICALL
Java_com_respirosync_RespiroSyncEngine_nativeDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    respiro_destroy(reinterpret_cast<RespiroHandle>(handle));
    LOGD("Native engine destroyed");
}

JNIEXPORT void JNICALL
Java_com_respirosync_RespiroSyncEngine_nativeStartSession(JNIEnv* env, jobject thiz, 
                                                          jlong handle, jlong timestamp_ms) {
    respiro_start_session(reinterpret_cast<RespiroHandle>(handle), 
                         static_cast<uint64_t>(timestamp_ms));
    LOGD("Session started");
}

JNIEXPORT void JNICALL
Java_com_respirosync_RespiroSyncEngine_nativeFeedGyroscope(JNIEnv* env, jobject thiz,
                                                           jlong handle,
                                                           jfloat x, jfloat y, jfloat z,
                                                           jlong timestamp_ms) {
    respiro_feed_gyro(reinterpret_cast<RespiroHandle>(handle),
                     x, y, z,
                     static_cast<uint64_t>(timestamp_ms));
}

JNIEXPORT void JNICALL
Java_com_respirosync_RespiroSyncEngine_nativeFeedAccelerometer(JNIEnv* env, jobject thiz,
                                                               jlong handle,
                                                               jfloat x, jfloat y, jfloat z,
                                                               jlong timestamp_ms) {
    respiro_feed_accel(reinterpret_cast<RespiroHandle>(handle),
                      x, y, z,
                      static_cast<uint64_t>(timestamp_ms));
}

JNIEXPORT jobject JNICALL
Java_com_respirosync_RespiroSyncEngine_nativeGetMetrics(JNIEnv* env, jobject thiz,
                                                        jlong handle, jlong timestamp_ms) {
    SleepMetrics metrics;
    respiro_get_metrics(reinterpret_cast<RespiroHandle>(handle),
                       static_cast<uint64_t>(timestamp_ms),
                       &metrics);
    
    // Create Java SleepMetrics object with proper exception handling
    jclass metricsClass = env->FindClass("com/respirosync/SleepMetrics");
    if (!metricsClass) {
        LOGE("Failed to find SleepMetrics class");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return nullptr;
    }
    
    // Constructor signature: 11 parameters (IFFFFFIIIFFI)V
    // Parameters: current_stage(I), confidence(F), breathing_rate_bpm(F),
    // breathing_regularity(F), movement_intensity(F), breath_cycles_detected(I),
    // possible_apnea(I), signal_quality(I), signal_noise_ratio(F),
    // instability_score(F), instability_detected(I), void return(V)
    jmethodID constructor = env->GetMethodID(metricsClass, "<init>", "(IFFFFFIIIFFI)V");
    if (!constructor) {
        LOGE("Failed to find SleepMetrics constructor");
        env->DeleteLocalRef(metricsClass);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return nullptr;
    }
    
    jobject result = env->NewObject(metricsClass, constructor,
                         metrics.current_stage,
                         metrics.confidence,
                         metrics.breathing_rate_bpm,
                         metrics.breathing_regularity,
                         metrics.movement_intensity,
                         metrics.breath_cycles_detected,
                         metrics.possible_apnea,
                         metrics.signal_quality,
                         metrics.signal_noise_ratio,
                         metrics.instability_score,
                         metrics.instability_detected);
    
    // Clean up local reference to prevent memory leaks
    env->DeleteLocalRef(metricsClass);
    
    return result;
}

} // extern "C"
