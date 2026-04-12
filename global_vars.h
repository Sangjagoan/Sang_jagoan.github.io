#pragma once

#include <WebServer.h>
#include <NTPClient.h>
#include <PubSubClient.h>  // kalau pakai MQTT client
#include <WiFiClientSecure.h>
#include <PZEM004Tv30.h>

typedef struct {
  float voltage;
  float current;
  float power;
  float energy;
  float frequency;
  float pf;
  float _voltage;
  float _current;
  float _power;
  float _energy;
  float _frequency;
  float _pf;
} PzemData;

// ================= ALARM SENSOR =================
typedef struct {
  float lowU;
  float losU;
  float lowP;
  float overP;
  float lowV;
  float low_V;
  float overV;
  float over_V;
  float overC;
  float over_C;
} CekSensor;

extern WebServer server;
extern NTPClient timeClient;
extern WiFiClientSecure espClient;

extern PubSubClient mqttClient;

extern PZEM004Tv30 pzem;
extern PZEM004Tv30 _pzem;
extern PzemData readings;

extern CekSensor cekSensor;