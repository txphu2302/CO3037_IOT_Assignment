// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onLoad);

let envChart;
const maxDataPoints = 30; // Giới hạn 30 điểm
let latestTemp = 0;
let latestHumi = 0;
let latestSoil = 0;
let pumpMode = "AUTO";
let pumpController = "AUTO";
let pumpState = "OFF";
let pumpAutoArmed = false;

let lastLedStatus = null;
let lastSysStatus = null;
let map;

function onLoad(event) {
    initWebSocket();
    initChart();

    // Nếu có div#map thì khởi tạo bản đồ (Chỉ STA mode mới có)
    if (document.getElementById("map")) {
        initMap();
        logEvent("Hệ thống khởi động thành công", "info");
    }
}

function initMap() {
    // Tọa độ Bách Khoa
    const lat = 10.880018410410052;
    const long = 106.80633605864662;
    map = L.map('map').setView([lat, long], 16);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; OpenStreetMap'
    }).addTo(map);

    L.marker([lat, long]).addTo(map)
        .bindPopup('<b>Trạm ESP32 YOLO UNO</b><br>ĐH Bách Khoa TP.HCM')
        .openPopup();
}

function logEvent(msg, type = "info") {
    const logsEl = document.getElementById("eventLogs");
    if (!logsEl) return;

    if (logsEl.innerHTML.includes("Chưa có dữ liệu")) {
        logsEl.innerHTML = "";
    }

    const now = new Date();
    const timeStr = now.toLocaleTimeString();

    const li = document.createElement("li");
    li.innerHTML = `<span class="log-time">[${timeStr}]</span> <span class="log-type-${type}">${msg}</span>`;

    logsEl.prepend(li); // Đẩy lên đầu

    if (logsEl.children.length > 50) {
        logsEl.removeChild(logsEl.lastChild);
    }
}

function onOpen(event) {
    console.log('Connection opened');
    logEvent("Đã kết nối với ESP32 (WebSocket)", "info");
}

function onClose(event) {
    console.log('Connection closed');
    logEvent("Mất kết nối! Đang thử lại...", "crit");
    setTimeout(initWebSocket, 2000);
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function initChart() {
    const ctx = document.getElementById('envChart').getContext('2d');
    envChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Nhiệt độ (°C)',
                    borderColor: '#fb923c',
                    backgroundColor: 'rgba(251, 146, 60, 0.1)',
                    data: [],
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Độ ẩm (%)',
                    borderColor: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.1)',
                    data: [],
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Độ ẩm Đất (%)',
                    borderColor: '#4ade80',
                    backgroundColor: 'rgba(74, 222, 128, 0.1)',
                    data: [],
                    tension: 0.4,
                    fill: true
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: { beginAtZero: true }
            },
            plugins: {
                legend: { position: 'top' }
            }
        }
    });
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Gửi:", data);
    } else {
        console.warn("⚠️ WebSocket chưa sẵn sàng!");
        alert("⚠️ WebSocket chưa kết nối!");
    }
}

