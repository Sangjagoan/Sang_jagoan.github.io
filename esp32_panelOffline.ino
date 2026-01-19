#include <Arduino.h>
#include "Kalman1D.h"
#include <PZEM004Tv30.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP.h> // Required for ESP.restart()
#include <AsyncTCP.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <AsyncDelay.h>
#include <EasyUltrasonic.h>
#include <SimpleKalmanFilter.h>
#include "webpageCode.h"

#define PINPRESSURE 35        // pin sensor tekanan air
#define PINBUKASTOPKRAN 32    // kusus membuka stopkran
#define PINTUTUPSTOPKRAN 21   // khusus menutup stopkran
#define PINSTOPKRANUTARA 22   // kusus membuka stopkran
#define PINSTOPKRANSELATAN 23 // khusus menutup stopkran
#define PINSIBLEATAS 33       // ralay sible atas
#define PINTSIBLEBAWAH 14     // relay siblebawah
#define BUZZER 13

// Serial Pin PZEM
#define PZEM_RX_PIN 16  // pin RX ESP32 -> pin TX PZEM
#define PZEM_TX_PIN 17  // pin TX ESP32 -> pin RX PZEM
#define _PZEM_RX_PIN 18 // pin RX ESP32 -> pin TX PZEM
#define _PZEM_TX_PIN 19 // pin TX ESP32 -> pin RX PZEM

#define TRIGPIN 26 // Digital pin connected to the trig pin of the ultrasonic sensor
#define ECHOPIN 27 // Digital pin connected to the echo pin of the ultrasonic sensor

#define ON HIGH // untuk ngedupin relay
#define OFF LOW // untuk mematikan relay

// WiFi credentials
const char *ssid = "SANG_JAGOAN";
const char *password = "Sandibaru512";
// const char* hostname = "ESP32";

// tetapkan ip address
IPAddress staticIP(192, 168, 1, 129);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(208, 67, 222, 222);
IPAddress secondaryDNS(208, 67, 220, 220);

// SimpleKalmanFilter kalmanFilterUltrasonic(2, 2, 0.01);
Kalman1D kalmanFilterUltrasonic(1e-5f, 1e-2f, 1e-3f, 0.0f);

EasyUltrasonic RCWL; // Create the ultrasonic object
// delay
AsyncDelay delay_ulatrasonic;
AsyncDelay delay_localTime;

// Server port
WebServer server(80);

// configurasi pin sensor ultrasonic

// configurasi jam
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 25200);

// Initialize PZEM sensor
PZEM004Tv30 pzem(Serial1, PZEM_RX_PIN, PZEM_TX_PIN, 0x01);
PZEM004Tv30 _pzem(Serial2, _PZEM_RX_PIN, _PZEM_TX_PIN, 0x01);

// define array fastled
CRGB leds[1];

//////////////////////data untuk membaca pressure///////////
Kalman1D kalmanFilterPressure;
unsigned long lastReadTimePressure = 0;
float val;
float pressureValueAdc;
float pressureValue;
float kalmanPressureValue;
float kg;
float psi;

//////////////////////data untuk membaca PZEM///////////
float voltage, current, power, energy, frequency, pf;       // PZEM 1
float _voltage, _current, _power, _energy, _frequency, _pf; // PZEM 2

///////////////data NTP////////////////////////////
uint8_t hour;
uint8_t minute;
bool state;

unsigned long millisSaatini;
unsigned long waktuSebelumnyaBuka = 0;
unsigned long waktuSebelumnyastateStopKran = 0;

// data level air
int banyaknyaError = 0;
const int JARAK_BATAS_BAWAH = 239;
const int JARAK_BATAS_ATAS = 15;
float non_kalman, cm, percentage, tinggiAir, batasAir;

// Function prototypes
void handleRoot();
void handleData();
void configureWiFi();
void localTimer();
void ultrasonic();
void PZEM();
float zeroIfNan(float v);
void pressure();
float peta(float val, float min1, float max1, float mmin2, float max2);
float petaKalmanToKg(float kalmanValue, float inMin, float inMax, float outMin, float outMax);
void error();
void stopKran();
void buzzer();
void sible();
void buka();
void tutup();
void bukaDarurat();
void reconnecct();
void serialMonitor();
int n_pressure(void);

