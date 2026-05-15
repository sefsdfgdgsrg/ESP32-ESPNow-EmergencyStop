#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED参数
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// 继电器控制引脚
#define RELAY_PIN 16

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ESP-NOW数据结构
typedef struct {
  uint8_t cmd;       // 1 = 急停
  uint32_t counter;  // 调试计数
} Message;

Message incomingMsg;
volatile bool emergencyStop = false;

// OLED显示：正常运行界面
void showNormal() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("System Running");

  display.setCursor(0, 16);
  display.println("ESP-NOW Receiver");

  display.setCursor(0, 32);
  display.println("Waiting for E-STOP");

  display.setCursor(0, 50);
  display.println("Relay: ON");

  display.display();
}

// OLED显示：急停界面
void showEmergency(uint32_t count) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("E-STOP!");

  display.setTextSize(1);
  display.setCursor(0, 28);
  display.println("Emergency Success!");

  display.setCursor(0, 42);
  display.print("Packet: ");
  display.println(count);

  display.setCursor(0, 54);
  display.println("Relay: OFF");

  display.display();
}

// ESP-NOW接收回调（兼容ESP32 Arduino 2.0.12）
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len == sizeof(Message)) {
    memcpy(&incomingMsg, data, sizeof(Message));

    if (incomingMsg.cmd == 1) {
      emergencyStop = true;

      Serial.print("[RECEIVER] E-STOP received, Count=");
      Serial.println(incomingMsg.counter);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);

  // 默认继电器吸合（正常运行）
  // 如果继电器逻辑反了，就改成 LOW
  digitalWrite(RELAY_PIN, HIGH);

  // 初始化I2C
  Wire.begin(21, 22);

  // 初始化OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[RECEIVER] OLED init failed!");
    while (true) delay(1000);
  }

  showNormal();

  // 初始化WiFi
  WiFi.mode(WIFI_STA);

  Serial.println("=================================");
  Serial.println("ESP32 Emergency Stop - RECEIVER(B)");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("=================================");

  // 初始化ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[RECEIVER] ESP-NOW init failed!");
    while (true) delay(1000);
  }

  // 注册接收回调
  esp_now_register_recv_cb(onReceive);

  Serial.println("[RECEIVER] Ready. Waiting emergency packet...");
}

void loop() {
  if (emergencyStop) {
    Serial.println("[RECEIVER] !!! EMERGENCY STOP TRIGGERED !!!");

    // 急停：继电器断电
    digitalWrite(RELAY_PIN, LOW);

    // OLED显示急停成功
    showEmergency(incomingMsg.counter);

    // 锁死急停状态
    while (true) {
      delay(1000);
    }
  }

  delay(10);
}