function onMessage(event) {
    console.log("📩 Nhận:", event.data);
    try {
        var data = JSON.parse(event.data);
        if (data.temperature !== undefined) {
            latestTemp = data.temperature;
            const tempEl = document.getElementById("temp_value");
            if (tempEl) tempEl.innerText = latestTemp;
        }
        if (data.led !== undefined) {
            const btn = document.getElementById("btnToggleLED");
            if (btn) {
                btn.className = data.led === "ON" ? "toggle-btn on" : "toggle-btn";
                btn.innerText = data.led;
            }
            if (lastLedStatus !== data.led) {
                if (lastLedStatus !== null) logEvent(`Đèn LED chuyển sang ${data.led}`, "info");
                lastLedStatus = data.led;
            }
        }
        if (data.humidity !== undefined) {
            latestHumi = data.humidity;
            const humiEl = document.getElementById("humi_value");
            if (humiEl) humiEl.innerText = latestHumi;
        }
        if (data.soil_moisture !== undefined) {
            latestSoil = data.soil_moisture;
            const soilEl = document.getElementById("soil_value");
            if (soilEl) soilEl.innerText = latestSoil;
            const pumpSoilEl = document.getElementById("pump_soil_value");
            if (pumpSoilEl) pumpSoilEl.innerText = `${Number(latestSoil).toFixed(1)}%`;
        }
        if (data.pump_state !== undefined) {
            pumpState = data.pump_state;
            updatePumpUI();
        }
        if (data.pump_mode !== undefined) {
            pumpMode = data.pump_mode;
            updatePumpUI();
        }
        if (data.pump_controller !== undefined) {
            pumpController = data.pump_controller;
            updatePumpUI();
        }
        if (data.pump_auto_armed !== undefined) {
            pumpAutoArmed = !!data.pump_auto_armed;
            updatePumpUI();
        }
        if (data.pump_threshold !== undefined) {
            const el = document.getElementById("pump_threshold_input");
            if (el) el.value = data.pump_threshold;
        }
        if (data.pump_hysteresis !== undefined) {
            const el = document.getElementById("pump_hysteresis_input");
            if (el) el.value = data.pump_hysteresis;
        }
        if (data.pump_schedule_time !== undefined) {
            const el = document.getElementById("pump_schedule_time");
            if (el) el.value = data.pump_schedule_time;
            const view = document.getElementById("pump_schedule_view");
            if (view) view.innerText = data.pump_schedule_time;
        }
        if (data.pump_schedule_duration !== undefined) {
            const el = document.getElementById("pump_schedule_duration");
            if (el) el.value = data.pump_schedule_duration;
            const view = document.getElementById("pump_duration_view");
            if (view) view.innerText = `${data.pump_schedule_duration}s`;
        }
        if (data.pump_schedule_enabled !== undefined) {
            const enabled = !!data.pump_schedule_enabled;
            const el = document.getElementById("pump_schedule_enable");
            if (el) el.checked = enabled;
            const view = document.getElementById("pump_schedule_enable_view");
            if (view) view.innerText = enabled ? "BẬT" : "TẮT";
        }
        if (data.system_status !== undefined) {
            const statusEl = document.getElementById("sys_status");
            if (statusEl) {
                statusEl.innerText = data.system_status;
            }
            if (lastSysStatus !== data.system_status) {
                if (lastSysStatus !== null) {
                    let logType = "info";
                    if (data.system_status === "Warning") logType = "warn";
                    if (data.system_status === "Critical") logType = "crit";
                    logEvent(`Cảnh báo: Trạng thái ${data.system_status}`, logType);
                }
                lastSysStatus = data.system_status;
            }
        }

        // Cập nhật biểu đồ nếu nhận gói tin nhiệt độ (đại diện chu kỳ đo)
        if (data.temperature !== undefined && envChart) {
            const now = new Date();
            const timeLabel = now.getHours() + ':' + now.getMinutes() + ':' + now.getSeconds();

            envChart.data.labels.push(timeLabel);
            envChart.data.datasets[0].data.push(latestTemp);
            envChart.data.datasets[1].data.push(latestHumi);
            envChart.data.datasets[2].data.push(latestSoil);

            // Giới hạn số điểm trên biểu đồ
            if (envChart.data.labels.length > maxDataPoints) {
                envChart.data.labels.shift();
                envChart.data.datasets.forEach(ds => ds.data.shift());
            }
            envChart.update();
        }
    } catch (e) {
        console.warn("Không phải JSON hợp lệ:", event.data);
    }
}


// ==================== UI NAVIGATION ====================
function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');
    document.getElementById(id).style.display = id === 'settings' ? 'flex' : 'block';
    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    event.currentTarget.classList.add('active');

    if (id === 'settings') {
        const now = Date.now();
        if (now - _lastScanTime > SCAN_CACHE_MS) {
            scanWiFi();
        }
    }
}

let isScanning = false;
let _scanPollTimer = null;
let _scanCompleted = false;
let _lastScanTime = 0;
const SCAN_CACHE_MS = 30000; // 30 giây

function rssiToBar(rssi) {
    if (rssi >= -55) return '█████'; // rất mạnh
    if (rssi >= -65) return '████░';
    if (rssi >= -75) return '███░░';
    if (rssi >= -85) return '██░░░';
    return '█░░░░';                  // yếu
}

function rssiToColor(rssi) {
    if (rssi >= -65) return '#4ade80'; // xanh lá
    if (rssi >= -75) return '#facc15'; // vàng
    return '#f87171';                  // đỏ
}

// Bắt đầu quét mới (gọi từ nút hoặc khi vào tab)
function scanWiFi() {
    if (isScanning) return;
    const scanStatus = document.getElementById('scan-status');
    const wifiSelect = document.getElementById('wifi-select');
    const rescanBtn = document.getElementById('btn-rescan');
    if (!scanStatus || !wifiSelect) return;

    isScanning = true;
    _scanCompleted = false;
    if (rescanBtn) rescanBtn.disabled = true;
    scanStatus.style.display = 'block';
    scanStatus.style.color = '#818cf8';
    scanStatus.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Đang quét WiFi...';
    wifiSelect.style.display = 'none';

    // Bước 1: khởi động quét async trên ESP32
    fetch('/scan/start')
        .then(() => {
            // Bước 2: poll kết quả mỗi 500ms
            _scanPollTimer = setInterval(_pollScanResult, 500);
        })
        .catch(err => {
            _scanDone(null);
            console.error('Lỗi bắt đầu quét:', err);
        });
}

