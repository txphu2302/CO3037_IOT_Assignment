// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onLoad);

let envChart;
const maxDataPoints = 30; // Giới hạn 30 điểm
let latestTemp = 0;
let latestHumi = 0;
let latestSoil = 0;

function onLoad(event) {
    initWebSocket();
    initChart();
}

function onOpen(event) {
    console.log('Connection opened');
}

function onClose(event) {
    console.log('Connection closed');
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
                    borderColor: '#ff5722',
                    backgroundColor: 'rgba(255, 87, 34, 0.1)',
                    data: [],
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Độ ẩm K2 (%)',
                    borderColor: '#03a9f4',
                    backgroundColor: 'rgba(3, 169, 244, 0.1)',
                    data: [],
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Độ ẩm Đất (%)',
                    borderColor: '#4caf50',
                    backgroundColor: 'rgba(76, 175, 80, 0.1)',
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
                if (data.system_status === "Normal") {
                    statusEl.style.color = "#4caf50";
                    statusEl.style.textShadow = "2px 2px 8px rgba(76, 175, 80, 0.2)";
                } else if (data.system_status === "Warning") {
                    statusEl.style.color = "#ff9800";
                    statusEl.style.textShadow = "2px 2px 8px rgba(255, 152, 0, 0.2)";
                } else if (data.system_status === "Critical") {
                    statusEl.style.color = "#f44336";
                    statusEl.style.textShadow = "2px 2px 8px rgba(244, 67, 54, 0.2)";
                }
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

    const ssid = document.getElementById("ssid").value.trim();
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
    alert("✅ Cấu hình Wi-Fi đã được gửi đến thiết bị!");
});
