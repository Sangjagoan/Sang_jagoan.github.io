#include <stdexcept>
#include "sensor_manager.h"

#include <EasyUltrasonic.h>
#include "Kalman1D.h"
#include <math.h>

#define MEDIAN_SIZE 3

SensorDataPressure sensorP;
SensorDataUltrasonic sensorU;

#define PINPRESSURE 34

int getPriority(const char* type) {
  if (strcmp(type, "error") == 0) return 3;
  if (strcmp(type, "warning") == 0) return 2;
  return 1;
}

int getToneByPriority(int priority) {
  switch (priority) {
    case 3: return TONE_FATAL;    // error
    case 2: return TONE_VOLTAGE;  // warning
    default: return TONE_OFF;
  }
}

extern Kalman1D kalmanFilterPressure;
extern Kalman1D kalmanFilterUltrasonic;

bool kalmanInitialized = false;

bool pressureInitialized = false;
extern uint32_t pressureStartTime;

extern EasyUltrasonic RCWL;
extern LevelConfig levelCfg;

void publishAlarm(const char* level, const char* message, float value);
void setBuzzerTone(int tone);

portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;
/* =========================================================
   18) SENSOR FUNCTIONS
   ========================================================= */
void pressure() {

  sensorP.pressureValueAdc = analogRead(PINPRESSURE);
  sensorP.pressureValue = n_pressure();

  uint32_t age = millis() - pressureStartTime;

  // ===== PHASE 1 : SENSOR WARMUP (abaikan nilai) =====
  if (age < 2000) {
    sensorP.kalmanPressureValue = sensorP.pressureValue;  // ikuti saja
    return;                                               // jangan dipakai sistem
  }

  portENTER_CRITICAL(&sensorMux);
  // ===== PHASE 2 : FAST CATCH (kejar nilai asli cepat) =====
  if (age < 5000) {

    if (!pressureInitialized) {
      kalmanFilterPressure.setProcessNoise(5e-3f);  // respons cepat
      kalmanFilterPressure.setMeasurementNoise(1e-2f);
      kalmanFilterPressure.setEstimatedError(1.0f);
      pressureInitialized = true;
    }

    sensorP.kalmanPressureValue = kalmanFilterPressure.updateEstimate(sensorP.pressureValue);
  }

  // ===== PHASE 3 : NORMAL SMOOTH =====
  else {

    kalmanFilterPressure.setProcessNoise(1e-4f);  // halus
    kalmanFilterPressure.setMeasurementNoise(2e-2f);
    kalmanFilterPressure.setEstimatedError(1e-2f);

    sensorP.kalmanPressureValue = kalmanFilterPressure.updateEstimate(sensorP.pressureValue);
  }

  sensorP.kg = mapFloat(sensorP.kalmanPressureValue, 300, 4000, 0, 8);
  sensorP.psi = sensorP.kg * 14.2233f;
  sensorP.bar = sensorP.psi / 14.5038;

  portEXIT_CRITICAL(&sensorMux);
}

SensorDataPressure getPressureSnapshot() {

  SensorDataPressure snap;

  portENTER_CRITICAL(&sensorMux);
  snap = sensorP;
  portEXIT_CRITICAL(&sensorMux);

  return snap;
}

float medianFilter(float newVal) {
  static float buffer[MEDIAN_SIZE];
  static uint8_t idx = 0;
  static bool initialized = false;

  if (!initialized) {
    for (int i = 0; i < MEDIAN_SIZE; i++) buffer[i] = newVal;
    initialized = true;
  }

  buffer[idx++] = newVal;
  if (idx >= MEDIAN_SIZE) idx = 0;

  float temp[MEDIAN_SIZE];
  memcpy(temp, buffer, sizeof(temp));

  for (int i = 0; i < MEDIAN_SIZE - 1; i++) {
    for (int j = i + 1; j < MEDIAN_SIZE; j++) {
      if (temp[j] < temp[i]) {
        float t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }
    }
  }

  return temp[MEDIAN_SIZE / 2];
}