function _pollScanResult() {
    fetch('/scan/result')
        .then(r => r.json())
        .then(data => {
            if (data.status === 'scanning') return; // chờ tiếp
            clearInterval(_scanPollTimer);
            _scanDone(data);
        })
        .catch(err => {
            clearInterval(_scanPollTimer);
            _scanDone(null);
            console.error('Lỗi poll quét WiFi:', err);
        });
}

function _scanDone(data) {
    if (_scanCompleted) return; // Chặn các fetch trễ gọi lại
    _scanCompleted = true;
    isScanning = false;
    const scanStatus = document.getElementById('scan-status');
    const wifiSelect = document.getElementById('wifi-select');
    const rescanBtn = document.getElementById('btn-rescan');
    if (rescanBtn) rescanBtn.disabled = false;

    if (!data || data.status === 'error' || !data.networks) {
        scanStatus.style.color = '#ef4444';
        scanStatus.innerHTML = '⚠️ Quét thất bại. <a href="#" onclick="rescanWiFi();return false;">Thử lại</a>';
        return;
    }

    const networks = data.networks;
    wifiSelect.innerHTML = '<option value="">-- Chọn mạng WiFi --</option>';

    if (networks.length === 0) {
        scanStatus.style.color = '#f59e0b';
        scanStatus.innerHTML = '⚠️ Không tìm thấy mạng nào.';
        return;
    }

    // Sắp xếp theo RSSI mạnh nhất lên đầu
    networks.sort((a, b) => b.rssi - a.rssi);

    networks.forEach(net => {
        const opt = document.createElement('option');
        opt.value = net.ssid;
        opt.text = net.ssid;
        wifiSelect.appendChild(opt);
    });

    wifiSelect.style.display = 'block';
    scanStatus.style.color = '#4ade80';
    _lastScanTime = Date.now(); // Cập nhật cache
    const age = Math.round(SCAN_CACHE_MS / 1000);
    scanStatus.innerHTML = `✅ Tìm thấy ${networks.length} mạng. <span style="opacity:0.6;font-size:0.8em">(tự làm mới sau ${age}s)</span>`;
}

// Nút quét lại
function rescanWiFi() {
    if (isScanning) return;
    clearInterval(_scanPollTimer);
    scanWiFi();
}


// ==================== DEVICE FUNCTIONS ====================
function toggleLED() {
    const btn = document.getElementById("btnToggleLED");
    btn.innerText = "Đang đổi...";
    fetch('/toggle-led')
        .then(r => r.text())
        .then(msg => {
            btn.className = msg === "ON" ? "toggle-btn on" : "toggle-btn";
            btn.innerText = msg;
        })
        .catch(err => {
            btn.innerText = "Lỗi!";
            console.error(err);
        });
}

function togglePump() {
    const btn = document.getElementById("btnTogglePump");
    if (!btn) return;
    btn.innerText = "Đang đổi...";
    fetch('/toggle-pump')
        .then(r => r.text())
        .then(msg => {
            btn.className = msg === "ON" ? "toggle-btn on" : "toggle-btn";
            btn.innerText = msg;
            pumpState = msg;
            pumpMode = "MANUAL";
            pumpController = "MANUAL";
            updatePumpUI();
        })
        .catch(err => {
            btn.innerText = "Lỗi!";
            console.error(err);
        });
}

function updatePumpUI() {
    const stateEl = document.getElementById("pump_state_value");
    if (stateEl) stateEl.innerText = pumpState;

    const modeEl = document.getElementById("pump_mode_value");
    if (modeEl) modeEl.innerText = pumpMode;

    const controllerEl = document.getElementById("pump_controller_value");
    if (controllerEl) {
        let controllerText = pumpController;
        if (pumpController === "AUTO_DISARMED") controllerText = "AUTO (chưa lưu config)";
        if (pumpController === "AUTO_WAIT") controllerText = "AUTO (đang chờ cảm biến)";
        controllerEl.innerText = controllerText;
    }

    const btn = document.getElementById("btnTogglePump");
    if (btn) {
        btn.className = pumpState === "ON" ? "toggle-btn on" : "toggle-btn";
        btn.innerText = pumpState;
    }

    const manualHint = document.getElementById("pump_manual_hint");
    if (manualHint) manualHint.innerText = `Trạng thái hiện tại: ${pumpState}`;

    const btnOn = document.getElementById("btnPumpOn");
    const btnOff = document.getElementById("btnPumpOff");
    if (btnOn) {
        btnOn.style.opacity = pumpState === "ON" ? "1" : "0.85";
        btnOn.style.outline = pumpState === "ON" ? "2px solid #22c55e" : "none";
    }
    if (btnOff) {
        btnOff.style.opacity = pumpState === "OFF" ? "1" : "0.85";
        btnOff.style.outline = pumpState === "OFF" ? "2px solid #94a3b8" : "none";
    }
}

