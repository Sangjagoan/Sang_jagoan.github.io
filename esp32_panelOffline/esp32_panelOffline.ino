#include <Arduino.h>
#include "esp_ota_ops.h"
#include "Kalman1D.h"
#include <PZEM004Tv30.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESP32httpUpdate.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <AsyncTCP.h>
#include <WiFiUdp.h>
#include <AsyncDelay.h>
#include <EasyUltrasonic.h>
#include <SimpleKalmanFilter.h>
#include "webpageCode.h"
#include "secrets.h"
#include "configWifi.h"

#define PINPRESSURE 35         // pin sensor tekanan air
#define PINBUKASTOPKRAN 32     // kusus membuka stopkran
#define PINTUTUPSTOPKRAN 21    // khusus menutup stopkran
#define PINSTOPKRANUTARA 22    // kusus membuka stopkran
#define PINSTOPKRANSELATAN 23  // khusus menutup stopkran
#define PINSIBLEATAS 33        // ralay sible atas
#define PINTSIBLEBAWAH 14      // relay siblebawah
#define BUZZER 13

// Serial Pin PZEM
#define PZEM_RX_PIN 16   // pin RX ESP32 -> pin TX PZEM
#define PZEM_TX_PIN 17   // pin TX ESP32 -> pin RX PZEM
#define _PZEM_RX_PIN 18  // pin RX ESP32 -> pin TX PZEM
#define _PZEM_TX_PIN 19  // pin TX ESP32 -> pin RX PZEM

#define KOSONG 25  // pin TX ESP32 -> pin RX PZEM

#define TRIGPIN 26  // Digital pin connected to the trig pin of the ultrasonic sensor
#define ECHOPIN 27  // Digital pin connected to the echo pin of the ultrasonic sensor

#define ON HIGH  // untuk ngedupin relay
#define OFF LOW  // untuk mematikan relay

//Github Repo detail
const char* github_owner = "Sangjagoan";                           // User Name Github
const char* github_repo = "ubdate-firmware-home-monitoringEsp32";  // alamat repositorys
const char* firmware_asset_name = "esp32_panelOffline.ino.bin";    //nama file firmware

//current Firmware Version
const char* currentFirmwareVersion = "2.2.1";

Kalman1D kalmanFilterUltrasonic(1e-5f, 1e-2f, 1e-3f, 0.0f);

EasyUltrasonic RCWL;  // Create the ultrasonic object
// delay
AsyncDelay Delay;

// Server port
WebServer server(80);

// configurasi jam
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200);

// Initialize PZEM sensor
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, 0x01);
PZEM004Tv30 _pzem(Serial1, _PZEM_RX_PIN, _PZEM_TX_PIN, 0x01);

//////////////////////data untuk membaca pressure///////////
Kalman1D kalmanFilterPressure;
float val;
float pressureValueAdc;
float pressureValue;
float kalmanPressureValue;
float kg;
float psi;

//////////////////////data untuk membaca PZEM///////////
float voltage, current, power, energy, frequency, pf;        // PZEM 1
float _voltage, _current, _power, _energy, _frequency, _pf;  // PZEM 2

///////////////data NTP////////////////////////////
uint8_t hour;
uint8_t minute;
bool state;

unsigned long millisSaatini;
unsigned long waktuSebelumnyaBuka = 0;
unsigned long waktuSebelumnyastateStopKran = 0;

// data level air
const int JARAK_BATAS_BAWAH = 239;
const int JARAK_BATAS_ATAS = 15;
float non_kalman, cm, percentage, tinggiAir, batasAir;


// fungtion sible
bool stateSible = true;
bool stateSible1 = true;

unsigned long millisDurasiSiang;
unsigned long millisDurasiMalam;

int statusSiang = LOW;
unsigned long millisStatusSiang;

int statusMalam = LOW;
unsigned long millisStatusMalam;

unsigned long lastTimeSible1 = 0;
unsigned long lastTimeSible2 = 0;
unsigned long lastTimeSible3 = 0;
unsigned long lastTimeSible4 = 0;
unsigned long lastTimeSible5 = 0;

