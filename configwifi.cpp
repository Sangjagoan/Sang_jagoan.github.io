#include "WiFiType.h"
#include <WiFi.h>
#include "configwifi.h"                                 
#include <ESPmDNS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include "nvs_flash.h"
#include <DNSServer.h>

extern DNSServer dnsServer;
extern const byte DNS_PORT = 53;

// char DEVICE_ID[32];
char AP_SSID[32];
const char* AP_PASS = "12345678";

// ===== GLOBAL =====
bool networkServicesStarted = false;

extern Preferences prefs;
extern String storedSSID;
extern String storedPASS;
extern PubSubClient mqttClient;
extern NTPClient timeClient;
extern const char* MQTT_CLIENT;

// ===== WIFI CONFIG STATE =====
bool wifiConfigMode = false;

String newSSID;
String newPASS;

String oldSSID;
String oldPASS;

uint32_t wifiStartTime = 0;

#define WIFI_TIMEOUT 15000

/* =========================================================
 * LOAD WIFI DARI NVS
 * ========================================================= */
void loadWiFiFromNVS() {
  prefs.begin("wifi", true);  // true read doang
  storedSSID = prefs.getString("ssid", "");
  storedPASS = prefs.getString("pass", "");
  prefs.end();

  Serial.println("[NVS] SSID: " + storedSSID);
}

/* =========================================================
 * SIMPAN WIFI
 * ========================================================= */
void saveWiFiToNVS(String s, String p) {
  prefs.begin("wifi", false);  // false read n write
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();

  storedSSID = s;
  storedPASS = p;

  Serial.println("[NVS] WiFi saved");

  WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
}

bool connectAndTestWiFi(String ssid, String pass) {

  Serial.println("[WiFi] Testing: " + ssid);

  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();

  while (millis() - start < 10000) {

    if (WiFi.status() == WL_CONNECTED) {

      // ⚠ cek apakah benar SSID yang dimaksud
      if (WiFi.SSID() == ssid) {

        Serial.println("[WiFi] Test OK → Saving");

        saveWiFiToNVS(ssid, pass);

        Serial.printf("WiFi Status: %d  IP: %s\n",
                      WiFi.status(),
                      WiFi.localIP().toString().c_str());

        return true;
      }
    }

    delay(200);
    Serial.print(".");
  }

  Serial.println("\n[WiFi] Test FAILED");

  WiFi.disconnect(true);

  // reconnect wifi lama
  if (storedSSID.length()) {
    WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
  }

  return false;
}

/* =========================================================
 * FACTORY RESET
 * ========================================================= */
void factoryResetWiFi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  storedSSID = "";
  storedPASS = "";

  Serial.println("[NVS] Cleared WiFi");
}

/* =========================================================
 * HEAD RESET
 * ========================================================= */
void factoryReset() {

  Serial.println("⚠️ FULL FACTORY RESET...");

  nvs_flash_erase();  // hapus semua
  nvs_flash_init();   // init ulang

  delay(500);

  Serial.println("🔄 Restarting...");

  ESP.restart();  // 🔥 WAJIB
}

/* =========================================================
 * INIT WIFI (AP selalu hidup)
 * ========================================================= */
void initWiFi() {

  // Mode ganda
  WiFi.mode(WIFI_AP_STA);

  // Start AP
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP %s, IP: %s\n", ap_ok ? "OK" : "FAILED", WiFi.softAPIP().toString().c_str());

  if (WiFi.status() != WL_CONNECTED) {
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  }
  // CONNECT STA JIKA ADA
  if (storedSSID.length()) {
    WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
    Serial.printf("Join ke STA %s", storedSSID.c_str());
  } else {
    Serial.println("MODE_AP");
    return;
  }

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }

  // CONNECT STA JIKA ADAWIFI
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] OK!");
    Serial.println(WiFi.localIP());
    MDNS.begin("EspHome_Control");
    networkServicesStarted = true;
  } else {
    Serial.println("[WiFi] TIMEOUT!");
    networkServicesStarted = false;
  }
}

/* =========================================================
 * HANDLE WIFI NON BLOCKING
 * ========================================================= */
void handleWiFi() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 3000) return;
  lastCheck = millis();

  wl_status_t st = WiFi.status();

  // ===== CONNECTED =====
  if (st == WL_CONNECTED) {
    if (!networkServicesStarted) {
      Serial.print("[WiFi] Connected IP: ");
      Serial.println(WiFi.localIP());

      startNetworkServices();

      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
      networkServicesStarted = true;
    }
    return;
  }

  // ===== LOST CONNECTION =====
  if (networkServicesStarted) {
    Serial.println("[WiFi] Disconnected");
    stopNetworkServices();
    networkServicesStarted = false;
  }

  // ===== AUTO RECONNECT =====
  static unsigned long lastReconnect = 0;
  if (millis() - lastReconnect > 10000 && storedSSID.length()) {
    Serial.println("[WiFi] Reconnect...");
    WiFi.disconnect(false, false);
    WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
    lastReconnect = millis();
  }
}

/* =========================================================
 * NETWORK SERVICES
 * ========================================================= */
void startNetworkServices() {
  Serial.println("[NET] Starting...");

  if (MDNS.begin("esp32panel"))
    Serial.println("mDNS OK");

  mqttClient.connect(MQTT_CLIENT);
  timeClient.begin();
}

void stopNetworkServices() {
  Serial.println("[NET] Stopping...");

  MDNS.end();
  mqttClient.disconnect();
  timeClient.end();
}

/* =========================================================
 * SSID
 * ========================================================= */

void initAPSSID() {
  uint64_t chipid = ESP.getEfuseMac();

  uint32_t id = (uint32_t)(chipid & 0xFFFFFFFF);  // ambil bagian bawah

  snprintf(AP_SSID, sizeof(AP_SSID), "MTQ_%08X", id);
}

/* =========================================================
 * STATUS
 * ========================================================= */
bool isWiFiReady() {
  return WiFi.status() == WL_CONNECTED;
}

String getClientIP() {
  return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0";
}