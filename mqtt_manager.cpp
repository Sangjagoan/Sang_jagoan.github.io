#include "Arduino.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "mqtt_manager.h"
#include "configwifi.h"
#include "mqtt_topics.h"
#include "global_vars.h"

extern SemaphoreHandle_t mqttMutex;

extern char DEVICE_ID[32];

// queue
extern MQTTMessage mqttQueue[];
extern volatile uint16_t mqttHead;
extern volatile uint16_t mqttTail;

extern volatile bool otaInProgress;

extern WifiProvisionState wifiState;
extern volatile bool systemHealthy;
extern volatile bool systemRunEnebleSensor;
extern volatile bool firstBootAfterOTA;
extern volatile bool wifiHealthy;

extern bool outSibleAtas;
extern bool outSibleBawah;
extern bool outKranBuka;
extern bool outKranTutup;

extern PLC_State plcState;
extern volatile SystemMode systemMode;

extern bool stopkranUtaraState;
extern bool stopkranSelatanState;

/* =========================================================
   20) MQTT FUNCTIONS
   ========================================================= */
void mqttMaintain() {

  static uint32_t lastReconnect = 0;

  // blok saat OTA
  if (otaInProgress)
    return;

  // harus ada WiFi
  if (WiFi.status() != WL_CONNECTED)
    return;

  // kalau sudah connect keluar
  if (mqttClient.connected())
    return;

  // delay reconnect 3 detik
  if (millis() - lastReconnect < 5000)
    return;

  lastReconnect = millis();

  uint64_t chipid = ESP.getEfuseMac();

  char clientId[32];

  snprintf(clientId, sizeof(clientId),
           "esp32_%04X%08X",
           (uint16_t)(chipid >> 32),
           (uint32_t)chipid);

  Serial.println("Connecting MQTT...");

  if (mqttClient.connect(
        clientId,
        "espuser",
        "Esp32Cloud2026!",
        "esp32/panel/status",
        1,
        true,
        "OFFLINE")) {

    Serial.println("MQTT Connected");

    // 🔥 SUBSCRIBE KHUSUS DEVICE INI
    char topic[64];
    snprintf(topic, sizeof(topic), "esp32/%s/#", DEVICE_ID);

    mqttClient.subscribe(topic);

    Serial.print("Subscribe ke: ");
    Serial.println(topic);
    Serial.printf("DEVICE ID: %s\n", DEVICE_ID);
    // status online
    mqttPublish("status", "ONLINE");
  } else {

    Serial.printf("MQTT Failed rc=%d\n", mqttClient.state());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("JSON parse failed");
    return;
  }

  String t = String(topic);

  // 🔥 FILTER DEVICE
  char prefix[64];
  snprintf(prefix, sizeof(prefix), "esp32/%s/", DEVICE_ID);

  // cek prefix
  if (strncmp(topic, prefix, strlen(prefix)) != 0) return;

  // routing tanpa String
  if (strstr(topic, "/config/daynight/set")) {
    handleDayNight(doc);
  } else if (strstr(topic, "/panel/stopkran/set")) {
    handleStopkran(doc);
  } else if (strstr(topic, "/panel/pump/set")) {
    handlePumpSetting(doc);
  } else if (strstr(topic, "panel/calis/set")) {
    handleCalisSetting(doc);
  } else if (strstr(topic, "/config/wifi/set")) {
    handleWifiMQTT(doc);
  } else if (strstr(topic, "/cmd/wifi/scan")) {
    scanWifi();
  } else if (strstr(topic, "/panel/pressure/set")) {
    handlePressureSetting(doc);
  } else if (strstr(topic, "panel/alarm/sensor/set")) {
    handleAlarmSensorSetting(doc);
  } else if (strstr(topic, "/config/esp/restart")) {
    ESP.restart();
  } else if (strstr(topic, "/panel/system/set")) {
    handleSystem(doc);
  } else if (strstr(topic, "/panel/pzem/reset")) {
    resetPzem(doc);
  }
}

/* =========================================================
   13) ALARM SENSOR
 ========================================================= */
