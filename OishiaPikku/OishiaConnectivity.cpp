#include "OishiaConnectivity.h"
#include "OishiaNetworkRules.h"
#include "OishiaDashboard.h"
#include "OishiaBluetooth.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "OishiaSettingsJson.h"
#include <WiFi.h>
#include "OishiaHttpServer.h"
#include "OishiaAuth.h"
#include "OishiaConfig.h"
#include "OishiaClock.h"
#include <esp_heap_caps.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <time.h>

namespace {
using namespace OishiaNetworkRules;
QueueHandle_t petQueue = nullptr;
QueueHandle_t commandQueue = nullptr;
QueueHandle_t infoQueue = nullptr;
QueueHandle_t setupQueue = nullptr;
QueueHandle_t settingsQueue = nullptr;
QueueHandle_t historyQueue = nullptr;
QueueHandle_t eventQueue = nullptr;
OishiaSettings personalSettings;
uint32_t lastSettingsAt = 0;
bool settingsSaved = false;
OishiaHttpServer server(80);
OishiaAuth auth;
char responseBuffer[4096];
uint32_t pairingPasskey = 0;
DNSServer setupDns;
Preferences networkPreferences;
OishiaPetSnapshot pet = {};
OishiaHistory history = {};
OishiaEventLog eventLog = {};
OishiaNetworkInfo info = {};

struct Credentials {
  uint32_t version;
  char ssid[33];
  char password[65];
};
Credentials saved = {};
Credentials candidate = {};
bool storageReady = false;
bool haveSaved = false;
bool trial = false;
bool pendingTrial = false;
bool connecting = false;
bool starting = false;
bool mdnsReady = false;
bool manualSetup = false;
uint32_t attemptAt = 0;
uint32_t retryAt = 0;
uint32_t pendingAt = 0;
uint32_t setupAt = 0;
uint32_t onlineAt = 0;
uint32_t lastInfoAt = 0;
uint32_t lastCommandAt = 0;
bool commandSent = false;
char hostname[20];
const char *settingsResult = "idle";
bool bleResponse = false;
uint32_t bleSession = 0;
uint32_t bleRequestId = 0;
bool scanActive = false;
bool scanRequested = false;
bool scanFailed = false;
uint32_t scanStartedAt = 0;
bool forgetPending = false;
uint32_t forgetAt = 0;
bool pendingFromSetup = false;
bool browserTimeAccepted = false;

void configureClock() {
  const char *zone = oishiaPosixTimeZone(personalSettings.timezone);
  // Keep all sources on Google's leap-smeared timescale; mixing smeared and
  // non-smeared NTP sources can produce inconsistent results near leap seconds.
  if (zone) configTzTime(zone, "time1.google.com", "time2.google.com", "time3.google.com");
}

void publishInfo() {
  info.bluetoothConnected = oishiaBluetoothConnected();
  xQueueOverwrite(infoQueue, &info);
}

void startSetup(bool manuallyRequested) {
  if (!info.setupActive) {
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(info.hotspot, info.hotspotPassword, 1, 0, 2)) {
      Serial.println("Oishia: could not start setup hotspot; hold the button to retry.");
      return;
    }
    info.setupActive = true;
    setupDns.start(53, "*", WiFi.softAPIP());
    Serial.printf("Oishia setup: %s | Wi-Fi password: %s | PIN: %s | http://192.168.4.1\n", info.hotspot, info.hotspotPassword, info.accessKey);
  }
  manualSetup = manuallyRequested;
  setupAt = millis();
  publishInfo();
}

void beginAttempt(uint32_t now) {
  WiFi.disconnect(false, false);
  connecting = false;
  starting = true;
  attemptAt = now;
  info.connected = false;
  info.ip[0] = '\0';
  if (mdnsReady) { MDNS.end(); mdnsReady = false; }
  publishInfo();
}

