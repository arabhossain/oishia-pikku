#pragma once
#include <stdint.h>
#include "OishiaSettings.h"

enum class OishiaCommandType : uint8_t { Love, Sleep, Wake, Message, Mute, Unmute, PreviewClock, SetTime, ClearEvents };

struct OishiaCommand {
  OishiaCommandType type;
  char message[22];
  uint32_t epoch;
};

// Copies cross tasks through queues. No pet/display objects are shared.
struct OishiaPetSnapshot {
  char mood[16];
  char message[48];
  uint32_t pets;
  uint32_t visits;
  uint8_t friendship;
  uint8_t happiness;
  bool sleeping;
  bool muted;
  bool memoryHealthy;
  bool memoryPending;
  bool tiltReady;
  uint32_t maxLoopMicros;
  bool climateValid;
  bool lightValid;
  bool motionDetected;
  bool clockVisible;
  bool quietHours;
  bool displayOff;
  bool pirWarming;
  bool pirStuck;
  bool dark;
  uint32_t presenceQuietSeconds;
  float temperature;
  float humidity;
  int light;
};

constexpr uint8_t OISHIA_HISTORY_CAPACITY = 60;
constexpr uint8_t OISHIA_EVENT_CAPACITY = 16;
struct OishiaHistorySample {
  uint32_t uptimeSeconds;
  uint32_t epoch;
  int16_t temperatureTenths;
  uint8_t humidity;
  uint16_t light;
  uint8_t valid;  // bit 0 climate, bit 1 light
};
struct OishiaHistory {
  uint8_t count;
  uint8_t start;
  OishiaHistorySample samples[OISHIA_HISTORY_CAPACITY];
};
struct OishiaEvent {
  uint32_t uptimeSeconds;
  uint32_t epoch;
  char type[12];
  char text[32];
};
struct OishiaEventLog {
  uint8_t count;
  uint8_t start;
  OishiaEvent events[OISHIA_EVENT_CAPACITY];
};

struct OishiaNetworkInfo {
  bool ready;
  bool connected;
  bool setupActive;
  bool bluetoothReady;
  bool bluetoothConnected;
  char hotspot[20];
  char accessKey[5];
  char pairingPasskey[7];
  char hotspotPassword[13];
  char ip[16];
};

bool beginConnectivity();
void publishPetSnapshot(const OishiaPetSnapshot &snapshot);
void publishOishiaHistory(const OishiaHistory &history);
void publishOishiaEvents(const OishiaEventLog &events);
bool receiveOishiaCommand(OishiaCommand &command);
void getNetworkInfo(OishiaNetworkInfo &info);
void requestNetworkSetup();
void requestOwnerReset();

bool receiveOishiaSettings(OishiaSettings &settings);
