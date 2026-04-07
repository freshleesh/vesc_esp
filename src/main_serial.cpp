#include <Arduino.h>
#include <VescUart.h>

#define VESC_RX 5
#define VESC_TX 17

VescUart vesc;

float steering = 0.0f;
float throttle = 0.0f;
float maxRPM = 10000.0f;

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, VESC_RX, VESC_TX);
    vesc.setSerialPort(&Serial2);

    Serial.println("=== VESC 시리얼 테스트 ===");
    Serial.println("명령어:");
    Serial.println("  S:+0.50,T:+0.80  - 조향/스로틀 설정");
    Serial.println("  X                - 정지");
    Serial.println("  ?                - VESC 상태 읽기");
    Serial.println("=========================");
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        Serial.print("> ");
        Serial.println(cmd);

        if (cmd.equalsIgnoreCase("X")) {
            steering = 0.0f;
            throttle = 0.0f;
            Serial.println("정지!");
        } else if (cmd.charAt(0) == '?') {
            if (vesc.getVescValues()) {
                Serial.printf("RPM: %.0f  전압: %.1fV  전류: %.1fA  FET: %.1fC\n",
                              vesc.data.rpm, vesc.data.inpVoltage,
                              vesc.data.avgMotorCurrent, vesc.data.tempFET);
            } else {
                Serial.println("VESC 응답 없음");
            }
        } else {
            float s, t;
            if (sscanf(cmd.c_str(), "S:%f,T:%f", &s, &t) == 2) {
                steering = constrain(s, -1.0f, 1.0f);
                throttle = constrain(t, -1.0f, 1.0f);
            } else {
                Serial.println("형식: S:+0.50,T:+0.80");
            }
        }

        float rpm = throttle * maxRPM;
        float servoPos = (steering + 1.0f) * 0.5f;
        Serial.printf("[OUT] Steer:%+.2f  Throt:%+.2f  RPM:%.0f  Servo:%.2f\n",
                      steering, throttle, rpm, servoPos);
    }

    vesc.setRPM(throttle * maxRPM);
    vesc.setServoPos((steering + 1.0f) * 0.5f);

    delay(20);
}
