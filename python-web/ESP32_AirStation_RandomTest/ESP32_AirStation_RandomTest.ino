/*******************************************************************************
 * ESP32 — PHIÊN BẢN 2: CHẾ ĐỘ TEST (RANDOM)
 * Tự sinh số liệu cảm biến ngẫu nhiên rồi đẩy lên Firebase.
 * KHÔNG cần STM32 / cảm biến — chỉ để test dashboard + AI phân tích.
 ******************************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>

/* ---- CẤU HÌNH WIFI NHÀ (ĐỂ VÀO MẠNG) ---- */
const char* WIFI_SSID     = "mywifi";       // ← Đổi tên WiFi nhà
const char* WIFI_PASSWORD = "11111111";   // ← Đổi Pass WiFi nhà

/* ---- CẤU HÌNH FIREBASE ---- */
const char* FIREBASE_URL =
    "https://techrum-43351-default-rtdb.asia-southeast1.firebasedatabase.app/air_station.json";

/* ---- Chu kỳ sinh số liệu mới ---- */
#define PUSH_INTERVAL 15000   // 15 giây sinh & đẩy 1 lần (đổi tùy ý)

/* ---- Biến lưu dữ liệu ---- */
struct AirData {
    float dust, co, lpg, smoke, h2, ch4, co2, nh3, alcohol, aqi;
    int   level;      // 1-5
} airData = {0};

/* ---- Kết nối WiFi (chỉ cần STA để ra Internet — bản test không cần phát AP) ---- */
void wifiConnect() {
    WiFi.mode(WIFI_STA);
    Serial.printf("[+] Dang ket noi WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int timeout = 40;   // 20 giây
    while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[+] WiFi OK! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[-] WiFi THAT BAI — khoi dong lai sau 5s");
        delay(5000);
        ESP.restart();
    }
}

/* ---- Sinh số float ngẫu nhiên trong khoảng [lo, hi] ---- */
float randf(float lo, float hi) {
    return lo + (float)random(0, 10001) / 10000.0 * (hi - lo);
}

/* ---- Tính level 1-5 từ số liệu (ngưỡng trùng với dashboard cho nhất quán) ---- */
int computeLevel() {
    int lv = 1;
    if (airData.dust > 10  || airData.co > 80  || airData.co2 > 400)   lv = 2;
    if (airData.dust > 80  || airData.co > 220 || airData.co2 > 1500 ||
        airData.smoke > 400 || airData.lpg > 400)                      lv = 3;
    if (airData.dust > 150 || airData.co > 380 || airData.co2 > 2500 ||
        airData.smoke > 1500)                                          lv = 4;
    if (airData.dust > 250 || airData.co > 500 || airData.smoke > 2500) lv = 5;
    return lv;
}

/* ---- Sinh 1 bộ số liệu ngẫu nhiên (trải rộng từ "Tốt" tới "Nguy hiểm") ---- */
void generateRandomData() {
    airData.dust    = randf(0, 300);
    airData.co      = randf(0, 600);
    airData.lpg     = randf(0, 2500);
    airData.smoke   = randf(0, 2500);
    airData.h2      = randf(0, 1500);
    airData.ch4     = randf(0, 1500);
    airData.co2     = randf(400, 3000);   // CO2 không khí thường ~400 ppm
    airData.nh3     = randf(0, 300);
    airData.alcohol = randf(0, 500);
    airData.aqi     = randf(0, 300);
    airData.level   = computeLevel();
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
    Serial.begin(115200);
    Serial.println("\n=== ESP32 Air Station — CHE DO TEST (RANDOM) ===");
    randomSeed(esp_random());   // seed ngẫu nhiên thật từ phần cứng ESP32
    wifiConnect();
}

/* ---- LOOP ---- */
void loop() {
    static unsigned long lastPush = 0;
    if (millis() - lastPush >= PUSH_INTERVAL) {
        generateRandomData();
        Serial.printf("[RANDOM] dust=%.1f co=%.1f lpg=%.1f smoke=%.1f co2=%.1f level=%d\n",
                      airData.dust, airData.co, airData.lpg,
                      airData.smoke, airData.co2, airData.level);
        pushFirebase();
        lastPush = millis();
    }
}
