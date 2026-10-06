#include "../learning/OishiaNetworkRules.h"
#include "../learning/OishiaBleFrames.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace OishiaNetworkRules;

int main() {
  assert(validPin("0123", 4));
  assert(validPin("0000", 4));
  assert(validPin("9999", 4));
  assert(!validPin("123", 3));
  assert(!validPin("12345", 5));
  assert(!validPin("12a4", 4));
  assert(!validPin("12 4", 4));
  assert(!validPin(nullptr, 4));
  assert(!validPin("1\0" "23", 4));
  auto credentials = [](const std::string &ssid, const std::string &password) {
    return validCredentials(ssid.data(), ssid.size(), password.data(), password.size());
  };
  assert(credentials("Home", "correct horse"));
  assert(credentials(" Home ", " password "));  // Spaces are significant.
  assert(credentials("Open network", ""));
  assert(credentials("家のWi-Fi", "12345678"));
  assert(credentials(std::string(32, 'x'), std::string(63, 'p')));
  assert(credentials("Home", std::string(64, 'a')));  // Raw hexadecimal PSK.
  assert(!credentials("", "12345678"));
  assert(!credentials(std::string(33, 'x'), "12345678"));
  assert(!credentials("Home", "short"));
  assert(!credentials("Home", std::string(64, 'z')));
  assert(!credentials("Home", std::string(65, 'a')));
  assert(!credentials("Home\n", "12345678"));
  assert(!credentials(std::string("Home\0evil", 9), "12345678"));
  assert(!credentials("Home", std::string("1234\0abc", 8)));
  assert(!validCredentials(nullptr, 0, "", 0));
  assert(!validCredentials("Home", 4, nullptr, 0));
  assert(validMessage("Hi Sathu!", 9));
  assert(validMessage("<3 & hugs", 9));
  assert(validMessage("123456789012345678901", 21));
  assert(!validMessage("1234567890123456789012", 22));
  assert(!validMessage("   ", 3));
  assert(!validMessage("hi\n", 3));
  assert(!validMessage("\xc3\xa9", 2));  // OLED font is ASCII.
  assert(!validMessage("A\0B", 3));
  assert(!validMessage(nullptr, 0));
  assert(!elapsed(19999, 0, CONNECT_TIMEOUT_MS));
  assert(elapsed(20000, 0, CONNECT_TIMEOUT_MS));
  assert(!elapsed(0x00000010u, 0xfffffff0u, 33));
  assert(elapsed(0x00000010u, 0xfffffff0u, 32));
  assert(!elapsed(29999, 0, RETRY_INTERVAL_MS));
  assert(elapsed(30000, 0, RETRY_INTERVAL_MS));
  OishiaBleFrames frames;
  std::string request = "{\"id\":7,\"text\":\"Hello\"}";
  for (unsigned char byte : request) assert(frames.push(byte) == OishiaBleFrames::Waiting);
  assert(frames.push('\n') == OishiaBleFrames::Complete);
  assert(std::string(frames.value()) == request);
  assert(!frames.partial());
  for (size_t i = 0; i < OishiaBleFrames::CAPACITY; ++i) frames.push('a');
  assert(frames.push('\n') == OishiaBleFrames::Complete);
  assert(strlen(frames.value()) == OishiaBleFrames::CAPACITY);
  for (size_t i = 0; i < OishiaBleFrames::CAPACITY + 1; ++i) frames.push('a');
  assert(frames.push('\n') == OishiaBleFrames::Rejected);
  assert(frames.push('\n') == OishiaBleFrames::Rejected);
  frames.push('a'); frames.push(0); frames.push('b');
  assert(frames.push('\n') == OishiaBleFrames::Rejected);
  frames.push('{'); frames.reset();  // Disconnect or timeout drops partial data.
  assert(frames.push('\n') == OishiaBleFrames::Rejected);
  for (int i = 0; i < 3; ++i) {
    frames.push('{'); frames.push('}');
    assert(frames.push('\n') == OishiaBleFrames::Complete);
    assert(std::string(frames.value()) == "{}");
  }
  std::cout << "Network input boundaries and rollover-safe timers passed.\n";
}
