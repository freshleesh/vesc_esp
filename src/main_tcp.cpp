#include <Arduino.h>
#include <ETH.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <VescUart.h>

// ── VESC UART 핀 ──
#define VESC_RX 5
#define VESC_TX 17

// ── Ethernet static IP ──
static const IPAddress LOCAL_IP(192, 168, 1, 100);
static const IPAddress GATEWAY(192, 168, 1, 1);
static const IPAddress SUBNET(255, 255, 255, 0);

// ── TCP Server ──
static const uint16_t TCP_PORT = 8080;
WiFiServer server(TCP_PORT);
WiFiClient client;

// ── VESC ──
VescUart vesc;

// ── 제어값 ──
float steering = 0.0f;   // -1.0 ~ +1.0
float throttle = 0.0f;   // -1.0 ~ +1.0
float maxRPM = 10000.0f; // 최대 RPM (필요에 따라 조정)

// ── Line buffer ──
static char lineBuf[64];
static uint8_t lineIdx = 0;

// ── Ethernet status ──
static bool ethConnected = false;

// ── 상태 출력 타이머 ──
static unsigned long lastStatusMs = 0;
static const unsigned long STATUS_INTERVAL = 1000;

void onEthEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("[ETH] Started");
            ETH.setHostname("rc-car-esp32");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("[ETH] Link Up");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.printf("[ETH] IP: %s\n", ETH.localIP().toString().c_str());
            ethConnected = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("[ETH] Link Down");
            ethConnected = false;
            break;
        default:
            break;
    }
}

/// Parse "S:+0.50,T:+0.80" from TCPBridge
bool parseLine(const char* line) {
    float s, t;
    if (sscanf(line, "S:%f,T:%f", &s, &t) == 2) {
        steering = constrain(s, -1.0f, 1.0f);
        throttle = constrain(t, -1.0f, 1.0f);
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    Serial.println("[ESP32] Booting...");

    // VESC UART 초기화
    Serial2.begin(115200, SERIAL_8N1, VESC_RX, VESC_TX);
    vesc.setSerialPort(&Serial2);

    // Ethernet 초기화
    WiFi.onEvent(onEthEvent);
    ETH.begin();
    ETH.config(LOCAL_IP, GATEWAY, SUBNET);

    server.begin();
    Serial.printf("[TCP] Listening on %s:%u\n",
                  LOCAL_IP.toString().c_str(), TCP_PORT);
    Serial.println("[VESC] Ready");
}

void loop() {
    // ── TCP 클라이언트 수락 ──
    if (!client || !client.connected()) {
        WiFiClient newClient = server.available();
        if (newClient) {
            client = newClient;
            lineIdx = 0;
            Serial.printf("[TCP] Client connected: %s\n",
                          client.remoteIP().toString().c_str());
        }
    }

    // ── TCP 데이터 수신 및 파싱 ──
    if (client && client.connected()) {
        while (client.available()) {
            char c = client.read();
            if (c == '\n') {
                lineBuf[lineIdx] = '\0';
                if (parseLine(lineBuf)) {
                    Serial.printf("Steering: %+.2f  Throttle: %+.2f\n",
                                  steering, throttle);
                }
                lineIdx = 0;
            } else if (lineIdx < sizeof(lineBuf) - 1) {
                lineBuf[lineIdx++] = c;
            }
        }
    }

    // ── VESC 제어 출력 ──
    // throttle (-1~+1) → RPM
    float rpm = throttle * maxRPM;
    vesc.setRPM(rpm);

    // steering (-1~+1) → 서보 (0.0~1.0, 0.5가 중립)
    vesc.setServoPos((steering + 1.0f) * 0.5f);

    // ── 주기적 상태 출력 ──
    unsigned long now = millis();
    if (now - lastStatusMs >= STATUS_INTERVAL) {
        lastStatusMs = now;
        Serial.printf("[STATUS] ETH:%s  TCP:%s  Steer:%+.2f  Throt:%+.2f  RPM:%.0f  Servo:%.2f\n",
                      ethConnected ? "OK" : "DOWN",
                      (client && client.connected()) ? "OK" : "NO",
                      steering, throttle,
                      throttle * maxRPM,
                      (steering + 1.0f) * 0.5f);
    }

    delay(20);
}