unsigned long millisAwalSible1;
unsigned long millisAwalSible2;
unsigned long millisAwalSible3;
unsigned long millisAwalSible4;
unsigned long millisAwalSible5;

bool statusSebelumnyaSible2 = false;
bool statusSible1 = false;
bool statusSible2 = false;
bool statusSible3 = false;
bool statusSible4 = false;
bool statusSible5 = false;


TaskHandle_t task0;

bool systemHealthy = true;
bool systemRunEnabled = false;
bool serverRunning = false;
bool firstBootAfterOTA = false;
enum SystemState {
  SYS_BOOT,
  SYS_OTA,
  SYS_RUN,
  SYS_ERROR
};
SystemState systemState;

void taskHandler0(void* pvParameteres) {
  for (;;) {
    sible();
    handleWiFi();
    vTaskDelay(10);
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  Serial2.begin(9600);

  Serial.println("\nBooting up...");
  Serial.println("Current Firmware Version: " + String(currentFirmwareVersion));

  detectOTABootState();  // cek rollback
  Inisialisasi();        // pin, kalman, sensor
  connectToWiFi();

  // ===== OTA CHECK =====
  if (!firstBootAfterOTA && WiFi.status() == WL_CONNECTED) {
    checkForFirmwareUpdate();
  } else {
    Serial.println("OTA dilewati (first boot / WiFi off)");
  }

  // ===== SENSOR HEALTH =====
  systemHealthy = checkAllSensorsSetup();

  int counter = 0;
  while (!systemHealthy && counter < 50) {
    Serial.println("systemHealthy : " + String(systemHealthy));
    counter++;
    delay(200);
  }

  Serial.println("systemHealthy : " + String(systemHealthy));
  if (!systemHealthy) {
    systemState = SYS_ERROR;
    return;
  }

  Serial.println("✅ SYSTEM HEALTHY");

  // ===== CONFIRM OTA =====
  if (firstBootAfterOTA) {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("✅ Firmware confirmed valid");
  }

  startServices();  // task + server

  systemState = SYS_RUN;
  systemRunEnabled = true;
}

void loop() {

  server.handleClient();
  millisSaatini = millis();
  periodicHealthCheck();

  if (systemState == SYS_ERROR) {
    failSafeMode();
    return;
  }

  if (systemState != SYS_RUN || !systemRunEnabled) {
    delay(100);
    return;
  }

  localTimer();
  // serialMonitor();
  delay(10);
}


// ===== OTA STATE CHECK =====
void detectOTABootState() {

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;

  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    firstBootAfterOTA = true;
    Serial.println("🧠 First boot after OTA");
  }
}