void sendJson(int code, const JsonDocument &document) {
  size_t offset = bleResponse ? snprintf(responseBuffer, sizeof(responseBuffer),
      "{\"id\":%lu,\"status\":%d,\"body\":", (unsigned long)bleRequestId, code) : 0;
  size_t needed = measureJson(document);
  if (document.overflowed() || needed + offset + 2 >= sizeof(responseBuffer)) {
    if (bleResponse) {
      snprintf(responseBuffer, sizeof(responseBuffer), "{\"id\":%lu,\"status\":503,\"body\":{\"error\":\"Response capacity exceeded\"}}", (unsigned long)bleRequestId);
      replyOishiaBluetooth(bleSession, responseBuffer);
    } else server.send(503, "application/json", "{\"error\":\"Response capacity exceeded\"}");
    return;
  }
  offset += serializeJson(document, responseBuffer + offset, sizeof(responseBuffer) - offset);
  if (bleResponse) { responseBuffer[offset++] = '}'; responseBuffer[offset] = 0; replyOishiaBluetooth(bleSession, responseBuffer); }
  else {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(code, "application/json", responseBuffer);
  }
}

void reply(int code, const char *message) {
  JsonDocument document;
  document[code < 400 ? "message" : "error"] = message;
  sendJson(code, document);
}

bool authorizedKey(const char *supplied) {
  if (!auth.authorize(supplied, millis())) { reply(401, "Session expired. Hold the button for five seconds, then log in with the PIN."); return false; }
  if (bleResponse) authorizeOishiaBluetooth(bleSession);
  return true;
}

void handleLogin(const char *pin, const char *replace = nullptr) {
  int result = auth.checkPin(pin, info.accessKey, millis());
  if (result != 200) {
    reply(result, result == 403 ? "Hold Oishia's button for five seconds to enable login." :
                  result == 429 ? "Too many PIN attempts. Wait one minute." : "That PIN does not match Oishia.");
    return;
  }
  if (replace) auth.revoke(replace);
  uint8_t bytes[16]; esp_fill_random(bytes, sizeof(bytes));
  char token[33];
  for (size_t i = 0; i < sizeof(bytes); ++i) snprintf(token + i * 2, 3, "%02x", bytes[i]);
  if (!auth.issue(token, millis())) { reply(503, "All four sessions are in use. Lock another dashboard or wait for its session to expire."); return; }
  if (bleResponse) authorizeOishiaBluetooth(bleSession);
  JsonDocument document;
  document["token"] = token;
  document["expiresIn"] = OishiaAuth::SESSION_IDLE_MS / 1000;
  sendJson(200, document);
}

bool readJson(JsonDocument &document, bool requireAuth = true) {
  if (requireAuth && !authorizedKey(server.header("X-Oishia-Key"))) return false;
  const char *body = server.arg("plain");
  if (!*body || strlen(body) > OishiaHttpRequest::MAX_BODY) { reply(413, "Request is too large or empty."); return false; }
  if (deserializeJson(document, body, DeserializationOption::NestingLimit(3)) || !document.is<JsonObject>()) {
    reply(400, "Invalid request."); return false;
  }
  return true;
}

void handleSettings(const JsonDocument &document) {
  if (!storageReady) { reply(503, "Settings storage is unavailable. Restart and try again."); return; }
  OishiaSettings next = personalSettings;
  if (const char *error = parseOishiaSettings(document, next)) { reply(400, error); return; }
  bool changed = memcmp(&next,&personalSettings,sizeof(next)) != 0;
  if (changed) {
    if (settingsSaved && !elapsed(millis(), lastSettingsAt, 2000)) { reply(429, "Wait two seconds between settings changes."); return; }
    if (!saveOishiaSettings(networkPreferences, next)) { reply(503, "Settings could not be saved. Previous values are still active."); return; }
    personalSettings = next;
    configureClock();
    settingsSaved = true; lastSettingsAt = millis();
    xQueueOverwrite(settingsQueue, &personalSettings);
  }
  JsonDocument response;
  response["message"] = "Settings saved.";
  settingsJson(response["settings"].to<JsonObject>(), personalSettings);
  sendJson(200, response);
}