void publishAlarmSensor() {

  if (!mqttClient.connected()) return;

  static char lastPublishAlarmSensor[128] = "";
  char payload[128];

  snprintf(payload, sizeof(payload),
           "{\"lwu\":%.2f,\"lsu\":%.2f,\"lp\":%.2f,\"op\":%.2f,\"lv\":%.2f,\"l_v\":%.2f,\"ov\":%.2f,\"o_v\":%.2f,\"oc\":%.2f,\"o_c\":%.2f}",
           cekSensor.lowU,
           cekSensor.losU,
           cekSensor.lowP,
           cekSensor.overP,
           cekSensor.lowV,
           cekSensor.low_V,
           cekSensor.overV,
           cekSensor.over_V,
           cekSensor.overC,
           cekSensor.over_C);

  if (strcmp(payload, lastPublishAlarmSensor) == 0)
    return;

  strncpy(lastPublishAlarmSensor, payload, sizeof(lastPublishAlarmSensor) - 1);
  lastPublishAlarmSensor[sizeof(lastPublishAlarmSensor) - 1] = '\0';

  mqttPublish(MQTT_TOPIC_ALARM_SENSOR, payload);
}

/* =========================================================
   14) HANDLE SAVE WIFI
 ========================================================= */
void handleWifiMQTT(JsonDocument& doc) {

  String ssid = doc["ssid"].as<String>();
  String pass = doc["password"].as<String>();

  if (ssid.length() == 0) {
    publishWifiProgress("INVALID");
    return;
  }

  /* ===== SCAN ===== */

  wifiState = WIFI_SCAN;
  publishWifiProgress("SCAN");

  // delay(500);
  vTaskDelay(pdMS_TO_TICKS(500));

  /* ===== TEST ===== */

  wifiState = WIFI_TEST;
  publishWifiProgress("TEST");

  if (!connectAndTestWiFi(ssid, pass)) {

    wifiState = WIFI_FAILED;
    publishWifiProgress("FAILED");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    return;
  }

  /* ===== SAVE ===== */

  wifiState = WIFI_SAVE;
  publishWifiProgress("SAVE");

  saveWiFiToNVS(ssid, pass);

  // delay(500);
  vTaskDelay(pdMS_TO_TICKS(500));

  /* ===== RESTART ===== */

  wifiState = WIFI_RESTART;
  publishWifiProgress("RESTART");

  // delay(500);
  vTaskDelay(pdMS_TO_TICKS(500));

  ESP.restart();
}

void publishWifiProgress(const char* state) {

  if (!mqttClient.connected()) return;

  char payload[128];

  snprintf(payload, sizeof(payload),
           "{\"state\":\"%s\"}", state);

  mqttPublish(MQTT_TOPIC_WIFI_PROGRESS, payload);
}

void publishWifiStatus() {

  if (!mqttClient.connected()) return;

  static char lastPublisWifiStatus[128] = "";

  char payload[200];

  buildWifiStatus(payload, sizeof(payload));

  if (strcmp(payload, lastPublisWifiStatus) == 0) {
    return;
  }

  strcpy(lastPublisWifiStatus, payload);
  mqttPublish(MQTT_TOPIC_WIFI_STATUS, payload);
}

void publishAlarm(const char* type, const char* msg, float value) {

  if (!mqttClient.connected()) return;

  static uint32_t lastAlarmTime = 0;
  static char lastPublishedAlarm[128] = "";

  char alarmBuffer[128];
  char valueStr[16];

  dtostrf(value, 0, 2, valueStr);

  snprintf(alarmBuffer, sizeof(alarmBuffer),
           "{\"type\":\"%s\",\"msg\":\"%s\",\"value\":%s}",
           type, msg, valueStr);

  uint32_t now = millis();

  if (strcmp(alarmBuffer, lastPublishedAlarm) == 0 && (now - lastAlarmTime) < 10000) {
    return;
  }

  strcpy(lastPublishedAlarm, alarmBuffer);
  lastAlarmTime = now;

  // 🔥 TARUH DI SINI
  Serial.println("MQTT SEND:");
  Serial.println(alarmBuffer);

  mqttPublish("alarm", alarmBuffer);
}

void publishHeartbeat() {

  if (!mqttClient.connected()) return;

  int len = snprintf(
    jsonBuffer,
    sizeof(jsonBuffer),
    "{\"hlt\":%s,\"run\":%s,\"ota\":%s,\"wifi\":%s,\"sa\":%s,\"sb\":%s}",
    systemHealthy ? "true" : "false",
    systemRunEnebleSensor ? "true" : "false",
    firstBootAfterOTA ? "true" : "false",
    wifiHealthy ? "true" : "false",
    outSibleAtas ? "true" : "false",
    outSibleBawah ? "true" : "false");

  if (len <= 0) return;

  if (strcmp(jsonBuffer, lastPublishedHeartBeat) == 0)
    return;

  strcpy(lastPublishedHeartBeat, jsonBuffer);

  mqttPublish(MQTT_TOPIC_HEARTBEAT, jsonBuffer);
}