void setup()
{
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);

  Serial.println("monitoring tandon");
  Serial.println();

  // Inisialisasi Kalman
  kalmanFilterPressure.setProcessNoise(1e-5);
  kalmanFilterPressure.setMeasurementNoise(1e-2);
  kalmanFilterPressure.setEstimatedError(1e-3);
  kalmanPressureValue = 0.0f;

  RCWL.attach(TRIGPIN, ECHOPIN); // Attaches the ultrasonic sensor on the specified pins on the ultrasonic object
  // ultrasonic.attach(TRIGPIN, ECHOPIN, 3, 300); // Uncomment this line and comment the above line if you are using the Ping))) ultrasonic sensor

  // Star coutting
  delay_ulatrasonic.start(500, AsyncDelay::MILLIS);
  delay_localTime.start(500, AsyncDelay::MILLIS);

  // configurasi pin relay
  pinMode(PINBUKASTOPKRAN, OUTPUT);
  pinMode(PINTUTUPSTOPKRAN, OUTPUT);
  pinMode(PINSIBLEATAS, OUTPUT);
  pinMode(PINTSIBLEBAWAH, OUTPUT);

  pinMode(BUZZER, OUTPUT);

  // Connect to WiFi
  configureWiFi();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  // reset energy

  // pzem.resetEnergy();
  // _pzem.resetEnergy();

  // Start server
  server.begin();
}

unsigned long lastReadTimePzem;
void loop()
{
  millisSaatini = millis();
  // Handle web server client requests
  server.handleClient();

  if (millisSaatini - lastReadTimePzem > 500)
  {
    lastReadTimePzem = millisSaatini;
    localTimer();
    ultrasonic();
    pressure();
    PZEM();
  }

  sible();
  error();
  serialMonitor();
  // reconnecct();
}

void localTimer()
{
  timeClient.update();
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();

  // Asumsikan getHours() mengembalikan 0-23. Sesuaikan jika berbeda.
  if ((hour >= 18 && hour <= 23) || (hour >= 0 && hour <= 3))
  {
    state = true; // mode tidak aktif
  }
  else
  {                // jam 4:00 - 17:59
    state = false; // mode aktif
  }
}

/////////////////////////////////////////////////fungsion sensor tekanan///////////////////////////////////////////////

// Contoh kalman filter instance sudah ada, misalnya:
// KalmanFilter kalmanFilterPressure;

void pressure()
{

  pressureValueAdc = n_pressure(); // 0-4095
  float noise = (random(-100, 101)) / 1000.0f;
  pressureValue = pressureValueAdc + noise;

  kalmanPressureValue = kalmanFilterPressure.updateEstimate(pressureValue);

  kg = petaKalmanToKg(kalmanPressureValue, 300.0f, 4000.0f, 0.0f, 8.0f);
  psi = kg * 14.2233f;
}

int n_pressure(void)
{
  float v, tot_v = 0.0f;
  for (int i = 0; i < 50; i++)
  {
    v = analogRead(PINPRESSURE);
    tot_v += v;
    delay(2);
  }
  v = tot_v / 50.0f;
  return (int)v;
}

