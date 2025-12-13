/*
 * RespiroSync Android Middleware (JNI)
 * Bridges Android SensorManager → RespiroSync Core → Kotlin/Java
 */

#include <jni.h>
#include <android/log.h>
#include <string>

#define LOG_TAG "RespiroSync"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Include the C API from core
extern "C" {
    typedef void* RespiroHandle;
    
    RespiroHandle respiro_create();
    void respiro_destroy(RespiroHandle handle);
    void respiro_start_session(RespiroHandle handle, uint64_t timestamp_ms);
    void respiro_feed_gyro(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms);
    void respiro_feed_accel(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms);
    
    typedef struct {
        int current_stage;
        float confidence;
        float breathing_rate_bpm;
        float breathing_regularity;
        float movement_intensity;
        int breath_cycles_detected;
        bool possible_apnea;
    } SleepMetrics;
    
    void respiro_get_metrics(RespiroHandle handle, uint64_t timestamp_ms, SleepMetrics* out_metrics);
}

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
    
    // Create Java SleepMetrics object
    jclass metricsClass = env->FindClass("com/respirosync/SleepMetrics");
    if (!metricsClass) {
        LOGE("Failed to find SleepMetrics class");
        return nullptr;
    }
    
    jmethodID constructor = env->GetMethodID(metricsClass, "<init>", "(IFFFFFIZ)V");
    if (!constructor) {
        LOGE("Failed to find SleepMetrics constructor");
        return nullptr;
    }
    
    return env->NewObject(metricsClass, constructor,
                         metrics.current_stage,
                         metrics.confidence,
                         metrics.breathing_rate_bpm,
                         metrics.breathing_regularity,
                         metrics.movement_intensity,
                         (jfloat)metrics.breath_cycles_detected,
                         metrics.breath_cycles_detected,
                         metrics.possible_apnea);
}

} // extern "C"