void publishIndikatorLed() {

  if (!mqttClient.connected()) return;

  static char lastPublisIndikator[128] = "";

  char indikatorBuffer[128];

  int len = snprintf(
    indikatorBuffer,
    sizeof(indikatorBuffer),
    "{\"ob\":%s,\"ot\":%s}",
    outKranBuka ? "true" : "false",
    outKranTutup ? "true" : "false");

  if (len <= 0) return;

  if (strcmp(indikatorBuffer, lastPublisIndikator) == 0)
    return;

  strcpy(lastPublisIndikator, indikatorBuffer);

  mqttPublish(MQTT_TOPIC_LEDINDIKATORKRAN, indikatorBuffer);
}

void publishState() {

  if (!mqttClient.connected()) return;

  snprintf(jsonBuffer, sizeof(jsonBuffer),
           "{\"plcState\":\"%s\",\"systemMode\":\"%s\",\"mode\":%d,\"nightStart\":%d,\"nightEnd\":%d}",
           stateName(plcState),
           stateTime(systemMode),
           dnConfig.mode,
           dnConfig.nightStart,
           dnConfig.nightEnd);

  if (strcmp(jsonBuffer, lastPublishedState) == 0)
    return;

  strncpy(lastPublishedState, jsonBuffer, sizeof(lastPublishedState) - 1);
  lastPublishedState[sizeof(lastPublishedState) - 1] = '\0';

  mqttPublish(MQTT_TOPIC_DAYNIGHT_STATE, jsonBuffer);

  Serial.println("State Published (changed)");
}

bool mqttPublish(const char* topic, const char* payload) {
  return mqttSafePublish(topic, payload, strlen(payload));
}

bool mqttSafePublish(const char* topic, const char* payload, uint16_t len) {

  char fullTopic[96];

  // kalau topic sudah full (misalnya sudah ada esp32/), skip prefix
  if (strncmp(topic, "esp32/", 6) == 0) {
    snprintf(fullTopic, sizeof(fullTopic), "%s", topic);
  } else {
    snprintf(fullTopic, sizeof(fullTopic), "esp32/%s/%s", DEVICE_ID, topic);
  }

  if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

      bool ok = mqttClient.publish(fullTopic, payload, len);

      xSemaphoreGive(mqttMutex);

      if (ok) return true;
    }
  }

  mqttQueuePush(fullTopic, payload, len);

  return false;
}

