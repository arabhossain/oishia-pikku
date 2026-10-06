#pragma once
#include "NetworkClient.h"
struct NetworkServer {
  explicit NetworkServer(uint16_t) {}
  void begin() {}
  NetworkClient accept() {
    if (!MockNetwork::waiting) return NetworkClient();
    MockNetwork::waiting=false; return NetworkClient(true);
  }
};
