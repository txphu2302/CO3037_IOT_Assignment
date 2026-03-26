# IoT Project Assignment - Task 1: Temperature-Based LED Blinking Control

## Overview
Task 1 implements a real-time temperature monitoring system that controls a single LED's blinking frequency based on temperature thresholds detected by the DHT20 sensor. This task demonstrates the removal of global variables and utilizes FreeRTOS semaphores for task synchronization, following the assignment requirements for Tasks 1, 2, and 3.

---

## Task 1: Single LED Control (Temperature-Based)

### Objective
Monitor temperature readings from the DHT20 sensor and dynamically adjust the LED blinking speed based on predefined temperature thresholds:
- **Normal State (< 25°C)**: Slow blink (1000ms delay)
- **Warning State (25-30°C)**: Medium blink (500ms delay)
- **Critical State (≥ 30°C)**: Fast blink (100ms delay)

### Hardware Components
| Component | GPIO Pin | Description |
|-----------|----------|-------------|
| DHT20 Sensor | SDA: 11, SCL: 12 | Temperature & Humidity sensor via I2C |
| Single LED | GPIO 48 | Controlled LED for state indication |

### Architecture

#### 1. **Data Encapsulation with SharedContext**
Instead of using global variables, all task data is encapsulated in a `SharedContext` structure defined in [include/global.h](include/global.h):

```c
struct SharedContext {
    float temperature;           // Current temperature reading
    float humidity;              // Current humidity reading
    SemaphoreHandle_t mutexContext;   // Mutex for thread-safe access
    SemaphoreHandle_t semLEDUpdate;   // Binary semaphore for LED task
    int ledState;                // 1: Normal, 2: Warning, 3: Critical
};
```

#### 2. **Task Responsibilities**

##### **Temperature & Humidity Monitor Task** ([src/temp_humi_monitor.cpp](src/temp_humi_monitor.cpp))
- Reads DHT20 sensor every 5 seconds
- Evaluates temperature against thresholds
- Updates `SharedContext` with:
  - Current temperature and humidity values
  - New LED state based on temperature range
- Signals the LED task via `semLEDUpdate` when state changes occur
- Protects all data access with `mutexContext`

**Pseudo-code:**
```
LOOP every 5 seconds:
  1. Read temperature from DHT20
  2. LOCK mutexContext
  3. Update ctx->temperature
  4. Calculate newLedState based on temperature thresholds
  5. IF newLedState != currentLedState:
       - Update ctx->ledState
       - SIGNAL semLEDUpdate (wake LED task)
  6. UNLOCK mutexContext
  7. Print readings to Serial
```

##### **LED Blink Control Task** ([src/led_blinky.cpp](src/led_blinky.cpp))
- Waits for state change notifications via `semLEDUpdate`
- Adjusts blink delay based on current LED state:
  - State 1: 1000ms (Normal)
  - State 2: 500ms (Warning)
  - State 3: 100ms (Critical)
- Implements breakable delays using semaphore waits instead of `vTaskDelay`
- Continuously toggles LED based on delay duration

**Pseudo-code:**
```
LOOP continuously:
  1. LOCK and read ctx->ledState
  2. Set delay_ms based on ledState
  3. Turn LED ON
  4. WAIT on semLEDUpdate OR timeout(delay_ms)
  5. Turn LED OFF
  6. WAIT on semLEDUpdate OR timeout(delay_ms)
```

### Synchronization Mechanism

#### FreeRTOS Primitives Used
1. **Mutex (`mutexContext`)**
   - Protects read/write operations on `SharedContext`
   - Prevents race conditions between monitor and LED tasks
   - Used with `xSemaphoreTake()` and `xSemaphoreGive()`

2. **Binary Semaphore (`semLEDUpdate`)**
   - Signals LED task when temperature state changes
   - Allows immediate response to threshold changes
   - Reduces wasted CPU cycles with breakable delays

**Flow Diagram:**
```
Temperature Monitor Task          LED Blink Task
        |                               |
        |-- Read DHT20                  |
        |-- Lock mutex                  |
        |-- Check threshold             |-- Wait on semLEDUpdate
        |-- Update ledState             |   (with timeout)
        |-- Signal LED (Give semaphore) |
        |-- Unlock mutex                |-- Adjust blink speed
        |-- Sleep 5s                    |-- Blink LED
        |                               |-- Loop
```

