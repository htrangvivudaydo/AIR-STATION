
# Smart Air Station (Trạm Giám Sát Không Khí Thông Minh)

Hệ thống giám sát chất lượng không khí thời gian thực tích hợp trí tuệ nhân tạo (Gemini AI), sử dụng vi điều khiển **STM32F411** chạy hệ điều hành thời gian thực (**FreeRTOS**) kết hợp gateway **ESP32** đẩy dữ liệu lên đám mây (**Firebase**).

---

## 📌 Các Tính Năng Nổi Bật

- **Đa nhiệm FreeRTOS (STM32)**: Quản lý tối ưu 3 tác vụ song song: đọc cảm biến (10Hz), cập nhật màn hình TFT (2Hz) và gửi báo cáo CSV qua UART (0.5Hz).
- **Dual Wi-Fi Mode (ESP32)**: Gateway ESP32 vừa đóng vai trò trạm phát AP (để kết nối cấu hình), vừa kết nối Wi-Fi nhà để đẩy dữ liệu lên Firebase Realtime Database qua REST API.
- **Web Dashboard Thời Gian Thực**: Giao diện hiển thị trực quan các thông số bụi mịn, khí độc (CO, LPG, Smoke, Methane, Mật độ bụi PM).
- **Trí Tuệ Nhân Tạo (Gemini AI)**: Phân tích sâu các chỉ số không khí hiện tại và đưa ra cảnh báo, lời khuyên sức khỏe thông minh dựa trên mô hình Gemini.

---

## 🛠 Yêu Cầu Phần Mềm & Công Cụ Cần Thiết

Để xây dựng và chạy dự án này, bạn cần cài đặt các công cụ và phần mềm sau:

### 1. Đối với STM32
- **IDE**: STM32CubeIDE (phiên bản 1.12.0 trở lên) hoặc Keil uVision 5.
- **Cấu hình & Driver**: STM32CubeMX (để cấu hình ngoại vi và tích hợp FreeRTOS).
- **Thư viện**: STM32F4xx HAL Library (được tải tự động khi tạo dự án STM32F411).
- **Phần mềm nạp**: STM32CubeProgrammer hoặc ST-Link Utility.

### 2. Đối với ESP32
- **IDE**: Arduino IDE (phiên bản 2.0 trở lên) hoặc VS Code + PlatformIO.
- **Board Package**: Gói hỗ trợ board ESP32 của Espressif (tải qua Board Manager).
- **Cáp kết nối**: Cáp Micro-USB hoặc Type-C để nạp code và debug.

### 3. Đối với Giao Diện Web & AI
- **Python**: Phiên bản Python 3.8 trở lên (để chạy server trung gian `proxy.py`).
- **Trình duyệt**: Chrome, Edge, Firefox hoặc Safari (hỗ trợ JavaScript ES6 Module).
- **Firebase Account**: Tài khoản Firebase để tạo cơ sở dữ liệu Realtime Database.
- **Gemini API Key**: API Key được đăng ký từ Google AI Studio.

---

## 📋 Thiết Bị Phần Cứng Sử Dụng

1. **Vi điều khiển chính**: STM32F411VET6 Discovery.
2. **Gateway Internet**: ESP32 DevKit V1.
3. **Cảm biến sử dụng**:
   - Cảm biến bụi mịn: **GP2Y1010AU0F** (Sharp).
   - Cảm biến khí hóa lỏng/khói: **MQ-2**.
   - Cảm biến khí CO/Methane: **MQ-9**.
   - Cảm biến chất lượng không khí (CO2/NH3/Alcohol): **MQ-135**.
4. **Màn hình hiển thị**: TFT LCD 1.44 inch **ST7735** (giao tiếp SPI).

---

## 📂 Cấu Trúc Thư Mục Dự Án

```text
├── stm32/                   # Mã nguồn STM32CubeIDE / Keil C
│   ├── Core/
│   │   ├── Src/
│   │   │   ├── main.c       # Hàm main & Khởi tạo thiết bị
│   │   │   ├── air_quality.c# FreeRTOS Tasks logic
│   │   │   ├── gp2y1010.c   # Driver cảm biến bụi Sharp
│   │   │   ├── mq_sensors.c # Driver cảm biến dòng MQ (MQ-2, MQ-9, MQ-135)
│   │   │   └── st7735.c     # Driver màn hình TFT ST7735
│   │   └── Inc/             # Khai báo các file header tương ứng
├── esp32_air_station/       # Mã nguồn ESP32 (Arduino IDE)
│   └── esp32_air_station.ino# Cấu hình Dual Wi-Fi & Đẩy dữ liệu Firebase
└── web/                     # Giao diện giám sát Web & Local Proxy
    ├── AIRSTATION.html      # Dashboard HTML/CSS/JS thời gian thực
    └── proxy.py             # Server Python trung gian tránh CORS cho Gemini API
```

---

## 🚀 Hướng Dẫn Cài Đặt & Chạy Dự Án

### 1. Nạp Code cho STM32
- Mở thư mục `/stm32` bằng STM32CubeIDE hoặc Keil uVision.
- Cấu hình các chân ADC, SPI1 (TFT), UART1 (Truyền thông ESP32) theo sơ đồ chân trong code.
- Build dự án và nạp code cho STM32F411 qua mạch nạp ST-Link.

### 2. Cài Đặt ESP32 Gateway
- Mở file `esp32_air_station/esp32_air_station.ino` bằng Arduino IDE.
- Cài đặt Board ESP32 và các thư viện cần thiết.
- Thay đổi thông số Wi-Fi và URL Firebase phù hợp với hệ thống của bạn:
  ```cpp
  const char* WIFI_SSID     = "Tên_WiFi_Nhà";
  const char* WIFI_PASSWORD = "Mật_Khẩu_WiFi";
  const char* FIREBASE_URL  = "https://your-rtdb-firebase.firebaseio.com/air_station.json";
  ```
- Kết nối chân TX của STM32 vào pin RXD2 (GPIO16) của ESP32. Nạp code cho ESP32.

### 3. Khởi Chạy Giao Diện Web & AI
- Truy cập vào Firebase Console, tạo Firebase Realtime Database và thay cấu hình DB của bạn vào thẻ `<script>` trong file `web/AIRSTATION.html`.
- Điền API Key Gemini của bạn vào file `web/proxy.py`.
- Mở Terminal tại thư mục `/web` và chạy server proxy:
  ```bash
  python proxy.py
  ```
- Truy cập địa chỉ `http://localhost:8080/AIRSTATION.html` trên trình duyệt để theo dõi dữ liệu và bấm **Phân tích ngay** để nhận lời khuyên sức khỏe từ AI.
