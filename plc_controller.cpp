#include "plc_controller.h"

// ambil global dari file utama
extern volatile bool systemHealthy;
extern volatile bool systemRunEnebleSensor;
extern SystemMode systemMode;
extern PLC_State plcState;

// output
extern bool outSibleAtas;
extern bool outSibleBawah;
extern bool outKranBuka;
extern bool outKranTutup;

// timer
extern uint32_t stateTimer;
extern uint32_t stateTimerNightDay;
extern uint32_t stateTimerNight;

// valve state
extern ValveState valveState;

/* =========================================================
   14) PLC STATE MACHINE
   ========================================================= */

PLC_State statePlcMesin(const SensorDataUltrasonic& level) {

  static float hysteresis = 5.0;

  uint32_t now = millis();

  if (now - stateTimer < STATE_DELAY) {
    return plcState;
  }

  PLC_State nextState = plcState;

  if (level.percentage >= levelCfg.levelStop - 0.5) {
    nextState = SIBLE_OFF_ALL;
  } else if (level.percentage >= levelCfg.levelBawah && level.percentage <= levelCfg.levelBawah + hysteresis) {
    nextState = SIBLE_BAWAH_ON;
  } else if (systemMode == MODE_DAY && level.percentage < levelCfg.levelDayStart) {
    nextState = SIBLE_DUA_ON;
  } else if (systemMode == MODE_NIGHT && level.percentage < levelCfg.levelNightStart) {
    nextState = SIBLE_DUA_ON1;
  }

  if (nextState != plcState) {
    plcState = nextState;
    stateTimer = now;
  }

  return plcState;
}

// Mode malam // mode siang MODE_DAY
void plcStateMachine() {

  SensorDataPressure press = getPressureSnapshot();
  SensorDataUltrasonic level = getUltrasonicSnapshot();
  static uint8_t lastMode = 255;

  static bool startupDone = false;
  uint32_t now = millis();

  if (!ntpReady) {
    systemMode = MODE_DAY;
  }

  if (!startupDone) {
    if (now < 15000) return;
    startupDone = true;
    stateTimer = now;
  }

  // detect mode change
  if (lastMode != systemMode) {
    stateTimerNightDay = now;
    lastMode = systemMode;
  }

  plcState = statePlcMesin(level);

  if (!systemHealthy || !systemRunEnebleSensor) {

    plcState = PLC_ERROR;

    failSafeMode();
    return;
  }

  /* ======================================================
     MODE DAY
  ====================================================== */
  if (systemMode == MODE_DAY) {

    bool startupActive = (level.percentage <= levelCfg.levelBawah && (now - stateTimerNightDay) < 10000);

    if (startupActive) {

      outSibleBawah = true;
      plcState = SIBLE_BAWAH_ON;
      if (press.pressureValueAdc < 1000) {
        outKranTutup = true;
        outKranBuka = false;
      }

      return;
    }

    switch (plcState) {

      case SIBLE_OFF_ALL:
        outSibleAtas = false;
        outSibleBawah = false;
        break;

      case SIBLE_BAWAH_ON:
        outSibleAtas = false;
        outSibleBawah = true;
        break;

      case SIBLE_DUA_ON:
        outSibleAtas = true;
        outSibleBawah = true;
        break;
    }

    stopKranControl();
  }

  /* ======================================================
     MODE NIGHT
     ====================================================== */
  else {

    outKranTutup = false;
    outKranBuka = true;

    uint32_t elapsed = now - stateTimerNightDay;

    // startup sequence
    if (elapsed < 500 && level.percentage > levelCfg.levelNightStart + 0.5) {
      outSibleAtas = false;
      outSibleBawah = false;
      return;
    }

    if (elapsed < 1000 && level.percentage <= levelCfg.levelNightStart) {
      outSibleBawah = true;
      return;
    }

    if (elapsed < 20000 && level.percentage <= levelCfg.levelNightStart) {
      outSibleAtas = true;
      return;
    }

    // normal state machine
    switch (plcState) {

      case SIBLE_OFF_ALL:

        outSibleAtas = false;
        outSibleBawah = false;
        stateTimerNight = now;
        break;

      case SIBLE_DUA_ON1:

        outSibleBawah = true;
        outSibleAtas = (now - stateTimerNight > 20000);

        break;
    }
  }
}

void updateOutputs() {

  static bool lastSibleAtas = false;
  static bool lastSibleBawah = false;
  static bool lastKranBuka = false;
  static bool lastKranTutup = false;

  if (outSibleAtas != lastSibleAtas) {
    digitalWrite(PINSIBLEATAS, outSibleAtas);
    lastSibleAtas = outSibleAtas;
  }

  if (outSibleBawah != lastSibleBawah) {
    digitalWrite(PINSIBLEBAWAH, outSibleBawah);
    lastSibleBawah = outSibleBawah;
  }

  if (outKranBuka != lastKranBuka) {
    digitalWrite(PINBUKASTOPKRAN, outKranBuka);
    lastKranBuka = outKranBuka;
  }

  if (outKranTutup != lastKranTutup) {
    digitalWrite(PINTUTUPSTOPKRAN, outKranTutup);
    lastKranTutup = outKranTutup;
  }
}