void ultrasonic() {

  static float lastValid = 0;
  static float smoothCm = -1;

  // ===== RAW =====
  float raw = RCWL.getDistanceCM();

  // ===== CLAMP DASAR =====
  if (raw < 2) raw = 2;
  if (raw > levelCfg.kedalamanPermukaan + 20)
    raw = levelCfg.kedalamanPermukaan + 20;

  // ===== MULTI-ECHO LOGIC =====
  float chosen = raw;

  if (lastValid != 0) {

    float delta = fabs(raw - lastValid);

    // kalau loncat terlalu jauh → kemungkinan echo salah
    if (delta > 60) {

      if (raw > lastValid) {
        // kemungkinan kena dinding → tolak
        chosen = lastValid;
      } else {
        // perubahan valid tapi terlalu cepat → pelankan
        chosen = lastValid + (raw - lastValid) * 0.3f;
      }
    }
  }

  lastValid = chosen;

  // ===== MEDIAN FILTER =====
  float filtered = medianFilter(chosen);

  float cmTemp;

  // ===== KALMAN =====
  if (!kalmanInitialized) {

    kalmanFilterUltrasonic.setProcessNoise(1e-4f);
    kalmanFilterUltrasonic.setMeasurementNoise(2e-2f);
    kalmanFilterUltrasonic.setEstimatedError(1e-2f);

    cmTemp = filtered;
    kalmanFilterUltrasonic.updateEstimate(filtered);

    kalmanInitialized = true;

  } else {

    cmTemp = kalmanFilterUltrasonic.updateEstimate(filtered);
  }

  // ===== SMOOTHING (EMA) =====
  if (smoothCm < 0) smoothCm = cmTemp;
  smoothCm = smoothCm * 0.9f + cmTemp * 0.1f;

  // ===== CLAMP FINAL =====
  smoothCm = constrain(smoothCm, levelCfg.jarakSensor, levelCfg.kedalamanPermukaan);

  // ===== PERSENTASE =====
  float percentTemp = mapFloat(
    smoothCm,
    levelCfg.jarakSensor,
    levelCfg.kedalamanPermukaan,
    100,
    0);

  percentTemp = constrain(percentTemp, 0, 100);

  // ===== SAVE =====
  portENTER_CRITICAL(&sensorMux);

  sensorU.non_kalman = raw;  // raw asli
  sensorU.cm = smoothCm;     // hasil final stabil
  sensorU.percentage = percentTemp;

  sensorU.tinggiAir = levelCfg.kedalamanPermukaan - smoothCm;
  sensorU.kedalamanAir = smoothCm - levelCfg.jarakSensor;

  portEXIT_CRITICAL(&sensorMux);
}

SensorDataUltrasonic getUltrasonicSnapshot() {

  SensorDataUltrasonic snap;

  portENTER_CRITICAL(&sensorMux);
  snap = sensorU;
  portEXIT_CRITICAL(&sensorMux);

  return snap;
}

void PZEM() {
  static float lastV1 = 220;
  static float lastV2 = 220;

  float v1 = pzem.voltage();
  lastV1 = safeValue(v1, lastV1);

  readings.voltage = lastV1;
  readings.current = pzem.current();
  readings.power = pzem.power();
  readings.energy = pzem.energy();
  readings.frequency = pzem.frequency();
  readings.pf = pzem.pf();

  vTaskDelay(pdMS_TO_TICKS(50));

  float v2 = _pzem.voltage();
  lastV2 = safeValue(v2, lastV2);
  readings._voltage = lastV2;
  readings._current = _pzem.current();
  readings._power = _pzem.power();
  readings._energy = _pzem.energy();
  readings._frequency = _pzem.frequency();
  readings._pf = _pzem.pf();
}

float safeValue(float newValue, float lastValue) {
  if (isnan(newValue)) return lastValue;
  if (newValue < 100 || newValue > 260) return lastValue;  // filter noise voltage
  return newValue;
}

/* =========================================================
   25) SENSOR CHECK (SETUP)
   ========================================================= */

bool checkAllSensorsSetup() {

  SensorDataUltrasonic u = getUltrasonicSnapshot();
  SensorDataPressure p = getPressureSnapshot();

  float pressure = p.pressureValueAdc;

  if (pressure < 200 || pressure > 1600) {
    Serial.println("❌ Pressure ERROR");
    return false;
  }

  float distanceIN = RCWL.getDistanceIN();
  float nonKalman = convertToCM(distanceIN);

  if (nonKalman > 500) {
    Serial.println("❌ Ultrasonic Los");
    return false;
  }

  if (nonKalman < 1) {
    Serial.println("❌ Ultrasonic Low");
    return false;
  }

  if (readings.voltage > 260) {
    Serial.println("❌ VOLTAGE OVERLOAD");
    return false;
  }

  if (readings._voltage > 260) {
    Serial.println("❌ _VOLTAGE OVERLOAD");
    return false;
  }

  if (readings.voltage < 160 && readings.voltage > 0) {
    Serial.println("❌ VOLTAGE LOW");
    return false;
  }

  if (readings._voltage < 160 && readings._voltage > 0) {
    Serial.println("❌ _VOLTAGE LOW");
    return false;
  }

  if (readings.voltage == 0) {
    Serial.println("❌ VOLTAGE ERROR");
    return false;
  }

  if (readings._voltage == 0) {
    Serial.println("❌ _VOLTAGE ERROR");
    return false;
  }

  return true;
}

