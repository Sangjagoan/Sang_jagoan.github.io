/**********************************************************************
   ================  ESP32 PANEL CONTROLLER  ==========================
    INDUSTRIAL EDITION — NON BLOCKING — PLC STATE MACHINE
    + TELEGRAM ALARM + MQTT
 *********************************************************************/
#include <Arduino.h>
#include "Kalman1D.h"
#include <PZEM004Tv30.h>
#include <EasyUltrasonic.h>
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESP32httpUpdate.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <FS.h>
#include "SPIFFS.h"
#include "plc_controller.h"
#include "mqtt_manager.h"

#include <Preferences.h>
Preferences prefs;

String storedSSID = "";
String storedPASS = "";

// #include "webpageCode.h"
#include "secrets.h"
#include "configWifi.h"
#include "sensor_manager.h"
#include <ESPmDNS.h>
#include <DNSServer.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;

#define WDT_TIMEOUT 10  // ======= NEW: WATCHDOG TIMEOUT =======

File fsUploadFile;

char DEVICE_ID[32];
/* =========================================================
   🔹 USER CONFIG (EDIT DI SINI)
   ========================================================= */

const int MQTT_PORT = 8883;
const char* MQTT_CLIENT = "esp32_panel_controller";

/* =========================================================
   1) PIN MAPPING
   ========================================================= */

// -------- INPUTS ----------
#define PINPRESSURE 34
#define TRIGPIN 26
#define ECHOPIN 27
#define PIN_WIFI_RESET 5

// -------- OUTPUTS ----------
#define BUZZER 13

// -------- PZEM SERIAL ----------
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define _PZEM_RX_PIN 18
#define _PZEM_TX_PIN 19

/* =========================================================
   2) OTA & FIRMWARE CONFIG
   ========================================================= */

const char* github_owner = "Sangjagoan";
const char* github_repo = "ubdate-firmware-home-monitoringEsp32";
const char* firmware_asset_name = "esp32_panelMqtt.ino.bin";
const char* currentFirmwareVersion = "1.7.2";

/* =========================================================
   3) OBJECTS (GLOBAL)
   ========================================================= */
// ---- FILTER & SENSORS ----
EasyUltrasonic RCWL;
Kalman1D kalmanFilterUltrasonic(1e-5f, 1e-2f, 1e-3f, 0.0f);
Kalman1D kalmanFilterPressure;

// ---- NETWORK & TIME ----
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200);

// ---- MQTT ----
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// ---- PZEM ----
PzemData readings;
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, 0x02);
PZEM004Tv30 _pzem(Serial1, _PZEM_RX_PIN, _PZEM_TX_PIN, 0x01);

volatile bool otaInProgress = false;
/* =========================================================
   5) SYSTEM STATE (PLC STYLE)
   ========================================================= */

WifiProvisionState wifiState = WIFI_IDLE;

PressureConfig pressureCfg;

ValveState valveState = VALVE_IDLE;

MQTTMessage mqttQueue[MQTT_QUEUE_SIZE];

volatile uint16_t mqttHead = 0;
volatile uint16_t mqttTail = 0;

PLC_State plcState = SIBLE_OFF_ALL;
volatile SystemMode systemMode = MODE_STARTING;
DayNightConfig dnConfig;
LevelConfig levelCfg;

/* =========================================================
   3) OBJECTS
   ========================================================= */

volatile DebugInfo debugInfo = { 0 };

volatile bool systemHealthy = true;
volatile bool systemRunEnebleSensor = false;
volatile bool firstBootAfterOTA = false;
volatile bool sensorFresh = false;
volatile bool wifiHealthy = true;  // ← status kesehatan WiFi

bool stopkranUtaraState = false;
bool stopkranSelatanState = false;
/* =========================================================
   6) TASK HANDLES
   ========================================================= */
SemaphoreHandle_t mqttMutex;
TaskHandle_t TaskSensorHandle;
TaskHandle_t TaskPLCHandle;
TaskHandle_t TaskHealthHandle;
TaskHandle_t TaskWatchdogHandle;
TaskHandle_t TaskNTPHandle;
TaskHandle_t TaskDebugHandle;
TaskHandle_t TaskTelegramHandle;
TaskHandle_t TaskMQTTHandle;

QueueHandle_t telegramQueue;

/* =========================================================
   7) NON-BLOCKING TIMERS
   ========================================================= */
uint32_t tSensor = 0;
uint32_t tPLC = 0;
uint32_t tHealth = 0;
uint32_t lastNtpSync = 0;
uint32_t t0 = 0;
volatile bool ntpReady = false;

bool outSibleAtas = false;
bool outSibleBawah = false;
bool outKranBuka = false;
bool outKranTutup = false;
/* ======== SAFETY / WATCHDOG ======== */
volatile uint32_t lastSensorTick = 0;
volatile uint32_t lastPLCTick = 0;
volatile uint32_t lastHealthTick = 0;
uint32_t lastWsSend = 0;

uint8_t sensorErrorCount = 0;
// ===== GLOBAL TIMER =====
uint32_t pressureStartTime = 0;

/* =========================================================
   8) SIBLE STATE VARIABLES
   ========================================================= */
String otaStatus = "IDLE";
int otaProgress = 0;

String wifiStatus = "IDLE";
int wifiProgress = 0;

uint32_t stateTimer = 0;
uint32_t stateTimerNightDay = 0;
uint32_t stateTimerNight = 0;

CekSensor cekSensor;
/* =========================================================
   9) SETUP
   ========================================================= */