// Fungsi tambah ke queue
bool mqttQueuePush(const char* topic, const char* payload, uint16_t len) {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(50)) != pdTRUE)
    return false;

  uint16_t next = (mqttHead + 1) % MQTT_QUEUE_SIZE;

  if (next == mqttTail) {
    xSemaphoreGive(mqttMutex);
    Serial.println("MQTT Queue FULL");
    return false;
  }

  strncpy(mqttQueue[mqttHead].topic, topic,
          sizeof(mqttQueue[mqttHead].topic) - 1);

  mqttQueue[mqttHead].topic[sizeof(mqttQueue[mqttHead].topic) - 1] = '\0';

  // 🔴 BATASI PANJANG PAYLOAD
  if (len >= MQTT_PAYLOAD_MAX)
    len = MQTT_PAYLOAD_MAX - 1;

  memcpy(mqttQueue[mqttHead].payload, payload, len);

  mqttQueue[mqttHead].payload[len] = 0;
  mqttQueue[mqttHead].len = len;

  mqttHead = next;

  xSemaphoreGive(mqttMutex);

  return true;
}
// Fungsi kirim dari queue
void mqttQueueProcess() {
  if (!mqttClient.connected())
    return;

  while (mqttTail != mqttHead) {
    MQTTMessage& msg = mqttQueue[mqttTail];

    bool ok = mqttClient.publish(msg.topic, msg.payload, msg.len);

    if (!ok) {
      Serial.println("MQTT send failed, retry later");
      vTaskDelay(pdMS_TO_TICKS(500));
      return;
    }
    mqttTail = (mqttTail + 1) % MQTT_QUEUE_SIZE;

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

uint16_t mqttQueueCount() {
  if (mqttHead >= mqttTail)
    return mqttHead - mqttTail;

  return MQTT_QUEUE_SIZE - mqttTail + mqttHead;
}

void publishPressureState() {

  static char lastPayload[192] = "";
  char payload[192];

  int len = buildPressureJSON(payload, sizeof(payload));

  if (len <= 0) return;

  if (strcmp(payload, lastPayload) == 0) return;

  strncpy(lastPayload, payload, sizeof(lastPayload));

  mqttPublish(MQTT_TOPIC_PRESSURE_STATE, payload);
}

void publishValveState() {

  static char lastValve[128] = "";

  char payload[128];

  snprintf(payload, sizeof(payload),
           "{\"utara\":%s,\"selatan\":%s}",
           digitalRead(PINSTOPKRANUTARA) ? "true" : "false",
           digitalRead(PINSTOPKRANSELATAN) ? "true" : "false");

  if (strcmp(payload, lastValve) == 0)
    return;

  strcpy(lastValve, payload);

  mqttPublish(MQTT_TOPIC_VALVE_STATE, payload);
}

void publishPumpState() {

  static char lastPump[128] = "";

  char payload[128];

  snprintf(payload, sizeof(payload),
           "{\"stop\":%.1f,\"one\":%.1f,\"day\":%.1f,\"night\":%.1f}",
           levelCfg.levelStop,
           levelCfg.levelBawah,
           levelCfg.levelDayStart,
           levelCfg.levelNightStart);

  // kalau payload sama → tidak publish
  if (strcmp(payload, lastPump) == 0)
    return;

  strcpy(lastPump, payload);

  mqttPublish(MQTT_TOPIC_PUMP_STATE, payload);
}

void publishCalisState() {

  static char lastPump[128] = "";

  char payload[128];

  snprintf(payload, sizeof(payload),
           "{\"js\":%.1f,\"kp\":%.1f}",

           levelCfg.jarakSensor,
           levelCfg.kedalamanPermukaan);

  // kalau payload sama → tidak publish
  if (strcmp(payload, lastPump) == 0)
    return;

  strcpy(lastPump, payload);

  mqttPublish(MQTT_TOPIC_CALIS_STATE, payload);
}

void handleStopkran(JsonDocument& doc) {

  String valve = doc["valve"];
  String state = doc["state"];

  bool on = (state == "on");

  prefs.begin("valve", false);

  if (valve == "utara") {

    digitalWrite(PINSTOPKRANUTARA, on ? HIGH : LOW);

    stopkranUtaraState = on;
    prefs.putBool("utara", on);
  }

  else if (valve == "selatan") {

    digitalWrite(PINSTOPKRANSELATAN, on ? HIGH : LOW);

    stopkranSelatanState = on;
    prefs.putBool("selatan", on);
  }

  prefs.end();

  Serial.println("Stopkran command executed");
}

/* =========================================================
   13) PUMP SETTING
   ========================================================= */
void loadLevelConfig() {

  prefs.begin("level", true);

  levelCfg.levelStop = prefs.getFloat("ls", 100);
  levelCfg.levelBawah = prefs.getFloat("lb", 97);
  levelCfg.levelDayStart = prefs.getFloat("ld", 88);
  levelCfg.levelNightStart = prefs.getFloat("ln", 79);

  prefs.end();

  Serial.println("Level config loaded");
}

void loadCalisConfig() {

  prefs.begin("calis", true);

  levelCfg.kedalamanPermukaan = prefs.getFloat("jb", 249);
  levelCfg.jarakSensor = prefs.getFloat("ja", 25);

  prefs.end();

  Serial.println("Level config loaded");
}

void handlePumpSetting(JsonDocument& doc) {
  levelCfg.levelStop = doc["stop"];         // level tertinggi off
  levelCfg.levelBawah = doc["one"];         // pompa nyala satu saat mode siang
  levelCfg.levelDayStart = doc["day"];      // pompa nyala semua saat mode siang
  levelCfg.levelNightStart = doc["night"];  // pompa nyala semua saat mode malam

  saveLevelConfig();

  Serial.println("MQTT Pump setting updated");
}

void handleCalisSetting(JsonDocument& doc) {
  levelCfg.kedalamanPermukaan = doc["kp"];  // Kedalam permukaan tandon
  levelCfg.jarakSensor = doc["js"];         // Jarak sensor dengan permukaan atas tandon

  saveCalisConfig();

  Serial.println("MQTT Pump setting updated");
}

void saveLevelConfig() {

  prefs.begin("level", false);

  prefs.putFloat("ls", levelCfg.levelStop);
  prefs.putFloat("lb", levelCfg.levelBawah);
  prefs.putFloat("ld", levelCfg.levelDayStart);
  prefs.putFloat("ln", levelCfg.levelNightStart);

  prefs.end();
}

void saveCalisConfig() {

  prefs.begin("calis", false);

  prefs.putFloat("jb", levelCfg.kedalamanPermukaan);
  prefs.putFloat("ja", levelCfg.jarakSensor);

  prefs.end();
}

/* =========================================================
   19) HANDLE SCAN WIFI
   ========================================================= */
void scanWifi() {

  String json = scanWifiJSON();

  mqttPublish(MQTT_TOPIC_WIFI_LIST, json.c_str());
}

void handleWifiScan() {

  String json = scanWifiJSON();

  server.send(200, "application/json", json);
}

String scanWifiJSON() {

  bool wasConnected = WiFi.status() == WL_CONNECTED;

  // ubah mode supaya scan stabil
  WiFi.mode(WIFI_AP_STA);

  // delay(200);
  vTaskDelay(pdMS_TO_TICKS(200));


  int n = WiFi.scanNetworks(false, true);

  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < n; i++) {

    JsonObject net = arr.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["enc"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }

  String json;
  serializeJson(doc, json);

  WiFi.scanDelete();

  // kembalikan mode jika perlu
  if (!wasConnected) {
    WiFi.mode(WIFI_AP);
  }

  Serial.printf("WiFi Scan Result: %d\n", n);

  return json;
}

void handleWifiInfo() {
  StaticJsonDocument<192> doc;

  doc["ssid"] = WiFi.SSID();
  doc["ip"] = getClientIP();
  doc["rssi"] = WiFi.RSSI();

  char buf[192];
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
}

void handlePressureSetting(JsonDocument& doc) {

  updatePressureConfig(doc);

  Serial.println("Pressure config updated via MQTT");
}

void handleAlarmSensorSetting(JsonDocument& doc) {

  ubdateAlarmSensorConfig(doc);
}

void handleSystem(JsonDocument& doc) {

  bool hlt = doc["hlt"];

  systemHealthy = hlt;

  publishHeartbeat();  // kirim state terbaru

  Serial.println("System health changed from web");
}

void resetPzem(JsonDocument& doc) {

  if (doc["confirm"] != "YES") {

    mqttPublish("pzem/reset/state", "CONFIRM_REQUIRED");
    return;
  }

  bool r1 = pzem.resetEnergy();
  bool r2 = _pzem.resetEnergy();

  if (r1 || r2) {

    mqttPublish("pzem/reset/state", "OK");

  } else {

    mqttPublish("pzem/reset/state", "FAIL");
  }
}

/* =========================================================
   19) HANDLE DATA ROOT
   ========================================================= */
void publishMQTTData() {

  static char lastPayload[512] = "";  // simpan payload sebelumnya
  char payload[512];

  int len = buildSystemJSON(payload, sizeof(payload));

  if (len <= 0 || len >= sizeof(payload)) {
    Serial.println("MQTT payload too large");
    return;
  }

  // 🔎 cek apakah data berubah
  if (strcmp(payload, lastPayload) == 0) {
    return;  // tidak berubah → tidak publish
  }

  // simpan payload baru
  strcpy(lastPayload, payload);

  if (mqttPublish(MQTT_TOPIC_DATA, payload)) {
    Serial.printf("MQTT OK size=%d heap=%u\n", len, ESP.getFreeHeap());
  } else {
    Serial.println("MQTT publish failed");
  }
}

void publishUptime() {

  static char lastPayload[128] = "";  // simpan payload sebelumnya
  char payload[128];

  int len = buildUptime(payload, sizeof(payload));

  if (strcmp(payload, lastPayload) == 0) {
    return;  // tidak berubah → tidak publish
  }

  // simpan payload baru
  strcpy(lastPayload, payload);

  mqttPublish(MQTT_TOPIC_UPTIME, payload);
}

void updatePressureConfig(JsonDocument& doc) {

  if (doc.containsKey("target")) pressureCfg.target = doc["target"];
  if (doc.containsKey("high")) pressureCfg.high = doc["high"];
  if (doc.containsKey("low")) pressureCfg.low = doc["low"];
  if (doc.containsKey("deadband")) pressureCfg.deadband = doc["deadband"];
  if (doc.containsKey("lockTime")) pressureCfg.lockTime = doc["lockTime"];
  if (doc.containsKey("pulseMin")) pressureCfg.pulseMin = doc["pulseMin"];
  if (doc.containsKey("pulseMax")) pressureCfg.pulseMax = doc["pulseMax"];
  if (doc.containsKey("overShoot")) pressureCfg.overShoot = doc["overShoot"];
  if (doc.containsKey("overLoad")) pressureCfg.overLoad = doc["overLoad"];

  if (doc.containsKey("settle")) pressureCfg.settleTime = doc["settle"];

  /* ===== BATAS AMAN ===== */

  pressureCfg.target = constrain(pressureCfg.target, 0.5, 5.0);
  pressureCfg.high = constrain(pressureCfg.high, 0.5, 6.0);
  pressureCfg.low = constrain(pressureCfg.low, 0.1, 5.0);
  pressureCfg.deadband = constrain(pressureCfg.deadband, 0.01, 0.20);
  pressureCfg.lockTime = constrain(pressureCfg.lockTime, 5000, 60000);
  pressureCfg.pulseMin = constrain(pressureCfg.pulseMin, 50, 2000);
  pressureCfg.pulseMax = constrain(pressureCfg.pulseMax, 100, 3000);
  pressureCfg.overShoot = constrain(pressureCfg.overShoot, 0.1, 1);
  pressureCfg.overLoad = constrain(pressureCfg.overLoad, 0.1, 1);

  pressureCfg.settleTime = constrain(pressureCfg.settleTime, 1000, 20000);

  // if (pressureCfg.low >= pressureCfg.high) {
  //   pressureCfg.low = pressureCfg.high - 0.1;
  // }

  // if (pressureCfg.target < pressureCfg.low)
  //   pressureCfg.target = pressureCfg.low + 0.05;

  // if (pressureCfg.target > pressureCfg.high)
  //   pressureCfg.target = pressureCfg.high - 0.05;

  normalizePressure();

  savePressureConfig();

  Serial.println("Pressure config updated");

  publishPressureState();
}

void ubdateAlarmSensorConfig(JsonDocument& doc) {

  if (doc.containsKey("lowUltra")) cekSensor.lowU = doc["lowUltra"];
  if (doc.containsKey("losUltra")) cekSensor.losU = doc["losUltra"];
  if (doc.containsKey("lowPress")) cekSensor.lowP = doc["lowPress"];
  if (doc.containsKey("overPress")) cekSensor.overP = doc["overPress"];
  if (doc.containsKey("lowVolt")) cekSensor.lowV = doc["lowVolt"];
  if (doc.containsKey("low_Volt")) cekSensor.low_V = doc["low_Volt"];
  if (doc.containsKey("overVolt")) cekSensor.overV = doc["overVolt"];
  if (doc.containsKey("over_Volt")) cekSensor.over_V = doc["over_Volt"];
  if (doc.containsKey("overCur")) cekSensor.overC = doc["overCur"];
  if (doc.containsKey("over_Cur")) cekSensor.over_C = doc["over_Cur"];

  cekSensor.lowU = constrain(cekSensor.lowU, 0.0, 20);
  cekSensor.losU = constrain(cekSensor.losU, 0.0, 1000);
  cekSensor.lowP = constrain(cekSensor.lowP, 0.0, 500);
  cekSensor.overP = constrain(cekSensor.overP, 0.0, 2000);
  cekSensor.lowV = constrain(cekSensor.lowV, 0.0, 180);
  cekSensor.low_V = constrain(cekSensor.low_V, 0.0, 180);
  cekSensor.overV = constrain(cekSensor.overV, 0.0, 300);
  cekSensor.over_V = constrain(cekSensor.over_V, 0.0, 300);
  cekSensor.overC = constrain(cekSensor.overC, 0.0, 20);
  cekSensor.over_C = constrain(cekSensor.over_C, 0.0, 20);

  saveAlarmSensorConfig();
  publishAlarmSensor();

}

/* ===== CEK LOGIKA DEAD BAND ===== */
void normalizePressure() {

  if (pressureCfg.low >= pressureCfg.high)
    pressureCfg.low = pressureCfg.high - 0.1;

  pressureCfg.target = constrain(
    pressureCfg.target,
    pressureCfg.low + 0.05,
    pressureCfg.high - 0.05);
}

void savePressureConfig() {

  prefs.begin("pressure", false);

  prefs.putFloat("t", pressureCfg.target);
  prefs.putFloat("h", pressureCfg.high);
  prefs.putFloat("l", pressureCfg.low);
  prefs.putFloat("db", pressureCfg.deadband);
  prefs.putUInt("lt", pressureCfg.lockTime);
  prefs.putUInt("pmin", pressureCfg.pulseMin);
  prefs.putUInt("pmax", pressureCfg.pulseMax);

  prefs.putUInt("st", pressureCfg.settleTime);
  prefs.putFloat("ov", pressureCfg.overShoot);
  prefs.putFloat("ol", pressureCfg.overLoad);

  prefs.end();
}

void saveAlarmSensorConfig() {

  prefs.begin("alarmSensor", false);

  prefs.putFloat("lw", cekSensor.lowU);
  prefs.putFloat("ls", cekSensor.losU);
  prefs.putFloat("lp", cekSensor.lowP);
  prefs.putFloat("op", cekSensor.overP);
  prefs.putFloat("lv", cekSensor.lowV);
  prefs.putFloat("l_v", cekSensor.low_V);
  prefs.putFloat("ov", cekSensor.overV);
  prefs.putFloat("o_v", cekSensor.over_V);
  prefs.putFloat("oc", cekSensor.overC);
  prefs.putFloat("o_c", cekSensor.over_C);

  prefs.end();
}

void loadPressureConfig() {

  prefs.begin("pressure", true);

  pressureCfg.target = prefs.getFloat("t", 2.0);
  pressureCfg.high = prefs.getFloat("h", 2.10);
  pressureCfg.low = prefs.getFloat("l", 1.90);
  pressureCfg.deadband = prefs.getFloat("db", 0.03);
  pressureCfg.lockTime = prefs.getUInt("lt", 20000);
  pressureCfg.pulseMin = prefs.getUInt("pmin", 100);
  pressureCfg.pulseMax = prefs.getUInt("pmax", 500);
  pressureCfg.settleTime = prefs.getUInt("st", 10000);
  pressureCfg.overShoot = prefs.getFloat("ov", 0.3);
  pressureCfg.overLoad = prefs.getFloat("ol", 0.7);

  prefs.end();

  Serial.println("Pressure config loaded");
}

void loadAlarmSensorConfig() {

  prefs.begin("alarmSensor", true);

  cekSensor.lowU = prefs.getFloat("lw", 1.0);
  cekSensor.losU = prefs.getFloat("ls", 500);
  cekSensor.lowP = prefs.getFloat("lp", 200);
  cekSensor.overP = prefs.getFloat("op", 1600);
  cekSensor.lowV = prefs.getFloat("lv", 160);
  cekSensor.low_V = prefs.getFloat("l_v", 160);
  cekSensor.overV = prefs.getFloat("ov", 260);
  cekSensor.over_V = prefs.getFloat("o_v", 260);
  cekSensor.overC = prefs.getFloat("oc", 5.3);
  cekSensor.over_C = prefs.getFloat("o_c", 8.7);

  prefs.end();

}

String getSystemHealth() {

  uint32_t heap = ESP.getFreeHeap();
  uint32_t minHeap = ESP.getMinFreeHeap();
  uint32_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  // ===== CRITICAL =====
  if (heap < 50000 || maxBlock < 12000)
    return "CRITICAL";

  // ===== WARNING =====
  if (heap < 90000 || maxBlock < 30000 || minHeap < 80000)
    return "WARNING";

  // ===== HEALTHY =====
  return "HEALTHY";
}

const char* stateName(PLC_State s) {
  switch (s) {
    case SIBLE_OFF_ALL: return "Off All";
    case SIBLE_BAWAH_ON: return "On Bawah";
    case SIBLE_DUA_ON: return "All On";
    case SIBLE_DUA_ON1: return "(All On)";
    case PLC_ERROR: return "Plc Error Cek Sible";
    default: return "No Declarasi";
  }
}

const char* stateTime(SystemMode s) {
  switch (s) {
    case MODE_DAY: return "Mode siang";
    case MODE_NIGHT: return "Mode malam";
    case MODE_STARTING: return "Mode starting";
    default: return "No Declarasi";
  }
}