#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

// Incremental, allocation-free subset of HTTP. One request per connection.
// Unsupported framing is rejected, never guessed or silently truncated.
class OishiaHttpRequest {
 public:
  static constexpr uint32_t TIMEOUT_MS = 3000;
  char method[5] = {}, path[96] = {}, key[65] = {}, contentType[64] = {};
  static constexpr size_t MAX_BODY = 1536;
  char body[MAX_BODY + 1] = {};
  int error = 0;
  bool complete = false;
  void begin(uint32_t now) { *this = OishiaHttpRequest(); started = now; }
  bool expired(uint32_t now) const { return uint32_t(now - started) >= TIMEOUT_MS; }
  void push(uint8_t c) {
    if (error || complete) return;
    if (inBody) {
      if (!c) { error = 400; return; }
      body[bodySize++] = char(c);
      if (bodySize == contentLength) complete = true;
      return;
    }
    if (++headerBytes > 1536) { error = 431; return; }
    if (c == '\n') {
      if (!lineSize || line[lineSize - 1] != '\r') { error = 400; return; }
      line[--lineSize] = 0;
      parseLine();
      lineSize = 0;
    } else {
      if (c == 0 || (c < 32 && c != '\r' && c != '\t') || c == 127 ||
          (lineSize && line[lineSize - 1] == '\r')) { error = 400; return; }
      if (lineSize == sizeof(line) - 1) { error = firstLine ? 414 : 431; return; }
      line[lineSize++] = char(c);
    }
  }
 private:
  char line[256] = {};
  size_t lineSize = 0, headerBytes = 0, contentLength = 0, bodySize = 0;
  uint32_t started = 0;
  bool firstLine = true, inBody = false, lengthSeen = false, keySeen = false, typeSeen = false;
  template <size_t N> bool copy(char (&dest)[N], const char *value) {
    if (strlen(value) >= N) { error = 431; return false; }
    strcpy(dest, value); return true;
  }
  void parseLine() {
    if (firstLine) {
      firstLine = false;
      char *space = strchr(line, ' ');
      if (!space) { error = 400; return; }
      *space++ = 0;
      if (strcmp(line, "GET") && strcmp(line, "POST")) { error = 405; return; }
      strcpy(method, line);
      char *version = strchr(space, ' ');
      if (!version) { error = 400; return; }
      *version++ = 0;
      if ((strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0")) || space[0] != '/') { error = 400; return; }
      if (strlen(space) >= sizeof(path)) { error = 414; return; }
      strcpy(path, space);
      return;
    }
    if (!lineSize) {
      if (!strcmp(method, "POST") && !lengthSeen) { error = 411; return; }
      if (!strcmp(method, "GET") && contentLength) { error = 400; return; }
      if (!strcmp(method, "POST") && strcmp(contentType, "application/json") &&
          strncmp(contentType, "application/json;", 17)) { error = 415; return; }
      inBody = contentLength != 0;
      complete = !inBody;
      return;
    }
    char *colon = strchr(line, ':');
    if (!colon || colon == line) { error = 400; return; }
    *colon++ = 0;
    for (const char *p = line; *p; ++p) {
      if (!( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
             (*p >= '0' && *p <= '9') || *p == '-')) { error = 400; return; }
    }
    while (*colon == ' ' || *colon == '\t') ++colon;
    size_t n = strlen(colon);
    while (n && (colon[n-1] == ' ' || colon[n-1] == '\t')) colon[--n] = 0;
    if (!strcasecmp(line, "Transfer-Encoding") || !strcasecmp(line, "Expect")) { error = 400; return; }
    if (!strcasecmp(line, "Content-Length")) {
      if (lengthSeen || !*colon) { error = 400; return; }
      lengthSeen = true;
      for (const char *p = colon; *p; ++p) {
        if (*p < '0' || *p > '9') { error = 400; return; }
        contentLength = contentLength * 10 + (*p - '0');
        if (contentLength > MAX_BODY) { error = 413; return; }
      }
    } else if (!strcasecmp(line, "X-Oishia-Key")) {
      if (keySeen) { error = 400; return; }
      keySeen = true; copy(key, colon);
    } else if (!strcasecmp(line, "Content-Type")) {
      if (typeSeen) { error = 400; return; }
      typeSeen = true; copy(contentType, colon);
    }
  }
};