void setup() {

  /* ================= SERIAL ================= */
  // Serial.begin(115200);
  Serial1.begin(9600);
  Serial2.begin(9600);

  Serial.println("\n=== BOOTING SYSTEM ===");

  /* ================= HARDWARE ================= */
  detectOTABootState();
  initHardware();
  failSafeMode();

  ledcSetup(0, 2000, 8);
  ledcAttachPin(BUZZER, 0);

  /* ================= STORAGE ================= */
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS Mounted");

    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.print("FILE: ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
  }

  /* ================= WIFI ================= */
  loadWiFiFromNVS();  // hanya baca credential
  loadDayNightConfig();
  loadLevelConfig();
  loadCalisConfig();
  loadStopkranState();
  loadAlarmSensorConfig();

  initAPSSID();             // 🚨 WAJIB
  Serial.println(AP_SSID);  // debug biar yakin

  initWiFi();  // AP langsung hidup + STA auto connect (non blocking)
  loadPressureConfig();

  initDeviceID();

  /* ================= MQTT ================= */
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(1024);
  espClient.setInsecure();  // sementara bypass SSL cert
  mqttClient.setCallback(callback);

  mqttMutex = xSemaphoreCreateMutex();

  if (mqttMutex == NULL) {
    Serial.println("MQTT Mutex creation failed!");
  }

  /* ================= TELEGRAM QUEUE ================= */
  telegramQueue = xQueueCreate(5, sizeof(char) * 200);

  /* ================= TIME ================= */
  timeClient.begin();
  timeClient.setPoolServerName("pool.ntp.org");  // server NTP
  timeClient.setUpdateInterval(600000);          // 10 menit

  Serial.printf("BOOT LOAD: %d %d %d\n",
                dnConfig.mode,
                dnConfig.nightStart,
                dnConfig.nightEnd);
  /* ================= BUZZER ================= */
  tone(BUZZER, 2000);
  delay(150);
  noTone(BUZZER);

  /* ================= OTA VALIDATE ================= */
  if (firstBootAfterOTA) {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("✅ Firmware confirmed valid");
  }

  /* ================= SYSTEM FLAGS ================= */
  pressureStartTime = millis();
  systemRunEnebleSensor = true;

  /* ================= SERVICES ================= */
  startServices();  // start webserver, websocket, dll
  sendTelegram(String(DEVICE_ID) + " ONLINE");
  Serial.println("=== SYSTEM READY (NON BLOCKING) ===");
}

/* =========================================================
   10) LOOP (NETWORKING ONLY)
   ========================================================= */
void loop() {

  if (otaInProgress) {
    server.handleClient();
    yield();
    return;
  }

  handleWiFi();
  server.handleClient();
  dnsServer.processNextRequest();
  buzzer();

  handleWifiResetButton();
  handleSerialWifiReset();

  if (systemRunEnebleSensor) {
    SensorDataUltrasonic level = getUltrasonicSnapshot();
    if (WiFi.status() != WL_CONNECTED) {
      setBuzzerTone(TONE_WIFI_LOST);
    } else if (level.percentage < 40) {
      setBuzzerTone(TONE_ULTRASONIC);
    } else {
      setBuzzerTone(TONE_OFF);
    }
  }

  // DEBUG STACK MONITOR
  // printTaskStack();
  serialMonitor_TABULAR();
}

// REKOMENDASI INDUSTRIAL RATE
// sensor read     = 1000 ms
// MQTT publish    = 2000–5000 ms
// heartbeat       = 5000 ms

/* =========================================================
   11) TASKS
   ========================================================= */
