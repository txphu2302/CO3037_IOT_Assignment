# IoT Project Assignment - Consolidated Multi-Task RTOS Monitoring & Cloud Control System

This project is a comprehensive environmental monitoring and remote control system developed on the **YOLO UNO (ESP32-S3)** microcontroller utilizing the **PlatformIO IDE** framework. The system implements the **FreeRTOS** real-time operating system to manage concurrent tasks, deploys an edge machine learning model using **TinyML (TensorFlow Lite Micro)** for real-time risk assessment, and integrates with the **CoreIOT Cloud Platform** via the **MQTT** protocol for cloud monitoring and bidirectional remote control.

This project enhances and extends the original template codebase by more than 30% through the absolute elimination of global variables (utilizing a thread-safe `SharedContext` structure synchronized via FreeRTOS Mutexes/Semaphores) and introducing advanced automated control features.

---

## 📋 Table of Contents
1. [Hardware Pinout Mapping](#-hardware-pinout-mapping)
2. [Task 1: Temperature-Conditioned Single LED Blinking](#task-1-temperature-conditioned-single-led-blinking)
3. [Task 2: Humidity-Based NeoPixel LED Control](#task-2-humidity-based-neopixel-led-control)
4. [Task 3: Temperature & Humidity LCD Monitoring & Global Variables Removal](#task-3-temperature--humidity-lcd-monitoring--global-variables-removal)
5. [Task 4: High-Performance AP Web Server & Dual Actuator Control](#task-4-high-performance-ap-web-server--dual-actuator-control)
6. [Task 5: Edge TinyML Risk Assessment & Performance Evaluation](#task-5-edge-tinyml-risk-assessment--performance-evaluation)
7. [Task 6: Cloud Data Publishing & Remote RPC Control to CoreIOT](#task-6-cloud-data-publishing--remote-rpc-control-to-coreiot)
8. [Directory Structure](#-directory-structure)
9. [Setup & Deployment Guide](#-setup--deployment-guide)
10. [References](#-references)

---

## 🔌 Hardware Pinout Mapping

| Component / Sensor / Actuator | Connection Type / Protocol | ESP32-S3 GPIO Pins | Detailed Description |
|---------------------------------|---------------------------|--------------------------|----------------------|
| **DHT20 Sensor** | I2C Protocol | SDA: `GPIO 11`<br>SCL: `GPIO 12` | Reads ambient temperature and humidity. |
| **I2C LCD 16x2 Display** | I2C Protocol (Address `0x27`) | SDA: `GPIO 11`<br>SCL: `GPIO 12` | Displays real-time measurement readings and alerts. |
| **Soil Moisture Sensor** | Analog Input (ADC1_CH0) | `GPIO 1` | Measures soil moisture levels. |
| **Water Pump (Relay)** | Digital Output | `GPIO 6` | Toggles the active/inactive state of the water pump. |
| **Built-in Single LED** | Digital Output | `GPIO 48` | Blinks according to active temperature alert levels. |
| **NeoPixel RGB LED** | WS2812B Protocol | `GPIO 45` | Displays alert color states matching humidity levels. |

---

## Task 1: Temperature-Conditioned Single LED Blinking

### 📌 Objective
Control the blinking frequency of the built-in single LED (`GPIO 48`) based on 3 distinct temperature thresholds read from the DHT20 sensor, utilizing FreeRTOS synchronization mechanisms for instantaneous frequency updates.

### ⚙️ Thresholds and Blinking Frequencies
* **Normal Temperature (T < 25°C)**: Slow blink (Cycle: `1000ms`).
* **Warning Temperature (25°C ≤ T < 30°C)**: Medium blink (Cycle: `500ms`).
* **Critical Temperature (T ≥ 30°C)**: Fast blink (Cycle: `100ms`).

### 🛡️ FreeRTOS Semaphore Synchronization
Instead of calling the blocking `delay()` function which would hang the task, we implement the `xSemaphoreTake(ctx->semLEDUpdate, pdMS_TO_TICKS(delay_ms))` structure:
* When the DHT20 sensor monitoring task detects a change in the active temperature threshold, it immediately signals the blink task by calling `xSemaphoreGive(ctx->semLEDUpdate)`.
* The blink task (`led_blinky`), which is blocked waiting on the semaphore, wakes up instantly and adjusts its cycle duration to match the new state without waiting for the previous cycle to finish.
* State access is protected thread-safely via the `ctx->mutexContext` Mutex.

---

## Task 2: Humidity-Based NeoPixel LED Control

### 📌 Objective
Dynamically update the color pattern of the NeoPixel RGB LED (`GPIO 45`) based on 3 humidity levels in real-time. It also integrates local/remote manual color override controls from the Web Portal and the Cloud.

### 🎨 Humidity Mapping and Color Patterns
* **Normal Humidity (H < 50%)**: Sáng màu **Green (Xanh lá)** - Represents a safe environment.
* **Warning Humidity (50% ≤ H < 70%)**: Sáng màu **Yellow (Vàng)** - Indicates elevated levels requiring attention.
* **Critical Humidity (H ≥ 70%)**: Sáng màu **Red (Đỏ)** - Alerts dangerous moisture levels.

### 🛡️ FreeRTOS Semaphore Synchronization
Similar to Task 1, the NeoPixel task (`neo_blinky`) runs in an event-driven loop:
* The task is efficiently blocked on `xSemaphoreTake(ctx->semNeoUpdate, pdMS_TO_TICKS(100))`.
* As soon as the DHT20 sensor task determines a state transition, it invokes `xSemaphoreGive(ctx->semNeoUpdate)` to refresh the LED color immediately.
* It supports manual color configuration by bypassing automatic patterns when the manual override flag (`ctx->neoManualOverride`) is set via the Web UI or Cloud.

---

## Task 3: Temperature & Humidity LCD Monitoring & Global Variables Removal

### 📌 Objective
* Monitor temperature, humidity, and system status dynamically on an I2C 16x2 LCD display.
* Eliminate 100% of global variables in the codebase to meet industry-grade standards for embedded software, preventing memory corruption and thread race conditions.

### 📺 LCD Display Warning States
The system raises alert levels based on a **worst-case scenario (max of both states)**: `Risk = max(temp_state, humidity_state)`.
* **Normal State**: LCD displays `Status: Normal`.
* **Warning State**: LCD displays `Status: Warning`.
* **Critical State**: LCD displays `Status: Critical`.

### 🛡️ Flicker-Free LCD Sync
To prevent screen flickering and unnecessary I2C bus congestion caused by writing to the LCD continuously, the LCD task only refreshes the second line (Status) when signaled by `xSemaphoreTake(ctx->semLCDUpdate, 0) == pdTRUE` from the monitoring task.

### 🚫 Complete Removal of Global Variables (Zero Global Variables)
All system variables, semaphores, mutexes, and network handles are encapsulated inside a dynamically allocated structure named `SharedContext`:

```cpp
struct SharedContext {
  float temperature;
  float humidity;
  float soilMoisture;
  bool soilReady;
  
  SemaphoreHandle_t mutexContext;
  SemaphoreHandle_t mutexSerial;
  SemaphoreHandle_t semLEDUpdate;
  int ledState;
  SemaphoreHandle_t semNeoUpdate;
  int neoState;
  SemaphoreHandle_t semLCDUpdate;
  int lcdState;

  // Local configurations loaded from LittleFS
  String wifiSsid;
  String wifiPass;
  String coreIotToken;
  String coreIotServer;
  String coreIotPort;
  ...
};
```
This context is instantiated dynamically using `new` in the `setup()` function of `main.cpp` and passed as a pointer `(void *)ctx` to the `pvParameters` of all FreeRTOS tasks. Shared variables are strictly protected using Mutex locks.

---

## Task 4: High-Performance AP Web Server & Dual Actuator Control

### 📌 Objective
Provide a highly optimized and mobile-responsive Web Dashboard stored entirely in the ESP32-S3's `LittleFS` flash filesystem. The web server runs in Access Point (AP) mode on boot if no network is configured.

### 🎨 Premium Web Interface Design
* **Embedded Stack**: Styled with a sleek CSS layout and real-time bidirectional communication via **WebSocket** (`/ws`) for instant telemetry graphing without reloading pages.
* **Live Charts**: Utilizes **Chart.js** to render real-time temperature and humidity trendlines dynamically.
* **Intelligent WiFi Scanner**: Automatically performs a non-blocking background WiFi scan, populating nearby 2.4GHz networks in a clean Web dropdown list. Users can connect by selecting their network and inputting credentials.

### 🕹️ Independent Actuator Control
The interface provides independent controls for:
1. **LED**: Manual toggle switch which is synchronized with the Cloud.
2. **Water Pump**: Supports three distinct operational modes:
   * **MANUAL**: Forced ON/OFF toggled from the UI.
   * **AUTO**: Activates when `Soil Moisture < Threshold`, implementing a customizable **Hysteresis** parameter to prevent rapid relay switching.
   * **SCHEDULE**: Performs daily automated watering scheduled using local real-time clocks synchronized with NTP servers.

---

## Task 5: Edge TinyML Risk Assessment & Performance Evaluation

### 📌 Objective
Train and deploy a feed-forward deep neural network (TinyML) on the ESP32-S3 microcontroller to classify environmental risk levels (1: Normal, 2: Warning, 3: Critical) based on temperature and humidity inputs.

### 📊 Dataset Collection & Labeling
* **Size**: Contains **5,792 labeled samples** ([ml/dataset.csv](ml/dataset.csv)) of temperature and humidity features.
* **Boundary Grid Sampling**: **1,792 samples** are densely collected near risk decision boundaries (e.g., T ~ 25°C, 30°C and H ~ 50%, 70%) to train the model to be highly precise in critical transition ranges.
* **Random Sampling**: **4,000 samples** are uniformly distributed across the dht sensor ranges to increase generalization capability.
* **Ground Truth Labeling**: Formulated by the rule-based logic: `Risk = max(temp_risk, humi_risk)`.

### 🧠 Neural Network Model Architecture
An optimized Keras Sequential model converted into TensorFlow Lite float32:
* **Input Layer**: 2 features `[Temperature, Humidity]`.
* **Hidden Layer 1**: 24 neurons, `ReLU` activation.
* **Hidden Layer 2**: 16 neurons, `ReLU` activation.
* **Output Layer**: 3 neurons with `Softmax` activation mapping class probabilities.
* **Binary Size**: ~30 KB (Float32) stored as a hex array in [include/dht_anomaly_model.h](include/dht_anomaly_model.h).

### 🖥️ On-Device Inference & Accuracy Evaluation
The inference task `tiny_ml_task` runs asynchronously every 5 seconds utilizing a **16 KB Tensor Arena** in SRAM:
* **Rolling Accuracy**: Achieves **95% - 98%** in real-time compared directly on-device with the rule-based ground truth.
* **Inference Latency**: Extremely fast, measuring between **30 - 50 ms** per cycle, driven by the Xtensa LX7 dual-core hardware acceleration.
* **Error Analysis**: The minor error (<5%) is mainly caused by sensor measurement noise on DHT20 and marginal decision ambiguity near exact thresholds.

---

## Task 6: Cloud Data Publishing & Remote RPC Control to CoreIOT

### 📌 Objective
Transmit local environmental telemetry and device attributes to the **CoreIOT Cloud Platform** (https://app.coreiot.io/) using the **MQTT** protocol when connected to the Internet via WiFi Station (STA) mode.

### 🔄 Telemetry Publishing Loop
* **MQTT Broker**: `app.coreiot.io` (Default port `1883`).
* **Frequency**: Published every **10 seconds**.
* **Topic Telemetry**: `v1/devices/me/telemetry`
* **JSON Payload Format**:
  ```json
  {
    "temperature": 27.5,
    "humidity": 62.4,
    "soil_moisture": 45.2,
    "system_status": "Normal",
    "pump_state": "OFF",
    "lat": 10.880018,
    "long": 106.806336
  }
  ```

### ⚙️ Attribute Synchronization
Device attributes like the manual LED override status (`ledState`) or pump mode (`modeState`) are published automatically to `v1/devices/me/attributes` immediately upon changes in the local Web Dashboard.

### 🎮 Bidirectional Control via RPC
The firmware subscribes to the request topic `v1/devices/me/rpc/request/+` and processes incoming control methods from the cloud dashboard:
* **`getValueLED` / `setValueLED`**: Query/Set manual LED override.
* **`getValuePump` / `setValuePump`**: Query/Set manual Pump state.
* **`getValueMode` / `setValueMode`**: Query/Set Pump operational mode (`AUTO` / `MANUAL`).

> [!TIP]
> All incoming state updates from the cloud dashboard via RPC are instantly broadcast back to all active local browser clients via WebSockets, ensuring seamless state synchronization across the entire system.

---

## 📁 Directory Structure

```text
├── .pio/                   # PlatformIO build outputs
├── data/                   # Web assets compiled into LittleFS flash image
│   ├── AP.html             # Access Point mode setup interface
│   ├── STA.html            # Station mode real-time control dashboard
│   ├── script.js           # WebSocket connection & Chart.js plotting logic
│   └── styles.css          # UI stylesheet
├── include/                # Global declaration files
│   ├── global.h            # Main FreeRTOS SharedContext definition
│   ├── dht_anomaly_model.h # Hex array representing the TinyML model
│   ├── risk_label.h        # Ground truth label definitions
│   └── ...
├── src/                    # C++ source files
│   ├── main.cpp            # System initialization & FreeRTOS task spawning
│   ├── temp_humi_monitor.cpp # DHT20 & LCD monitoring loop (Task 3)
│   ├── led_blinky.cpp      # Temperature-conditioned blinking task (Task 1)
│   ├── neo_blinky.cpp      # Humidity-based NeoPixel task (Task 2)
│   ├── pump.cpp            # Auto/Scheduled pump control task
│   ├── coreiot.cpp         # CoreIOT cloud MQTT client task (Task 6)
│   ├── tinyml.cpp          # Edge TensorFlow Lite inference task (Task 5)
│   └── task_webserver.cpp  # Async Web Server & WebSocket handler (Task 4)
├── ml/                     # Machine learning development assets
│   ├── dataset.csv         # Labeled sensor dataset
│   ├── train_export.py     # Python script to train and export TFLite header
│   └── requirements.txt    # Python requirements list
└── platformio.ini          # PlatformIO configurations & package dependencies
```

---

## 🚀 Setup & Deployment Guide

### 1. Development Environment
* Visual Studio Code (VSCode) with the **PlatformIO IDE** extension installed.
* USB-C data cable connecting the **YOLO UNO (ESP32-S3)** board.
* **Python 3.8+** with dependencies listed in `ml/requirements.txt` to run the TinyML training pipeline.

### 2. Flashing the Filesystem (LittleFS Web Assets)
* On the left sidebar of VSCode, click the PlatformIO logo (ant head).
* Navigate to **Project Tasks** -> select the target environment -> **Platform** -> **Build Filesystem Image**.
* Once constructed, click **Upload Filesystem Image** to flash the web portal files onto the ESP32 memory.

### 3. Flashing the Firmware
* Click the checkmark **(✓)** in the bottom VSCode status bar to compile the C++ source files.
* Click the right arrow **(→)** to upload the firmware binary to the board.
* Open the **Serial Monitor** (plug icon) set at `115200` baud to observe live debugging logs.

---

## 📚 References

* [TensorFlow Lite Micro Library Repository](https://github.com/tensorflow/tflite-micro)
* [CoreIOT Cloud Platform documentation](https://app.coreiot.io/)
* [FreeRTOS Task & Synchronization References](https://www.freertos.org/)
* [ESPAsyncWebServer GitHub Repository](https://github.com/me-no-dev/ESPAsyncWebServer)
* [ArduinoJson Serialization Guide](https://arduinojson.org/)
* [ESP32-S3 Technical Reference Manual - Espressif](https://www.espressif.com/)