/* =========================================================
   26) SENSOR CHECK (RUNTIME)
   ========================================================= */
bool checkAllSensors() {

  bool ok = true;
  int highestPriority = 0;
  static int lastTone = -1;
  SensorDataUltrasonic u = getUltrasonicSnapshot();
  SensorDataPressure p = getPressureSnapshot();

  auto handleAlarm = [&](const char* type, const char* msg, float value, int tone) {
    publishAlarm(type, msg, value);

    int p = getPriority(type);
    if (p > highestPriority) {
      highestPriority = p;

      if (tone != lastTone) {
        setBuzzerTone(tone);
        lastTone = tone;
      }
    }

    ok = false;
  };

  // 🔥 ALL CHECKS
  if (u.cm > cekSensor.losU) {
    Serial.println("⚠️ Ultrasonic Lost");
    handleAlarm("warning", "Ultrasonic Lost", u.cm, TONE_ULTRASONIC);
  }

  if (u.cm < cekSensor.lowU) {
    Serial.println("❌ Ultrasonic Low");
    handleAlarm("error", "Ultrasonic Low", u.cm, TONE_ULTRASONIC);
  }

  if (p.kalmanPressureValue < cekSensor.lowP) {
    handleAlarm("error", "Pressure Sensor Missing", p.kalmanPressureValue, TONE_PRESSURE_LOW);
  }

  if (p.kalmanPressureValue > cekSensor.overP) {
    handleAlarm("warning", "Pressure OVERLOAD", p.kalmanPressureValue, TONE_PRESSURE_HIGH);
  }

  if (readings.voltage > cekSensor.overV) {
    handleAlarm("warning", "Voltage Overload", readings.voltage, TONE_VOLTAGE);
  }

  if (readings._voltage > cekSensor.over_V) {
    handleAlarm("warning", "_Voltage Overload", readings._voltage, TONE_VOLTAGE);
  }

  if (readings.voltage < cekSensor.lowV) {
    handleAlarm("warning", "Voltage Low", readings.voltage, TONE_VOLTAGE);
  }

  if (readings._voltage < cekSensor.low_V) {
    handleAlarm("warning", "_Voltage Low", readings._voltage, TONE_VOLTAGE);
  }

  if (readings.current > cekSensor.overC) {
    handleAlarm("warning", "Current OverLoad", readings.current, TONE_CURRENT);
  }

  if (readings._current > cekSensor.over_C) {
    handleAlarm("warning", "_Current OverLoad", readings._current, TONE_CURRENT);
  }

  // 🔥 AUTO CLEAR
  static bool lastStateOk = true;

  if (ok && !lastStateOk) {
    publishAlarm("clear", "System Normal", 0);
    setBuzzerTone(TONE_OFF);  // 🔥 penting
    Serial.println("✅ System Normal");
  }

  lastStateOk = ok;

  return ok;
}

/* =========================================================
   27) UTILITY
   ========================================================= */
float mapFloat(float x, float inMin, float inMax, float outMin, float outMax) {
  float t = (x - inMin) / (inMax - inMin);
  t = constrain(t, 0.0f, 1.0f);
  return outMin + t * (outMax - outMin);
}

int n_pressure() {
  static uint16_t buffer[50];
  static uint8_t idx = 0;

  buffer[idx++] = analogRead(PINPRESSURE);

  if (idx >= 50) {
    idx = 0;
    uint32_t sum = 0;
    for (int i = 0; i < 50; i++) sum += buffer[i];
    return sum / 50;
  }

  return buffer[(idx + 49) % 50];  // last valid
}


// SensorDataUltrasonic level = getUltrasonicSnapshot();

// if (level.percentage < 30) {
//   bukaPompa();
// }
