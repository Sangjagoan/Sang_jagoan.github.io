#pragma once
#include <ArduinoJson.h>
#include <Preferences.h>
#include "system_state.h"

enum ModeType {
  MODE_AUTO = 0,
  MODE_FORCE_DAY = 1,
  MODE_FORCE_NIGHT = 2
};

struct DayNightConfig {
  ModeType mode;
  uint8_t nightStart;
  uint8_t nightEnd;
};

extern DayNightConfig dnConfig;
extern Preferences prefs;

void loadDayNightConfig();
void handleDayNight(JsonDocument& doc);
void saveDayNightConfig();
SystemMode calculateDayNight(int hour);