void Inisialisasi() {
  // Inisialisasi Kalman
  kalmanFilterPressure.setProcessNoise(1e-5);
  kalmanFilterPressure.setMeasurementNoise(1e-2);
  kalmanFilterPressure.setEstimatedError(1e-3);
  kalmanPressureValue = 0.0f;

  // Init hardware
  RCWL.attach(TRIGPIN, ECHOPIN);
  pinMode(PINBUKASTOPKRAN, OUTPUT);
  pinMode(PINTUTUPSTOPKRAN, OUTPUT);
  pinMode(PINSIBLEATAS, OUTPUT);
  pinMode(PINTSIBLEBAWAH, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  Delay.start(1000, AsyncDelay::MILLIS);
}

bool checkAllSensorsSetup() {

  // ===== Pressure =====
  float pressure = n_pressure();  // ADC 0–4095
  if (pressure < 10 || pressure > 1700) {
    Serial.println("❌ Pressure ERROR");
    return false;
  }

  // ===== Ultrasonic =====
  float distanceIN = RCWL.getDistanceIN();
  non_kalman = convertToCM(distanceIN);
  if (non_kalman > 500) {
    Serial.println("❌ Ultrasonic Los");
    return false;
  }

  if (non_kalman < 1) {
    Serial.println("❌ Ultrasonic Low");
    return false;
  }

  // ===== PZEM =====
  PZEM();
  if (voltage > 260) {
    Serial.println("❌ VOLTAGE OVERLOAD");
    return false;
  }


  if (voltage < 160 && voltage > 0) {
    Serial.println("❌ VOLTAGE LOW");
    return false;
  }

  if (_voltage < 160 && _voltage > 0) {
    Serial.println("❌ _VOLTAGE LOW");
    return false;
  }

  if (voltage == 0) {
    Serial.println("❌ VOLTAGE ERROR");
    return false;
  }

  if (_voltage == 0) {
    Serial.println("❌ _VOLTAGE ERROR");
    return false;
  }

  if (current > 5.5) {
    Serial.println("❌ CURRENT OVER LOAD");
    return false;
  }

  if (_current > 9.5) {
    Serial.println("❌ _CURRENT OVER LOAD");
    return false;
  }
  return true;  // ✅ SEMUA OK
}

bool checkAllSensors() {

  // ===== Ultrasonic =====
  if (cm > 500) {
    Serial.println("❌ Ultrasonic Los");
    return false;
  }

  if (cm < 1) {
    Serial.println("❌ Ultrasonic Low");
    return false;
  }

  // ===== Pressure =====
  if (kalmanPressureValue < 10) {
    Serial.println("❌ Pressure LOW");
    return false;
  }

  if (kalmanPressureValue > 1600) {
    Serial.println("❌ Pressure OVERLOAD");
    return false;
  }

  // ===== PZEM =====
  if (voltage > 260) {
    Serial.println("❌ VOLTAGE OVERLOAD");
    return false;
  }


  if (voltage < 160 && voltage > 0) {
    Serial.println("❌ VOLTAGE LOW");
    return false;
  }

  if (_voltage < 160 && _voltage > 0) {
    Serial.println("❌ _VOLTAGE LOW");
    return false;
  }

  if (current > 5.5) {
    Serial.println("❌ CURRENT OVER LOAD");
    return false;
  }

  if (_current > 9.5) {
    Serial.println("❌ _CURRENT OVER LOAD");
    return false;
  }
  return true;  // ✅ SEMUA OK
}

// =================================================================================
// HELPER FUNCTIONS
// =================================================================================

// ===== START TASK & SERVER =====
void startServices() {
  xTaskCreatePinnedToCore(taskHandler0, "Task0", 10000, NULL, 1, &task0, 0);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.begin();
  serverRunning = true;
}

void checkForFirmwareUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping update check.");
    return;
  }

  String apiUrl = "https://api.github.com/repos/" + String(github_owner) + "/" + String(github_repo) + "/releases/latest";

  Serial.println("---------------------------------");
  Serial.println("Checking for new firmware...");
  Serial.println("Fetching release info from: " + apiUrl);

  HTTPClient http;
  http.begin(apiUrl);

  // 1. Increase timeout to prevent "IncompleteInput" on slow connections
  http.setTimeout(10000);

  http.addHeader("Authorization", "token " + String(github_pat));
  http.addHeader("Accept", "application/vnd.github.v3+json");
  http.setUserAgent("ESP32-OTA-Client");

  Serial.println("Sending API request...");
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("API request failed. HTTP code: %d\n", httpCode);
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

  if (latestVersion > currentFirmwareVersion) {

    Serial.println(">>> New firmware available! <<<");
    systemRunEnabled = false;

    prepareForOTA();

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
    downloadAndApplyFirmware(firmwareUrl);

  } else {
    Serial.println("Device is up to date. No update needed.");
    http.end();
  }
  Serial.println("---------------------------------");
}

void prepareForOTA() {
  systemRunEnabled = false;
  systemState = SYS_OTA;

  if (serverRunning) {
    server.stop();
    serverRunning = false;
  }

  if (task0 != NULL) {
    vTaskSuspend(task0);
  }

  WiFi.setSleep(false);
}

