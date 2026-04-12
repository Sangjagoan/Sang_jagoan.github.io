#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "plc_controller.h"
#include "handle_dayNight.h"
#include "system_state.h"
#include "global_vars.h"

// ================= MQTT CONFIG =================
#define MQTT_QUEUE_SIZE 20
#define MQTT_PAYLOAD_MAX 512

extern char lastPublishedHeartBeat[125];
extern char lastPublishedState[256];
extern char jsonBuffer[640];
extern char bufferSystem[512];

// ================= WIFI STATE =================
enum WifiProvisionState {
  WIFI_IDLE,
  WIFI_SCAN,
  WIFI_TEST,
  WIFI_SAVE,
  WIFI_RESTART,
  WIFI_FAILED
};

// ================= DEBUG =================
struct DebugInfo {
  uint16_t sensor;
  uint16_t plc;
  uint16_t health;
  uint16_t wdt;
  uint16_t ntp;
  uint16_t debug;
  uint16_t tel;
  uint16_t mqtt;
};

// ================= MQTT MESSAGE =================
struct MQTTMessage {
  char topic[64];
  char payload[MQTT_PAYLOAD_MAX];
  uint16_t len;
};

// ================= CORE =================
void mqttMaintain();
void callback(char* topic, byte* payload, unsigned int length);

// ================= MQTT CORE =================
bool mqttPublish(const char* topic, const char* payload);
bool mqttSafePublish(const char* topic, const char* payload, uint16_t len);

// ================= QUEUE =================
bool mqttQueuePush(const char* topic, const char* payload, uint16_t len);
void mqttQueueProcess();
uint16_t mqttQueueCount();

void normalizePressure();
// ================= PUBLISH =================
void publishState();
void publishHeartbeat();
void publishMQTTData();
void publishIndikatorLed();

void publishPressureState();
void publishValveState();
void publishPumpState();
void publishAlarmSensor();

void publishWifiStatus();
void publishWifiProgress(const char* state);

// ================= BUILD JSON =================
int buildPressureJSON(char* buffer, size_t size);
int buildSystemJSON(char* buffer, size_t size);
int buildUptime(char* buffer, size_t size);
void buildWifiStatus(char* buffer, size_t size);

// ================= HANDLER =================
void handleWifiMQTT(JsonDocument& doc);
void handlePumpSetting(JsonDocument& doc);
void handleCalisSetting(JsonDocument& doc);
void handlePressureSetting(JsonDocument& doc);
void handleAlarmSensorSetting(JsonDocument& doc);
void handleStopkran(JsonDocument& doc);
void handleSystem(JsonDocument& doc);
void resetPzem(JsonDocument& doc);

// ================= WIFI =================
void scanWifi();
void handleWifiScan();
void handleWifiInfo();
String scanWifiJSON();

// ================= CONFIG =================
void loadLevelConfig();
void loadCalisConfig();
void loadPressureConfig();
void loadAlarmSensorConfig();
void saveLevelConfig();
void saveCalisConfig();
void savePressureConfig();
void saveAlarmSensorConfig();
void updatePressureConfig(JsonDocument& doc);
void ubdateAlarmSensorConfig(JsonDocument& doc);
void publishCalisState();
void normalizePressure();
// ================= SYSTEM =================
String getSystemHealth();
const char* stateName(PLC_State s);
const char* stateTime(SystemMode s);

// ================= UPTIME =================
void publishUptime();