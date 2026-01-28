#ifndef CONFIGWIFI_H
#define CONFIGWIFI_H

#include <WiFi.h>

// WiFi credentials
const char* ssid = "SANG_JAGOAN";
const char* password = "Sandibaru512";

// tetapkan ip address
IPAddress staticIP(192, 168, 1, 128);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(208, 67, 222, 222);
IPAddress secondaryDNS(208, 67, 220, 220);

enum WiFiState {
  WIFI_IDLE,
  WIFI_CONNECTING,
  WIFI_CONNECTED
};

WiFiState wifiState = WIFI_IDLE;

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    wifiState = WIFI_CONNECTED;
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
    // Configuring static IP
    // WiFi.hostname(hostname);
    if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Failed to configure Static IP");
    } else {
      Serial.println("Static IP configured!");
      Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
      Serial.println("Starting firmware checking process");
    }

  } else {
    Serial.println("\nFailed to connect to WiFi. Will retry later.");
  }
}

unsigned long wifiTimer;
void handleWiFi() {
  switch (wifiState) {

    case WIFI_IDLE:
      Serial.println("[WiFi] Start connecting...");
      WiFi.begin(ssid, password);
      wifiTimer = millis();
      wifiState = WIFI_CONNECTING;
      break;

    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected!");
        Serial.println(WiFi.localIP());
        if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
          Serial.println("Failed to configure Static IP");
        } else {
          Serial.println("Static IP configured!");
          Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
        }
        wifiState = WIFI_CONNECTED;

      } else if (millis() - wifiTimer > 10000) {
        Serial.println("[WiFi] Timeout, retry...");
        WiFi.disconnect();
        wifiState = WIFI_IDLE;
      }
      break;

    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Lost connection!");
        wifiState = WIFI_IDLE;
      }

      break;
  }
}

#endif