void downloadAndApplyFirmware(String url) {
  HTTPClient https;
  Serial.println("Starting firmware download from: " + url);

  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.setUserAgent("ESP32-OTA-Client");
  https.begin(url);
  https.addHeader("Accept", "application/octet-stream");
  https.addHeader("Authorization", "token " + String(github_pat));

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Download failed, HTTP code: %d\n", httpCode);
    https.end();
    return;
  }

  int contentLength = https.getSize();
  if (contentLength <= 0) {
    Serial.println("Error: Invalid content length.");
    https.end();
    return;
  }

  // Begin the OTA update
  if (!Update.begin(contentLength)) {
    Serial.printf("Update begin failed: %s\n", Update.errorString());
    https.end();
    return;
  }
  Serial.println("Writing firmware... (this may take a moment)");
  WiFiClient* stream = https.getStreamPtr();
  uint8_t buff[1024];
  size_t totalWritten = 0;
  int lastProgress = -1;

  while (totalWritten < contentLength) {
    int available = stream->available();
    if (available > 0) {
      int readLen = stream->read(buff, min((size_t)available, sizeof(buff)));
      if (readLen < 0) {
        Serial.println("Error reading from stream");
        Update.abort();
        https.end();
        return;
      }

      if (Update.write(buff, readLen) != readLen) {
        Serial.printf("Error: Update.write failed: %s\n", Update.errorString());
        Update.abort();
        https.end();
        return;
      }

      totalWritten += readLen;
      int progress = (int)((totalWritten * 100L) / contentLength);
      if (progress > lastProgress && (progress % 5 == 0 || progress == 100)) {
        Serial.printf("Progress: %d%%", progress);
        Serial.println();
        if (progress == 100) {
          Serial.println();
        } else {
          Serial.print("\r");
        }
        lastProgress = progress;
      }
    }
    delay(1);
  }
  Serial.println();

  if (totalWritten != contentLength) {
    Serial.printf("Error: Write incomplete. Wrote %d of %d bytes\n", totalWritten, contentLength);
    Update.abort();
  } else if (!Update.end()) {  // Finalize the update
    Serial.printf("Error: Update end failed: %s\n", Update.errorString());
  } else {
    Serial.println("Update complete! Restarting...");
    delay(1000);
    ESP.restart();
  }
  https.end();
}

int stateCheckAllSensors = LOW;
bool stateCheckAllSensorsLong = false;
unsigned long CheckAllSensorsMillis;
unsigned long durasionCheckAllSensors;

void periodicHealthCheck() {

  if (!checkAllSensors() && stateCheckAllSensors == LOW && !stateCheckAllSensorsLong) {
    CheckAllSensorsMillis = millis();
    stateCheckAllSensors = HIGH;
  }

  durasionCheckAllSensors = millis() - CheckAllSensorsMillis;
  if (!checkAllSensors() && !stateCheckAllSensorsLong && durasionCheckAllSensors > 6000) {
    stateCheckAllSensorsLong = true;
    systemHealthy = false;
    systemRunEnabled = false;
    systemState = SYS_ERROR;
  }

  if (checkAllSensors() && stateCheckAllSensors == HIGH) {
    stateCheckAllSensorsLong = false;
    stateCheckAllSensors = LOW;
  }
}

void localTimer() {
  timeClient.update();
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();

  // Asumsikan getHours() mengembalikan 0-23. Sesuaikan jika berbeda.
  if ((hour >= 18 && hour <= 23) || (hour >= 0 && hour <= 3)) {
    state = true;   // mode tidak aktif
  } else {          // jam 4:00 - 17:59
    state = false;  // mode aktif
  }
}

/////////////////////////////////////////////////fungsion sensor tekanan///////////////////////////////////////////////

// Contoh kalman filter instance sudah ada, misalnya:
// KalmanFilter kalmanFilterPressure;

void pressure() {

  pressureValueAdc = n_pressure();  // 0-4095
  float noise = (random(-100, 101)) / 1000.0f;
  pressureValue = pressureValueAdc + noise;

  kalmanPressureValue = kalmanFilterPressure.updateEstimate(pressureValue);

  kg = petaKalmanToKg(kalmanPressureValue, 300.0f, 4000.0f, 0.0f, 8.0f);
  psi = kg * 14.2233f;
}

