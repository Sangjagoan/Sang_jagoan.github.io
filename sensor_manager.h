
#pragma once

#include <Arduino.h>
#include "global_vars.h"

// ===== BUZZER TONES =====
enum BuzzerTone {
  TONE_OFF = 0,
  TONE_OK = 1000,
  TONE_ULTRASONIC = 500,
  TONE_PRESSURE_LOW = 1500,
  TONE_PRESSURE_HIGH = 1800,
  TONE_VOLTAGE = 2000,
  TONE_CURRENT = 2300,
  TONE_FATAL = 2600,
  TONE_WIFI_LOST = 2800
};

struct LevelConfig {
  float kedalamanPermukaan;
  float jarakSensor;

  float levelStop;
  float levelBawah;
  float levelDayStart;
  float levelNightStart;
};

struct SensorDataPressure {
  float pressureValueAdc;
  float pressureValue;
  float kalmanPressureValue;
  float kg;
  float psi;
  float bar;
};

typedef struct {
  float non_kalman;
  float cm;
  float percentage;
  float tinggiAir;
  float kedalamanAir;
  
} SensorDataUltrasonic;

void pressure();
void ultrasonic();
void PZEM();

float safeValue(float newValue, float lastValue);

int n_pressure();

bool checkAllSensors();
bool checkAllSensorsSetup();

float mapFloat(float x, float inMin, float inMax, float outMin, float outMax);

SensorDataUltrasonic getUltrasonicSnapshot();
SensorDataPressure getPressureSnapshot();