/* =========================================================
   15) STOP KRAN CONTROL
   ========================================================= */
uint32_t pressureLockTimer = 0;
bool pressureLocked = false;
#define VALVE_SAFETY_LIMIT 60000
float valveRate = 0.0005;  // perubahan bar per ms (nilai awal)
float lastPressureLearn = 0;
uint32_t lastMoveTime = 0;
bool learnReady = false;

uint32_t valvePulse = 0;
uint32_t valveTimer = 0;
uint32_t valveTotalMoveTime = 0;
uint32_t overShootshootTimer = 0;
bool overShootshootActive = false;

void stopKranControl() {

  float pressure = getPressureFiltered();
  float error = pressureCfg.target - pressure;
  /* ===== overShootSHOOT PROTECTION ===== */
  if (pressure > pressureCfg.target + pressureCfg.overLoad) {

    outKranTutup = false;
    outKranBuka = true;

  } else if (pressure > pressureCfg.target + pressureCfg.overShoot) {

    if (!overShootshootActive) {
      overShootshootActive = true;
      overShootshootTimer = millis();
    }

    uint32_t elapsed = millis() - overShootshootTimer;

    // 1 detik valve tutup
    if (elapsed < 1000) {

      outKranTutup = false;
      outKranBuka = true;

    }
    // 5 detik valve berhenti
    else if (elapsed < 9000) {

      outKranTutup = false;
      outKranBuka = false;

    }
    // reset siklus
    else {
      overShootshootActive = false;
      overShootshootTimer = millis();
    }

    valveState = VALVE_IDLE;

    Serial.println("⚠ overShootshoot protection");

    return;
  }

  // ===== ANTI HUNTING LOCK =====
  if (pressureLocked) {

    if (millis() - pressureLockTimer < pressureCfg.lockTime) {
      return;  // valve tidak boleh bergerak
    }

    pressureLocked = false;
  }

  switch (valveState) {

    case VALVE_IDLE:

      valvePulse = calculatePulse(pressure);

      // jika dekat target jangan gerakkan valve
      if (abs(error) < pressureCfg.deadband) {

        pressureLocked = true;
        pressureLockTimer = millis();

        break;
      }

      if (pressure > pressureCfg.high) {

        lastPressureLearn = pressure;
        learnReady = true;
        // tutup valve sedikit
        outKranTutup = false;
        outKranBuka = true;

        valveTimer = millis();
        valveState = VALVE_CLOSING;
      }

      else if (pressure < pressureCfg.low) {

        lastPressureLearn = pressure;
        learnReady = true;
        // buka valve sedikit
        outKranTutup = true;
        outKranBuka = false;

        valveTimer = millis();
        valveState = VALVE_OPENING;
      }

      break;


    case VALVE_OPENING:

      if (millis() - valveTimer > valvePulse) {

        outKranTutup = false;
        outKranBuka = false;

        valveTotalMoveTime += valvePulse;

        valveTimer = millis();
        valveState = VALVE_SETTLING;
      }

      break;


    case VALVE_CLOSING:

      if (millis() - valveTimer > valvePulse) {

        outKranTutup = false;
        outKranBuka = false;

        valveTotalMoveTime += valvePulse;

        valveTimer = millis();
        valveState = VALVE_SETTLING;
      }

      break;

    case VALVE_SETTLING:

      if (millis() - valveTimer > pressureCfg.settleTime) {

        float pressureNow = getPressureFiltered();

        if (learnReady && valvePulse > 0) {

          float delta = abs(pressureNow - lastPressureLearn);

          float rateMeasured = delta / valvePulse;

          // adaptive filter
          valveRate = valveRate * 0.8 + rateMeasured * 0.2;

          Serial.printf("Valve learn rate: %.6f bar/ms\n", valveRate);
        }

        learnReady = false;

        valveState = VALVE_IDLE;
        valveTotalMoveTime = 0;
      }

      break;
  }

  /* ================= SAFETY PROTECTION ================= */

  if (valveTotalMoveTime > VALVE_SAFETY_LIMIT) {

    outKranTutup = false;
    outKranBuka = false;

    Serial.println("⚠ Valve Safety Stop");

    valveState = VALVE_IDLE;
    valveTotalMoveTime = 0;
  }
}

float pressureFiltered = 0;

float getPressureFiltered() {

  SensorDataPressure p = getPressureSnapshot();

  float pressure = p.kg;

  // low pass filter
  pressureFiltered = pressureFiltered * 0.90 + pressure * 0.10;

  return pressureFiltered;
}

uint32_t calculatePulse(float pressure) {

  float error = abs(pressureCfg.target - pressure);

  float gain = 300;

  float pulseF = pressureCfg.pulseMin + gain * sqrt(max(error, 0.01f));

  uint32_t pulse = (uint32_t)pulseF;

  pulse = constrain(pulse,
                    pressureCfg.pulseMin,
                    pressureCfg.pulseMax);

  return pulse;
}