int n_pressure(void) {
  float v, tot_v = 0.0f;
  for (int i = 0; i < 50; i++) {
    v = analogRead(PINPRESSURE);
    tot_v += v;
    delay(2);
  }
  v = tot_v / 50.0f;
  return (int)v;
}

float petaKalmanToKg(float kalmanValue, float inMin, float inMax, float outMin, float outMax) {
  float t = (kalmanValue - inMin) / (inMax - inMin);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  return outMin + t * (outMax - outMin);
}

/////////////////////////////////////////////////fungsion sensor tekanan///////////////////////////////////////////////

////////////////////////////////////////////////funsi sensor ultrasonic////////////////////////////////////////////////////

// Fungsi utilitas
float peta(float val, float min1, float max1, float min2, float max2) {
  // Guard against div by zero
  if (max1 - min1 == 0.0f)
    return min2;
  float t = (val - min1) / (max1 - min1);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  return min2 + t * (max2 - min2);
}

void ultrasonic() {
  float distanceIN = RCWL.getDistanceIN();  // Read the distance in inches
  non_kalman = convertToCM(distanceIN);
  // calculate the estimated value with Kalman Filter
  cm = kalmanFilterUltrasonic.updateEstimate(non_kalman);
  percentage = peta(cm, JARAK_BATAS_BAWAH, JARAK_BATAS_ATAS, 0, 100);
  percentage = constrain(percentage, 0, 100);
  tinggiAir = JARAK_BATAS_BAWAH - cm;
  batasAir = cm - JARAK_BATAS_ATAS;

  // Serial.println("nilai sensor        : " + String(cm));
  // Serial.println("kalmanPressureValue : " + String(non_kalman, 4));
  // Serial.println("");
}

////////////////////////////////////////////////funsi sensor ultrasonic////////////////////////////////////////////////////

/////////////////////////////////////////////fungtion sensor energy////////////////////////////////////////////////////////
void PZEM() {
  voltage = pzem.voltage();
  voltage = zeroIfNan(voltage);
  current = pzem.current();
  current = zeroIfNan(current);
  power = pzem.power();
  power = zeroIfNan(power);
  energy = pzem.energy();
  energy = zeroIfNan(energy);
  frequency = pzem.frequency();
  frequency = zeroIfNan(frequency);
  pf = pzem.pf();
  pf + zeroIfNan(pf);
  delay(10);
  _voltage = _pzem.voltage();
  _voltage = zeroIfNan(_voltage);
  _current = _pzem.current();
  _current = zeroIfNan(_current);
  _power = _pzem.power();
  _power = zeroIfNan(_power);
  _energy = _pzem.energy();
  _energy = zeroIfNan(_energy);
  _frequency = _pzem.frequency();
  _frequency = zeroIfNan(_frequency);
  _pf = _pzem.pf();
  _pf = zeroIfNan(_pf);
}

// convert isnan ke nol
float zeroIfNan(float v) {
  if (isnan(v)) {
    v = 0;
  }
  return v;
}

/////////////////////////////////////////////fungtion sensor energy////////////////////////////////////////////////////////

