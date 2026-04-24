# IoT Project Assignment - Task 3: Temperature and Humidity Monitoring with LCD Display

## Overview
Task 3 implements a comprehensive temperature and humidity monitoring system with real-time LCD display using FreeRTOS on an ESP32-S3 microcontroller. This task demonstrates advanced synchronization mechanisms using **binary semaphores and mutex locks** to eliminate global variables and enable efficient inter-task communication. The system evaluates sensor readings against predefined thresholds and displays monitoring status on a 16x2 I2C LCD screen.

**Key Achievement:** Complete elimination of global variables by encapsulating all shared state within the `SharedContext` structure and using FreeRTOS semaphores for event-driven architecture.

---

## Task 3: Temperature and Humidity Monitoring with LCD Display

### Objective
Monitor temperature and humidity readings from the DHT20 sensor and display:
- **Real-time sensor values** (temperature and humidity) on the first line of LCD
- **System status** (Normal/Warning/Critical) on the second line based on threshold evaluation
- **At least 3 display states** determined by sensor threshold conditions

### Hardware Components
| Component | GPIO Pin | Protocol | Description |
|-----------|----------|----------|-------------|
| DHT20 Sensor | SDA: 11, SCL: 12 | I2C | Temperature/Humidity sensor |
| LCD 16x2 Display | SDA: 11, SCL: 12 | I2C (Address: 0x21) | Real-time monitoring display |

### Hardware Configuration
```
ESP32-S3
├─ GPIO 11/12 → I2C Bus (SDA/SCL)
│  ├─ DHT20 Sensor
│  └─ LCD Display (0x21)
```

---

## Architecture

### Data Structure
All task data is encapsulated in `SharedContext` (defined in [include/global.h](include/global.h)):

```c
struct SharedContext {
    float temperature;              // Current temperature reading (°C)
    float humidity;                 // Current humidity reading (%)
    SemaphoreHandle_t mutexContext;     // Mutex for thread-safe access
    SemaphoreHandle_t semLCDUpdate;     // Binary semaphore for LCD task
    int lcdState;                   // 1: Normal, 2: Warning, 3: Critical
};
```

### Task Responsibilities

#### **1. Temperature & Humidity Monitor Task** ([src/temp_humi_monitor.cpp](src/temp_humi_monitor.cpp))

This unified sensor reading task monitors both temperature and humidity:

**Task 3 Specific Logic:**
- Reads DHT20 sensor every 5 seconds
- Evaluates temperature and humidity against predefined thresholds
- Determines overall LCD state based on worst-case condition (max of temperature state and humidity state)
- Updates `ctx->lcdState` only when threshold boundaries are crossed
- Signals the LCD task via `semLCDUpdate` when state changes occur
- Displays real-time sensor readings on LCD every 5 seconds
- Protects all data access with `mutexContext`

**Threshold Evaluation Logic:**
```c
// Temperature thresholds
int tempState = 1;  // Default: Normal
if (temperature >= 30.0) tempState = 3;        // Critical
else if (temperature >= 25.0) tempState = 2;   // Warning

// Humidity thresholds
int humidityState = 1;  // Default: Normal
if (humidity >= 70.0) humidityState = 3;       // Critical
else if (humidity >= 50.0) humidityState = 2;  // Warning

// LCD State = Worst case between temperature and humidity
int newLcdState = (tempState > humidityState) ? tempState : humidityState;

// Only signal if state actually changed
if (newLcdState != ctx->lcdState) {
    ctx->lcdState = newLcdState;
    xSemaphoreGive(ctx->semLCDUpdate);  // Wake LCD task to update status
}
```

**Pseudo-code Flow:**
```
LOOP every 5 seconds:
  1. Read temperature and humidity from DHT20
  2. Lock mutexContext
  3. Update ctx->temperature and ctx->humidity
  4. Calculate temperature threshold state
  5. Calculate humidity threshold state
  6. Determine LCD state = WORSE(temp_state, humidity_state)
  7. IF newLcdState != currentLcdState:
       └─ Update ctx->lcdState
       └─ Give semLCDUpdate (signal LCD task)
  8. Unlock mutexContext
  9. Update LCD display with current readings
  10. Sleep 5 seconds
```

#### **2. LCD Display Task** ([src/temp_humi_monitor.cpp](src/temp_humi_monitor.cpp))

Handles real-time display updates with sensor readings and status:

**Key Features:**
- Event-driven: Checks `semLCDUpdate` notifications for status changes
- Real-time display: Updates sensor readings every 5 seconds
- Thread-safe access to shared state via mutex
- Low CPU utilization: Non-blocking semaphore check

**Initialization Phase:**
```c
// 1. Initialize I2C bus and DHT20 sensor
Wire.begin(11, 12);
dht20.begin();

// 2. Initialize LCD display
LiquidCrystal_I2C lcd(0x21, 16, 2);
lcd.begin();
lcd.backlight();
lcd.clear();

// 3. Display welcome message
lcd.setCursor(1, 0);
lcd.print("IOT ASSIGNMENT");
delay(5000);

// 4. Get initial state
xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
int state = ctx->lcdState;
xSemaphoreGive(ctx->mutexContext);
```