void handleStatus() {
  xQueuePeek(petQueue, &pet, 0);
  JsonDocument document;
  JsonObject p = document["pet"].to<JsonObject>();
  p["mood"] = pet.mood;
  p["message"] = pet.message;
  p["happiness"] = pet.happiness;
  p["friendship"] = pet.friendship;
  p["pets"] = pet.pets;
  p["visits"] = pet.visits;
  p["sleeping"] = pet.sleeping;
  p["muted"] = pet.muted;
  p["memoryHealthy"] = pet.memoryHealthy;
  p["memoryPending"] = pet.memoryPending;
  p["tiltReady"] = pet.tiltReady;
  if (pet.climateValid) { p["temperature"] = pet.temperature; p["humidity"] = pet.humidity; }
  else { p["temperature"] = nullptr; p["humidity"] = nullptr; }
  if (pet.lightValid) { p["light"] = pet.light; p["dark"] = pet.dark; }
  else { p["light"] = nullptr; p["dark"] = nullptr; }
  p["motion"] = pet.motionDetected;
  p["clockVisible"] = pet.clockVisible;
  p["quietHours"] = pet.quietHours;
  p["displayOff"] = pet.displayOff;
  p["pirWarming"] = pet.pirWarming;
  p["pirStuck"] = pet.pirStuck;
  p["presenceQuietSeconds"] = pet.presenceQuietSeconds;
  // Compatibility for older clients; this build has no distance sensor.
  p["distance"] = nullptr;
  JsonObject n = document["network"].to<JsonObject>();
  n["connected"] = info.connected;
  n["connecting"] = connecting || starting;
  n["setupActive"] = info.setupActive;
  n["hotspot"] = info.hotspot;
  n["hostname"] = hostname;
  char bluetoothName[20];
  snprintf(bluetoothName, sizeof(bluetoothName), "Oishia-%06lX", (unsigned long)((ESP.getEfuseMac() >> 24) & 0xffffff));
  n["bluetoothName"] = bluetoothName;
  n["ip"] = info.ip;
  n["ssid"] = info.connected ? WiFi.SSID() : String(haveSaved ? saved.ssid : "");
  n["settingsResult"] = settingsResult;
  n["storageReady"] = storageReady;
  n["testing"] = trial || pendingTrial;
  n["bluetoothReady"] = info.bluetoothReady;
  n["bluetoothConnected"] = oishiaBluetoothConnected();
  n["rssi"] = info.connected ? WiFi.RSSI() : 0;
  time_t clockNow = time(nullptr);
  JsonObject clock = document["time"].to<JsonObject>();
  clock["synced"] = oishiaClockTimeValid(clockNow);
  clock["timezone"] = personalSettings.timezone;
  clock["source"] = browserTimeAccepted ? "dashboard" : "NTP";
  if (oishiaClockTimeValid(clockNow)) {
    struct tm local = {};
    localtime_r(&clockNow, &local);
    char localText[24];
    strftime(localText, sizeof(localText), "%Y-%m-%d %H:%M:%S", &local);
    clock["local"] = localText;
  } else clock["local"] = nullptr;
  settingsJson(document["settings"].to<JsonObject>(), personalSettings);
  const OishiaSettings defaults;
  settingsJson(document["settingsDefaults"].to<JsonObject>(), defaults);
  document["device"]["name"] = personalSettings.petName;
  document["device"]["firmware"] = OISHIA_FIRMWARE_VERSION;
  document["device"]["apiVersion"] = OISHIA_API_VERSION;
  document["device"]["resetReason"] = int(esp_reset_reason());
  document["device"]["largestFreeBlock"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  document["device"]["networkStackFree"] = uxTaskGetStackHighWaterMark(nullptr);
  document["device"]["maxLoopMicros"] = pet.maxLoopMicros;
  document["device"]["freeHeap"] = ESP.getFreeHeap();
  document["device"]["minFreeHeap"] = ESP.getMinFreeHeap();
  document["device"]["historySamples"] = history.count;
  document["device"]["eventCount"] = eventLog.count;
  sendJson(200, document);
}

void handleHistory() {
  xQueuePeek(historyQueue, &history, 0);
  JsonDocument document;
  JsonArray uptime = document["uptimeSeconds"].to<JsonArray>();
  JsonArray epoch = document["epoch"].to<JsonArray>();
  JsonArray temperatures = document["temperature"].to<JsonArray>();
  JsonArray humidities = document["humidity"].to<JsonArray>();
  JsonArray lights = document["light"].to<JsonArray>();
  for (uint8_t i = 0; i < history.count; ++i) {
    const OishiaHistorySample &sample = history.samples[(history.start + i) % OISHIA_HISTORY_CAPACITY];
    uptime.add(sample.uptimeSeconds); if (sample.epoch) epoch.add(sample.epoch); else epoch.add(nullptr);
    if (sample.valid & 1) { temperatures.add(sample.temperatureTenths / 10.0f); humidities.add(sample.humidity); }
    else { temperatures.add(nullptr); humidities.add(nullptr); }
    if (sample.valid & 2) lights.add(sample.light); else lights.add(nullptr);
  }
  sendJson(200, document);
}

void handleEvents() {
  xQueuePeek(eventQueue, &eventLog, 0);
  JsonDocument document;
  JsonArray events = document["events"].to<JsonArray>();
  for (uint8_t i = 0; i < eventLog.count; ++i) {
    const OishiaEvent &entry = eventLog.events[(eventLog.start + i) % OISHIA_EVENT_CAPACITY];
    JsonObject item = events.add<JsonObject>();
    item["uptimeSeconds"] = entry.uptimeSeconds;
    if (entry.epoch) item["epoch"] = entry.epoch; else item["epoch"] = nullptr;
    item["type"] = entry.type; item["text"] = entry.text;
  }
  sendJson(200, document);
}

void handleCommand(const JsonDocument &document) {
  if (!document["action"].is<const char *>()) { reply(400, "Choose an action."); return; }
  const char *action = document["action"];
  OishiaCommand command = {};
  if (!strcmp(action, "love")) command.type = OishiaCommandType::Love;
  else if (!strcmp(action, "sleep")) command.type = OishiaCommandType::Sleep;
  else if (!strcmp(action, "wake")) command.type = OishiaCommandType::Wake;
  else if (!strcmp(action, "mute")) command.type = OishiaCommandType::Mute;
  else if (!strcmp(action, "unmute")) command.type = OishiaCommandType::Unmute;
  else if (!strcmp(action, "clock")) command.type = OishiaCommandType::PreviewClock;
  else if (!strcmp(action, "clear_events")) command.type = OishiaCommandType::ClearEvents;
  else if (!strcmp(action, "set_time")) {
    if (!document["epoch"].is<uint32_t>() || !validOishiaBrowserEpoch(document["epoch"].as<uint32_t>())) {
      reply(400, "Browser time is outside the supported range."); return;
    }
    command.type = OishiaCommandType::SetTime;
    command.epoch = document["epoch"].as<uint32_t>();
  }
  else if (!strcmp(action, "message")) {
    JsonString message = document["text"].as<JsonString>();
    if (!validMessage(message.c_str(), message.size())) { reply(400, "Use 1–21 printable English characters."); return; }
    command.type = OishiaCommandType::Message;
    memcpy(command.message, message.c_str(), message.size());
  } else { reply(400, "Unknown action."); return; }
  uint32_t now = millis();
  if (commandSent && !elapsed(now, lastCommandAt, 250)) { reply(429, "Give Oishia a moment, then try again."); return; }
  if (xQueueSend(commandQueue, &command, 0) != pdTRUE) { reply(503, "Oishia is busy. Please try again."); return; }
  if (command.type == OishiaCommandType::SetTime) browserTimeAccepted = true;
  commandSent = true;
  lastCommandAt = now;
  reply(202, "Sent to Oishia.");
}

void handleWifi(const JsonDocument &document) {
  if (!storageReady) { reply(503, "Wi-Fi settings storage is unavailable. Restart Oishia and try again."); return; }
  if (trial || pendingTrial || forgetPending || scanActive || scanRequested) { reply(409, "Oishia is busy with Wi-Fi. Try again in a moment."); return; }
  JsonString ssid = document["ssid"].as<JsonString>();
  JsonString password = document["password"].as<JsonString>();
  if (!validCredentials(ssid.c_str(), ssid.size(), password.c_str(), password.size())) {
    reply(400, "Use a 1–32 byte network name and an 8–63 character password (or leave it empty for an open network)."); return;
  }
  candidate = {};
  candidate.version = 1;
  memcpy(candidate.ssid, ssid.c_str(), ssid.size());
  memcpy(candidate.password, password.c_str(), password.size());
  pendingTrial = true;
  pendingAt = millis();
  pendingFromSetup = info.setupActive;
  settingsResult = "testing";
  reply(202, "Trying your network. Settings will be saved after a successful connection.");
}

void handleScan() {
  if (trial || pendingTrial || connecting || starting || forgetPending) { reply(409, "Wait for the current connection attempt to finish."); return; }
  JsonDocument document;
  if (scanFailed) { scanFailed = false; reply(503, "Network scan failed. Enter your network name manually or try again."); return; }
  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING || scanRequested) {
    document["scanning"] = true;
  } else if (count >= 0) {
    document["scanning"] = false;
    auto networks = document["networks"].to<JsonArray>();
    for (int i = 0; i < count && networks.size() < 8; ++i) {
      String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;
      bool duplicate = false;
      for (JsonObject network : networks) if (network["ssid"].as<String>() == ssid) duplicate = true;
      if (duplicate) continue;
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = ssid;
      network["rssi"] = WiFi.RSSI(i);
      network["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    scanActive = false;
    WiFi.scanDelete();
  } else {
    scanRequested = true;
    document["scanning"] = true;
  }
  sendJson(200, document);
}

void handleForget(const JsonDocument &document) {
  if (document["confirm"].as<String>() != "forget-wifi") { reply(400, "Confirm forgetting the saved Wi-Fi network."); return; }
  if (!storageReady || trial || pendingTrial || forgetPending || scanActive || scanRequested) { reply(409, "Wait until Wi-Fi setup finishes, then try again."); return; }
  forgetPending = true;
  forgetAt = millis();
  reply(202, "Returning to the setup hotspot. Friendship memory is kept.");
}

void configureServer() {
  server.on("/api/settings", HTTP_POST, []() { JsonDocument d; if (readJson(d)) handleSettings(d); });
  server.on("/api/auth", HTTP_POST, []() { JsonDocument d; if (readJson(d, false)) handleLogin(server.header("X-Oishia-Key"), d["replace"].as<const char *>()); });
  server.on("/api/logout", HTTP_POST, []() {
    JsonDocument d; if (!readJson(d)) return;
    auth.revoke(server.header("X-Oishia-Key")); reply(200, "Dashboard locked.");
  });
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Options", "DENY");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; frame-ancestors 'none'; form-action 'self'; base-uri 'none'");
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html; charset=utf-8", OISHIA_DASHBOARD_GZIP, sizeof(OISHIA_DASHBOARD_GZIP));
  });
  server.on("/api/status", HTTP_GET, []() { if (authorizedKey(server.header("X-Oishia-Key"))) handleStatus(); });
  server.on("/api/history", HTTP_GET, []() { if (authorizedKey(server.header("X-Oishia-Key"))) handleHistory(); });
  server.on("/api/events", HTTP_GET, []() { if (authorizedKey(server.header("X-Oishia-Key"))) handleEvents(); });
  server.on("/api/command", HTTP_POST, []() { JsonDocument d; if (readJson(d)) handleCommand(d); });
  server.on("/api/wifi", HTTP_POST, []() { JsonDocument d; if (readJson(d)) handleWifi(d); });
  server.on("/api/networks", HTTP_GET, []() { if (authorizedKey(server.header("X-Oishia-Key"))) handleScan(); });
  server.on("/api/wifi/forget", HTTP_POST, []() { JsonDocument d; if (readJson(d)) handleForget(d); });
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });
  server.onNotFound([]() {
    if (info.setupActive && server.method() == HTTP_GET &&
        server.client().localIP() == WiFi.softAPIP() && !server.uri().startsWith("/api/")) {
      server.sendHeader("Location", "http://192.168.4.1/");
      server.send(302, "text/plain", "Open Oishia setup");
    } else reply(404, "Page not found.");
  });
  server.begin();
}

