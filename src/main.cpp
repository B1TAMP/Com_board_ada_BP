#include <Arduino.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include "AS5600.h"

#define SDA_PIN 22
#define SCL_PIN 19

// ===== ESP-NOW Premenne =====
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Receiver MAC (broadcast)

typedef struct {
  float angle;
  uint32_t timestamp;
} Message;

esp_now_peer_info_t peerInfo;

AS5600 as5600;
float angleOffset = 0;
unsigned long last_send = 0;
const unsigned long SEND_INTERVAL = 100; // 100ms - vzor 10x za sekundu

// ===== ESP-NOW Callbacks =====
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Callback ked su data poslane
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Serial.println("✓ Poslane OK");
  } else {
    Serial.println("✗ Chyba odesielania");
  }
}

void printMAC(const uint8_t *macaddr) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
  Serial.print(buf);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n--- AS5600 - AUTO KALIBRACIA ---");
  
  Wire.begin(SDA_PIN, SCL_PIN);
  
  if (as5600.begin()) {
    Serial.println("STATUS: Senzor AS5600 OK!");
    
    delay(500);
    angleOffset = as5600.readAngle() * AS5600_RAW_TO_DEGREES;
    Serial.print("✓ Offset nastavený na: ");
    Serial.print(angleOffset, 2);
    Serial.println("°");
    Serial.println("Senzor je teraz na 0°\n");
    
  } else {
    Serial.println("STATUS: CHYBA!");
    while(1);
  }
  
  // ===== ESP-NOW Setup =====
  Serial.println("--- ESP-NOW Setup ---");
  
  // Nastav WiFi mode (potrebne pre ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Get MAC adresa
  Serial.print("QT Py MAC: ");
  Serial.println(WiFi.macAddress());
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init CHYBA!");
    return;
  }
  
  // Registruj callback pre odesielanie
  esp_now_register_send_cb(OnDataSent);
  
  // Prida peer (receiver) - v broadcast mode (FF:FF:...)
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Chyba pri pridavani peera");
    return;
  }
  
  Serial.println("✓ ESP-NOW inicializovany");
  Serial.println("Zacinam posielat data...");
  Serial.println("Start!\n");
}

// ===== Main Loop =====
void loop() {
  // Citaj senzor
  float rawAngle = as5600.readAngle() * AS5600_RAW_TO_DEGREES;
  float angle = rawAngle - angleOffset;
  
  if (angle < 0) {
    angle += 360;
  }
  
  Serial.print("Uhol: ");
  Serial.print(angle, 2);
  Serial.println(" °");
  
  // Posli data kazych SEND_INTERVAL ms
  if (millis() - last_send >= SEND_INTERVAL) {
    last_send = millis();
    
    // Vytvor spravu
    Message msg;
    msg.angle = angle;
    msg.timestamp = millis();
    
    // Posli ESP-NOW
    esp_err_t result = esp_now_send(receiverMAC, (uint8_t *) &msg, sizeof(msg));
    
    if (result == ESP_OK) {
      Serial.print("  → ESP-NOW poslane: ");
      Serial.print(angle, 2);
      Serial.println("°");
    } else {
      Serial.println("  ✗ ESP-NOW chyba!");
    }
  }
  
  delay(10); // Mala prodleva medzi meraniami
}
