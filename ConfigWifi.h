#ifndef CONFIGWIFI_H
#define CONFIGWIFI_H

#include <WiFi.h>
#include <ArduinoJson.h> 

extern bool networkServicesStarted;
extern char AP_SSID[32];
// ===== FUNCTIONS =====
void initWiFi();
void handleWiFi();
void initAPSSID();
bool isWiFiReady();
void initDeviceID();
String getClientIP();
void loadWiFiFromNVS();
void factoryResetWiFi();
void stopNetworkServices();
void startNetworkServices();
void saveWiFiToNVS(String s, String p);
bool connectAndTestWiFi(String ssid, String pass);
#endif
