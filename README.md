# IoT Project Assignment - Task 2: NeoPixel Control (Humidity-Based)

## Overview
Task 2 implements a real-time humidity monitoring system that controls a NeoPixel RGB LED's color based on humidity thresholds detected by the DHT20 sensor. This task demonstrates event-driven architecture using FreeRTOS semaphores for efficient task synchronization, eliminating the need for constant polling.

---

## Task 2: NeoPixel RGB LED Control (Humidity-Based)

### Objective
Monitor humidity readings from the DHT20 sensor and display real-time color changes on a NeoPixel RGB LED based on predefined humidity thresholds:
- **Normal State (< 50%)**: Green color
- **Warning State (50-70%)**: Yellow color  
- **Critical State (≥ 70%)**: Red color

### Hardware Components
| Component | GPIO Pin | Description |
|-----------|----------|-------------|
| NeoPixel RGB LED | GPIO 45 | WS2812B addressable RGB LED strip |
| DHT20 Sensor | SDA: 11, SCL: 12 | Humidity & Temperature sensor via I2C |
| Power Supply | VCC/GND | 5V for NeoPixel operation |

### Hardware Configuration
```
ESP32-S3
├─ GPIO 45  → NeoPixel Data Pin
└─ GPIO 11/12 → DHT20 I2C (SDA/SCL) [shared with Task 1]

```

---

## Architecture

### Data Structure
All task data is encapsulated in `SharedContext` (defined in [include/global.h](include/global.h)):

```c
struct SharedContext {
    float temperature;              // Current temperature reading
    float humidity;                 // Current humidity reading
    SemaphoreHandle_t mutexContext;     // Mutex for thread-safe access
    SemaphoreHandle_t semNeoUpdate;     // Binary semaphore for NeoPixel task
    int neoState;                   // 1: Normal, 2: Warning, 3: Critical
};
```

### Task Responsibilities

#### **1. Temperature & Humidity Monitor Task** ([src/temp_humi_monitor.cpp](src/temp_humi_monitor.cpp))

This unified sensor reading task monitors both temperature (for Task 1) and humidity (for Task 2):

**Task 2 Specific Logic:**
- Reads DHT20 sensor every 5 seconds
- Evaluates humidity against predefined thresholds
- Determines new NeoPixel state based on humidity level
- Updates `ctx->neoState` only when threshold boundaries are crossed
- Signals the NeoPixel task via `semNeoUpdate` when state changes occur
- Protects all data access with `mutexContext`

**Humidity Threshold Logic:**
```c
int newNeoState = 1; // Default: Normal

if (humidity >= 70.0) {
    newNeoState = 3;  // Critical
} else if (humidity >= 50.0) {
    newNeoState = 2;  // Warning
}

// Only signal if state actually changed
if (newNeoState != ctx->neoState) {
    ctx->neoState = newNeoState;
    xSemaphoreGive(ctx->semNeoUpdate);  // Wake NeoPixel task
}
```

**Pseudo-code Flow:**
```
LOOP every 5 seconds:
  1. Read humidity from DHT20
  2. Lock mutexContext
  3. Update ctx->humidity
  4. Calculate newNeoState based on humidity thresholds
  5. IF newNeoState != currentNeoState:
       └─ Update ctx->neoState
       └─ Give semNeoUpdate (wake NeoPixel task)
  6. Unlock mutexContext
  7. Sleep 5 seconds
```

#### **2. NeoPixel Control Task** ([src/neo_blinky.cpp](src/neo_blinky.cpp))

Handles real-time color updates based on humidity state changes:

**Key Features:**
- Event-driven: Waits for `semNeoUpdate` notifications from monitor task
- Responds immediately to humidity threshold changes
- Uses Adafruit_NeoPixel library to set RGB colors
- Thread-safe access to shared state via mutex
- Low CPU utilization: Blocks indefinitely until signaled

**Initialization Phase:**
```c
// 1. Initialize NeoPixel library
Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
strip.begin();
strip.clear();
strip.show();

// 2. Get initial state from context
xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
int state = ctx->neoState;
xSemaphoreGive(ctx->mutexContext);

// 3. Display initial color
if (state == 1) strip.setPixelColor(0, strip.Color(0, 255, 0));   // Green
else if (state == 2) strip.setPixelColor(0, strip.Color(255, 255, 0)); // Yellow
else if (state == 3) strip.setPixelColor(0, strip.Color(255, 0, 0));   // Red
strip.show();
```

**Main Loop:**
```c
while(1) {
    // 1. Block and wait for state change signal
    xSemaphoreTake(ctx->semNeoUpdate, portMAX_DELAY);
    
    // 2. Safely read current state
    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    int state = ctx->neoState;
    xSemaphoreGive(ctx->mutexContext);
    
    // 3. Update NeoPixel color based on state
    if (state == 1) {
        strip.setPixelColor(0, strip.Color(0, 255, 0));   // Green: Normal
    } else if (state == 2) {
        strip.setPixelColor(0, strip.Color(255, 255, 0)); // Yellow: Warning
    } else if (state == 3) {
        strip.setPixelColor(0, strip.Color(255, 0, 0));   // Red: Critical
    }
    
    // 4. Apply changes to LED
    strip.show();
}
```

---

## Synchronization Mechanism

### FreeRTOS Primitives Used

#### **1. Binary Semaphore (`semNeoUpdate`)**
Purpose: Signal NeoPixel task when humidity state changes

