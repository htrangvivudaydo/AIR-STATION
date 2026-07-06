/*******************************************************************************
 * ESP32 — Nhận UART từ STM32, đẩy lên Firebase Realtime Database
 * PHIÊN BẢN 1: BỘ LỌC TRUNG BÌNH — gom đủ 5 lần đọc mới tính trung bình & đẩy
 * TÍCH HỢP: CHẾ ĐỘ DUAL WIFI (VỪA THU VỪA PHÁT)
 ******************************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <string.h>

/* ---- CẤU HÌNH WIFI NHÀ (ĐỂ VÀO MẠNG) ---- */
const char* WIFI_SSID     = "Furina";       // ← Đổi tên WiFi nhà
const char* WIFI_PASSWORD = "1234567999";   // ← Đổi Pass WiFi nhà

/* ---- CẤU HÌNH WIFI DO ESP32 TỰ PHÁT (TRẠM PHÁT) ---- */
const char* AP_SSID       = "AirStation_ESP32";
const char* AP_PASSWORD   = "12345678";

/* ---- CẤU HÌNH FIREBASE ---- */
const char* FIREBASE_URL =
    "https://techrum-43351-default-rtdb.asia-southeast1.firebasedatabase.app/air_station.json";

/* ---- UART2 nhận từ STM32 ---- */
#define RXD2 16   // ESP32 GPIO16 ← STM32 TX
#define TXD2 17   // Không dùng nhưng phải khai

/* ---- SỐ MẪU GOM ĐỂ TÍNH TRUNG BÌNH ---- */
#define SAMPLE_COUNT 5   // Cứ đủ 5 lần đọc hợp lệ thì tính trung bình & đẩy lên

/* ---- Biến lưu dữ liệu (tạm giữ 1 lần đọc, hoặc giữ giá trị trung bình) ---- */
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

/* ---- Bộ tích lũy để cộng dồn rồi chia trung bình ---- */
struct AirAccum {
    float dust, co, lpg, smoke, h2, ch4, co2, nh3, alcohol, aqi;
    long  level;      // cộng dồn level rồi chia & làm tròn
    int   count;      // đã gom được bao nhiêu mẫu
} accum;

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

/* ---- Parse 1 dòng CSV từ STM32 (ghi vào airData) ---- */
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

/* ---- Cộng dồn 1 mẫu (airData hiện tại) vào bộ tích lũy ---- */
void addSample() {
    accum.dust    += airData.dust;
    accum.co      += airData.co;
    accum.lpg     += airData.lpg;
    accum.smoke   += airData.smoke;
    accum.h2      += airData.h2;
    accum.ch4     += airData.ch4;
    accum.co2     += airData.co2;
    accum.nh3     += airData.nh3;
    accum.alcohol += airData.alcohol;
    accum.aqi     += airData.aqi;
    accum.level   += airData.level;
    accum.count++;
}

/* ---- Reset bộ tích lũy về 0 ---- */
void resetAccum() {
    memset(&accum, 0, sizeof(accum));
}

/* ---- Tính trung bình rồi đẩy lên Firebase ---- */
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

/* ---- Tính trung bình của SAMPLE_COUNT mẫu, ghi vào airData rồi đẩy ---- */
void averageAndPush() {
    float n = (float)accum.count;

    airData.dust    = accum.dust    / n;
    airData.co      = accum.co      / n;
    airData.lpg     = accum.lpg     / n;
    airData.smoke   = accum.smoke   / n;
    airData.h2      = accum.h2      / n;
    airData.ch4     = accum.ch4     / n;
    airData.co2     = accum.co2     / n;
    airData.nh3     = accum.nh3     / n;
    airData.alcohol = accum.alcohol / n;
    airData.aqi     = accum.aqi     / n;
    airData.level   = (int)round(accum.level / n);   // trung bình level rồi làm tròn

    Serial.printf("[TRUNG BINH %d mau] dust=%.1f co=%.1f lpg=%.1f co2=%.1f level=%d -> day Firebase\n",
                  accum.count, airData.dust, airData.co, airData.lpg,
                  airData.co2, airData.level);

    pushFirebase();
    resetAccum();
}

/* ---- SETUP ---- */
void setup() {
    Serial.begin(115200);                                  // Debug qua USB
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);         // UART từ STM32
    Serial.println("\n=== ESP32 Air Station (LOC TRUNG BINH 5 MAU) ===");

    resetAccum();
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
            addSample();
            Serial.printf("  Da gom %d/%d mau (dust=%.1f co=%.1f co2=%.1f)\n",
                          accum.count, SAMPLE_COUNT,
                          airData.dust, airData.co, airData.co2);

            // Đủ 5 mẫu -> tính trung bình & đẩy lên Firebase
            if (accum.count >= SAMPLE_COUNT) {
                averageAndPush();
            }
        }
    }
}
