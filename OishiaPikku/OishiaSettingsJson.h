#pragma once
#include <ArduinoJson.h>
#include "OishiaSettings.h"

inline void settingsJson(JsonObject out, const OishiaSettings &settings) {
  out["petName"] = settings.petName;
  out["ownerName"] = settings.ownerName;
  out["tiltInvert"] = bool(settings.tiltInvert);
  out["autoSleep"] = bool(settings.autoSleep);
  out["darkThreshold"] = settings.darkThreshold;
  out["brightThreshold"] = settings.brightThreshold;
  out["sleepAfterSeconds"] = settings.sleepAfterSeconds;
  out["motionCooldownSeconds"] = settings.motionCooldownSeconds;
  out["timezone"] = settings.timezone;
  out["clockWhenAway"] = bool(settings.clockWhenAway);
  out["clock24Hour"] = bool(settings.clock24Hour);
  out["clockAfterSeconds"] = settings.clockAfterSeconds;
  out["portraitEnabled"] = bool(settings.portraitEnabled);
  out["clockStyle"] = settings.clockStyle;
  out["clockShowSeconds"] = bool(settings.clockShowSeconds);
  out["displayDayContrast"] = settings.displayDayContrast;
  out["displayNightContrast"] = settings.displayNightContrast;
  out["displayOffQuiet"] = bool(settings.displayOffQuiet);
  out["soundProfile"] = settings.soundProfile;
  out["quietHoursEnabled"] = bool(settings.quietHoursEnabled);
  out["quietStartHour"] = settings.quietStartHour; out["quietStartMinute"] = settings.quietStartMinute;
  out["quietEndHour"] = settings.quietEndHour; out["quietEndMinute"] = settings.quietEndMinute;
  out["comfortableMinTenths"] = settings.comfortableMinTenths;
  out["comfortableMaxTenths"] = settings.comfortableMaxTenths;
  out["comfortableMinHumidity"] = settings.comfortableMinHumidity;
  out["comfortableMaxHumidity"] = settings.comfortableMaxHumidity;
  out["pirStuckSeconds"] = settings.pirStuckSeconds;
  JsonArray routines = out["routines"].to<JsonArray>();
  for (const auto &routine : settings.routines) {
    JsonObject r = routines.add<JsonObject>(); r["enabled"] = bool(routine.enabled); r["action"] = routine.action;
    r["hour"] = routine.hour; r["minute"] = routine.minute; r["weekdays"] = routine.weekdays; r["message"] = routine.message;
  }
}