**Characteristics:**
- Given (released) by monitor task when `neoState` changes
- Taken (acquired) by NeoPixel task after waiting
- Uses `portMAX_DELAY` timeout = blocks indefinitely until signaled
- Prevents wasteful polling and reduces CPU load

**Flow:**
```
Time    Monitor Task                    NeoPixel Task
────────────────────────────────────────────────────────
t0      Read humidity (48%)
        Calculate state (Normal → 1)
        State changed!
        Give semNeoUpdate  ──────────→  Wake up from block
                                        xSemaphoreTake returns
t1                                      Lock & read state
                                        Set color = Green
                                        Display
t2                                      xSemaphoreTake(timeout=MAX)
                                        Block waiting...
```

#### **2. Mutex (`mutexContext`)**
Purpose: Protect access to shared `SharedContext` data

**Protected Operations:**
- Read/write `ctx->humidity`
- Read/write `ctx->neoState`
- Any modification to shared data

**Usage Pattern:**
```c
xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
// Critical section: Read or modify ctx->humidity, ctx->neoState
xSemaphoreGive(ctx->mutexContext);
```

---

## Implementation Details

### NeoPixel Library: Adafruit_NeoPixel

The implementation uses the **Adafruit_NeoPixel** library for controlling WS2812B RGB LEDs:

```cpp
// Initialization
Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
strip.begin();  // Initialize library and GPIO

// Setting color
strip.setPixelColor(pixelIndex, color);

// Displaying changes
strip.show();   // Transmit color data to LED

// Color definition
uint32_t color = strip.Color(red, green, blue);  // RGB values 0-255
```

### Color Mapping & RGB Values

| State | Condition | Color | RGB Value | Meaning |
|-------|-----------|-------|-----------|---------|
| 1 | humidity < 50% | Green | (0, 255, 0) | Normal/Healthy |
| 2 | 50% ≤ humidity < 70% | Yellow | (255, 255, 0) | Warning/Caution |
| 3 | humidity ≥ 70% | Red | (255, 0, 0) | Critical/Alert |

**Color Selection Rationale:**
- **Green** → Universal symbol for "all clear" or normal conditions
- **Yellow** → Warning indicator, needs attention
- **Red** → Critical alert, immediate action required
- Standard in monitoring dashboards and industrial systems

### Humidity Thresholds

```
         0% ─────────────── 50% ─────────────── 70% ─────── 100%
         │                  │                  │
      [Normal: Green]   [Warning: Yellow]  [Critical: Red]
         │                  │                  │
         └──────────────────┴──────────────────┘
                   Humidity Scale
```

**Threshold Logic:**
- **Normal**: humidity < 50% (dry conditions)
- **Warning**: 50% ≤ humidity < 70% (moderate humidity)
- **Critical**: humidity ≥ 70% (high humidity, potential condensation)

---



## State Transition Diagram

```
┌──────────────────────────────────────────────────────────┐
│                  Humidity Monitor                         │
│              (reads DHT20 every 5s)                       │
└──────────────┬───────────────────────────────────────────┘
               │
               ↓
        ┌─────────────────────────────────────┐
        │     Evaluate Humidity Thresholds    │
        └────────┬──────────────────┬─────────┘
                 │                  │
        ┌────────▼─────┐    ┌──────▼──────┐
        │State Changed? │    │No Change    │
        └────────┬─────┘    │ Skip Signal  │
                 │          └─────────────┘
                 │ YES
    ┌────────────▼────────────────┐
    │ Give semNeoUpdate           │
    │ (Wake NeoPixel Task)        │
    └────────┬─────────────────────┘
             │
             ↓
    ┌──────────────────────────────┐
    │  NeoPixel Control Task       │
    │ (woken by semaphore signal)  │
    └────────┬──────────────────────┘
             │
    ┌────────▼─────────────────────┐
    │  Read neoState              │
    │  (with mutex protection)    │
    └────────┬──────────────────────┘
             │
    ┌────────▼─────────────────────┐
    │  Set Color Based on State:  │
    │  1 → Green                  │
    │  2 → Yellow                 │
    │  3 → Red                    │
    └────────┬──────────────────────┘
             │
    ┌────────▼─────────────────────┐
    │  Display Color on NeoPixel  │
    │  strip.show()               │
    └────────┬──────────────────────┘
             │
    ┌────────▼──────────────────────┐
    │ Return to Wait State          │
    │ (portMAX_DELAY)               │
    └───────────────────────────────┘
```

---


## File Structure

```
src/
├── temp_humi_monitor.cpp    # Sensor reading + Task 1 & Task 2 logic
├── led_blinky.cpp           # Task 1: LED control (temperature)
├── neo_blinky.cpp           # Task 2: NeoPixel control (humidity)
└── main.cpp                 # FreeRTOS initialization & task creation

include/
├── global.h                 # SharedContext & semaphore declarations
├── temp_humi_monitor.h      # Monitor task declaration
├── led_blinky.h             # LED task declaration
└── neo_blinky.h             # NeoPixel task declaration (GPIO 45, LED_COUNT=1)

lib/
├── DHT20/                   # DHT20 temperature/humidity sensor driver
├── Adafruit_NeoPixel/       # Adafruit NeoPixel control library
└── ... (other libraries)
```

---


## References

- [FreeRTOS Documentation](https://www.freertos.org/)
- [FreeRTOS Binary Semaphores](https://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html)
- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [PlatformIO Documentation](https://docs.platformio.org/)


