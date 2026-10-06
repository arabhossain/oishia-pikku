#pragma once
#include <Arduino.h>
#include <NetworkServer.h>
#include <NetworkClient.h>
#include <lwip/sockets.h>
#include <errno.h>
#include "OishiaHttpRequest.h"

constexpr int HTTP_GET = 1, HTTP_POST = 2;
// Small adapter for this firmware's fixed routes. RX and TX each do at most
// 512 bytes per tick. Nonblocking socket calls and absolute deadlines apply
// to the entire request/response, including stalled readers.
class OishiaHttpServer {
 public:
  explicit OishiaHttpServer(uint16_t port) : listener(port) {}
  void begin() { listener.begin(); }
  void on(const char *path, int method, void (*handler)()) {
    if (routeCount < 10) routes[routeCount++] = {path, method, handler};
  }
  void onNotFound(void (*handler)()) { fallback = handler; }
  const char *header(const char *name) const {
    return !strcmp(name, "X-Oishia-Key") ? request.key : request.contentType;
  }
  const char *arg(const char *) const { return request.body; }
  String uri() const { return request.path; }
  int method() const { return !strcmp(request.method, "GET") ? HTTP_GET : HTTP_POST; }
  NetworkClient &client() { return peer; }
  void sendHeader(const char *name, const char *value) {
    int n = snprintf(extra + extraSize, sizeof(extra) - extraSize, "%s: %s\r\n", name, value);
    if (n < 0 || size_t(n) >= sizeof(extra) - extraSize) { peer.stop(); return; }
    extraSize += n;
  }
  void send(int code, const char *type = "text/plain", const char *body = "") {
    size_t length = strlen(body);
    if (length >= sizeof(payload)) { code = 500; type = "text/plain"; body = "Response too large"; length = strlen(body); }
    memcpy(payload, body, length);
    prepare(code, type, reinterpret_cast<const uint8_t *>(payload), length);
  }
  void send_P(int code, const char *type, const uint8_t *body, size_t length) { prepare(code, type, body, length); }
  void handleClient() {
    if (!active) {
      peer = listener.accept();
      if (!peer) return;
      active = true; sending = false; extraSize = 0; extra[0] = 0;
      request.begin(millis());
    }
    if (!peer.connected()) { close(); return; }
    uint32_t now = millis();
    if (sending) {
      if (uint32_t(now - sentAt) >= 5000) { close(); return; }
      const uint8_t *data = headerOffset < headerSize ? reinterpret_cast<const uint8_t *>(responseHeader) : responseBody;
      size_t &offset = headerOffset < headerSize ? headerOffset : bodyOffset;
      size_t total = headerOffset < headerSize ? headerSize : bodySize;
      if (offset == total) { close(); return; }
      size_t count = min(size_t(512), total - offset);
      int n = ::send(peer.fd(), data + offset, count, MSG_DONTWAIT);
      if (n > 0) offset += n;
      else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) close();
      if (headerOffset == headerSize && bodyOffset == bodySize) close();
      return;
    }
    if (request.expired(now)) { send(408, "text/plain", "Request timed out"); return; }
    uint8_t buffer[512];
    int n = ::recv(peer.fd(), buffer, sizeof(buffer), MSG_DONTWAIT);
    if (n == 0) { close(); return; }
    if (n < 0) { if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) close(); return; }
    for (int i = 0; i < n && !request.error && !request.complete; ++i) request.push(buffer[i]);
    if (request.error) { send(request.error, "application/json", "{\"error\":\"Invalid or oversized HTTP request.\"}"); return; }
    if (!request.complete) return;
    for (size_t i = 0; i < routeCount; ++i) {
      if (!strcmp(request.path, routes[i].path) && method() == routes[i].method) { routes[i].handler(); return; }
    }
    if (fallback) fallback(); else send(404);
  }
 private:
  struct Route { const char *path; int method; void (*handler)(); } routes[10] = {};
  NetworkServer listener;
  NetworkClient peer;
  OishiaHttpRequest request;
  size_t routeCount = 0;
  void (*fallback)() = nullptr;
  char extra[768] = {}, responseHeader[1024] = {}, payload[4096] = {};
  const uint8_t *responseBody = nullptr;
  size_t extraSize = 0, headerSize = 0, headerOffset = 0, bodySize = 0, bodyOffset = 0;
  uint32_t sentAt = 0;
  bool active = false, sending = false;
  void close() { peer.stop(); active = false; sending = false; }
  void prepare(int code, const char *type, const uint8_t *body, size_t length) {
    int n = snprintf(responseHeader, sizeof(responseHeader),
      "HTTP/1.1 %d Response\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\n%s\r\n",
      code, type, unsigned(length), extra);
    if (n < 0 || size_t(n) >= sizeof(responseHeader)) { close(); return; }
    headerSize = n; headerOffset = bodyOffset = 0;
    responseBody = body; bodySize = length;
    sentAt = millis(); sending = true;
  }
};