void setSible(bool atas, bool bawah) {
  digitalWrite(PINSIBLEATAS, atas ? HIGH : LOW);
  digitalWrite(PINTSIBLEBAWAH, bawah ? HIGH : LOW);
}
void sible() {

  // mode saat normal
  if (systemRunEnabled) {
    // jika state true, mode malam aktif
    // jika mode malam aktiv maka sible mengesi ketandon semua.

    if (Delay.isExpired()) {
      PZEM();
      ultrasonic();
      pressure();
      Delay.repeat();
    }

    if (state) {

      // saat malam
      //  awal mode malamm, hanya sible bawah yang hidup

      // jika saat awal mode malam kedalaman air lebih dari 20 cm
      // maka sible langsung hidup satu persatu.

      if (non_kalman >= 15 && stateSible1 && statusMalam == LOW) {

        digitalWrite(PINTSIBLEBAWAH, HIGH);  // sible bawah hidup
        millisStatusMalam = millisSaatini;
        statusMalam = HIGH;
      }

      // reset millisDurasiMalam ke no8765432 =-09
      millisDurasiMalam = millisSaatini - millisStatusMalam;

      if (non_kalman >= 15 && stateSible1 && millisDurasiMalam > 20000) {
        digitalWrite(PINSIBLEATAS, HIGH);
        statusMalam = LOW;
        stateSible1 = false;
      }

      if (non_kalman < 15 && stateSible1) {
        statusMalam = LOW;
        stateSible1 = false;
        digitalWrite(PINSIBLEATAS, LOW);
        digitalWrite(PINTSIBLEBAWAH, LOW);
      }

      if (!stateSible1) {

        if (cm >= 1 && cm <= 15) {  ///// dari 1 sampai 13

          if (!statusSible1) {
            millisAwalSible1 = millisSaatini;
            statusSible1 = true;
            Serial.println("Berusaha mematikan semua sible");
          }

          lastTimeSible1 = millisSaatini - millisAwalSible1;

          if (lastTimeSible1 > 10000) {

            digitalWrite(PINSIBLEATAS, LOW);
            digitalWrite(PINTSIBLEBAWAH, LOW);
            Serial.println("proses sukses");
            statusSible1 = false;
          }
        }

        if (cm > 50) {  // 50 keatas

          if (!statusSebelumnyaSible2) {

            millisAwalSible2 = millisSaatini;
            statusSebelumnyaSible2 = true;
            Serial.println("cek1");
          }

          lastTimeSible2 = millisSaatini - millisAwalSible2;

          if (!statusSible2 && lastTimeSible2 > 10000) {
            digitalWrite(PINTSIBLEBAWAH, HIGH);
            Serial.println("cek2");
            statusSible2 = true;
          }

          if (statusSible2 && lastTimeSible2 > 30000) {

            digitalWrite(PINSIBLEATAS, HIGH);
            Serial.println("cek3");
            statusSebelumnyaSible2 = false;
            statusSible2 = false;
          }
        }
      }

      stateSible = true;
    }

    // mode siang
    if (!state) {
      // saat siang

      if (stateSible && statusSiang == LOW) {

        if (pressureValueAdc < 1000) {
          digitalWrite(PINTUTUPSTOPKRAN, HIGH);
          digitalWrite(PINBUKASTOPKRAN, LOW);
        }

        digitalWrite(PINTSIBLEBAWAH, HIGH);
        millisStatusSiang = millisSaatini;
        statusSiang = HIGH;
        Serial.println("cek statusSiang");
      }

      millisDurasiSiang = millisSaatini - millisStatusSiang;

      if (stateSible && millisDurasiSiang > 20000) {

        statusSiang = LOW;
        stateSible = false;
        Serial.println("cek stateSible");
      }

      if (!stateSible) {

        if (cm >= 1 && cm <= 13) {  // dari 1 sampai 10

          if (!statusSible3) {
            millisAwalSible3 = millisSaatini;
            statusSible3 = true;
          }

          lastTimeSible3 = millisSaatini - millisAwalSible3;

          if (lastTimeSible3 > 4000) {

            digitalWrite(PINSIBLEATAS, LOW);
            digitalWrite(PINTSIBLEBAWAH, LOW);
            statusSible3 = false;
          }
        } else {

          statusSible3 = false;
        }

        if (cm >= 14 && cm <= 16) {  /// dari 11 sampai 13

          if (!statusSible4) {
            millisAwalSible4 = millisSaatini;
            statusSible4 = true;
          }

          lastTimeSible4 = millisSaatini - millisAwalSible4;

          if (lastTimeSible4 > 4000) {

            digitalWrite(PINSIBLEATAS, LOW);
            digitalWrite(PINTSIBLEBAWAH, HIGH);
            statusSible4 = false;
          }
        } else {
          statusSible4 = false;
        }

        if (cm > 30) {

          if (!statusSible5) {
            millisAwalSible5 = millisSaatini;
            statusSible5 = true;
            Serial.println("cek1");
          }

          lastTimeSible5 = millisSaatini - millisAwalSible5;

          if (lastTimeSible5 > 10000) {
            digitalWrite(PINSIBLEATAS, HIGH);
            digitalWrite(PINTSIBLEBAWAH, HIGH);
            statusSible5 = false;
            Serial.println("cek2");
          }
        } else {

          statusSible5 = false;
        }
      }

      stateSible1 = true;
    }

    stopKran();
  }
}