// Return a validation error, or fill next with a complete settings request.
inline const char *parseOishiaSettings(const JsonDocument &document, OishiaSettings &next) {
  JsonString petName = document["petName"].as<JsonString>();
  JsonString ownerName = document["ownerName"].as<JsonString>();
  if (!validOishiaName(petName.c_str(), petName.size()) || !validOishiaName(ownerName.c_str(), ownerName.size())) {
    return "Names need 1–12 printable English characters without leading or trailing spaces.";
  }
  JsonString timezone = document["timezone"].as<JsonString>();
  if (!timezone || timezone.size() >= 32 ||
      !validOishiaTimeZone(timezone.c_str())) {
    return "Choose a timezone from the dashboard list.";
  }
  // A const document exposes read-only views; is<JsonArray>() always fails here.
  if (!document["tiltInvert"].is<bool>() || !document["autoSleep"].is<bool>() ||
      !document["clockWhenAway"].is<bool>() || !document["clock24Hour"].is<bool>() ||
      !document["darkThreshold"].is<uint32_t>() || !document["brightThreshold"].is<uint32_t>() ||
      !document["sleepAfterSeconds"].is<uint32_t>() || !document["motionCooldownSeconds"].is<uint32_t>() ||
      !document["clockAfterSeconds"].is<uint32_t>() || !document["portraitEnabled"].is<bool>() ||
      !document["clockStyle"].is<uint8_t>() || !document["clockShowSeconds"].is<bool>() ||
      !document["displayDayContrast"].is<uint8_t>() || !document["displayNightContrast"].is<uint8_t>() ||
      !document["displayOffQuiet"].is<bool>() || !document["soundProfile"].is<uint8_t>() ||
      !document["quietHoursEnabled"].is<bool>() || !document["quietStartHour"].is<uint8_t>() ||
      !document["quietStartMinute"].is<uint8_t>() || !document["quietEndHour"].is<uint8_t>() ||
      !document["quietEndMinute"].is<uint8_t>() || !document["comfortableMinTenths"].is<int16_t>() ||
      !document["comfortableMaxTenths"].is<int16_t>() || !document["comfortableMinHumidity"].is<uint8_t>() ||
      !document["comfortableMaxHumidity"].is<uint8_t>() || !document["pirStuckSeconds"].is<uint16_t>() ||
      !document["routines"].is<JsonArrayConst>() || document["routines"].size() != 3) {
    return "Send all settings with whole-number limits and boolean switches.";
  }
  memcpy(next.petName, petName.c_str(), petName.size()); next.petName[petName.size()] = 0;
  memcpy(next.ownerName, ownerName.c_str(), ownerName.size()); next.ownerName[ownerName.size()] = 0;
  next.tiltInvert = document["tiltInvert"].as<bool>();
  next.autoSleep = document["autoSleep"].as<bool>();
  next.darkThreshold = document["darkThreshold"];
  next.brightThreshold = document["brightThreshold"];
  next.sleepAfterSeconds = document["sleepAfterSeconds"];
  next.motionCooldownSeconds = document["motionCooldownSeconds"];
  memcpy(next.timezone, timezone.c_str(), timezone.size()); next.timezone[timezone.size()] = 0;
  next.clockWhenAway = document["clockWhenAway"].as<bool>();
  next.clock24Hour = document["clock24Hour"].as<bool>();
  next.clockAfterSeconds = document["clockAfterSeconds"];
  next.portraitEnabled=document["portraitEnabled"].as<bool>(); next.clockStyle=document["clockStyle"];
  next.clockShowSeconds=document["clockShowSeconds"].as<bool>(); next.displayDayContrast=document["displayDayContrast"];
  next.displayNightContrast=document["displayNightContrast"]; next.displayOffQuiet=document["displayOffQuiet"].as<bool>();
  next.soundProfile=document["soundProfile"]; next.quietHoursEnabled=document["quietHoursEnabled"].as<bool>();
  next.quietStartHour=document["quietStartHour"]; next.quietStartMinute=document["quietStartMinute"];
  next.quietEndHour=document["quietEndHour"]; next.quietEndMinute=document["quietEndMinute"];
  next.comfortableMinTenths=document["comfortableMinTenths"]; next.comfortableMaxTenths=document["comfortableMaxTenths"];
  next.comfortableMinHumidity=document["comfortableMinHumidity"]; next.comfortableMaxHumidity=document["comfortableMaxHumidity"];
  next.pirStuckSeconds=document["pirStuckSeconds"];
  uint8_t ri=0; for (JsonObjectConst input : document["routines"].as<JsonArrayConst>()) {
    OishiaRoutine &r=next.routines[ri++]; if(!input["enabled"].is<bool>()||!input["action"].is<uint8_t>()||
      !input["hour"].is<uint8_t>()||!input["minute"].is<uint8_t>()||!input["weekdays"].is<uint8_t>()||!input["message"].is<const char *>()){
      return "Each routine needs enabled, action, time, weekdays, and message.";}
    JsonString message = input["message"].as<JsonString>();
    if (!validOishiaMessage(message.c_str(), message.size() + 1)) {
      return "Routine messages need 1–21 printable English characters without outside spaces.";
    }
    r.enabled=input["enabled"].as<bool>();r.action=input["action"];r.hour=input["hour"];r.minute=input["minute"];r.weekdays=input["weekdays"];
    memcpy(r.message, message.c_str(), message.size()); r.message[message.size()] = 0;
  }
  if (!validOishiaSettings(next)) { return "One or more preference, comfort, quiet-hour, display, or routine values are invalid."; }
  return nullptr;
}
