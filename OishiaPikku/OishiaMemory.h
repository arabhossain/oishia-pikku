#pragma once
#include <stdint.h>
#include <stddef.h>

struct OishiaMemoryRecord {
  uint32_t version = 1;
  uint32_t pets = 0;
  uint32_t visits = 0;
  uint32_t muted = 0;
  bool valid() const { return version == 1 && muted <= 1; }
};
static_assert(sizeof(OishiaMemoryRecord) == 16, "Persistent record layout changed");

// One NVS blob commit updates all fields atomically. The legacy keys remain
// available for migration; failure never clears dirty state.
template <class Store>
bool saveOishiaMemory(Store &store, const OishiaMemoryRecord &record, bool &dirty) {
  if (store.putBytes("pet-v1", &record, sizeof(record)) != sizeof(record)) return false;
  dirty = false;
  return true;
}
