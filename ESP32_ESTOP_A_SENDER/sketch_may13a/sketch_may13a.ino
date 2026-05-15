#include <WiFi.h>
#include <esp_now.h>

#define BUTTON_PIN 4   // 按钮接 GPIO4，另一端接 GND

typedef struct {
  uint8_t cmd;       // 1 = 急停
  uint32_t counter;  // 调试计数
} Message;

Message msg;

volatile bool emergencyFlag = false;
uint32_t sendCounter = 0;

// 按钮中断函数
void IRAM_ATTR buttonISR() {
  emergencyFlag = true;
}

// ESP-NOW发送回调函数
void onSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("[SENDER] Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

void setup() {
  Serial.begin(115200);

  // 设置按钮输入模式，上拉
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 按钮下降沿触发中断（按下按钮时 GPIO4 变 LOW）
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  // ESP-NOW必须STA模式
  WiFi.mode(WIFI_STA);

  Serial.println("=================================");
  Serial.println("ESP32 Emergency Stop - SENDER(A)");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("=================================");

  // 初始化ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[SENDER] ESP-NOW init failed!");
    while (true) delay(1000);
  }

  // 注册发送回调
  esp_now_register_send_cb(onSend);

  // 添加广播 peer（不需要知道接收端MAC）
  esp_now_peer_info_t peerInfo = {};
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[SENDER] Failed to add broadcast peer!");
    while (true) delay(1000);
  }

  Serial.println("[SENDER] Ready. Press Button to send E-STOP.");
}

void loop() {
  if (emergencyFlag) {
    emergencyFlag = false;

    msg.cmd = 1;
    msg.counter = ++sendCounter;

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));

    if (result == ESP_OK) {
      Serial.print("[SENDER] Emergency packet sent. Count=");
      Serial.println(msg.counter);
    } else {
      
      Serial.println("[SENDER] Send Error!");
    }

    delay(200); // 防抖
  }
}