void Task_Sensor(void* pvParameters) {
  static uint32_t lastPrint = 0;
  for (;;) {
    uint32_t now = millis();

    if (now - tSensor >= 1000) {  // sensor     1 sec
      tSensor = now;
      ultrasonic();
      pressure();
      PZEM();
      sensorFresh = true;
      lastSensorTick = millis();  // WATCHDOG
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Task_PLC(void* pvParameters) {
  static uint32_t lastPrint = 0;
  for (;;) {
    uint32_t now = millis();

    if (now - tPLC >= 50) {  // 20 Hz
      tPLC = now;

      if (systemRunEnebleSensor) {
        plcStateMachine();
        updateOutputs();
      }

      lastPLCTick = millis();  // WATCHDOG
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Task_Health(void* pvParameters) {
  for (;;) {

    if (!systemHealthy) {
      plcState = PLC_ERROR;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // tunggu sensor update
    if (!sensorFresh) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    sensorFresh = false;

    lastHealthTick = millis();  // WATCHDOG

    systemRunEnebleSensor = checkAllSensors();
    if (!systemRunEnebleSensor) {
      sensorErrorCount = min(sensorErrorCount + 1, 10);
    } else {
      sensorErrorCount = 0;
    }

    if (sensorErrorCount >= 5 && systemHealthy) {
      systemHealthy = false;
      plcState = PLC_ERROR;
      sendTelegram("systemHealthy cek sensor");
      failSafeMode();
      // Serial.println("🚨 SYSTEM ENTERING ERROR STATE!");
    }
    // Serial.println(sensorErrorCount);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/* =========================================================
   TASK WATCHDOG
   ========================================================= */
void Task_Watchdog(void* pvParameters) {
  static bool sensorFault = false;
  static uint32_t lastPrint = 0;
  for (;;) {
    uint32_t now = millis();

    if (!sensorFault && lastSensorTick > 0 && isTimeout(lastSensorTick, 3000)) {
      Serial.println("🚨 WATCHDOG: SENSOR TASK HANG!");
      systemHealthy = false;
      plcState = PLC_ERROR;
      sensorFault = true;
    }

    if (!sensorFault && lastPLCTick > 0 && isTimeout(lastPLCTick, 2000)) {
      Serial.println("🚨 WATCHDOG: PLC TASK HANG!");
      systemHealthy = false;
      plcState = PLC_ERROR;
      sensorFault = true;
    }

    if (!sensorFault && lastHealthTick > 0 && isTimeout(lastHealthTick, 3000)) {
      Serial.println("🚨 WATCHDOG: HEALTH TASK HANG!");
      systemHealthy = false;
      plcState = PLC_ERROR;
      sensorFault = true;
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/* =========================================================
fungsi helper WATCHDOG
   ========================================================= */
bool isTimeout(uint32_t last, uint32_t timeout) {
  return (uint32_t)(millis() - last) > timeout;
}

void Task_NTP(void* pvParameters) {
  for (;;) {
    if (!isWiFiReady()) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    // ===== FIRST SYNC =====
    if (!timeClient.isTimeSet()) {

      Serial.println("[NTP] First sync...");

      if (timeClient.forceUpdate()) {

        Serial.println("[NTP] OK");

        lastNtpSync = millis();
        ntpReady = true;

        systemMode = calculateDayNight(timeClient.getHours());
      }

      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    // ===== PERIODIC SYNC (10 menit) =====
    if (millis() - lastNtpSync > 600000) {
      Serial.println("[NTP] Periodic sync");

      if (timeClient.forceUpdate()) {
        Serial.println("[NTP] OK");
        lastNtpSync = millis();

        // update mode setelah sync
        systemMode = calculateDayNight(timeClient.getHours());
      }
    }

    // ===== UPDATE MODE TANPA SYNC (tiap 1 menit) =====
    static uint32_t lastModeCheck = 0;
    if (millis() - lastModeCheck > 60000) {
      lastModeCheck = millis();
      systemMode = calculateDayNight(timeClient.getHours());
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

/* =========================================================
   TASK MQTT
   ========================================================= */
void Task_MQTT(void* pv) {
  static uint32_t lastData = 0;
  static uint32_t lastWifi = 0;

  for (;;) {
    mqttMaintain();

    if (mqttClient.connected()) {
      mqttClient.loop();

      // kirim backlog queue
      mqttQueueProcess();

      publishHeartbeat();

      publishIndikatorLed();

      if (millis() - lastWifi > 10000) {
        publishWifiStatus();
        lastWifi = millis();
      }

      // publish data tiap 5 detik
      if (millis() - lastData > 5000) {
        publishMQTTData();
        lastData = millis();
      }

      publishUptime();
      publishState();
      publishPumpState();
      publishCalisState();
      publishValveState();
      publishAlarmSensor();
      publishPressureState();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void Task_Debug(void* pv) {
  for (;;) {
    debugInfo.sensor = uxTaskGetStackHighWaterMark(TaskSensorHandle);
    debugInfo.plc = uxTaskGetStackHighWaterMark(TaskPLCHandle);
    debugInfo.health = uxTaskGetStackHighWaterMark(TaskHealthHandle);
    debugInfo.wdt = uxTaskGetStackHighWaterMark(TaskWatchdogHandle);
    debugInfo.ntp = uxTaskGetStackHighWaterMark(TaskNTPHandle);
    debugInfo.mqtt = uxTaskGetStackHighWaterMark(TaskMQTTHandle);
    debugInfo.tel = uxTaskGetStackHighWaterMark(TaskTelegramHandle);
    debugInfo.debug = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelay(pdMS_TO_TICKS(3000));  // tiap 3 detik (aman)
  }
}

void Task_Telegram(void* pv) {

  WiFiClientSecure client;
  client.setInsecure();

  char msg[200];

  for (;;) {

    if (xQueueReceive(telegramQueue, &msg, portMAX_DELAY)) {

      if (WiFi.status() != WL_CONNECTED)
        continue;

      Serial.println("Sending Telegram...");

      char request[300];

      snprintf(request, sizeof(request),
               "GET /bot%s/sendMessage?chat_id=%s&text=%s HTTP/1.1\r\n"
               "Host: api.telegram.org\r\n"
               "User-Agent: ESP32\r\n"
               "Connection: close\r\n\r\n",
               TELEGRAM_BOT_TOKEN,
               TELEGRAM_CHAT_ID,
               msg);

      if (client.connect("api.telegram.org", 443)) {

        Serial.println("Telegram Connected");

        delay(50);

        client.print(request);

        while (client.connected()) {
          while (client.available()) {
            client.read();
          }
        }

        client.stop();
      }

      vTaskDelay(pdMS_TO_TICKS(1500));
    }
  }
}

/* =========================================================
   12) START SERVICES
   ========================================================= */

void startServices() {

  // ===== REALTIME CORE (1) =====
  xTaskCreatePinnedToCore(Task_Telegram, "Telegram", 8192, NULL, 1, &TaskTelegramHandle, 1);
  xTaskCreatePinnedToCore(Task_Watchdog, "WatchdogTask", 2048, NULL, 4, &TaskWatchdogHandle, 1);
  xTaskCreatePinnedToCore(Task_NTP, "NTPTask", 2048, NULL, 1, &TaskNTPHandle, 1);
  xTaskCreatePinnedToCore(Task_MQTT, "MQTTTask", 6144, NULL, 1, &TaskMQTTHandle, 1);
  // ===== SERVICE CORE (0) =====
  xTaskCreatePinnedToCore(Task_PLC, "PLCTask", 3360, NULL, 3, &TaskPLCHandle, 0);
  xTaskCreatePinnedToCore(Task_Sensor, "SensorTask", 2976, NULL, 2, &TaskSensorHandle, 0);
  xTaskCreatePinnedToCore(Task_Health, "HealthTask", 2976, NULL, 1, &TaskHealthHandle, 0);
  xTaskCreatePinnedToCore(Task_Debug, "DebugTask", 2976, NULL, 1, &TaskDebugHandle, 0);
  // sisa ram per task
  /* =========================================================
 Serial.printf("[STACK] Sensor: %u\n", uxTaspress.kgetStackHighWaterMark(NULL)); // cetak sisa ram
  2000 byte | Sangat aman
  1000 – 2000 | Aman
  400 – 1000 | Mulai bahaya
  200 – 400 | Akan crash cepat
  < 200 | Tinggal tunggu reset
   ========================================================= */
  server.on("/api/pressure", HTTP_GET, []() {
    char payload[192];

    int len = buildPressureJSON(payload, sizeof(payload));

    if (len <= 0) {
      server.send(500, "text/plain", "JSON error");
      return;
    }

    server.send(200, "application/json", payload);
  });

  server.on("/api/pressure", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "text/plain", "Bad Request");
      return;
    }

    StaticJsonDocument<256> doc;
    deserializeJson(doc, server.arg("plain"));

    updatePressureConfig(doc);

    server.send(200, "text/plain", "OK");
  });

  server.on(
    "/update/firmware", HTTP_POST,
    []() {
      // 🔥 JANGAN restart di sini
      server.send(200, "text/plain", "OK");
    },
    handleFirmwareUpload);

  //===================Upload File Spiffs===============================
  server.on(
    "/upload/spiffs", HTTP_POST, []() {
      server.send(200, "text/plain", "OK");
    },
    handleWebFileUpload);

  server.on("/list", HTTP_GET, []() {
    String output = "";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    while (file) {
      output += file.name();
      output += "\n";
      file = root.openNextFile();
    }

    server.send(200, "text/plain", output);
  });

  //===================Ota Github===============================
  server.on("/ota", HTTP_GET, handleOTA);

  server.on("/ota/status", HTTP_GET, []() {
    String json = "{";
    json += "\"status\":\"" + otaStatus + "\",";
    json += "\"progress\":" + String(otaProgress);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/version", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"version\":\"" + String(currentFirmwareVersion) + "\"}");
  });

  server.on("/system", HTTP_GET, []() {
    server.sendHeader("Location", "/system.html", true);
    server.send(302, "text/plain", "");
  });

  server.on("/settings", HTTP_GET, []() {
    server.sendHeader("Location", "/settings.html", true);
    server.send(302, "text/plain", "");
  });

  server.on("/reset/esp", HTTP_POST, []() {
    prepareForOTA();
    ESP.restart();
    server.send(200, "text", "OK");
  });

  //==========================Wifi=================================//
  server.on("/wifi/save", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      wifiStatus = "BAD_REQUEST";
      wifiProgress = 0;
      server.send(400, "text/plain", "Bad Request");
      return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    deserializeJson(doc, body);

    String newSSID = doc["ssid"].as<String>();
    String newPASS = doc["password"].as<String>();

    if (newSSID.length() == 0) {
      wifiStatus = "SSID_EMPTY";
      wifiProgress = 0;
      server.send(400, "text/plain", "SSID empty");
      return;
    }

    Serial.println("[WiFi] Testing: " + newSSID);
    wifiStatus = "CONNECTING";
    wifiProgress = 10;

    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(newSSID.c_str(), newPASS.c_str());

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      Serial.print(".");

      // 🔹 UPDATE PROGRESS
      wifiProgress += 5;
      if (wifiProgress > 80) wifiProgress = 80;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Test OK → Saving to NVS");

      wifiStatus = "SAVING";
      wifiProgress = 90;

      saveWiFiToNVS(newSSID, newPASS);

      wifiStatus = "RESTART";
      wifiProgress = 100;

      server.send(200, "text/plain", "OK_CONNECTED");
      vTaskDelay(50 / portTICK_PERIOD_MS);
      ESP.restart();
    } else {
      Serial.println("[WiFi] Test FAILED → NOT SAVED");

      wifiStatus = "FAILED";
      wifiProgress = 0;

      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      server.send(400, "text/plain", "CONNECT_FAILED");
    }
  });

  server.on("/wifi/reset", HTTP_POST, []() {
    server.send(200, "text/plain", "WIFI_RESET_OK");
    delay(300);
    factoryResetWiFi();
    ESP.restart();
  });

  server.on("/wifi/status", HTTP_GET, []() {
    char payload[200];

    buildWifiStatus(payload, sizeof(payload));

    server.send(200, "application/json", payload);
  });

  server.on("/reset/pzem", HTTP_POST, []() {
    // ===== KONFIRMASI WAJIB =====
    if (server.header("X-Confirm") != "YES") {
      server.send(403, "text/plain", "CONFIRM_REQUIRED");
      return;
    }

    bool r1 = pzem.resetEnergy();
    bool r2 = _pzem.resetEnergy();

    Serial.printf("Reset PZEM1: %d\n", r1);
    Serial.printf("Reset PZEM2: %d\n", r2);

    if (r1 || r2) {
      server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "FAIL");
    }
  });

  // Endpoint GET Config
  server.on("/api/daynight", HTTP_GET, []() {
    DynamicJsonDocument doc(256);

    doc["mode"] = dnConfig.mode;
    doc["nightStart"] = dnConfig.nightStart;
    doc["nightEnd"] = dnConfig.nightEnd;

    int currentHour = timeClient.getHours();
    SystemMode currentState = calculateDayNight(currentHour);

    doc["currentState"] = stateTime(systemMode);
    doc["plc_State"] = stateName(plcState);

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  // Endpoint POST Config
  server.on("/api/daynight", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "text/plain", "Bad Request");
      return;
    }

    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));

    dnConfig.mode = (ModeType)doc["mode"];
    dnConfig.nightStart = doc["nightStart"];
    dnConfig.nightEnd = doc["nightEnd"];

    saveDayNightConfig();

    systemMode = calculateDayNight(timeClient.getHours());

    server.send(200, "text/plain", "OK");
  });

  server.on("/generate_204", []() {
    server.send(200, "text/plain", "OK");
  });

  server.on("/hotspot-detect.html", []() {
    server.send(200, "text/html", "<h1>ESP32 CONFIG</h1>");
  });

  server.on("/connecttest.txt", []() {
    server.send(200, "text/plain", "OK");
  });

  // server.onNotFound([]() {
  //   // 🔥 kalau masih AP mode → paksa captive portal
  //   if (WiFi.status() != WL_CONNECTED) {
  //     server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
  //     server.send(302, "text/plain", "");
  //     return;
  //   }

  //   // 🔥 kalau sudah connect → normal SPA
  //   String path = server.uri();

  //   if (SPIFFS.exists(path)) {
  //     File file = SPIFFS.open(path, FILE_READ);
  //     server.streamFile(file, getContentType(path));
  //     file.close();
  //     return;
  //   }

  //   // fallback SPA
  //   File file = SPIFFS.open("/settings.html", FILE_READ);
  //   server.streamFile(file, "text/html");
  //   file.close();
  // });

  server.onNotFound([]() {
    String path = server.uri();

    // 🔥 kalau file ada → serve dulu
    if (SPIFFS.exists(path)) {
      File file = SPIFFS.open(path, FILE_READ);
      server.streamFile(file, getContentType(path));
      file.close();
      return;
    }

    // 🔥 AP MODE → fallback ke index / settings
    if (WiFi.status() != WL_CONNECTED) {
      File file = SPIFFS.open("/settings.html", FILE_READ);
      server.streamFile(file, "text/html");
      file.close();
      return;
    }

    // 🔥 normal SPA (wifi connected)
    File file = SPIFFS.open("/settings.html", FILE_READ);
    server.streamFile(file, "text/html");
    file.close();
  });

  //============================web===========================//
  server.on("/data", HTTP_GET, handleData);
  server.on("/wifi/info", handleWifiInfo);
  server.on("/wifi/scan", handleWifiScan);
  server.begin();
}

/* =========================================================
   14) HANDLE STOPKRAN
   ========================================================= */
void loadStopkranState() {

  prefs.begin("valve", true);

  stopkranUtaraState = prefs.getBool("utara", false);
  stopkranSelatanState = prefs.getBool("selatan", false);

  prefs.end();

  digitalWrite(PINSTOPKRANUTARA, stopkranUtaraState ? HIGH : LOW);
  digitalWrite(PINSTOPKRANSELATAN, stopkranSelatanState ? HIGH : LOW);

  Serial.println("Stopkran state loaded");
}

/* =========================================================
   14) HANDLE STOPKRAN variable
   ========================================================= */

void initDeviceID() {
  uint64_t chipid = ESP.getEfuseMac();

  snprintf(DEVICE_ID, sizeof(DEVICE_ID),
           "ESP32_%04X%08X",
           (uint16_t)(chipid >> 32),
           (uint32_t)chipid);
}

/* =========================================================
   16) FAIL SAFE MODE
   ========================================================= */
void failSafeMode() {
  digitalWrite(PINSIBLEATAS, LOW);
  digitalWrite(PINSIBLEBAWAH, LOW);
  digitalWrite(PINTUTUPSTOPKRAN, LOW);
  digitalWrite(PINBUKASTOPKRAN, LOW);
  buzzer();
}

/* =========================================================
   17) BUZZER (NON BLOCKING)
   ========================================================= */

uint32_t waktuBuzzer = 0;
bool buzzerState = false;
int buzzerFreq = 0;

void buzzer() {
  static uint32_t lastBeep = 0;
  static bool beepOn = false;

  // ===== MODE KHUSUS: WIFI LOST (beep setiap 3 detik) =====
  if (buzzerFreq == TONE_WIFI_LOST) {

    // Mulai beep setiap 3 detik
    if (!beepOn && millis() - lastBeep >= 3000) {
      lastBeep = millis();
      beepOn = true;
    }

    // Bunyi selama 200 ms
    if (beepOn && millis() - lastBeep <= 200) {
      tone(BUZZER, buzzerFreq);
    } else {
      noTone(BUZZER);
      beepOn = false;
    }
    return;  // penting supaya tidak masuk pola default
  }

  // ===== MODE NORMAL (semua tone lain) =====
  if (millis() - waktuBuzzer >= 300) {
    waktuBuzzer = millis();
    buzzerState = !buzzerState;

    if (buzzerState && buzzerFreq > 0) {
      tone(BUZZER, buzzerFreq);
    } else {
      noTone(BUZZER);
    }
  }
}

void setBuzzerTone(int freq) {
  buzzerFreq = freq;  // simpan nada yang akan dibunyikan
}

/* =========================================================
   19) HANDLE UBDATE OTA DITHUB
   ========================================================= */
bool checkIfUpdateAvailable() {

  Serial.println("Checking OTA...");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.github.com/repos/" + String(github_owner) + "/" + String(github_repo) + "/releases/latest";

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  http.setTimeout(15000);
  http.addHeader("User-Agent", "ESP32");
  http.addHeader("Accept", "application/vnd.github.v3+json");
  http.addHeader("Authorization", "token " + String(github_pat));

  int httpCode = http.GET();

  Serial.printf("HTTP CODE: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  Serial.println("Parsing JSON...");

  // ===== FILTER JSON (hemat RAM) =====
  StaticJsonDocument<128> filter;
  filter["tag_name"] = true;

  // ===== JSON DOCUMENT =====
  DynamicJsonDocument doc(512);

  DeserializationError err =
    deserializeJson(doc,
                    http.getStream(),
                    DeserializationOption::Filter(filter));

  http.end();

  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  String latestVersion = doc["tag_name"].as<String>();

  Serial.println("Current : " + String(currentFirmwareVersion));
  Serial.println("Latest  : " + latestVersion);

  if (latestVersion.length() == 0) {
    Serial.println("Invalid latest version");
    return false;
  }

  bool result = isNewerVersion(latestVersion, currentFirmwareVersion);

  Serial.print("UPDATE AVAILABLE: ");
  Serial.println(result);

  return result;
}

void handleOTA() {

  Serial.println("=== OTA ENDPOINT HIT ===");

  otaInProgress = true;  // 🔴 BLOK MQTT

  if (!checkIfUpdateAvailable()) {

    otaInProgress = false;
    server.send(200, "text/plain", "UP_TO_DATE");
    return;
  }

  server.send(200, "text/plain", "START_OTA");

  checkForFirmwareUpdate();
}

/* =========================================================
   19) HANDLE UBDATE FIRMWARE MANUAL
   ========================================================= */
void handleFirmwareUpload() {

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    Serial.printf("Firmware Update: %s\n", upload.filename.c_str());

    // 🔥 MATIKAN SEMUA DI AWAL
    failSafeMode();

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {

    prepareForOUbdateFirmware();

    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_END) {

    if (Update.end(true)) {

      Serial.printf("Update Success: %u bytes\n", upload.totalSize);

      // 🔥 MATIKAN LAGI (double safety)
      failSafeMode();

      delay(500);

      Serial.println("Rebooting after OTA...");
      ESP.restart();  // ✅ PINDAH KE SINI

    } else {
      Update.printError(Serial);
    }
  }
}

/* =========================================================
   19) UBLOAD FILE SPIFFS
   ========================================================= */
void handleWebFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    String filename = upload.filename;

    if (!filename.startsWith("/"))
      filename = "/" + filename;

    Serial.println("Upload Start: " + filename);

    fsUploadFile = SPIFFS.open(filename, FILE_WRITE);

  } else if (upload.status == UPLOAD_FILE_WRITE) {

    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }

  } else if (upload.status == UPLOAD_FILE_END) {

    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.println("Upload Success");
    }
  }
}

String getContentType(String filename) {

  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg")) return "image/jpeg";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".svg")) return "image/svg+xml";
  if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

/* =========================================================
   19) HANDLE SYSTEM MONITORING
   ========================================================= */
void handleData() {

  char buffer[1024];

  int len = buildSystemJSON(buffer, sizeof(buffer));

  if (len <= 0) {
    server.send(500, "text/plain", "error");
    return;
  }

  server.send(200, "application/json", buffer);
}

/* =========================================================
   21) TELEGRAM ALERT
   ========================================================= */

void sendTelegram(const String& message) {

  static uint32_t lastTelegramTime = 0;
  static String lastMessage = "";

  const uint32_t TELEGRAM_COOLDOWN = 15000;  // 15 detik

  if (!telegramQueue || !isWiFiReady())
    return;

  uint32_t now = millis();

  // jika pesan sama DAN belum lewat cooldown → skip
  if (message == lastMessage && (now - lastTelegramTime) < TELEGRAM_COOLDOWN)
    return;

  lastTelegramTime = now;
  lastMessage = message;

  char msg[200];
  message.substring(0, 199).toCharArray(msg, sizeof(msg));

  xQueueSend(telegramQueue, msg, 0);
}

/* =========================================================
   22) OTA STATE CHECK
   ========================================================= */

void detectOTABootState() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;

  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    firstBootAfterOTA = true;
  }
}

/* =========================================================
   23) HARDWARE INIT
   ========================================================= */

void initHardware() {
  kalmanFilterUltrasonic.setProcessNoise(2e-4f);      // Q
  kalmanFilterUltrasonic.setMeasurementNoise(0.08f);  // R
  kalmanFilterUltrasonic.setEstimatedError(1.0f);     // P

  RCWL.attach(TRIGPIN, ECHOPIN);
  pinMode(PIN_WIFI_RESET, INPUT_PULLUP);
  pinMode(PINBUKASTOPKRAN, OUTPUT);
  pinMode(PINTUTUPSTOPKRAN, OUTPUT);
  pinMode(PINSTOPKRANUTARA, OUTPUT);
  pinMode(PINSTOPKRANSELATAN, OUTPUT);
  pinMode(PINSIBLEATAS, OUTPUT);
  pinMode(PINSIBLEBAWAH, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(PINSTOPKRANUTARA, LOW);
  digitalWrite(PINSTOPKRANSELATAN, LOW);
}

/* =========================================================
   24) OTA UPDATE (TIDAK DIUBAH, HANYA AMAN TASK)
   ========================================================= */
void prepareForOTA() {

  Serial.println("🟡 PREPARING FOR OTA");

  systemRunEnebleSensor = false;
  systemHealthy = false;
  plcState = PLC_ERROR;

  mqttClient.disconnect();
  espClient.stop();

  vTaskSuspend(TaskSensorHandle);
  vTaskSuspend(TaskPLCHandle);
  vTaskSuspend(TaskHealthHandle);
  vTaskSuspend(TaskWatchdogHandle);
  vTaskSuspend(TaskNTPHandle);
  vTaskSuspend(TaskDebugHandle);
  vTaskSuspend(TaskTelegramHandle);
  vTaskSuspend(TaskMQTTHandle);

  lastSensorTick = millis();
  lastPLCTick = millis();
  lastHealthTick = millis();
  failSafeMode();

  WiFi.setSleep(false);
}

void prepareForOUbdateFirmware() {
}
/* =========================================================
   UBDATE FIRMWARE
   ========================================================= */

void checkForFirmwareUpdate() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping update check.");
    return;
  }

  // 🔴 FIX UTAMA: stop MQTT TLS dulu
  mqttClient.disconnect();
  espClient.stop();
  delay(200);

  String apiUrl =
    "https://api.github.com/repos/" + String(github_owner) + "/" + String(github_repo) + "/releases/latest";

  Serial.println("---------------------------------");
  Serial.println("Checking for new firmware...");
  Serial.println("Fetching release info from: " + apiUrl);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, apiUrl)) {
    Serial.println("HTTP begin failed");
    return;
  }

  http.setTimeout(15000);
  http.addHeader("Authorization", "token " + String(github_pat));
  http.addHeader("Accept", "application/vnd.github.v3+json");
  http.addHeader("User-Agent", "ESP32");

  Serial.println("Sending API request...");

  int httpCode = http.GET();

  Serial.print("HTTP CODE: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return;
  }

  Serial.printf("API request successful (HTTP %d). Parsing JSON.\n", httpCode);

  // 2. Create a Filter to ignore unnecessary data
  // We ONLY want tag_name and the assets list. This saves massive RAM.
  StaticJsonDocument<512> filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["id"] = true;

  // 3. Use DynamicJsonDocument with more memory (16KB)
  // The GitHub response is large, even with filtering, we need buffer space.
  DynamicJsonDocument doc(16384);

  // 4. Deserialize with the filter
  DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

  // Note: We do NOT call http.end() yet because we might need the stream,
  // but usually deserializeJson consumes it. We will close it after logic.

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    http.end();
    return;
  }

  String latestVersion = doc["tag_name"].as<String>();
  if (latestVersion.isEmpty() || latestVersion == "null") {
    Serial.println("Could not find 'tag_name' in JSON response.");
    http.end();
    return;
  }

  Serial.println("Current Version: " + String(currentFirmwareVersion));
  Serial.println("Latest Version:  " + latestVersion);

  if (isNewerVersion(latestVersion, currentFirmwareVersion)) {

    Serial.println(">>> New firmware available! <<<");

    Serial.println(">>>looping stop <<<");
    Serial.println("Searching for asset named: " + String(firmware_asset_name));
    String firmwareUrl = "";

    // Iterate through assets
    JsonArray assets = doc["assets"].as<JsonArray>();

    for (JsonObject asset : assets) {
      String assetName = asset["name"].as<String>();
      Serial.println("Found asset: " + assetName);

      if (assetName == String(firmware_asset_name)) {
        String assetId = asset["id"].as<String>();
        // Construct the direct download URL for the asset
        firmwareUrl = "https://api.github.com/repos/" + String(github_owner) + "/" + String(github_repo) + "/releases/assets/" + assetId;
        Serial.println("Found matching asset! Preparing to download.");
        break;
      }
    }

    // Close the JSON connection before starting the download connection
    http.end();

    if (firmwareUrl.isEmpty()) {
      Serial.println("Error: Could not find the specified firmware asset in the release.");
      return;
    }
    // Start the download
    prepareForOTA();
    downloadAndApplyFirmware(firmwareUrl);

  } else {
    Serial.println("Device is up to date. No update needed.");
    http.end();
  }
  Serial.println("---------------------------------");
}

void downloadAndApplyFirmware(String url) {
  HTTPClient https;
  https.setReuse(true);
  Serial.println("Starting firmware download from: " + url);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.setUserAgent("ESP32-OTA-Client");
  https.begin(url);
  https.addHeader("Accept", "application/octet-stream");
  https.addHeader("Authorization", "token " + String(github_pat));

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Download failed, HTTP code: %d\n", httpCode);
    otaStatus = "ERROR";
    https.end();
    return;
  }

  int contentLength = https.getSize();
  if (contentLength <= 0) {
    Serial.println("Error: Invalid content length.");
    otaStatus = "ERROR";
    https.end();
    return;
  }

  // === INISIALISASI OTA (PENTING UNTUK PROGRESS BAR) ===
  otaStatus = "CHECKING";

  if (!Update.begin(contentLength)) {
    Serial.printf("Update begin failed: %s\n", Update.errorString());
    otaStatus = "ERROR";
    https.end();
    return;
  }

  Serial.println("Writing firmware...");
  otaStatus = "DOWNLOADING";

  WiFiClient* stream = https.getStreamPtr();
  uint8_t buff[1024];
  size_t totalWritten = 0;
  int lastProgress = -1;

  while (totalWritten < contentLength) {
    server.handleClient();  // 🔹 Biar web tetap responsif
    yield();                // 🔹 Jaga WiFi tetap hidup

    int available = stream->available();
    if (available > 0) {
      int readLen = stream->read(buff, min((size_t)available, sizeof(buff)));

      if (readLen < 0) {
        Serial.println("Error reading from stream");
        Update.abort();
        otaStatus = "ERROR";
        https.end();
        return;
      }

      if (Update.write(buff, readLen) != readLen) {
        Serial.printf("Update.write failed: %s\n", Update.errorString());
        Update.abort();
        otaStatus = "ERROR";
        https.end();
        return;
      }

      totalWritten += readLen;

      // === UPDATE PROGRESS BAR (SANGAT PENTING) ===
      int progress = (int)((totalWritten * 100L) / contentLength);
      otaProgress = progress;
      otaStatus = "FLASHING";  // ✅ Status yang dibaca web

      if (progress > lastProgress && (progress % 5 == 0 || progress == 100)) {
        Serial.printf("Progress: %d%%\n", progress);
        lastProgress = progress;
      }
    }
  }

  Serial.println("\nDownload selesai.");
  otaStatus = "FLASHING DONE";  // ✅ Status yang dibaca web
  if (totalWritten != contentLength) {
    Serial.printf("Error: Write incomplete (%d / %d bytes)\n",
                  totalWritten, contentLength);
    Update.abort();
    otaStatus = "ERROR";
  } else if (!Update.end()) {
    Serial.printf("Update end failed: %s\n", Update.errorString());
    otaStatus = "ERROR";
  } else {
    Serial.println("Update complete! Restarting...");
    otaStatus = "REBOOT";  // ✅ Web akan menampilkan "Rebooting..."
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ESP.restart();
  }

  https.end();
}

