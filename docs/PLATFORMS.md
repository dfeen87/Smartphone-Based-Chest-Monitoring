# PLATFORMS.md

## Overview

This document describes how the RespiroSync core engine integrates with supported platforms and outlines strict boundaries between platform code and core logic.

The guiding principle is **thin bindings, thick core**.

## Core Engine Boundary

The RespiroSync core:
* Is written in C++
* Owns all signal processing and state
* Exposes a stable C API
* Does not depend on platform frameworks

All platforms interact with the core exclusively via:

```
core/respirosync_core.h
```

## Android Integration

### Architecture

```
Android Sensors
      │
      ▼
Kotlin / Java Layer
      │ (JNI)
      ▼
C API (respirosync_core.h)
      │
      ▼
C++ Core Engine
```

### Responsibilities

**Android Layer:**
* Acquire sensor data
* Manage threading and lifecycle
* Forward samples to the core

**Core Engine:**
* All processing
* All state
* All metrics

### JNI Notes

* JNI functions should be thin pass-throughs
* No buffering or filtering in Java/Kotlin
* Sensor timestamps must be monotonic

## iOS Integration

### Architecture

```
iOS Sensors
     │
     ▼
Swift / SwiftUI
     │
Objective-C++
     │
C API (respirosync_core.h)
     │
C++ Core Engine
```

### Responsibilities

**Swift Layer:**
* UI and presentation
* Sensor access via Core Motion
* Lifecycle control

**Objective-C++ Bridge:**
* Language boundary only
* No logic or state

**Core Engine:**
* Identical to Android

## Threading Model

* The core engine is **not thread-safe**
* All calls should occur on a single, consistent thread
* Synchronization is the responsibility of the platform layer

This simplifies the core and ensures deterministic behavior.

## Memory Ownership

* `RespiroHandle` is owned by the caller
* Must be destroyed via `respiro_destroy`
* Output structures are caller-allocated

No internal memory is exposed across the API boundary.

## Platform Guarantees

As of v1.0.0:
* ABI is stable
* Behavior is deterministic given identical inputs
* Platform code must not depend on C++ internals

## What Platform Code Must Not Do

* Re-implement signal processing
* Interpret raw metrics clinically
* Assume internal buffer sizes or algorithms
* Access C++ objects directly

Violating these rules breaks portability.

## Extending to New Platforms

The same pattern applies to:
* Desktop
* Embedded systems
* Wearables
* Server-side analysis

Only a C compiler and a sensor feed are required.

## Final Note

RespiroSync intentionally keeps platform layers minimal. This ensures:
* Consistent behavior across devices
* Easier validation
* Long-term maintainability