**Main Loop:**
```c
while(1) {
    // 1. Read sensor data
    dht20.read();
    float temperature = dht20.getTemperature();
    float humidity = dht20.getHumidity();
    
    // 2. Update shared context (with mutex protection)
    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    ctx->temperature = temperature;
    ctx->humidity = humidity;
    // ... [state evaluation logic here] ...
    xSemaphoreGive(ctx->mutexContext);
    
    // 3. Display real-time sensor readings (Line 1)
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C H:");
    lcd.print(humidity, 1);
    lcd.print("%");
    
    // 4. Check for status update (non-blocking)
    if (xSemaphoreTake(ctx->semLCDUpdate, 0) == pdTRUE) {
        // Status changed, update Line 2
        xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
        int state = ctx->lcdState;
        xSemaphoreGive(ctx->mutexContext);
        
        lcd.setCursor(0, 1);
        lcd.print("Status: ");
        lcd.print(statusText(state));
    }
    
    // 5. Sleep and repeat
    vTaskDelay(pdMS_TO_TICKS(5000));
}
```

**Status Text Mapping:**
```c
static const char* statusText(int state) {
    if (state == 3) return "Critical";
    if (state == 2) return "Warning";
    return "Normal  ";  // Padded for 16-char LCD
}
```

---

## Synchronization Mechanism

### FreeRTOS Primitives Used

#### **1. Binary Semaphore (`semLCDUpdate`)**
Purpose: Signal LCD task when overall system state changes

**Characteristics:**
- Given (released) by monitor task when `lcdState` changes
- Taken (acquired) by LCD task in non-blocking mode (0 timeout)
- Only signals when threshold boundaries are crossed, not on every read
- Prevents unnecessary LCD updates

**Flow:**
```
Time    Monitor Task                    LCD Display
────────────────────────────────────────────────────
t0      Read: Temp=23°C, Humidity=45%
        tempState=1, humidityState=1
        lcdState=1 (unchanged, no signal)
        Display readings on LCD
        Sleep 5 seconds

t5      Read: Temp=26°C, Humidity=45%
        tempState=2, humidityState=1
        newLcdState=2 (CHANGED!)
        Update lcdState=2
        Give semLCDUpdate ──────────→ Check semaphore
                                      Semaphore available!
                                      Update status line
                                      Display: "Status: Warning"

t10     Read: Temp=26°C, Humidity=45%
        tempState=2, humidityState=1
        lcdState=2 (unchanged, no signal)
        Display readings on LCD
        Check semaphore (not available)
        Skip status update
```

#### **2. Mutex (`mutexContext`)**
Purpose: Protect access to shared `SharedContext` data

**Protected Operations:**
- Read/write `ctx->temperature`
- Read/write `ctx->humidity`
- Read/write `ctx->lcdState`
- Any modification to shared data

**Usage Pattern:**
```c
xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
// Critical section: Read or modify shared data
ctx->temperature = newTemp;
ctx->humidity = newHumidity;
ctx->lcdState = newState;
xSemaphoreGive(ctx->mutexContext);
```

---

## Implementation Details

### LCD Display States & Thresholds

**Three Distinct Display States:**

| State | Temperature Condition | Humidity Condition | LCD Display | Meaning |
|-------|----------------------|-------------------|-------------|---------|
| **1: Normal** | < 25°C | < 50% | "Status: Normal" | All clear, optimal conditions |
| **2: Warning** | 25-30°C | 50-70% | "Status: Warning" | Needs attention, caution advised |
| **3: Critical** | ≥ 30°C | ≥ 70% | "Status: Critical" | Immediate action required |

**State Determination:**
```
LCD State = MAX(Temperature State, Humidity State)

Example 1:
- Temperature = 26°C → State 2 (Warning)
- Humidity = 45% → State 1 (Normal)
- LCD State = MAX(2, 1) = 2 (Warning)

Example 2:
- Temperature = 24°C → State 1 (Normal)
- Humidity = 72% → State 3 (Critical)
- LCD State = MAX(1, 3) = 3 (Critical)
```

### LCD Display Format

```
┌────────────────────┐
│T:23.5C H:45.0%    │  Line 1: Real-time readings
│Status: Normal     │  Line 2: System status
└────────────────────┘

Character breakdown:
Line 1: "T:<temp>C H:<humidity>%"
Line 2: "Status: <status_text>"
```

**Line 1 Format:**
- Position 0-1: "T:"
- Position 2-5: Temperature (xxx.x°C format, 1 decimal place)
- Position 6-7: "C "
- Position 8-9: "H:"
- Position 10-13: Humidity (xx.x% format, 1 decimal place)
- Position 14-15: "%"

**Line 2 Format:**
- Position 0-7: "Status: "
- Position 8-15: Status text (Normal/Warning/Critical)

---

## Threshold Boundaries

### Temperature Threshold Diagram

```
         0°C ─────── 25°C ─────── 30°C ──────── 50°C
         │            │            │
      [Normal]    [Warning]   [Critical]
      State: 1    State: 2    State: 3
         │            │            │
      T < 25°C   25°C ≤ T < 30°C  T ≥ 30°C
```

