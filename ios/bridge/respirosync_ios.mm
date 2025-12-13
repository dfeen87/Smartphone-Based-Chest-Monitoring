/*
 * RespiroSync iOS Middleware
 * Bridges CoreMotion → RespiroSync Core → Swift/Objective-C
 */

#import <Foundation/Foundation.h>
#import <CoreMotion/CoreMotion.h>

// Include the C API from core
extern "C" {
    typedef void* RespiroHandle;
    
    RespiroHandle respiro_create();
    void respiro_destroy(RespiroHandle handle);
    void respiro_start_session(RespiroHandle handle, uint64_t timestamp_ms);
    void respiro_feed_gyro(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms);
    void respiro_feed_accel(RespiroHandle handle, float x, float y, float z, uint64_t timestamp_ms);
    
    typedef struct {
        int current_stage;          // 0=AWAKE, 1=LIGHT, 2=DEEP, 3=REM, 4=UNKNOWN
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
// Objective-C Wrapper
// ============================================================================

@interface RespiroSyncMetrics : NSObject
@property (nonatomic, assign) NSInteger sleepStage;        // 0-4 (AWAKE, LIGHT, DEEP, REM, UNKNOWN)
@property (nonatomic, assign) CGFloat confidence;          // 0.0-1.0
@property (nonatomic, assign) CGFloat breathingRateBPM;
@property (nonatomic, assign) CGFloat breathingRegularity; // 0.0-1.0
@property (nonatomic, assign) CGFloat movementIntensity;   // 0.0-1.0
@property (nonatomic, assign) NSInteger breathCyclesDetected;
@property (nonatomic, assign) BOOL possibleApnea;
@end

@implementation RespiroSyncMetrics
@end

// ============================================================================

@interface RespiroSync : NSObject

- (instancetype)init;
- (void)startSession;
- (void)stopSession;
- (RespiroSyncMetrics *)getCurrentMetrics;

@end

@implementation RespiroSync {
    CMMotionManager *motionManager;
    RespiroHandle coreEngine;
    NSOperationQueue *sensorQueue;
    BOOL isRunning;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        motionManager = [[CMMotionManager alloc] init];
        coreEngine = respiro_create();
        sensorQueue = [[NSOperationQueue alloc] init];
        sensorQueue.maxConcurrentOperationCount = 1;
        isRunning = NO;
        
        // Configure motion manager for optimal battery/performance
        motionManager.gyroUpdateInterval = 0.02;  // 50 Hz
        motionManager.accelerometerUpdateInterval = 0.02;
        motionManager.deviceMotionUpdateInterval = 0.02;
    }
    return self;
}

- (void)dealloc {
    [self stopSession];
    respiro_destroy(coreEngine);
}

- (void)startSession {
    if (isRunning) return;
    
    uint64_t timestamp = (uint64_t)([[NSDate date] timeIntervalSince1970] * 1000);
    respiro_start_session(coreEngine, timestamp);
    
    // Start gyroscope updates
    if (motionManager.isGyroAvailable) {
        [motionManager startGyroUpdatesToQueue:sensorQueue 
                                    withHandler:^(CMGyroData *data, NSError *error) {
            if (error) {
                NSLog(@"Gyro error: %@", error);
                return;
            }
            
            uint64_t ts = (uint64_t)([[NSDate date] timeIntervalSince1970] * 1000);
            respiro_feed_gyro(self->coreEngine,
                            (float)data.rotationRate.x,
                            (float)data.rotationRate.y,
                            (float)data.rotationRate.z,
                            ts);
        }];
    }
    
    // Start accelerometer updates
    if (motionManager.isAccelerometerAvailable) {
        [motionManager startAccelerometerUpdatesToQueue:sensorQueue 
                                            withHandler:^(CMAccelerometerData *data, NSError *error) {
            if (error) {
                NSLog(@"Accel error: %@", error);
                return;
            }
            
            uint64_t ts = (uint64_t)([[NSDate date] timeIntervalSince1970] * 1000);
            respiro_feed_accel(self->coreEngine,
                             (float)data.acceleration.x,
                             (float)data.acceleration.y,
                             (float)data.acceleration.z,
                             ts);
        }];
    }
    
    isRunning = YES;
    NSLog(@"RespiroSync session started");
}

- (void)stopSession {
    if (!isRunning) return;
    
    [motionManager stopGyroUpdates];
    [motionManager stopAccelerometerUpdates];
    isRunning = NO;
    
    NSLog(@"RespiroSync session stopped");
}

- (RespiroSyncMetrics *)getCurrentMetrics {
    uint64_t timestamp = (uint64_t)([[NSDate date] timeIntervalSince1970] * 1000);
    
    SleepMetrics cMetrics;
    respiro_get_metrics(coreEngine, timestamp, &cMetrics);
    
    // Convert C struct to Objective-C object
    RespiroSyncMetrics *metrics = [[RespiroSyncMetrics alloc] init];
    metrics.sleepStage = cMetrics.current_stage;
    metrics.confidence = cMetrics.confidence;
    metrics.breathingRateBPM = cMetrics.breathing_rate_bpm;
    metrics.breathingRegularity = cMetrics.breathing_regularity;
    metrics.movementIntensity = cMetrics.movement_intensity;
    metrics.breathCyclesDetected = cMetrics.breath_cycles_detected;
    metrics.possibleApnea = cMetrics.possible_apnea;
    
    return metrics;
}

@end

/*
 * SWIFT USAGE:
 * 
 * import Foundation
 * 
 * let respiro = RespiroSync()
 * respiro.startSession()
 * 
 * // Poll metrics (e.g., every second)
 * Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
 *     let metrics = respiro.getCurrentMetrics()
 *     print("BPM: \(metrics.breathingRateBPM)")
 *     print("Stage: \(metrics.sleepStage)")
 *     print("Confidence: \(metrics.confidence)")
 *     
 *     if metrics.possibleApnea {
 *         print("⚠️ Possible apnea detected")
 *     }
 * }
 * 
 * // Later:
 * respiro.stopSession()
 */
