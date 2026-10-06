#pragma once
#include <Arduino.h>
#include "OishiaConfig.h"
#if OISHIA_VERBOSE_LOGS
#define PetLog Serial
#else
struct OishiaQuietLog {
  template <class... Args> void print(const Args &...) const {}
  template <class... Args> void println(const Args &...) const {}
  template <class... Args> void printf(const Args &...) const {}
};
static const OishiaQuietLog PetLog;
#endif