float petaKalmanToKg(float kalmanValue, float inMin, float inMax, float outMin, float outMax)
{
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
float peta(float val, float min1, float max1, float min2, float max2)
{
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

void ultrasonic()
{
  float distanceIN = RCWL.getDistanceIN(); // Read the distance in inches
  non_kalman = convertToCM(distanceIN);

  float measured_value = non_kalman + random(-300, 300) / 300.0;

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
void PZEM()
{
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
float zeroIfNan(float v)
{
  if (isnan(v))
  {
    v = 0;
  }
  return v;
}

/////////////////////////////////////////////fungtion sensor energy////////////////////////////////////////////////////////

// fungtion sible
bool stateError = true;
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

void sible()
{

  // mode saat normal
  if (stateError)
  {
    // jika state true, mode malam aktif
    // jika mode malam aktiv maka sible mengesi ketandon semua.
    if (state)
    {

      // saat malam
      //  awal mode malamm, hanya sible bawah yang hidup

      // jika saat awal mode malam kedalaman air lebih dari 20 cm
      // maka sible langsung hidup satu persatu.

      if (non_kalman >= 15 && stateSible1 && statusMalam == LOW)
      {

        digitalWrite(PINTSIBLEBAWAH, HIGH); // sible bawah hidup
        millisStatusMalam = millisSaatini;
        statusMalam = HIGH;
      }

      // reset millisDurasiMalam ke no8765432 =-09
      millisDurasiMalam = millisSaatini - millisStatusMalam;

      if (non_kalman >= 15 && stateSible1 && millisDurasiMalam > 40000)
      {
        digitalWrite(PINSIBLEATAS, HIGH);
        statusMalam = LOW;
        stateSible1 = false;
      }

      if (non_kalman <= 15 && stateSible1)
      {
        statusMalam = LOW;
        stateSible1 = false;
        digitalWrite(PINSIBLEATAS, HIGH);
      }

      if (!stateSible1)
      {

        if (cm >= 1 && cm <= 15)
        { ///// dari 1 sampai 13

          if (!statusSible1)
          {
            millisAwalSible1 = millisSaatini;
            statusSible1 = true;
            Serial.println("Berusaha mematikan semua sible");
          }

          lastTimeSible1 = millisSaatini - millisAwalSible1;

          if (lastTimeSible1 > 10000)
          {

            digitalWrite(PINSIBLEATAS, LOW);
            digitalWrite(PINTSIBLEBAWAH, LOW);
            Serial.println("proses sukses");
            statusSible1 = false;
          }
        }

        if (cm > 50)
        { // 50 keatas

          if (!statusSebelumnyaSible2)
          {

            millisAwalSible2 = millisSaatini;
            statusSebelumnyaSible2 = true;
            Serial.println("cek1");
          }

          lastTimeSible2 = millisSaatini - millisAwalSible2;

          if (!statusSible2 && lastTimeSible2 > 10000)
          {
            digitalWrite(PINTSIBLEBAWAH, HIGH);
            Serial.println("cek2");
            statusSible2 = true;
          }

          if (statusSible2 && lastTimeSible2 > 30000)
          {

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
    if (!state)
    {
      // saat siang

      if (stateSible && statusSiang == LOW)
      {

        if (pressureValueAdc < 1000)
        {
          digitalWrite(PINTUTUPSTOPKRAN, HIGH);
          digitalWrite(PINBUKASTOPKRAN, LOW);
        }

        digitalWrite(PINTSIBLEBAWAH, HIGH);
        millisStatusSiang = millisSaatini;
        statusSiang = HIGH;
        Serial.println("cek statusSiang");
      }

      millisDurasiSiang = millisSaatini - millisStatusSiang;

      if (stateSible && millisDurasiSiang > 20000)
      {

        statusSiang = LOW;
        stateSible = false;
        Serial.println("cek stateSible");
      }

      if (!stateSible)
      {

        if (cm >= 1 && cm <= 13)
        { // dari 1 sampai 10

          if (!statusSible3)
          {
            millisAwalSible3 = millisSaatini;
            statusSible3 = true;
          }

          lastTimeSible3 = millisSaatini - millisAwalSible3;

          if (lastTimeSible3 > 4000)
          {

            digitalWrite(PINSIBLEATAS, LOW);
            digitalWrite(PINTSIBLEBAWAH, LOW);
            statusSible3 = false;
          }
        }
        else
        {

          statusSible3 = false;
        }

        if (cm >= 11 && cm <= 15)
        { /// dari 11 sampai 13

          if (!statusSible4)
          {
            millisAwalSible4 = millisSaatini;
            statusSible4 = true;
          }

          lastTimeSible4 = millisSaatini - millisAwalSible4;

          if (lastTimeSible4 > 4000)
          {

            digitalWrite(PINSIBLEATAS, LOW);
            digitalWrite(PINTSIBLEBAWAH, HIGH);
            statusSible4 = false;
          }
        }
        else
        {
          statusSible4 = false;
        }

        if (cm > 30)
        {

          if (!statusSible5)
          {
            millisAwalSible5 = millisSaatini;
            statusSible5 = true;
            Serial.println("cek1");
          }

          lastTimeSible5 = millisSaatini - millisAwalSible5;

          if (lastTimeSible5 > 10000)
          {
            digitalWrite(PINSIBLEATAS, HIGH);
            digitalWrite(PINTSIBLEBAWAH, HIGH);
            statusSible5 = false;
            Serial.println("cek2");
          }
        }
        else
        {

          statusSible5 = false;
        }
      }

      stateSible1 = true;
    }

    stopKran();
  }
  else
  { // mode saat error

    buzzer();
    Serial.println("Cek sensor ultrasonic : " + String(stateError));
    digitalWrite(PINSIBLEATAS, LOW);
    digitalWrite(PINTSIBLEBAWAH, LOW);
  }
}

void stopKran()
{

  if (state)
  {
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    digitalWrite(PINBUKASTOPKRAN, HIGH);
  }

  if (!stateSible && !state)
  {

    if (kalmanPressureValue > 1530)
    {

      digitalWrite(PINTUTUPSTOPKRAN, LOW);
      digitalWrite(PINBUKASTOPKRAN, HIGH);
    }
    else if ((kalmanPressureValue > 1400) && (kalmanPressureValue < 1530))
    {
      bukaDarurat();
    }
    else if ((kalmanPressureValue > 1280) && (kalmanPressureValue < 1400))
    {
      buka();
    }
    else if (kalmanPressureValue < 1130)
    {
      tutup();
    }
    else
    {
      digitalWrite(PINTUTUPSTOPKRAN, LOW);
      digitalWrite(PINBUKASTOPKRAN, LOW);
    }
  }
}

void buka()
{

  if (millisSaatini - waktuSebelumnyaBuka > 40000)
  {

    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    digitalWrite(PINBUKASTOPKRAN, HIGH);
  }

  if (millisSaatini - waktuSebelumnyaBuka > 41000)
  {

    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    digitalWrite(PINBUKASTOPKRAN, LOW);
    waktuSebelumnyaBuka = millisSaatini;
  }
}

unsigned long waktuSebelumnyaTutup;
void tutup()
{

  if (millisSaatini - waktuSebelumnyaTutup > 40000)
  {

    digitalWrite(PINBUKASTOPKRAN, LOW);
    digitalWrite(PINTUTUPSTOPKRAN, HIGH);
  }

  if (millisSaatini - waktuSebelumnyaTutup > 41000)
  {

    digitalWrite(PINBUKASTOPKRAN, LOW);
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
    waktuSebelumnyaTutup = millisSaatini;
  }
}

unsigned long waktuSebelumnyaDarurat;
void bukaDarurat()
{

  if (millisSaatini - waktuSebelumnyaDarurat > 8000)
  {

    digitalWrite(PINBUKASTOPKRAN, HIGH);
    digitalWrite(PINTUTUPSTOPKRAN, LOW);
  }

  if (millisSaatini - waktuSebelumnyaDarurat > 9000)
  {

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

void error()
{

  if (millisSaatini - millisSebelumnyaError > 50)
  {

    if (cm == 0 && StatusError == LOW && !statustahanError)
    {
      StatusError = HIGH;
      tahanMillisError = millisSaatini;
    }

    waktuError = millisSaatini - tahanMillisError;

    if (cm == 0 && !statustahanError && waktuError > 8000)
    {
      statustahanError = true;
      stateError = false;
      Serial.println("error system ultrasonic: " + String(stateError));
    }

    if (cm > 0 && StatusError == HIGH)
    {
      statustahanError = false;
      StatusError = LOW;

      if (waktuError < 8000)
      { // ngecek apakah sensor ultrasonic offset
        banyaknyaError++;
      }
    }

    millisSebelumnyaError = millisSaatini;
  }
}

unsigned long waktuBuzzer = 0;
bool buzzerX = false;
void buzzer()
{
  if (millisSaatini - waktuBuzzer > 500)
  {
    buzzerX = !buzzerX;
    waktuBuzzer = millisSaatini;
  }

  digitalWrite(BUZZER, buzzerX);
}

// fungtion wifi
void configureWiFi()
{
  // FastLED.addLeds<WS2812, 15, GRB>(leds, 1);  // GRB ordering is typical

  // Menghubungkan ke wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Mengbungkan Ke Wifi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.println("berusaha menyambungkan kewifi");
    leds[0] = CRGB::Red;
    FastLED.show();
  }

  // Configuring static IP
  // WiFi.hostname(hostname);
  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("Failed to configure Static IP");
  }
  else
  {
    Serial.println("Static IP configured!");
  }

  Serial.println("Terhubung WiFi");
  Serial.println("");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  leds[0] = CRGB::Blue;
  FastLED.show();
  timeClient.begin();
}

// fungtion Serial monitor
unsigned long lastReadTimeSerialMonitor;
void serialMonitor()
{

  if (millisSaatini - lastReadTimeSerialMonitor > 1000)
  {
    // Serial.println("Voltage : " + String(voltage, 1) + "V | " + "Current : " + String(current, 2) + "A | " + "Power : " + String(power, 1) + "W | " + "Energy : " + String(energy, 3) + "Kwh | " + "frequency : " + String(frequency, 1) + "Hz | " + "pf : " + String(pf, 2));
    // Serial.println();

    // Serial.println("_Voltage : " + String(_voltage, 1) + "V | " + "_Current : " + String(_current, 2) + "A | " + "_Power : " + String(_power, 1) + "W | " + "_Energy : " + String(_energy, 3) + "Kwh | " + "_frequency : " + String(_frequency, 1) + "Hz | " + "_pf : " + String(_pf, 2));
    // Serial.println();

    Serial.println("kalmanPressureValue : " + String(kalmanPressureValue) + " | " + "nilai kg      : " + String(kg, 2) + "Kg | " + "nilai psi : " + String(psi, 2) + "Psi");
    Serial.println("nilai sensor        : " + String(pressureValueAdc, 4) + " | " + "pressureValue : " + String(pressureValue, 4));
    Serial.println();

    Serial.println("cm : " + String(cm, 2) + "Cm | " + "percentage : " + String(percentage, 2) + "% | " + "tinggiAir : " + String(tinggiAir, 2) + "Cm");
    Serial.println("non_kalman : " + String(non_kalman, 2) + "Cm | ");
    Serial.println();

    Serial.println("ultrasonic 0 : " + String(banyaknyaError));
    Serial.println();
    Serial.println("state        : " + String(state));
    Serial.println();

    Serial.println("stateError   : " + String(stateError));
    Serial.println();

    Serial.println("stateSible   : " + String(stateSible));
    Serial.println("stateSible1  : " + String(stateSible1));
    Serial.println();

    Serial.println("statusSiang  : " + String(statusSiang));
    Serial.println("statusMalam  : " + String(statusMalam));
    Serial.println();

    Serial.println("statusSible1 : " + String(statusSible1));
    Serial.println("statusSible2 : " + String(statusSible2));
    Serial.println("statusSible3 : " + String(statusSible3));
    Serial.println("statusSible4 : " + String(statusSible4));
    Serial.println("statusSible5 : " + String(statusSible5));
    Serial.println();

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

    lastReadTimeSerialMonitor = millisSaatini;
  }
}

unsigned long previousMillis;
void reconnecct()
{

  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (millisSaatini - previousMillis >= 3000))
  {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = millisSaatini;
  }
  else
  {
    Serial.println("Terhubung WiFi");
    Serial.println("");
    Serial.print("IP Address: ");
  }
}

// Web server handlers - No changes needed, looks good
void handleRoot()
{
  server.send(200, "text/html", webpageCode);
}

void handleData()
{
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
  // Serialize JSON to string
  String response;
  serializeJson(doc, response);

  server.send(200, "application/json", response);
}