void loadSettings() {
  uint32_t id = uint32_t(ESP.getEfuseMac() >> 24) & 0xffffff;
  snprintf(info.hotspot, sizeof(info.hotspot), "%s", "OishiaPikku");
  snprintf(hostname, sizeof(hostname), "oishia-%06lx", (unsigned long)id);
  storageReady = networkPreferences.begin("aru-net", false);
  if (storageReady) personalSettings = loadOishiaSettings(networkPreferences);
  configureClock();
  xQueueOverwrite(settingsQueue, &personalSettings);
  // Use the configured hotspot password even when a legacy key is stored.
  snprintf(info.hotspotPassword, sizeof(info.hotspotPassword), "%s", "27222222");
  // The four-digit dashboard/Bluetooth PIN is stored separately.
  String pin = storageReady ? networkPreferences.getString("pin", "") : "";
  if (!validPin(pin.c_str(), pin.length())) {
    snprintf(info.accessKey, sizeof(info.accessKey), "%04u", unsigned(esp_random() % 10000));
    if (storageReady && networkPreferences.putString("pin", info.accessKey) != 4) storageReady = false;
  } else pin.toCharArray(info.accessKey, sizeof(info.accessKey));
  if (storageReady && networkPreferences.getBytesLength("wifi") == sizeof(saved)) {
    networkPreferences.getBytes("wifi", &saved, sizeof(saved));
    haveSaved = saved.version == 1 && memchr(saved.ssid, 0, sizeof(saved.ssid)) && memchr(saved.password, 0, sizeof(saved.password));
    if (haveSaved) haveSaved = validCredentials(saved.ssid, strlen(saved.ssid), saved.password, strlen(saved.password));
  }
  pairingPasskey = esp_random() % 1000000;
  snprintf(info.pairingPasskey, sizeof(info.pairingPasskey), "%06lu", (unsigned long)pairingPasskey);
  auth.open(millis());
  Serial.printf("Oishia login PIN: %s | BLE pairing: %s\n", info.accessKey, info.pairingPasskey);
  if (!storageReady) Serial.println("Oishia: network storage unavailable; setup is session-only.");
}

