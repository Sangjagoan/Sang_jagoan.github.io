#include "handle_dayNight.h"
#include <NTPClient.h>
#include "global_vars.h"

extern SystemMode systemMode;

/* =========================================================
   13) DAY / NIGHT MODE
   ========================================================= */
void loadDayNightConfig() {

  prefs.begin("system", true);

  String json = prefs.getString("dn", "");

  if (json.length() > 0) {

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, json);

    if (!err) {

      dnConfig.mode = (ModeType)doc["mode"].as<uint8_t>();
      dnConfig.nightStart = doc["nightStart"].as<uint8_t>();
      dnConfig.nightEnd = doc["nightEnd"].as<uint8_t>();

      // VALIDASI
      if (dnConfig.nightStart > 23) dnConfig.nightStart = 18;
      if (dnConfig.nightEnd > 23) dnConfig.nightEnd = 4;
      if (dnConfig.mode > 2) dnConfig.mode = MODE_AUTO;

    } else {
      Serial.println("JSON corrupt → reset default");

      dnConfig.mode = MODE_AUTO;
      dnConfig.nightStart = 18;
      dnConfig.nightEnd = 4;
    }

  } else {

    dnConfig.mode = MODE_AUTO;
    dnConfig.nightStart = 18;
    dnConfig.nightEnd = 4;
  }

  prefs.end();
}

void handleDayNight(JsonDocument& doc) {

  dnConfig.mode = (ModeType)doc["mode"];
  dnConfig.nightStart = doc["nightStart"];
  dnConfig.nightEnd = doc["nightEnd"];

  saveDayNightConfig();

  systemMode = calculateDayNight(timeClient.getHours());

  Serial.println("DayNight config updated");
}

void saveDayNightConfig() {

  prefs.begin("system", false);

  StaticJsonDocument<128> doc;
  doc["mode"] = dnConfig.mode;
  doc["nightStart"] = dnConfig.nightStart;
  doc["nightEnd"] = dnConfig.nightEnd;

  String json;
  serializeJson(doc, json);

  prefs.putString("dn", json);

  prefs.end();
}

SystemMode calculateDayNight(int hour) {

  if (dnConfig.mode == MODE_FORCE_DAY)
    return MODE_DAY;

  if (dnConfig.mode == MODE_FORCE_NIGHT)
    return MODE_NIGHT;

  // AUTO MODE

  if (dnConfig.nightStart == dnConfig.nightEnd)
    return MODE_DAY;  // avoid 24h night bug

  if (dnConfig.nightStart < dnConfig.nightEnd) {
    if (hour >= dnConfig.nightStart && hour < dnConfig.nightEnd)
      return MODE_NIGHT;
  } else {
    if (hour >= dnConfig.nightStart || hour < dnConfig.nightEnd)
      return MODE_NIGHT;
  }

  return MODE_DAY;
}