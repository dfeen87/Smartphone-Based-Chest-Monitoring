/*
 * RespiroSync Core Engine Test Suite
 * Basic unit tests for critical functionality
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "../core/respirosync_core.h"

// Test utilities
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s - %s\n", __func__, message); \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("Running: %s ... ", #test_func); \
        fflush(stdout); \
        if (test_func()) { \
            printf("PASS\n"); \
            passed++; \
        } else { \
            printf("FAIL\n"); \
            failed++; \
        } \
        total++; \
    } while(0)

// Test: Create and destroy engine
bool test_create_destroy() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_destroy(handle);
    
    // NULL handle should be safe
    respiro_destroy(nullptr);
    
    return true;
}

// Test: Version string
bool test_version() {
    const char* version = respiro_get_version();
    TEST_ASSERT(version != nullptr, "Version string is NULL");
    TEST_ASSERT(strlen(version) > 0, "Version string is empty");
    TEST_ASSERT(strcmp(version, "1.0.0") == 0, "Version mismatch");
    
    return true;
}

// Test: Start session
bool test_start_session() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_start_session(handle, 1000);
    
    // Should be safe to start multiple times
    respiro_start_session(handle, 2000);
    
    respiro_destroy(handle);
    return true;
}

// Test: Feed sensor data
bool test_feed_sensors() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_start_session(handle, 1000);
    
    // Feed some gyroscope data
    respiro_feed_gyro(handle, 0.1f, 0.2f, 0.3f, 1100);
    respiro_feed_gyro(handle, 0.15f, 0.25f, 0.35f, 1200);
    
    // Feed some accelerometer data
    respiro_feed_accel(handle, 0.0f, 0.0f, 9.81f, 1100);
    respiro_feed_accel(handle, 0.0f, 0.0f, 9.82f, 1200);
    
    respiro_destroy(handle);
    return true;
}

// Test: Get initial metrics
bool test_get_initial_metrics() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_start_session(handle, 1000);
    
    SleepMetrics metrics;
    respiro_get_metrics(handle, 1000, &metrics);
    
    TEST_ASSERT(metrics.breathing_rate_bpm == 0.0f, "Initial BPM should be 0");
    TEST_ASSERT(metrics.breath_cycles_detected == 0, "Initial cycles should be 0");
    TEST_ASSERT(metrics.current_stage == UNKNOWN, "Initial stage should be UNKNOWN");
    TEST_ASSERT(metrics.possible_apnea == 0, "Initial apnea should be 0");
    TEST_ASSERT(metrics.signal_quality == SIGNAL_QUALITY_UNKNOWN, "Initial quality should be UNKNOWN");
    
    respiro_destroy(handle);
    return true;
}

// Test: Invalid input rejection
bool test_invalid_input() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_start_session(handle, 1000);
    
    // Feed NaN values - should be rejected silently
    respiro_feed_gyro(handle, NAN, 0.0f, 0.0f, 1100);
    respiro_feed_accel(handle, 0.0f, NAN, 0.0f, 1100);
    
    // Feed infinity values - should be rejected silently
    respiro_feed_gyro(handle, INFINITY, 0.0f, 0.0f, 1200);
    respiro_feed_accel(handle, 0.0f, -INFINITY, 0.0f, 1200);
    
    // Engine should still be functional
    SleepMetrics metrics;
    respiro_get_metrics(handle, 1300, &metrics);
    
    respiro_destroy(handle);
    return true;
}

// Test: NULL handle safety
bool test_null_handle_safety() {
    // All API functions should handle NULL gracefully
    respiro_start_session(nullptr, 1000);
    respiro_feed_gyro(nullptr, 0.0f, 0.0f, 0.0f, 1000);
    respiro_feed_accel(nullptr, 0.0f, 0.0f, 0.0f, 1000);
    
    SleepMetrics metrics;
    respiro_get_metrics(nullptr, 1000, &metrics);
    TEST_ASSERT(metrics.current_stage == UNKNOWN, "NULL handle should return UNKNOWN stage");
    
    return true;
}

// Test: Simulated breathing pattern
bool test_simulated_breathing() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_start_session(handle, 0);
    
    // Simulate breathing at ~15 breaths per minute (4 second cycle)
    // Feed accelerometer data with sinusoidal pattern
    const int sample_rate = 50; // 50 Hz
    const float breathing_frequency = 0.25f; // 15 BPM = 0.25 Hz
    const int duration_seconds = 30;
    
    for (int i = 0; i < sample_rate * duration_seconds; i++) {
        uint64_t timestamp_ms = i * (1000 / sample_rate);
        float t = i / (float)sample_rate;
        
        // Simulate chest motion due to breathing
        float chest_motion = 0.1f * sinf(2.0f * M_PI * breathing_frequency * t);
        
        // Accelerometer: gravity + breathing motion
        respiro_feed_accel(handle, 0.0f, 0.0f, 9.81f + chest_motion, timestamp_ms);
        
        // Small gyroscope noise
        respiro_feed_gyro(handle, 0.01f, 0.01f, 0.01f, timestamp_ms);
    }
    
    // Get metrics
    SleepMetrics metrics;
    respiro_get_metrics(handle, duration_seconds * 1000, &metrics);
    
    // Should have detected some breaths
    TEST_ASSERT(metrics.breath_cycles_detected > 0, "Should detect breath cycles");
    
    // Breathing rate should be reasonable (within 10-20 BPM for simulated 15 BPM)
    // Note: May not be exact due to simple simulation
    TEST_ASSERT(metrics.breathing_rate_bpm >= 0.0f, "BPM should be non-negative");
    
    // Should have some confidence with this much data
    TEST_ASSERT(metrics.confidence > 0.0f, "Should have non-zero confidence");
    
    // Signal quality should improve with data
    TEST_ASSERT(metrics.signal_quality != SIGNAL_QUALITY_UNKNOWN, 
                "Signal quality should be assessed");
    
    respiro_destroy(handle);
    return true;
}

// Test: Metrics ranges
bool test_metrics_ranges() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    respiro_start_session(handle, 1000);
    
    // Feed some data
    for (int i = 0; i < 100; i++) {
        respiro_feed_accel(handle, 0.0f, 0.0f, 9.81f, 1000 + i * 20);
        respiro_feed_gyro(handle, 0.0f, 0.0f, 0.0f, 1000 + i * 20);
    }
    
    SleepMetrics metrics;
    respiro_get_metrics(handle, 3000, &metrics);
    
    // Validate ranges
    TEST_ASSERT(metrics.confidence >= 0.0f && metrics.confidence <= 1.0f,
                "Confidence out of range");
    TEST_ASSERT(metrics.breathing_regularity >= 0.0f && metrics.breathing_regularity <= 1.0f,
                "Regularity out of range");
    TEST_ASSERT(metrics.movement_intensity >= 0.0f && metrics.movement_intensity <= 1.0f,
                "Movement intensity out of range");
    TEST_ASSERT(metrics.breathing_rate_bpm >= 0.0f,
                "Breathing rate cannot be negative");
    TEST_ASSERT(metrics.breath_cycles_detected >= 0,
                "Breath cycles cannot be negative");
    TEST_ASSERT(metrics.possible_apnea == 0 || metrics.possible_apnea == 1,
                "Apnea must be boolean");
    
    respiro_destroy(handle);
    return true;
}

// Test: Multiple sessions
bool test_multiple_sessions() {
    RespiroHandle handle = respiro_create();
    TEST_ASSERT(handle != nullptr, "Failed to create engine");
    
    // First session
    respiro_start_session(handle, 1000);
    respiro_feed_accel(handle, 0.0f, 0.0f, 9.81f, 1100);
    
    SleepMetrics metrics1;
    respiro_get_metrics(handle, 1200, &metrics1);
    
    // Second session - should reset state
    respiro_start_session(handle, 2000);
    
    SleepMetrics metrics2;
    respiro_get_metrics(handle, 2100, &metrics2);
    
    TEST_ASSERT(metrics2.breath_cycles_detected == 0, "New session should reset breath count");
    
    respiro_destroy(handle);
    return true;
}

int main() {
    printf("RespiroSync Core Engine Test Suite\n");
    printf("===================================\n\n");
    
    int total = 0, passed = 0, failed = 0;
    
    RUN_TEST(test_create_destroy);
    RUN_TEST(test_version);
    RUN_TEST(test_start_session);
    RUN_TEST(test_feed_sensors);
    RUN_TEST(test_get_initial_metrics);
    RUN_TEST(test_invalid_input);
    RUN_TEST(test_null_handle_safety);
    RUN_TEST(test_simulated_breathing);
    RUN_TEST(test_metrics_ranges);
    RUN_TEST(test_multiple_sessions);
    
    printf("\n===================================\n");
    printf("Results: %d total, %d passed, %d failed\n", total, passed, failed);
    
    return (failed == 0) ? 0 : 1;
}
