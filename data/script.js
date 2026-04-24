// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
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
            const tempEl = document.getElementById("temp_value");
            if (tempEl) tempEl.innerText = data.temperature;
        }
        if (data.humidity !== undefined) {
            const humiEl = document.getElementById("humi_value");
            if (humiEl) humiEl.innerText = data.humidity;
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