void updateWifi(uint32_t now) {
  auth.tick(now);  // Retire idle tokens even when no client sends a request.
  uint8_t request;
  if (xQueueReceive(setupQueue, &request, 0) == pdTRUE) {
    auth.open(now);
    if (request == 2) {
      auth.reset(now);
      xQueueReset(commandQueue);
      pairingPasskey = esp_random() % 1000000;
      snprintf(info.pairingPasskey, sizeof(info.pairingPasskey), "%06lu", (unsigned long)pairingPasskey);
      bool bondsReset = resetOishiaBluetoothBonds(pairingPasskey);
      snprintf(info.accessKey, sizeof(info.accessKey), "%04u", unsigned(esp_random() % 10000));
      bool pinSaved = storageReady && networkPreferences.putString("pin", info.accessKey) == 4;
      if (!pinSaved) storageReady = false;
      settingsResult = pinSaved && bondsReset ? "access_reset" : "access_reset_error";
    }
    startSetup(true);
  }
  allowOishiaPairing(auth.commissioning(now));
  if (forgetPending && elapsed(now, forgetAt, 1000)) {
    forgetPending = false;
    if (networkPreferences.isKey("wifi") && !networkPreferences.remove("wifi")) {
      settingsResult = "storage_error";
      return;
    }
    haveSaved = false;
    saved = {};
    connecting = starting = false;
    WiFi.disconnect(false, false);
    info.connected = false;
    info.ip[0] = 0;
    if (mdnsReady) { MDNS.end(); mdnsReady = false; }
    settingsResult = "forgotten";
    startSetup(true);
  }
  if (scanRequested) {
    scanRequested = false;
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 120);
    scanActive = true;
    scanStartedAt = now;
  }
  if (scanActive && (WiFi.scanComplete() != WIFI_SCAN_RUNNING || elapsed(now, scanStartedAt, 15000))) {
    scanFailed = WiFi.scanComplete() < 0;
    if (scanFailed) WiFi.scanDelete();
    scanActive = false;
  }
  // First expose setup on the current network; changing the AP channel may break
  // the HTTP socket that accepted the request, so defer switching networks.
  if (pendingTrial && elapsed(now, pendingAt, 500) && !info.setupActive) {
    startSetup(false);
  }
  if (pendingTrial && elapsed(now, pendingAt, pendingFromSetup ? 1500 : 5000)) {
    pendingTrial = false;
    trial = true;
    startSetup(false);
    beginAttempt(now);
  }
  // Separate disconnect and begin so an old connection cannot validate a new password.
  if (starting) {
    if (!elapsed(now, attemptAt, 300)) return;
    const Credentials &credentials = trial ? candidate : saved;
    WiFi.begin(credentials.ssid, credentials.password);
    starting = false;
    connecting = true;
    attemptAt = millis();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (trial) {
      if (networkPreferences.putBytes("wifi", &candidate, sizeof(candidate)) == sizeof(candidate)) {
        saved = candidate;
        haveSaved = true;
        settingsResult = "saved";
      } else {
        settingsResult = "storage_error";
        trial = false;
        candidate = {};
        WiFi.disconnect(false, false);
        startSetup(false);
        if (haveSaved) beginAttempt(now);
        else connecting = false;
        return;
      }
      trial = false;
      candidate = {};
    }
    connecting = false;
    if (!info.connected) {
      info.connected = true;
      onlineAt = now;
      WiFi.localIP().toString().toCharArray(info.ip, sizeof(info.ip));
      mdnsReady = MDNS.begin(hostname);
      if (mdnsReady) MDNS.addService("http", "tcp", 80);
      Serial.printf("Oishia dashboard: http://%s | http://%s.local\n", info.ip, hostname);
      publishInfo();
    }
    if (info.setupActive && !pendingTrial && elapsed(now, onlineAt, SETUP_GRACE_MS) &&
        elapsed(now, setupAt, manualSetup ? MANUAL_SETUP_MS : SETUP_GRACE_MS)) {
      WiFi.softAPdisconnect(false);
      WiFi.mode(WIFI_STA);
      setupDns.stop();
      info.setupActive = false;
      publishInfo();
    }
    return;
  }
  if (info.connected) {
    info.connected = false;
    info.ip[0] = '\0';
    beginAttempt(now);
    return;
  }
  if (connecting && elapsed(now, attemptAt, CONNECT_TIMEOUT_MS)) {
    connecting = false;
    WiFi.disconnect(false, false);
    retryAt = now;
    startSetup(false);
    if (trial) {
      trial = false;
      candidate = {};
      settingsResult = "failed";
      if (haveSaved) beginAttempt(now);
    }
  } else if (!connecting && !scanActive && !scanRequested && haveSaved && elapsed(now, retryAt, RETRY_INTERVAL_MS)) {
    beginAttempt(now);
  }
}

