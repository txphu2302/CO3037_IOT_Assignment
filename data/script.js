// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onLoad);

let envChart;
const maxDataPoints = 30; // Giới hạn 30 điểm
let latestTemp = 0;
let latestHumi = 0;
let latestSoil = 0;

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
        scanWiFi();
    }
}

let isScanning = false;

function scanWiFi() {
    if (isScanning) return;
    const scanStatus = document.getElementById('scan-status');
    const wifiSelect = document.getElementById('wifi-select');
    
    if (!scanStatus || !wifiSelect) return;

    isScanning = true;
    scanStatus.style.display = 'block';
    scanStatus.style.color = '#818cf8';
    scanStatus.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Đang tìm WiFi...';
    wifiSelect.style.display = 'none';

    fetch('/scan')
        .then(r => r.json())
        .then(data => {
            isScanning = false;
            wifiSelect.innerHTML = '<option value="">-- Hoặc chọn mạng WiFi có sẵn --</option>';
            
            if (data.length === 0) {
                scanStatus.style.color = '#ef4444';
                scanStatus.innerHTML = '⚠️ Không tìm thấy mạng WiFi nào.';
                return;
            }

            data.forEach(network => {
                const option = document.createElement('option');
                option.value = network.ssid;
                option.text = network.ssid;
                wifiSelect.appendChild(option);
            });
            
            wifiSelect.style.display = 'block';
            scanStatus.style.display = 'none';
        })
        .catch(err => {
            isScanning = false;
            scanStatus.style.color = '#ef4444';
            scanStatus.innerHTML = '⚠️ Lỗi khi quét WiFi!';
            console.error('Lỗi quét WiFi:', err);
        });
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
    btn.innerText = "Đang đổi...";
    fetch('/toggle-pump')
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
