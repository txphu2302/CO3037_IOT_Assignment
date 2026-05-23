# IoT Project Assignment - Task 6: Data Publishing to CoreIOT Cloud Server

## 📝 Overview

Task 6 implements secure cloud integration by publishing real-time environmental telemetry from the ESP32-S3 microcontroller to the **CoreIOT Cloud Platform** (https://app.coreiot.io/). It leverages the **MQTT (Message Queuing Telemetry Transport)** protocol for lightweight, bidirectional communication. The system publishes sensor data and device attributes, and handles incoming Remote Procedure Calls (RPC) to allow real-time control of the water pump, LED, and system modes directly from the cloud dashboard.

---

## ⚠️ WiFi Station (STA) Mode Requirement

> [!IMPORTANT]
> The ESP32-S3 microcontroller must be in **Station (STA) Mode** (connected to a local WiFi network with Internet access) to publish data to the CoreIOT cloud server. 

### Connection Flow & Transition (Luồng kết nối & chuyển đổi)
1. **Initial Boot (AP Mode)**: On startup, if no WiFi credentials exist in the `/info.dat` configuration, the ESP32-S3 boots into **Access Point (AP) Mode**, broadcasting its own network SSID (`Yolo Uno`).
2. **Web Portal Configuration**: The user accesses the local Web Dashboard at `192.168.4.1` and navigates to the **Settings** tab. The ESP32-S3 dynamically scans local 2.4GHz WiFi networks, allowing the user to select their home WiFi and enter their **CoreIOT Device Access Token**.
3. **Transition to STA Mode**: Upon clicking **Save Configuration (Lưu cấu hình)**, the settings are written to LittleFS in JSON format (`/info.dat`). The microcontroller restarts.
4. **Active MQTT Connection**: Upon reboot, the system loads the credentials, initializes WiFi in **AP+STA Mode**, connects to the Internet, and gives the `semInternetConnected` FreeRTOS Semaphore. The `coreiot_task` immediately starts, connecting to the MQTT broker at `app.coreiot.io` using the configured Device Access Token.

---

## 📁 Configuration File Structure (`/info.dat`)

The configuration parameters are persisted locally in the LittleFS filesystem:
```json
{
  "WIFI_SSID": "Your_WiFi_SSID",
  "WIFI_PASS": "Your_WiFi_Password",
  "CORE_IOT_TOKEN": "Your_CoreIOT_Device_Access_Token",
  "CORE_IOT_SERVER": "app.coreiot.io",
  "CORE_IOT_PORT": "1883"
}
```

---

## 📊 Telemetry Specifications (Thông số Telemetry)

The ESP32-S3 publishes structured sensor data to the CoreIOT platform every **10 seconds** using the telemetry topic.

- **MQTT Telemetry Topic**: `v1/devices/me/telemetry`
- **JSON Payload Format**:
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

### Data Fields Table
| Key | Type | Description |
|-----|------|-------------|
| `temperature` | float | Real-time temperature from DHT20 sensor (°C) |
| `humidity` | float | Real-time humidity from DHT20 sensor (%) |
| `soil_moisture` | float | Soil moisture percentage (%) |
| `system_status`| string | System alert level: `"Normal"`, `"Warning"`, or `"Critical"` |
| `pump_state` | string | Current state of the water pump: `"ON"` or `"OFF"` |
| `lat` | float | Hardcoded latitude for location mapping (e.g., `10.880018`) |
| `long` | float | Hardcoded longitude for location mapping (e.g., `106.806336`) |

---

## ⚙️ Attribute Synchronization (Đồng bộ Thuộc tính)

Local changes made via the Web UI are synchronized with CoreIOT as device attributes, ensuring the cloud dashboard reflects local hardware state adjustments immediately.

- **MQTT Attributes Topic**: `v1/devices/me/attributes`
- **Synchronized Attributes**:
  - `ledState` (boolean): `true` when manual LED override is active; `false` when inactive.
  - `modeState` (boolean): `true` when pump mode is `MANUAL`; `false` when mode is `AUTO`.

---

## 🎮 Remote Control via RPC (Điều khiển từ xa qua RPC)

CoreIOT utilizes **Remote Procedure Calls (RPC)** to send commands to the ESP32-S3 device. The device subscribes to the request topic and processes incoming JSON commands.

- **Subscribe Topic**: `v1/devices/me/rpc/request/+`
- **Response Topic**: `v1/devices/me/rpc/response/{requestId}`

The firmware handles 6 distinct RPC methods:

### 1. LED Control
- **`getValueLED`**:
  - *Response*: `true` / `false` representing the current manual LED state.
- **`setValueLED`**:
  - *Params*: `true` / `false`
  - *Action*: Updates the manual LED override and state. Pushes state to Web UI via WebSockets and publishes the `ledState` attribute.

### 2. Water Pump Control
- **`getValuePump`**:
  - *Response*: `true` / `false` representing the current water pump state.
- **`setValuePump`**:
  - *Params*: `true` / `false`
  - *Action*: Switches the pump to `MANUAL` mode, sets the pump state, publishes the `modeState` attribute, and broadcasts the update to Web UI clients.

### 3. Pump Mode Control
- **`getValueMode`**:
  - *Response*: `"AUTO"` / `"MANUAL"`
- **`setValueMode`**:
  - *Params*: `"AUTO"` / `"MANUAL"` (or boolean/integer equivalents)
  - *Action*: Toggles the operation mode of the pump, updates `pumpController` state, and pushes updates to Web UI.

---

## 🏗️ Architecture & Thread-Safety (Kiến trúc & Đảm bảo đa luồng)

To operate reliably in a multi-tasking FreeRTOS environment, the CoreIOT implementation includes several safety designs:

1. **Shared Context Protection (`mutexContext`)**:
   - FreeRTOS Mutex ensures that readings from `temperature`, `humidity`, and `soilMoisture` are accessed thread-safely without memory corruption.
2. **MQTT Client Mutual Exclusion (`mutexMqtt`)**:
   - Access to the underlying `PubSubClient` is restricted by `mutexMqtt` since it is shared between the telemetry publishing routine and other handlers.
3. **Deadlock Avoidance in Callbacks**:
   - Instead of calling blocking mutex requests inside the MQTT incoming callback (which runs in the context of `PubSubClient::loop()`), local state updates are queued safely, and non-blocking asynchronous publication triggers are set (e.g., `pendingLedAttributeUpdate`).

---

## 🚀 Setup & Execution Guide (Hướng dẫn Triển khai & Chạy hệ thống)

### Step 1: Create a Device on CoreIOT
1. Log in to [CoreIOT Console](https://app.coreiot.io/).
2. Navigate to **Devices** and click **Add Device**.
3. Set the Device Name (e.g., `ESP32-S3-DHT20`) and select the appropriate Device Profile.
4. Once created, click on the device and copy the **Device Access Token** from the details page.

### Step 2: Build & Flash Firmware
1. Open the project in VSCode with **PlatformIO**.
2. Build the filesystem image: **PlatformIO -> env:esp32... -> Platform -> Build Filesystem Image**.
3. Upload the filesystem: **PlatformIO -> env:esp32... -> Platform -> Upload Filesystem Image**.
4. Build and upload the main firmware: Click the arrow button **(→)** in the bottom status bar.

### Step 3: Device Provisioning (WiFi & CoreIOT Token)
1. On boot, connect your smartphone/laptop to the WiFi network broadcast by the ESP32 (SSID: `Yolo Uno`, Password: `yolouno_default` or matching configurations).
2. Open a browser and go to `192.168.4.1`.
3. Go to the **⚙️ Settings** (Cài đặt) tab.
4. Select your local 2.4GHz WiFi SSID from the dynamically populated dropdown list and enter the password.
5. Paste your **CoreIOT Device Access Token** into the token field.
6. Make sure the MQTT server is set to `app.coreiot.io` and the port to `1883`.
7. Click **Lưu cấu hình (Save Configuration)**.

### Step 4: Verification
1. Open the Serial Monitor at `115200` baud. You should observe:
   - Successful WiFi connection and assigned local IP.
   - CoreIOT connection log: `MQTT connected` or `CoreIOT MQTT connection established`.
2. Go to the **CoreIOT Cloud Dashboard**:
   - Under the **Latest Telemetry** tab of your device, you should see temperature, humidity, soil moisture, system status, pump state, latitude, and longitude updating every 10 seconds.
   - Test controlling the LED or Water Pump from the dashboard using the RPC widgets and verify that the physical board actuators trigger instantly.

---

## 📚 CoreIOT References

- [CoreIOT Cloud Platform Website](https://app.coreiot.io/)
- [PubSubClient MQTT Library for Arduino](https://pubsubclient.knolleary.net/)
- [ArduinoJson Library Reference](https://arduinojson.org/)
- [FreeRTOS Task & Mutex Documentation](https://www.freertos.org/)