void failSafeMode() {
  digitalWrite(PINSIBLEATAS, LOW);
  digitalWrite(PINTSIBLEBAWAH, LOW);
  digitalWrite(PINBUKASTOPKRAN, LOW);
  digitalWrite(PINTUTUPSTOPKRAN, LOW);
  buzzer();
}


void stopKran() {

  if (state) {
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    digitalWrite(PINBUKASTOPKRAN, HIGH);
  }

  if (!stateSible && !state) {

    if (kalmanPressureValue > 1530) {

      digitalWrite(PINTUTUPSTOPKRAN, LOW);
      digitalWrite(PINBUKASTOPKRAN, HIGH);
    } else if ((kalmanPressureValue > 1400) && (kalmanPressureValue < 1530)) {
      bukaDarurat();
    } else if ((kalmanPressureValue > 1280) && (kalmanPressureValue < 1400)) {
      buka();
    } else if (kalmanPressureValue < 1130) {
      tutup();
    } else {
      digitalWrite(PINTUTUPSTOPKRAN, LOW);
      digitalWrite(PINBUKASTOPKRAN, LOW);
    }
  }
}

void buka() {

  if (millisSaatini - waktuSebelumnyaBuka > 40000) {

    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    digitalWrite(PINBUKASTOPKRAN, HIGH);
  }

  if (millisSaatini - waktuSebelumnyaBuka > 41000) {

    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    digitalWrite(PINBUKASTOPKRAN, LOW);
    waktuSebelumnyaBuka = millisSaatini;
  }
}

unsigned long waktuSebelumnyaTutup;
void tutup() {

  if (millisSaatini - waktuSebelumnyaTutup > 40000) {

    digitalWrite(PINBUKASTOPKRAN, LOW);
    digitalWrite(PINTUTUPSTOPKRAN, HIGH);
  }

  if (millisSaatini - waktuSebelumnyaTutup > 41000) {

    digitalWrite(PINBUKASTOPKRAN, LOW);
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    waktuSebelumnyaTutup = millisSaatini;
  }
}

unsigned long waktuSebelumnyaDarurat;
void bukaDarurat() {

  if (millisSaatini - waktuSebelumnyaDarurat > 8000) {

    digitalWrite(PINBUKASTOPKRAN, HIGH);
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
  }

  if (millisSaatini - waktuSebelumnyaDarurat > 9000) {

    digitalWrite(PINBUKASTOPKRAN, LOW);
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    waktuSebelumnyaDarurat = millisSaatini;
  }
}

unsigned long millisSebelumnyaError;
int StatusError = LOW;
bool statustahanError = false;

unsigned long tahanMillisError;
unsigned long waktuError;

unsigned long waktuBuzzer = 0;
bool buzzerX = false;
void buzzer() {
  if (millisSaatini - waktuBuzzer > 500) {
    buzzerX = !buzzerX;
    waktuBuzzer = millisSaatini;
  }

  digitalWrite(BUZZER, buzzerX);
}

