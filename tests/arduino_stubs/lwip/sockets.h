#pragma once
#include "NetworkClient.h"
#include <errno.h>
#define MSG_DONTWAIT 1
inline int recv(int, void *buffer, size_t size, int) {
  if (MockNetwork::input.empty()) { errno=EAGAIN; return -1; }
  size = std::min(size, MockNetwork::input.size());
  memcpy(buffer, MockNetwork::input.data(), size);
  MockNetwork::input.erase(0, size); return int(size);
}
inline int send(int, const void *buffer, size_t size, int) {
  if (MockNetwork::blockWrites) { errno=EAGAIN; return -1; }
  MockNetwork::largestWrite=std::max(MockNetwork::largestWrite,size);
  MockNetwork::output.append(static_cast<const char *>(buffer), size); return int(size);
}