### Humidity Threshold Diagram

```
         0% ────── 50% ────── 70% ──────── 100%
         │         │         │
      [Normal] [Warning] [Critical]
      State: 1 State: 2  State: 3
         │         │         │
      H < 50%  50% ≤ H < 70% H ≥ 70%
```

### Combined State Logic

```
Overall LCD State = MAXIMUM of temperature state and humidity state

┌─────────────────────────────────────────────────────────────┐
│  Temperature State  │  Humidity State  │  LCD State Display  │
├─────────────────────┼──────────────────┼─────────────────────┤
│  1 (Normal)         │  1 (Normal)      │  1 (Normal)         │
│  1 (Normal)         │  2 (Warning)     │  2 (Warning)        │
│  1 (Normal)         │  3 (Critical)    │  3 (Critical)       │
│  2 (Warning)        │  1 (Normal)      │  2 (Warning)        │
│  2 (Warning)        │  2 (Warning)     │  2 (Warning)        │
│  2 (Warning)        │  3 (Critical)    │  3 (Critical)       │
│  3 (Critical)       │  1 (Normal)      │  3 (Critical)       │
│  3 (Critical)       │  2 (Warning)     │  3 (Critical)       │
│  3 (Critical)       │  3 (Critical)    │  3 (Critical)       │
└─────────────────────┴──────────────────┴─────────────────────┘
```

---

## State Transition Diagram

```
┌──────────────────────────────────────────────────────────┐
│         Temperature & Humidity Monitor                    │
│              (reads DHT20 every 5s)                       │
└──────────────┬───────────────────────────────────────────┘
               │
               ↓
        ┌─────────────────────────────────────┐
        │  Evaluate Temperature & Humidity    │
        │         Thresholds                  │
        └────────┬──────────────────┬─────────┘
                 │                  │
        ┌────────▼─────┐    ┌──────▼──────┐
        │State Changed? │    │No Change    │
        └────────┬─────┘    │ Skip Signal  │
                 │          └─────────────┘
                 │ YES
    ┌────────────▼────────────────┐
    │ Update lcdState             │
    │ Give semLCDUpdate           │
    │ (Wake LCD Task)             │
    └────────┬─────────────────────┘
             │
             ↓
    ┌──────────────────────────────┐
    │  LCD Display Task            │
    │ (woken by semaphore signal)  │
    └────────┬──────────────────────┘
             │
    ┌────────▼─────────────────────┐
    │  Read lcdState              │
    │  (with mutex protection)    │
    └────────┬──────────────────────┘
             │
    ┌────────▼─────────────────────┐
    │  Update Status Text on Line 2│
    │  1 → "Status: Normal"       │
    │  2 → "Status: Warning"      │
    │  3 → "Status: Critical"     │
    └────────┬──────────────────────┘
             │
    ┌────────▼─────────────────────┐
    │  Apply Changes to LCD       │
    │  lcd.print() / lcd.show()   │
    └────────┬──────────────────────┘
             │
    ┌────────▼──────────────────────┐
    │ Return to Wait State          │
    │ Check semaphore (timeout=0)   │
    └───────────────────────────────┘
```

---

## Semaphore Signaling Flow

```
Monitor Task                          LCD Task
═════════════════════════════════════════════════════════
Every 5 seconds:
  1. Read sensors
  2. Calculate states
  3. If state changed:
     ├─ Update ctx->lcdState
     └─ Give semLCDUpdate ──────→ Semaphore Available
                                  (signal received)
                                  ↓
                                  Check semaphore
                                  (xSemaphoreTake with timeout=0)
                                  ↓
                                  If available:
                                  - Read lcdState
                                  - Update status line
                                  - Display on LCD
                                  ↓
  4. Update sensor readings on LCD
  5. Sleep 5 seconds                Return to checking
                                    semaphore
```

---

## File Structure

```
src/
├── temp_humi_monitor.cpp    # Sensor reading + state evaluation + LCD display
├── main.cpp                 # FreeRTOS initialization & task creation
└── global.cpp               # Global initialization

include/
├── global.h                 # SharedContext & semaphore declarations
├── temp_humi_monitor.h      # Monitor/LCD task declaration
└── project_includes.h       # Common includes

lib/
├── DHT20/                   # DHT20 temperature/humidity sensor driver
├── LCD/                     # LiquidCrystal I2C library
└── ... (other libraries)
```

---

## References

- [FreeRTOS Documentation](https://www.freertos.org/)
- [FreeRTOS Binary Semaphores](https://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html)
- [FreeRTOS Mutexes](https://www.freertos.org/Real-time-operating-system-semaphores.html)
- [LiquidCrystal I2C Library](https://github.com/johnrickman/LiquidCrystal_I2C)
- [DHT20 Sensor Datasheet](https://datasheet.lcsc.com/szlcsc/2404191731_Aosong-Hunan-Elec-DHT20_C2686448.pdf)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [PlatformIO Documentation](https://docs.platformio.org/)


