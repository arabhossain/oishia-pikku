#include "../learning/OishiaSettingsJson.h"
#include "Preferences.h"
#include <cassert>
#include <iostream>
#include <string>

std::string encode(const OishiaSettings &settings) {
  JsonDocument document;
  settingsJson(document.to<JsonObject>(), settings);
  std::string json;
  serializeJson(document, json);
  return json;
}

int main() {
  OishiaSettings expected;
  for (bool bluetooth : {false, true}) {
    JsonDocument document;
    assert(!deserializeJson(document, encode(expected), DeserializationOption::NestingLimit(3)));
    if (bluetooth) { document["op"] = "settings"; document["id"] = 7; document["key"] = "session-token"; }
    OishiaSettings parsed;
    assert(parseOishiaSettings(document, parsed) == nullptr);
    assert(encode(parsed) == encode(expected));

    document["petName"] = "Pikku";
    document["autoSleep"] = false;
    document["quietHoursEnabled"] = true;
    document["comfortableMinTenths"] = -50;
    document["displayDayContrast"] = 255;
    document["clockAfterSeconds"] = 86400;
    document["routines"][0]["enabled"] = true;
    document["routines"][0]["hour"] = 7;
    document["routines"][0]["message"] = "Wake up!";
    assert(parseOishiaSettings(document, parsed) == nullptr);
    assert(std::string(parsed.petName) == "Pikku");
    assert(!parsed.autoSleep && parsed.quietHoursEnabled);
    assert(parsed.comfortableMinTenths == -50 && parsed.displayDayContrast == 255);
    assert(parsed.clockAfterSeconds == 86400 && parsed.routines[0].enabled);
    assert(parsed.routines[0].hour == 7 && std::string(parsed.routines[0].message) == "Wake up!");
    Preferences store;
    assert(saveOishiaSettings(store, parsed));
    assert(encode(loadOishiaSettings(store)) == encode(parsed));
    store.fail = true;
    parsed.autoSleep = true;
    assert(!saveOishiaSettings(store, parsed));
    assert(!loadOishiaSettings(store).autoSleep);
  }

  auto reject = [&](auto mutate) {
    JsonDocument document;
    assert(!deserializeJson(document, encode(expected)));
    mutate(document);
    OishiaSettings parsed;
    assert(parseOishiaSettings(document, parsed) != nullptr);
  };
  reject([](JsonDocument &d) { d.remove("autoSleep"); });
  reject([](JsonDocument &d) { d["autoSleep"] = 1; });
  reject([](JsonDocument &d) { d["autoSleep"] = "true"; });
  reject([](JsonDocument &d) { d["darkThreshold"] = 800.5; });
  reject([](JsonDocument &d) { d["displayDayContrast"] = 256; });
  reject([](JsonDocument &d) { d["brightThreshold"] = 700; });
  reject([](JsonDocument &d) { d.remove("routines"); });
  reject([](JsonDocument &d) { d["routines"].to<JsonObject>(); });
  reject([](JsonDocument &d) { d["routines"].as<JsonArray>().remove(2); });
  reject([](JsonDocument &d) { d["routines"].as<JsonArray>().add<JsonObject>(); });
  reject([](JsonDocument &d) { d["routines"][0]["enabled"] = 1; });
  reject([](JsonDocument &d) { d["routines"][0]["hour"] = 24; });
  std::cout << "Settings JSON validation and persistence round trips passed for HTTP and BLE payloads.\n";
}