### Implementation Details

#### Global Variables Removal
**Before (Global Variables):**
```c
extern float glob_temperature;    // ❌ Global
extern float glob_humidity;       // ❌ Global
```

**After (Context-Based):**
```c
SharedContext* ctx = (SharedContext*)pvParameters;  // ✅ Local via task parameter
ctx->temperature = temperature;
ctx->humidity = humidity;
```

#### Mutex-Protected Access
All access to shared data follows this pattern:
```c
xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
// Critical section: read or write ctx->temperature, ctx->humidity, ctx->ledState
xSemaphoreGive(ctx->mutexContext);
```

#### Breakable Delays
Instead of blocking with `vTaskDelay()`, the LED task uses semaphore timeouts:
```c
// LED can be interrupted if state changes
if (xSemaphoreTake(ctx->semLEDUpdate, pdMS_TO_TICKS(delay_ms))) {
    // Semaphore given early → state changed, restart loop
    digitalWrite(LED_GPIO, LOW);
    continue;
}
// Timeout occurred → continue normal blinking
```

---

## State Transition Diagram

```
        Temperature < 25°C
            (Normal)
              ↓ ↑
          1000ms delay
          
        25°C ≤ Temp < 30°C
           (Warning)
              ↓ ↑
          500ms delay
          
        Temperature ≥ 30°C
           (Critical)
              ↓ ↑
          100ms delay
```

---


## Testing & Verification

### Manual Verification Steps

1. **Normal State Testing (< 25°C)**
   - Ensure temperature reading is below 25°C
   - Observe LED blinking slowly (1000ms on/off)
   - Verify Serial output shows "Normal" state

2. **Warning State Testing (25-30°C)**
   - Gradually increase temperature (apply warm object near sensor)
   - When temperature enters 25-30°C range, observe LED blink speed increase to 500ms
   - Verify state change is immediate without delay

3. **Critical State Testing (≥ 30°C)**
   - Continue warming the sensor to reach ≥ 30°C
   - LED should blink very rapidly at 100ms intervals
   - Verify Serial output shows temperature readings

4. **Code Verification**
   - Compile successfully: `pio run` (should have 0 errors)
   - Inspect object code to confirm no global variable sections
   - Verify all `SharedContext` access is protected by mutex

### Expected Behavior
| Temperature Range | LED Behavior | Blink Delay |
|------------------|--------------|------------|
| < 25°C | Slow, steady blinks | 1000ms |
| 25-30°C | Medium speed blinks | 500ms |
| ≥ 30°C | Rapid, fast blinks | 100ms |

---

## File Structure

```
src/
├── temp_humi_monitor.cpp    # Temperature/Humidity sensor reading & monitoring
├── led_blinky.cpp           # LED control task with dynamic blinking
└── main.cpp                 # SharedContext initialization & FreeRTOS setup

include/
├── global.h                 # SharedContext definition & semaphore declarations
├── temp_humi_monitor.h      # Monitor task function declaration
└── led_blinky.h             # LED task function declaration

lib/
├── DHT20/                   # Temperature/Humidity sensor driver
└── ... (other libraries)
```

---

## Notes & Design Decisions

1. **Polling Interval**: Temperature monitor polls every 5 seconds to balance responsiveness with power consumption.

2. **Semaphore Signaling**: The monitor task signals the LED task only on state changes, not continuously, reducing unnecessary task wakeups.

3. **Mutex Protection**: All `SharedContext` access is protected to prevent race conditions, even though this is a single-core device (ensures portability).

4. **Breakable Delays**: Using semaphore timeouts instead of `vTaskDelay` allows the LED task to respond immediately if the semaphore is given early (state change notification).

5. **Error Handling**: DHT20 read failures are detected (NaN checks) and handled gracefully without crashing the temperature monitoring loop.

---

## References

- [FreeRTOS Documentation](https://www.freertos.org/)
- [ESP32 GPIO Configuration](https://github.com/espressif/esp-idf)
- [DHT20 Sensor Library](lib/DHT20/)
- [PlatformIO Documentation](https://docs.platformio.org/)
