#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// Type definition for WiFi connection callback
typedef void (*WiFiConnectedCallback)();

// WiFi configuration - Use build flags or defaults
#ifndef WIFI_AP_NAME
#define WIFI_AP_NAME "EspAudio-Setup"
#endif

#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "likesbutts"
#endif

#ifndef WIFI_PORTAL_TIMEOUT
#define WIFI_PORTAL_TIMEOUT 180
#endif

// OTA configuration - Use build flags or defaults
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "espaudio"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "likesbutts"
#endif

#ifndef OTA_PORT
#define OTA_PORT 3232
#endif

// Function declarations
void handleLogs();
void initWiFi(WiFiConnectedCallback onConnected = nullptr);
void initOTA();
void startOTA();
void stopOTA();
bool connectToWiFi();
void saveWiFiCredentials(const String& ssid, const String& password);
void startConfigPortal();
bool startConfigPortalSafe();
void handleRoot();
void handleSave();
void handleWiFiLoop();

// External variables (defined in wifi_manager.cpp)
extern WebServer server;
extern DNSServer dnsServer;
extern Preferences wifiPrefs;
extern bool isConfigMode;
extern unsigned long portalStartTime;

#endif // WIFI_MANAGER_H