void handleBluetooth() {
  updateOishiaBluetooth();
  OishiaBleRequest request;
  if (!receiveOishiaBleRequest(request)) return;
  bleResponse = true;
  bleSession = request.session;
  bleRequestId = 0;
  JsonDocument document;
  if (deserializeJson(document, request.json, DeserializationOption::NestingLimit(3)) ||
      !document.is<JsonObject>() || !document["id"].is<uint32_t>()) {
    reply(400, "Invalid JSON request or missing numeric id.");
  } else {
    bleRequestId = document["id"];
    String supplied = document["key"].as<String>();
    String operation = document["op"].as<String>();
    if (operation == "auth") handleLogin(supplied.c_str(), document["replace"].as<const char *>());
    else if (authorizedKey(supplied.c_str())) {
      if (operation == "logout") { auth.revoke(supplied.c_str()); reply(200, "Dashboard locked."); }
      else if (operation == "status") handleStatus();
      else if (operation == "history") handleHistory();
      else if (operation == "events") handleEvents();
      else if (operation == "settings") handleSettings(document);
      else if (operation == "command") handleCommand(document);
      else if (operation == "wifi") handleWifi(document);
      else if (operation == "networks") handleScan();
      else if (operation == "forget") handleForget(document);
      else reply(400, "Unknown operation.");
    }
  }
  bleResponse = false;
}