// fungtion Serial monitor
unsigned long lastReadTimeSerialMonitor;
void serialMonitor() {
  unsigned long now = millis();
  if (now - lastReadTimeSerialMonitor > 1000) {
    lastReadTimeSerialMonitor = now;
    // Serial.println("Voltage : " + String(voltage, 1) + "V | " + "Current : " + String(current, 2) + "A | " + "Power : " + String(power, 1) + "W | " + "Energy : " + String(energy, 3) + "Kwh | " + "frequency : " + String(frequency, 1) + "Hz | " + "pf : " + String(pf, 2));
    // Serial.println();

    // Serial.println("_Voltage : " + String(_voltage, 1) + "V | " + "_Current : " + String(_current, 2) + "A | " + "_Power : " + String(_power, 1) + "W | " + "_Energy : " + String(_energy, 3) + "Kwh | " + "_frequency : " + String(_frequency, 1) + "Hz | " + "_pf : " + String(_pf, 2));
    // Serial.println();

    // Serial.println("kalmanPressureValue : " + String(kalmanPressureValue) + " | " + "nilai kg      : " + String(kg, 2) + "Kg | " + "nilai psi : " + String(psi, 2) + "Psi");
    // Serial.println("nilai sensor        : " + String(pressureValueAdc, 4) + " | " + "pressureValue : " + String(pressureValue, 4));
    // Serial.println();

    // Serial.println("cm : " + String(cm, 2) + "Cm | " + "percentage : " + String(percentage, 2) + "% | " + "tinggiAir : " + String(tinggiAir, 2) + "Cm");
    // Serial.println("non_kalman : " + String(non_kalman, 2) + "Cm | ");
    // Serial.println();

    // Serial.println("ultrasonic 0 : " + String(banyaknyaError));
    // Serial.println();
    // Serial.println("state        : " + String(state));
    // Serial.println();

    Serial.println("systemRunEnabled   : " + String(systemRunEnabled));
    // Serial.println();

    // Serial.println("stateSible   : " + String(stateSible));
    // Serial.println("stateSible1  : " + String(stateSible1));
    // Serial.println();

    // Serial.println("statusSiang  : " + String(statusSiang));
    // Serial.println("statusMalam  : " + String(statusMalam));
    // Serial.println();

    // Serial.println("statusSible1 : " + String(statusSible1));
    // Serial.println("statusSible2 : " + String(statusSible2));
    // Serial.println("statusSible3 : " + String(statusSible3));
    // Serial.println("statusSible4 : " + String(statusSible4));
    // Serial.println("statusSible5 : " + String(statusSible5));
    // Serial.println();

    // Serial.print(val);

    // Serial.println();

    Serial.println(timeClient.getFormattedTime());

    // Serial.println("");

    // Serial.println("");
    // Serial.println("lastTimeSible1        : " + String(lastTimeSible1));
    // Serial.println("lastTimeSible2        : " + String(lastTimeSible2));
    // Serial.println("lastTimeSible3        : " + String(lastTimeSible3));
    // Serial.println("lastTimeSible4        : " + String(lastTimeSible4));
    // Serial.println("lastTimeSible5        : " + String(lastTimeSible5));
    // Serial.println("millisDurasiMalam : " + String(millisDurasiMalam));
    // Serial.println("millisStatusSiang : " + String(millisDurasiSiang));
    // Serial.println("");

    // Serial.println("millisDurasiMalam : " + String(millisDurasiMalam));

    // Serial.println("statusSebelumnyaSible2 : " + String(statusSebelumnyaSible2));
  }
}

// Web server handlers - No changes needed, looks good
void handleRoot() {
  server.send(200, "text/html", webpageCode);
}

void handleData() {
  // Create JSON document
  DynamicJsonDocument doc(1024);

  doc["voltage"] = voltage;
  doc["current"] = current;
  doc["power"] = power;
  doc["frequency"] = frequency;

  doc["_voltage"] = _voltage;
  doc["_current"] = _current;
  doc["_power"] = _power;
  doc["_frequency"] = _frequency;

  doc["level"] = percentage;
  doc["jarak"] = batasAir;
  doc["tinggi"] = tinggiAir;


  doc["kg"] = kg;
  doc["psi"] = psi;
  doc["kalmanPressureValue"] = kalmanPressureValue;

  doc["version"] = String(currentFirmwareVersion);
  // Serialize JSON to string-
  String response;
  serializeJson(doc, response);

  server.send(200, "application/json", response);
}
