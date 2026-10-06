#include "../learning/OishiaHttpRequest.h"
#include "../learning/OishiaAuth.h"
#include "../learning/OishiaMemory.h"
#include <cassert>
#include <iostream>
#include <string>

OishiaHttpRequest parse(const std::string &text) {
  OishiaHttpRequest request; request.begin(0xfffffff0);
  for (uint8_t c : text) request.push(c);
  return request;
}
int main() {
  const std::string post = "POST /api/command HTTP/1.1\r\nContent-Type: application/json\r\n";
  auto good = parse(post + "X-Oishia-Key: token\r\nContent-Length: 2\r\n\r\n{}");
  assert(good.complete && !good.error && std::string(good.body) == "{}");
  assert(std::string(good.key) == "token");
  assert(parse("GET / HTTP/1.1\r\nHost: device\r\n\r\n").complete);
  assert(parse(post + "Content-Length: 1537\r\n").error == 413); // Reject without reading any body.
  assert(parse(post + "Content-Length: 999999999999999999999\r\n").error == 413);
  assert(parse(post + "Content-Length: -1\r\n").error == 400);
  assert(parse(post + "Content-Length: 2\r\nContent-Length: 2\r\n").error == 400);
  assert(parse(post + "Transfer-Encoding: chunked\r\n").error == 400);
  assert(parse(post + "X-Oishia-Key: one\r\nX-Oishia-Key: two\r\n").error == 400);
  assert(parse(post + " Folded: header\r\n").error == 400);
  assert(parse("POST / HTTP/1.1\r\nContent-Type: multipart/form-data\r\nContent-Length: 2\r\n\r\n").error == 415);
  assert(parse(post + "\r\n").error == 411);
  assert(parse("GET / HTTP/1.1\r\nContent-Length: 1\r\n\r\n").error == 400);
  assert(parse("GET / HTTP/1.1\n").error == 400);
  assert(parse("GET / HTTP/1.1\rX").error == 400);
  assert(parse("GET /" + std::string(300, 'x')).error == 414);
  assert(parse("GET / HTTP/1.1\r\nHeader: " + std::string(300, 'x')).error == 431);
  std::string many = "GET / HTTP/1.1\r\n";
  for (int i = 0; i < 200; ++i) many += "X-Test: value\r\n";
  assert(parse(many).error == 431);
  auto partial = parse(post + "Content-Length: 3\r\n\r\n{}");
  assert(!partial.complete && !partial.error);
  assert(!partial.expired(uint32_t(0xfffffff0u + 2999)));
  partial.push(' '); // New bytes must not reset the absolute deadline.
  assert(partial.expired(uint32_t(0xfffffff0u + 3000)));
  assert(parse(post + "Content-Length: 1536\r\n\r\n" + std::string(1536,'a')).complete);
  assert(parse(post + "Content-Length: 1\r\n\r\n" + std::string(1,'\0')).error == 400);

  OishiaTimer timer;
  assert(!timer.running(0));
  timer.start(0xfffffff0, 32);
  assert(timer.running(0x0f)); assert(timer.due(0x10));
  assert(timer.due(0xfffffff5)); // Expiration stays latched through another wrap.
  timer.start(100, 50); assert(timer.running(149)); timer.stop(); assert(timer.due(149));

  OishiaAuth auth;
  assert(auth.checkPin("0123", "0123", 0) == 403);
  auth.open(0xfffffff0);
  for (int i = 0; i < 5; ++i) assert(auth.checkPin("9999", "0123", 0xfffffff1) == 401);
  assert(auth.checkPin("0123", "0123", 0) == 429); // Correct PIN cannot bypass cooldown.
  assert(auth.checkPin("0123", "0123", uint32_t(0xfffffff1u + 60000)) == 200);
  assert(auth.checkPin("0123", "0123", uint32_t(0xfffffff0u + 300000)) == 403);
  const char *token = "0123456789abcdef0123456789abcdef";
  assert(auth.issue(token, 100)); assert(!auth.authorize("0123", 101));
  assert(auth.authorize(token, 101));
  assert(!auth.authorize(token, 101 + OishiaAuth::SESSION_IDLE_MS));
  assert(auth.issue(token, 10)); auth.tick(10 + OishiaAuth::SESSION_IDLE_MS);
  assert(!auth.authorize(token, 11)); // A later millis wrap cannot revive an idle token.
  assert(auth.issue(token, 200)); auth.revoke(token); assert(!auth.authorize(token, 201));
  assert(auth.issue(token, 300)); auth.reset(301); assert(!auth.authorize(token, 301));
  for (int i=0; i<4; ++i) assert(auth.issue(token, 500));
  assert(!auth.issue(token, 500));
  auth.reset(1000); assert(auth.issue(token, 1000));
  for (uint32_t now=1000; now<1000+OishiaAuth::SESSION_MAX_MS; now+=1000000) assert(auth.authorize(token, now));
  assert(!auth.authorize(token, 1000+OishiaAuth::SESSION_MAX_MS));

  struct Store {
    bool fail = true;
    size_t putBytes(const char *, const void *, size_t size) { return fail ? 0 : size; }
  } store;
  bool dirty = true; OishiaMemoryRecord memory{1, 42, 17, 1};
  assert(!saveOishiaMemory(store, memory, dirty) && dirty);
  store.fail = false; assert(saveOishiaMemory(store, memory, dirty) && !dirty);
  assert(memory.valid()); memory.version = 2; assert(!memory.valid());
  std::cout << "Bounded HTTP, deadlines, auth limits, session expiry, timers, and persistence passed.\n";
}
