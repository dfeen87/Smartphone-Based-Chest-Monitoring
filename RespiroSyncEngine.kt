/*
 * RespiroSync Android Kotlin API
 * High-level interface for Android apps
 * 
 * Package: com.respirosync
 */

package com.respirosync

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.util.Log

// ============================================================================
// DATA CLASSES
// ============================================================================

enum class SleepStage(val value: Int) {
    AWAKE(0),
    LIGHT_SLEEP(1),
    DEEP_SLEEP(2),
    REM_SLEEP(3),
    UNKNOWN(4);
    
    companion object {
        fun fromInt(value: Int) = values().firstOrNull { it.value == value } ?: UNKNOWN
    }
}

data class SleepMetrics(
    val sleepStage: SleepStage,
    val confidence: Float,           // 0.0-1.0
    val breathingRateBPM: Float,
    val breathingRegularity: Float,  // 0.0-1.0
    val movementIntensity: Float,    // 0.0-1.0
    val breathCyclesDetected: Int,
    val possibleApnea: Boolean
) {
    // Constructor called from JNI
    constructor(
        stage: Int,
        confidence: Float,
        bpm: Float,
        regularity: Float,
        movement: Float,
        unused: Float, // JNI artifact, ignore
        cycles: Int,
        apnea: Boolean
    ) : this(
        SleepStage.fromInt(stage),
        confidence,
        bpm,
        regularity,
        movement,
        cycles,
        apnea
    )
}

// ============================================================================
// MAIN API
// ============================================================================

class RespiroSyncEngine(private val context: Context) : SensorEventListener {
    
    companion object {
        private const val TAG = "RespiroSync"
        
        init {
            System.loadLibrary("respirosync")
        }
    }
    
    // Native methods (implemented in JNI)
    private external fun nativeCreate(): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeStartSession(handle: Long, timestampMs: Long)
    private external fun nativeFeedGyroscope(handle: Long, x: Float, y: Float, z: Float, timestampMs: Long)
    private external fun nativeFeedAccelerometer(handle: Long, x: Float, y: Float, z: Float, timestampMs: Long)
    private external fun nativeGetMetrics(handle: Long, timestampMs: Long): SleepMetrics?
    
    // State
    private var nativeHandle: Long = 0
    private var sensorManager: SensorManager? = null
    private var gyroscope: Sensor? = null
    private var accelerometer: Sensor? = null
    private var isRunning = false
    
    init {
        nativeHandle = nativeCreate()
        sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
        gyroscope = sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
        accelerometer = sensorManager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        
        if (gyroscope == null) {
            Log.w(TAG, "Gyroscope not available on this device")
        }
        if (accelerometer == null) {
            Log.e(TAG, "Accelerometer not available on this device")
        }
    }
    
    fun startSession() {
        if (isRunning) {
            Log.w(TAG, "Session already running")
            return
        }
        
        val timestampMs = System.currentTimeMillis()
        nativeStartSession(nativeHandle, timestampMs)
        
        // Register sensor listeners at 50Hz (20ms interval)
        val samplingPeriodUs = 20_000 // 20ms = 50Hz
        
        gyroscope?.let {
            sensorManager?.registerListener(this, it, samplingPeriodUs)
            Log.d(TAG, "Gyroscope registered")
        }
        
        accelerometer?.let {
            sensorManager?.registerListener(this, it, samplingPeriodUs)
            Log.d(TAG, "Accelerometer registered")
        }
        
        isRunning = true
        Log.i(TAG, "RespiroSync session started")
    }
    
    fun stopSession() {
        if (!isRunning) return
        
        sensorManager?.unregisterListener(this)
        isRunning = false
        Log.i(TAG, "RespiroSync session stopped")
    }
    
    fun getCurrentMetrics(): SleepMetrics? {
        val timestampMs = System.currentTimeMillis()
        return nativeGetMetrics(nativeHandle, timestampMs)
    }
    
    fun release() {
        stopSession()
        nativeDestroy(nativeHandle)
        nativeHandle = 0
        Log.d(TAG, "Engine released")
    }
    
    // ========================================================================
    // SensorEventListener Implementation
    // ========================================================================
    
    override fun onSensorChanged(event: SensorEvent?) {
        event ?: return
        
        val timestampMs = System.currentTimeMillis()
        
        when (event.sensor.type) {
            Sensor.TYPE_GYROSCOPE -> {
                nativeFeedGyroscope(
                    nativeHandle,
                    event.values[0],
                    event.values[1],
                    event.values[2],
                    timestampMs
                )
            }
            
            Sensor.TYPE_ACCELEROMETER -> {
                nativeFeedAccelerometer(
                    nativeHandle,
                    event.values[0],
                    event.values[1],
                    event.values[2],
                    timestampMs
                )
            }
        }
    }
    
    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {
        // Not needed for this use case
    }
}

/*
 * KOTLIN USAGE EXAMPLE:
 * 
 * class MainActivity : AppCompatActivity() {
 *     private lateinit var respiro: RespiroSyncEngine
 *     private val handler = Handler(Looper.getMainLooper())
 *     
 *     override fun onCreate(savedInstanceState: Bundle?) {
 *         super.onCreate(savedInstanceState)
 *         
 *         respiro = RespiroSyncEngine(this)
 *         respiro.startSession()
 *         
 *         // Poll metrics every second
 *         handler.postDelayed(object : Runnable {
 *             override fun run() {
 *                 val metrics = respiro.getCurrentMetrics()
 *                 metrics?.let {
 *                     Log.d("Sleep", "BPM: ${it.breathingRateBPM}")
 *                     Log.d("Sleep", "Stage: ${it.sleepStage}")
 *                     Log.d("Sleep", "Confidence: ${it.confidence}")
 *                     
 *                     if (it.possibleApnea) {
 *                         Log.w("Sleep", "⚠️ Possible apnea detected")
 *                     }
 *                 }
 *                 
 *                 handler.postDelayed(this, 1000)
 *             }
 *         }, 1000)
 *     }
 *     
 *     override fun onDestroy() {
 *         super.onDestroy()
 *         respiro.release()
 *         handler.removeCallbacksAndMessages(null)
 *     }
 * }
 */
