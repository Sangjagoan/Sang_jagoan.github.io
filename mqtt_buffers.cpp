#include "mqtt_manager.h"

char lastPublishedHeartBeat[125] = "";
char lastPublishedState[256] = "";
char jsonBuffer[640];
char bufferSystem[512];

extern volatile DebugInfo debugInfo;

void buildWifiStatus(char* buffer, size_t size) {

  bool connected = WiFi.status() == WL_CONNECTED;

  snprintf(buffer, size,
           "{"
           "\"connected\":%s,"
           "\"ssid\":\"%s\","
           "\"ip\":\"%s\","
           "\"rssi\":%d"
           "}",
           connected ? "true" : "false",
           connected ? WiFi.SSID().c_str() : "",
           connected ? WiFi.localIP().toString().c_str() : "",
           connected ? WiFi.RSSI() : -100);
}

int buildSystemJSON(char* buffer, size_t size) {

  SensorDataUltrasonic u = getUltrasonicSnapshot();
  SensorDataPressure p = getPressureSnapshot();

  return snprintf(buffer, size,
                  "{"
                  "\"v1\":%.2f,\"a1\":%.2f,\"p1\":%.2f,\"e1\":%.2f,\"f1\":%.2f,\"pf1\":%.2f,"
                  "\"v2\":%.2f,\"a2\":%.2f,\"p2\":%.2f,\"e2\":%.2f,\"f2\":%.2f,\"pf2\":%.2f,"
                  "\"lvl\":%.2f,\"jar\":%.2f,\"tAir\":%.2f,\"nk\":%.2f,\"k\":%.2f,"
                  "\"kg\":%.2f,\"psi\":%.2f,\"kPsi\":%.2f,\"nkPsi\":%.2f,\"br\":%.2f,"
                  "\"hp\":%u,\"mh\":%u,\"mb\":%u,\"hl\":\"%s\",\"up\":%u,\"mq\":%u,"
                  "\"s\":%u,\"w\":%u,\"n\":%u,\"p\":%u,\"h\":%u,\"m\":%u,\"t\":%u,\"d\":%u,\"mq\":%s"
                  "}",

                  readings.voltage,
                  readings.current,
                  readings.power,
                  readings.energy,
                  readings.frequency,
                  readings.pf,

                  readings._voltage,
                  readings._current,
                  readings._power,
                  readings._energy,
                  readings._frequency,
                  readings._pf,

                  u.percentage,
                  u.kedalamanAir,
                  u.tinggiAir,
                  u.non_kalman,
                  u.cm,

                  p.kg,
                  p.psi,
                  p.pressureValue,
                  p.pressureValueAdc,
                  p.bar,

                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  getSystemHealth(),
                  millis() / 1000,
                  mqttQueueCount(),

                  debugInfo.sensor,
                  debugInfo.wdt,
                  debugInfo.ntp,
                  debugInfo.plc,
                  debugInfo.health,
                  debugInfo.mqtt,
                  debugInfo.tel,
                  debugInfo.debug,
                  mqttClient.connected() ? "true" : "false");
}

int buildUptime(char* buffer, size_t size) {

  return snprintf(buffer, size,
                  "{\"up\":%u}",
                  millis() / 1000);
}

int buildPressureJSON(char* buffer, size_t size) {

  return snprintf(buffer, size,
                  "{"
                  "\"target\":%.2f,"
                  "\"high\":%.2f,"
                  "\"low\":%.2f,"
                  "\"deadband\":%.2f,"
                  "\"pulseMin\":%u,"
                  "\"pulseMax\":%u,"
                  "\"settle\":%u,"
                  "\"lock\":%u,"
                  "\"ovr\":%.2f,"
                  "\"ol\":%.2f"
                  "}",

                  pressureCfg.target,
                  pressureCfg.high,
                  pressureCfg.low,
                  pressureCfg.deadband,
                  pressureCfg.pulseMin,
                  pressureCfg.pulseMax,
                  pressureCfg.settleTime,
                  pressureCfg.lockTime,
                  pressureCfg.overShoot,
                  pressureCfg.overLoad);
}
