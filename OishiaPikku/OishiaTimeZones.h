#pragma once
#include <stddef.h>
#include <string.h>

// ESP32/newlib uses POSIX TZ rules rather than the IANA timezone database.
// Keep a stable IANA-style id in settings and translate it through this table.
struct OishiaTimeZone {
  const char *id;
  const char *posix;
};

inline const OishiaTimeZone *oishiaTimeZones(size_t &count) {
  static const OishiaTimeZone zones[] = {
    {"UTC", "UTC0"},
    {"Pacific/Honolulu", "HST10"},
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Halifax", "AST4ADT,M3.2.0,M11.1.0"},
    {"America/St_Johns", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"America/Mexico_City", "CST6"},
    {"America/Bogota", "<-05>5"},
    {"America/Lima", "<-05>5"},
    {"America/Caracas", "<-04>4"},
    {"America/Santiago", "<-04>4<-03>,M9.1.6/24,M4.1.6/24"},
    {"America/Sao_Paulo", "<-03>3"},
    {"America/Argentina/Buenos_Aires", "<-03>3"},
    {"Atlantic/Reykjavik", "GMT0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Istanbul", "TRT-3"},
    {"Europe/Moscow", "MSK-3"},
    {"Africa/Casablanca", "<+01>-1"},
    {"Africa/Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"Africa/Johannesburg", "SAST-2"},
    {"Africa/Lagos", "WAT-1"},
    {"Africa/Nairobi", "EAT-3"},
    {"Asia/Dubai", "GST-4"},
    {"Asia/Karachi", "PKT-5"},
    {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Kathmandu", "NPT-5:45"},
    {"Asia/Dhaka", "BST-6"},
    {"Asia/Yangon", "MMT-6:30"},
    {"Asia/Bangkok", "ICT-7"},
    {"Asia/Jakarta", "WIB-7"},
    {"Asia/Singapore", "SGT-8"},
    {"Asia/Manila", "PHT-8"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Hong_Kong", "HKT-8"},
    {"Asia/Taipei", "CST-8"},
    {"Asia/Seoul", "KST-9"},
    {"Asia/Tokyo", "JST-9"},
    {"Australia/Perth", "AWST-8"},
    {"Australia/Darwin", "ACST-9:30"},
    {"Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Australia/Brisbane", "AEST-10"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Guam", "ChST-10"},
    {"Pacific/Noumea", "NCT-11"},
    {"Pacific/Fiji", "FJT-12"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"Pacific/Chatham", "CHAST-12:45CHADT,M9.5.0/2:45,M4.1.0/3:45"},
    {"Pacific/Apia", "<+13>-13"}
  };
  count = sizeof(zones) / sizeof(zones[0]);
  return zones;
}

inline const char *oishiaPosixTimeZone(const char *id) {
  size_t count = 0;
  const OishiaTimeZone *zones = oishiaTimeZones(count);
  for (size_t i = 0; i < count; ++i) if (!strcmp(id, zones[i].id)) return zones[i].posix;
  return nullptr;
}

inline bool validOishiaTimeZone(const char *id) {
  return id && oishiaPosixTimeZone(id);
}