function setPumpMode(mode) {
    Send_Data(JSON.stringify({
        page: "pump",
        action: "set_mode",
        value: { mode: mode }
    }));
    pumpMode = mode;
    if (mode === "AUTO") pumpController = "AUTO";
    if (mode === "MANUAL") pumpController = "MANUAL";
    updatePumpUI();
}

function savePumpAutoConfig() {
    const threshold = parseInt(document.getElementById("pump_threshold_input")?.value || "40", 10);
    const hysteresis = parseInt(document.getElementById("pump_hysteresis_input")?.value || "5", 10);
    Send_Data(JSON.stringify({
        page: "pump",
        action: "set_auto",
        value: {
            threshold: threshold,
            hysteresis: hysteresis
        }
    }));
    pumpAutoArmed = true;
    alert("Đã lưu AUTO config. Chế độ AUTO được kích hoạt.");
}

function savePumpSchedule() {
    const time = document.getElementById("pump_schedule_time")?.value || "06:00";
    const duration = parseInt(document.getElementById("pump_schedule_duration")?.value || "15", 10);
    const enabled = !!document.getElementById("pump_schedule_enable")?.checked;

    Send_Data(JSON.stringify({
        page: "pump",
        action: "set_schedule",
        value: {
            enabled: enabled,
            time: time,
            duration: duration
        }
    }));

    const viewTime = document.getElementById("pump_schedule_view");
    if (viewTime) viewTime.innerText = time;
    const viewDuration = document.getElementById("pump_duration_view");
    if (viewDuration) viewDuration.innerText = `${duration}s`;
    const viewEnabled = document.getElementById("pump_schedule_enable_view");
    if (viewEnabled) viewEnabled.innerText = enabled ? "BẬT" : "TẮT";
    alert("Đã lưu lịch tưới.");
}

function setPumpManualState(state) {
    fetch('/toggle-pump?state=' + encodeURIComponent(state))
        .then(r => r.text())
        .then(msg => {
            pumpState = msg;
            pumpMode = "MANUAL";
            pumpController = "MANUAL";
            updatePumpUI();
        })
        .catch(err => console.error(err));
}

function toggleNeo() {
    const btn = document.getElementById("btnToggleNeo");
    btn.innerText = "Đang đổi...";
    fetch('/toggle-neo')
        .then(r => r.text())
        .then(msg => {
            btn.className = msg === "ON" ? "toggle-btn on" : "toggle-btn";
            btn.innerText = msg;
        })
        .catch(err => {
            btn.innerText = "Lỗi!";
            console.error(err);
        });
}

function changeNeoColor(colorHex) {
    const btn = document.getElementById("btnToggleNeo");
    fetch(`/toggle-neo?color=${encodeURIComponent(colorHex)}`)
        .then(r => r.text())
        .then(msg => {
            btn.className = msg === "ON" ? "toggle-btn on" : "toggle-btn";
            btn.innerText = msg;
        })
        .catch(err => console.error(err));
}


// ==================== SETTINGS FORM ====================
document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const ssidInput = document.getElementById("ssid");
    const mqttTokenInput = document.getElementById("mqtt_token");

    if (ssidInput) {
        // --- Form cấu hình WiFi (AP Mode) ---
        const ssid = ssidInput.value.trim();
        const password = document.getElementById("password").value.trim();

        const settingsJSON = JSON.stringify({
            page: "setting",
            value: {
                ssid: ssid,
                password: password,
                token: "",
                server: "",
                port: ""
            }
        });

        Send_Data(settingsJSON);
        alert("✅ Lưu cấu hình thành công! Thiết bị đang khởi động lại để kết nối WiFi...");
    } else if (mqttTokenInput) {
        // --- Form cấu hình MQTT Core IoT (STA Mode) ---
        const token = mqttTokenInput.value.trim();
        const server = document.getElementById("mqtt_server").value.trim();
        const port = document.getElementById("mqtt_port").value.trim();

        const settingsJSON = JSON.stringify({
            page: "setting",
            value: {
                // Để nguyên khoảng trắng hoặc null để C++ không bị lỗi
                ssid: "STA_MODE_KEEP",
                password: "STA_MODE_KEEP",
                token: token,
                server: server,
                port: port
            }
        });

        Send_Data(settingsJSON);
        alert("✅ Lưu cấu hình thành công! Đã gửi thông số Core IoT đến thiết bị...");
    }
});
