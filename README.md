# IoT Project Assignment - Task 4: Web Server in Access Point Mode

Dự án này cung cấp một giải pháp hoàn chỉnh để giám sát và điều khiển thiết bị IoT từ xa sử dụng vi điều khiển ESP32 (dòng YOLO UNO). Dự án tích hợp các công nghệ như FreeRTOS, AsyncWebServer, WebSocket và LittleFS để mang lại trải nghiệm tối ưu và tính ổn định cao.

## 🌟 Các tính năng nổi bật

### 1. Bảng điều khiển Web (Web Dashboard)
- Giao diện người dùng hiện đại, thân thiện, được thiết kế với HTML/CSS/JS tĩnh và lưu trữ trực tiếp trong phân vùng `LittleFS` của ESP32.
- **Giám sát thời gian thực:** Hiển thị trực tiếp các thông số Nhiệt độ, Độ ẩm (Cảm biến DHT20) và Độ ẩm đất.
- **Biểu đồ động:** Vẽ biểu đồ biểu diễn sự thay đổi của dữ liệu môi trường theo thời gian thực thông qua thư viện `Chart.js`.
- **Điều khiển thiết bị:** Hỗ trợ Bật/Tắt các thiết bị ngoại vi như Đèn LED và Máy Bơm nước.
- **Điều khiển NeoPixel:** Tích hợp bộ chọn bảng màu (Color Palette) để điều khiển màu sắc đèn Neo Pixel RGB trực tiếp từ màn hình web.

### 2. Cấu hình mạng thông minh (AP Mode & WiFi Scanner)
- **Chế độ Access Point (AP):** Khi chưa có thông tin mạng WiFi, thiết bị sẽ tự động phát ra sóng WiFi (chế độ AP) để người dùng dùng điện thoại truy cập vào thiết lập.
- **Tự động quét WiFi (WiFi Scanner):** Ngay khi mở mục Cài đặt trên Web, mạch sẽ tự động quét các mạng WiFi (băng tần 2.4GHz) xung quanh và hiển thị trong danh sách thả xuống. Người dùng chỉ cần click chọn mà không cần phải gõ tay (SSID).
- **Lưu cấu hình an toàn:** Hỗ trợ lưu trữ cấu hình mạng Wi-Fi và tham số máy chủ Core IoT thông qua các file dữ liệu độc lập.

### 3. Kiến trúc Đa tiến trình (FreeRTOS)
- Hệ thống hoạt động dựa trên các Task (tiến trình) chạy song song và độc lập.
- Sử dụng **Semaphore (Binary Semaphore)** để đồng bộ hóa các sự kiện phần cứng (ví dụ: phát hiện độ ẩm vượt mức cho phép sẽ thay đổi màu NeoPixel lập tức mà không cần dùng hàm `delay`).
- Sử dụng **Mutex** để khóa/bảo vệ dữ liệu chia sẻ (SharedContext) giữa Webserver và Cảm biến, tránh lỗi xung đột bộ nhớ.

---

## ⚙️ Yêu cầu phần cứng

- Bảng mạch vi điều khiển **YOLO UNO (ESP32-S3)**
- Cảm biến Nhiệt độ / Độ ẩm **DHT20** (Giao tiếp I2C)
- Cảm biến độ ẩm đất
- Dây đèn/Led **NeoPixel (WS2812B)**
- Đèn LED cơ bản và Relay (Máy bơm nước)

---

## 📁 Cấu trúc thư mục

```text
├── data/                  # Thư mục chứa các file giao diện (Nạp vào LittleFS)
│   ├── index.html         # Giao diện chính (Bao gồm Dashboard & Cài đặt)
│   ├── script.js          # Logic xử lý giao diện, WebSocket, Biểu đồ và Quét WiFi
│   ├── styles.css         # File định dạng CSS cho toàn bộ web
│   └── chart.js           # Thư viện vẽ biểu đồ
├── include/               # Chứa các file Header (.h)
│   ├── global.h           # Định nghĩa SharedContext, biến toàn cục cho FreeRTOS
│   └── ...
├── src/                   # Chứa các file mã nguồn C/C++ thực thi chính
│   ├── main.cpp           # Khởi tạo hệ thống và khởi chạy các Task (FreeRTOS)
│   ├── task_webserver.cpp # Định nghĩa Web API và WebSocket handler
│   ├── task_wifi.cpp      # Điều hướng chuyển đổi giữa AP Mode và STA Mode
│   ├── neo_blinky.cpp     # Nhận tín hiệu điều khiển đèn NeoPixel
│   └── temp_humi_monitor.cpp # Task đọc cảm biến DHT20 liên tục
└── platformio.ini         # File cấu hình thư viện và board mạch của PlatformIO
```

---

## 🚀 Hướng dẫn cài đặt và nạp Code

### 1. Môi trường phát triển
Dự án này được tối ưu cho phần mềm **Visual Studio Code (VSCode)** cài đặt kèm tiện ích mở rộng **PlatformIO IDE**.

### 2. Nạp dữ liệu giao diện Web (Upload Filesystem)
Trang web tĩnh của dự án không nằm trong code C++ mà nằm ở bộ nhớ Flash (LittleFS). Bạn bắt buộc phải nạp nó trước:
1. Nhấn vào biểu tượng con kiến (PlatformIO) ở thanh công cụ bên trái VSCode.
2. Mở mục **Project Tasks** -> `env:esp32...` -> **Platform** -> Click vào **Build Filesystem Image**.
3. Cắm mạch ESP32 vào máy tính, sau đó click vào **Upload Filesystem Image**.

### 3. Nạp mã nguồn thực thi (Upload Code)
1. Ở cạnh dưới màn hình VSCode, nhấn vào biểu tượng dấu tick **(✓)** để Build (Biên dịch) mã nguồn C++.
2. Nhấn vào biểu tượng mũi tên sang phải **(→)** để Upload (Nạp) code vào mạch ESP32.

### 4. Cách sử dụng tính năng cấu hình Web
1. Khởi động ESP32. Vì chưa có WiFi, nó sẽ phát ra mạng WiFi của riêng nó (Access Point).
2. Dùng điện thoại kết nối vào mạng WiFi này (Nhớ **tắt 4G/Dữ liệu di động** để không bị lỗi không tải được trang).
3. Mở trình duyệt web, truy cập địa chỉ IP mặc định: `192.168.4.1`.
4. Trang web quản lý sẽ hiện ra. Bạn chuyển sang tab **⚙️ Cài đặt**. 
5. Lúc này ESP32 sẽ tự động dò tìm các mạng WiFi 2.4GHz ở xung quanh và hiện danh sách. Chọn WiFi nhà bạn, nhập mật khẩu rồi bấm nút **Lưu cấu hình**.
6. Mạch sẽ tự động lưu lại, tắt trạm phát (AP) và kết nối với Router WiFi nhà bạn như một thiết bị IoT bình thường (Chế độ STA).

---

## 🔧 Tính năng tự động hóa cục bộ (Task 2)
Bên cạnh việc điều khiển qua Web, thiết bị hoạt động như một hệ thống cảnh báo môi trường tự động (Đồng bộ bằng FreeRTOS Semaphore):
- **Bình thường (Độ ẩm < 50%):** Đèn NeoPixel sáng màu Xanh lá.
- **Cảnh báo (Độ ẩm 50% - 70%):** Đèn NeoPixel chuyển sang màu Vàng.
- **Nguy hiểm (Độ ẩm ≥ 70%):** Đèn NeoPixel chuyển sang màu Đỏ.
*(Logic này phản ứng tức thời theo thời gian thực mà không bị ảnh hưởng bởi đường truyền mạng).*