/* =========================================================
   27) UTILITY
   ========================================================= */
bool isNewerVersion(String latest, String current) {

  latest.replace("v", "");
  current.replace("v", "");

  int l1, l2, l3;
  int c1, c2, c3;

  sscanf(latest.c_str(), "%d.%d.%d", &l1, &l2, &l3);
  sscanf(current.c_str(), "%d.%d.%d", &c1, &c2, &c3);

  if (l1 != c1) return l1 > c1;
  if (l2 != c2) return l2 > c2;
  return l3 > c3;
}

/* =========================================================
   28) handleWifiResetButton
   ========================================================= */

void handleWifiResetButton() {
  static uint32_t pressStart = 0;
  static bool pressed = false;

  if (digitalRead(PIN_WIFI_RESET) == LOW) {  // tombol ditekan
    if (!pressed) {
      pressStart = millis();
      pressed = true;
    }

    // Tahan 3 detik untuk reset (anti salah tekan)
    if (millis() - pressStart > 3000) {
      Serial.println("[BUTTON] WiFi Factory Reset!");
      factoryResetWiFi();
      ESP.restart();
    }
  } else {
    pressed = false;
  }
}

void handleSerialWifiReset() {

  static String serialBuffer = "";

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      serialBuffer.trim();

      if (serialBuffer.equalsIgnoreCase("WIFIRESET")) {
        Serial.println("\n[CMD] WiFi FACTORY RESET via SERIAL");
        Serial.println("[CMD] ESP will restart...");

        tone(BUZZER, 2600);
        delay(200);
        noTone(BUZZER);

        factoryResetWiFi();  // fungsi yang sudah ada di configWifi.h
      }

      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

/* =========================================================
   29) SERIAL MONITOR
   ========================================================= */

static uint32_t last = 0;
void serialMonitor_TABULAR() {

  SensorDataUltrasonic level = getUltrasonicSnapshot();
  if (millis() - last < 2000) return;  // tiap 5 detik saja
  last = millis();

  // Serial.println(stateName(plcState));
  Serial.println(stateTime(systemMode));
  // Serial.println(stateWifiMode(wifiMode));
  Serial.printf("MQTT Queue: %u\n", mqttQueueCount());

  // Serial.println("===============================================================");
  // Serial.println("|                ===== SYSTEM MONITOR =====                  |");
  // Serial.println("===============================================================");

  // Serial.printf("| %-20s : %8.1f V | %-8s : %6.2f A |\n",
  //               "Voltage", readings.voltage, "Current", readings.current);
  // Serial.printf("| %-20s : %8.1f W | %-8s : %6.3f kWh |\n",
  //               "Power", readings.power, "Energy", readings.energy);
  // Serial.printf("| %-20s : %8.1f Hz | %-8s : %6.2f |\n",
  //               "Frequency", readings.frequency, "PF", readings.pf);

  // Serial.println("---------------------------------------------------------------");

  // Serial.printf("| %-20s : %8.1f V | %-8s : %6.2f A |\n",
  //               "_Voltage", readings._voltage, "_Current", readings._current);
  // Serial.printf("| %-20s : %8.1f W | %-8s : %6.3f kWh |\n",
  //               "_Power", readings._power, "_Energy", readings._energy);
  // Serial.printf("| %-20s : %8.1f Hz | %-8s : %6.2f |\n",
  //               "_Frequency", readings._frequency, "_PF", readings._pf);

  // Serial.println("---------------------------------------------------------------");

  // Serial.printf("| %-20s : %8.2f |\n", "Pressure (press.kg)", press.kg);
  // Serial.printf("| %-20s : %8.2f |\n", "Pressure (press.psi)", press.psi);
  // Serial.printf("| %-20s : %8.4f |\n", "ADC", press.pressureValueAdc);
  // Serial.printf("| %-20s : %8.4f |\n", "press.pressureValue", press.pressureValue);
  // Serial.printf("| %-20s : %8.4f |\n", "press.pressureValue", press.pressureValue);

  // Serial.println("---------------------------------------------------------------");

  Serial.printf("| %-20s : %6.2f % |\n", "kalman", level.cm);
  Serial.printf("| %-20s : %6.2f level.cm |\n", "Percent", level.percentage);
  Serial.printf("| %-20s : %6.2f level.cm |\n", "level.tinggiAir", level.tinggiAir);
  Serial.printf("| %-20s : %6.2f |\n", "level.non_kalman", level.non_kalman);
  Serial.println("---------------------------------------------------------------");

  // Serial.printf("| %-20s : %d |\n", "networkServicesStarted", networkServicesStarted);
  // Serial.printf("| %-20s : %d |\n", "sensorErrorCount", sensorErrorCount);
  // Serial.printf("| %-20s : %d |\n", "systemRunEnebleSensor", systemRunEnebleSensor);
  // Serial.printf("| %-20s : %d |\n", "networkServicesStarted", networkServicesStarted);

  // Serial.printf("| %-20s : %d |\n", "systemHealthy", systemHealthy);

  // Serial.println("===============================================================");
  // Serial.println("Akses Web:");
  // Serial.println("→ http://EspHome_Control.local");
  // Serial.println("===============================================================");
  // Serial.println(WiFi.RSSI());
}

String getTimeString() {

  if (!isWiFiReady())
    return "NO WIFI";

  if (!timeClient.isTimeSet())
    return "SYNC...";

  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  int s = timeClient.getSeconds();

  char buf[16];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

void printTaskStack() {

  static uint32_t last = 0;
  if ((uint32_t)(millis() - last) < 2000) return;
  last = millis();

  // Serial.println("\n=========== SYSTEM DEBUG ===========");
  // Serial.print("Time : ");
  // Serial.println(getTimeString());

  // Serial.printf("Sensor : %u\n", uxTaspress.kgetStackHighWaterMark(TaskSensorHandle));
  // Serial.printf("PLC    : %u\n", uxTaspress.kgetStackHighWaterMark(TaskPLCHandle));
  // Serial.printf("Health : %u\n", uxTaspress.kgetStackHighWaterMark(TaskHealthHandle));
  // Serial.printf("WDT    : %u\n", uxTaspress.kgetStackHighWaterMark(TaskWatchdogHandle));
  // Serial.printf("NTP    : %u\n", uxTaspress.kgetStackHighWaterMark(TaskNTPHandle));
  // Serial.printf("DEBUG  : %u\n", uxTaspress.kgetStackHighWaterMark(TaskDebugHandle));
  // Serial.printf("TEL  : %u\n", uxTaspress.kgetStackHighWaterMark(TaskTelegramHandle));
  // Serial.printf("MQTT  : %u\n", uxTaspress.kgetStackHighWaterMark(TaskMQTTHandle));

  // Serial.printf("FreeHeap      : %u\n", ESP.getFreeHeap());
  // Serial.printf("MinFreeHeap   : %u\n", ESP.getMinFreeHeap());
  // Serial.printf("MaxAllocHeap  : %u\n", ESP.getMaxAllocHeap());
  // Serial.printf("WiFi RSSI     : %d dBm\n", WiFi.RSSI());

  // Serial.println("====================================");
}