void networkTask(void *) {
  loadSettings();
  WiFi.persistent(false);
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  info.ready = true;
  if (haveSaved) beginAttempt(millis());
  else startSetup(false);
  configureServer();
  // Keep the BLE name compatible with the dashboard's Oishia- discovery filter.
  char bluetoothName[20];
  snprintf(bluetoothName, sizeof(bluetoothName), "Oishia-%06lX",
           (unsigned long)((ESP.getEfuseMac() >> 24) & 0xffffff));
  allowOishiaPairing(auth.commissioning(millis()));
  info.bluetoothReady = beginOishiaBluetooth(bluetoothName, pairingPasskey);
  Serial.println(info.bluetoothReady ? "Oishia Bluetooth BLE ready" : "Oishia Bluetooth unavailable; Wi-Fi remains available");
  publishInfo();
  for (;;) {
    updateWifi(millis());
    if (info.setupActive) setupDns.processNextRequest();
    xQueuePeek(petQueue, &pet, 0);
    xQueuePeek(historyQueue, &history, 0);
    xQueuePeek(eventQueue, &eventLog, 0);
    server.handleClient();
    handleBluetooth();
    if (elapsed(millis(), lastInfoAt, 1000)) { lastInfoAt = millis(); publishInfo(); }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
}  // namespace

bool beginConnectivity() {
  petQueue = xQueueCreate(1, sizeof(OishiaPetSnapshot));
  commandQueue = xQueueCreate(4, sizeof(OishiaCommand));
  infoQueue = xQueueCreate(1, sizeof(OishiaNetworkInfo));
  setupQueue = xQueueCreate(1, sizeof(uint8_t));
  settingsQueue = xQueueCreate(1, sizeof(OishiaSettings));
  historyQueue = xQueueCreate(1, sizeof(OishiaHistory));
  eventQueue = xQueueCreate(1, sizeof(OishiaEventLog));
  bool allocated = petQueue && commandQueue && infoQueue && setupQueue && settingsQueue && historyQueue && eventQueue;
  if (allocated && xTaskCreate(networkTask, "oishia-network", 8192, nullptr, 1, nullptr) == pdPASS) return true;
  if (petQueue) vQueueDelete(petQueue);
  if (commandQueue) vQueueDelete(commandQueue);
  if (infoQueue) vQueueDelete(infoQueue);
  if (setupQueue) vQueueDelete(setupQueue);
  if (settingsQueue) vQueueDelete(settingsQueue);
  if (historyQueue) vQueueDelete(historyQueue);
  if (eventQueue) vQueueDelete(eventQueue);
  petQueue = commandQueue = infoQueue = setupQueue = settingsQueue = historyQueue = eventQueue = nullptr;
  return false;
}

void publishPetSnapshot(const OishiaPetSnapshot &snapshot) { if (petQueue) xQueueOverwrite(petQueue, &snapshot); }
void publishOishiaHistory(const OishiaHistory &value) { if (historyQueue) xQueueOverwrite(historyQueue, &value); }
void publishOishiaEvents(const OishiaEventLog &value) { if (eventQueue) xQueueOverwrite(eventQueue, &value); }
bool receiveOishiaCommand(OishiaCommand &command) { return commandQueue && xQueueReceive(commandQueue, &command, 0) == pdTRUE; }
void getNetworkInfo(OishiaNetworkInfo &out) { if (infoQueue) xQueuePeek(infoQueue, &out, 0); }
void requestNetworkSetup() { uint8_t request = 1; if (setupQueue) xQueueOverwrite(setupQueue, &request); }

void requestOwnerReset() { uint8_t request = 2; if (setupQueue) xQueueOverwrite(setupQueue, &request); }

bool receiveOishiaSettings(OishiaSettings &out) { return settingsQueue && xQueueReceive(settingsQueue, &out, 0) == pdTRUE; }
