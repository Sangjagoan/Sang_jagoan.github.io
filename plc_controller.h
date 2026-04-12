#pragma once
#include <Arduino.h>
#include "sensor_manager.h"
#include "system_state.h"

struct PressureConfig {
  float target;
  float high;
  float low;
  float overShoot;
  float overLoad;

  float deadband;

  uint16_t pulseMin;
  uint16_t pulseMax;

  uint16_t settleTime;
  uint16_t lockTime;
};

// config
extern LevelConfig levelCfg;
extern PressureConfig pressureCfg;

extern volatile bool ntpReady;
extern void failSafeMode();

// ===== STATE =====
PLC_State statePlcMesin(const SensorDataUltrasonic& level);
void plcStateMachine();
void updateOutputs();

// ===== VALVE =====
void stopKranControl();
float getPressureFiltered();
uint32_t calculatePulse(float pressure);