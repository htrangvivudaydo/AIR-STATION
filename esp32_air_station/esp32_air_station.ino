/*******************************************************************************
 * ESP32 — Nhận UART từ STM32, đẩy lên Firebase Realtime Database
 * TÍCH HỢP: CHẾ ĐỘ DUAL WIFI (VỪA THU VỪA PHÁT)
 ******************************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>

/* ---- CẤU HÌNH WIFI NHÀ (ĐỂ VÀO MẠNG) ---- */
const char* WIFI_SSID     = "BKCON";       // ← Đổi tên WiFi nhà
const char* WIFI_PASSWORD = "1211@1311";        // ← Đổi Pass WiFi nhà

/* ---- CẤU HÌNH WIFI DO ESP32 TỰ PHÁT (TRẠM PHÁT) ---- */
const char* AP_SSID       = "AirStation_ESP32"; 
const char* AP_PASSWORD   = "12345678"; 

/* ---- CẤU HÌNH FIREBASE ---- */
const char* FIREBASE_URL =
    "https://techrum-43351-default-rtdb.asia-southeast1.firebasedatabase.app/air_station.json";

/* ---- UART2 nhận từ STM32 ---- */
#define RXD2 16   // ESP32 GPIO16 ← STM32 TX
#define TXD2 17   // Không dùng nhưng phải khai

/* ---- Biến lưu dữ liệu ---- */
struct AirData {
    float dust;       // µg/m³
    float co;         // ppm
    float lpg;        // ppm
    float smoke;      // ppm
    float h2;         // ppm
    float ch4;        // ppm
    float co2;        // ppm
    float nh3;        // ppm
    float alcohol;    // ppm
    float aqi;
    int   level;      // 1-5
    bool  valid;
} airData = {0};

/* ---- Kết nối WiFi (Dual Mode) ---- */
void wifiConnect() {
    // Kích hoạt chế độ Dual: Trạm phát (AP) + Thu sóng (STA)
    WiFi.mode(WIFI_AP_STA);

    // 1. Khởi tạo mạng WiFi tự phát
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("\n[+] Da phat WiFi rieng: %s (Pass: %s)\n", AP_SSID, AP_PASSWORD);

    // 2. Kết nối WiFi nhà để thông ra Internet
    Serial.printf("[+] Dang ket noi WiFi nha: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int timeout = 40;   // 20 giây
    while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[+] WiFi OK! IP Internet: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[-] WiFi THAT BAI — khoi dong lai sau 5s");
        delay(5000);
        ESP.restart();
    }
}

/* ---- Parse 1 dòng CSV từ STM32 ---- */
bool parseCSV(String line) {
    if (line.startsWith("t(") || line.startsWith("\r") || line.length() < 10)
        return false;

    float vals[11];
    int idx = 0;
    int start = 0;

    int comma = line.indexOf(',');
    if (comma < 0) return false;
    start = comma + 1;

    for (int i = 0; i < 11 && start < (int)line.length(); i++) {
        comma = line.indexOf(',', start);
        String token;
        if (comma < 0)
            token = line.substring(start);
        else
            token = line.substring(start, comma);

        token.trim();
        vals[i] = token.toFloat();
        idx++;
        start = comma + 1;
        if (comma < 0) break;
    }

    if (idx < 11) return false;

    airData.dust    = vals[0];
    airData.co      = vals[1];
    airData.lpg     = vals[2];
    airData.smoke   = vals[3];
    airData.h2      = vals[4];
    airData.ch4     = vals[5];
    airData.co2     = vals[6];
    airData.nh3     = vals[7];
    airData.alcohol = vals[8];
    airData.aqi     = vals[9];
    airData.level   = (int)vals[10];
    airData.valid   = true;

    return true;
}

/* ---- Đẩy dữ liệu lên Firebase bằng REST API (PUT) ---- */
void pushFirebase() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi mat ket noi, dang thu lai...");
        wifiConnect();
        return;
    }

    char json[350];
    snprintf(json, sizeof(json),
        "{\"dust\":%.1f,\"co\":%.1f,\"lpg\":%.1f,\"smoke\":%.1f,"
        "\"h2\":%.1f,\"ch4\":%.1f,\"co2\":%.1f,\"nh3\":%.1f,"
        "\"alcohol\":%.1f,\"aqi\":%.1f,\"level\":%d}",
        airData.dust, airData.co, airData.lpg, airData.smoke,
        airData.h2, airData.ch4, airData.co2, airData.nh3,
        airData.alcohol, airData.aqi, airData.level
    );

    HTTPClient http;
    http.begin(FIREBASE_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.PUT(json);
    if (code == 200) {
        Serial.println("Firebase OK");
    } else {
        Serial.printf("Firebase loi: %d\n", code);
    }
    http.end();
}

/* ---- SETUP ---- */
void setup() {
    Serial.begin(115200);                                  // Debug qua USB
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);        // UART từ STM32
    Serial.println("\n=== ESP32 Air Station ===");

    wifiConnect();
}

/* ---- LOOP ---- */
void loop() {
    if (Serial2.available()) {
        String line = Serial2.readStringUntil('\n');
        line.trim();

        Serial.print("STM32> ");
        Serial.println(line);

        if (parseCSV(line)) {
            Serial.printf("  dust=%.1f co=%.1f lpg=%.1f co2=%.1f level=%d\n",
                          airData.dust, airData.co, airData.lpg,
                          airData.co2, airData.level);
           static unsigned long lastPush = 0;
            if (millis() - lastPush >= 15000) {   // 15 giây mới gửi 1 lần
                pushFirebase();
                lastPush = millis();
            }
        }
    }
}