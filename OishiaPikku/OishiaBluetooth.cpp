#include "OishiaBluetooth.h"
#include "OishiaBleFrames.h"
#include <NimBLEDevice.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
constexpr const char *SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char *RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char *TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *tx = nullptr;
QueueHandle_t requests = nullptr;
std::atomic<bool> connected{false}, restartAdvertising{false}, pairingAllowed{false}, txFailed{false};
std::atomic<uint32_t> session{0}, disconnectedAt{0}, connectedAt{0}, lastAuthorizedAt{0}, passkey{0};
std::atomic<bool> authenticated{false};
std::atomic<uint16_t> activeConnection{BLE_HS_CONN_HANDLE_NONE};
// Only BLE callbacks access the RX parser. Only the network task accesses TX.
OishiaBleFrames frames;
uint32_t lastByteAt = 0;
char outbound[4096] = {};
size_t outboundLength = 0, outboundOffset = 0;
uint32_t outboundSession = 0, outboundAt = 0, lastPacketAt = 0;

class Connections : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &peer) override {
    bool known = NimBLEDevice::isBonded(peer.getIdAddress());
    if (connected.load() || (!known && (!pairingAllowed.load() || NimBLEDevice::getNumBonds() >= 3))) {
      server->disconnect(peer.getConnHandle()); return;
    }
    frames.reset();
    activeConnection.store(peer.getConnHandle());
    session.fetch_add(1);
    authenticated.store(false);
    connectedAt.store(millis());
    connected.store(true);
    NimBLEDevice::stopAdvertising();
  }
  uint32_t onPassKeyDisplay() override { return passkey.load(); }
  void onAuthenticationComplete(NimBLEConnInfo &peer) override {
    if (!peer.isEncrypted() || !peer.isAuthenticated()) bleServer->disconnect(peer.getConnHandle());
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &peer, int) override {
    if (peer.getConnHandle() != activeConnection.load()) return;
    frames.reset();
    connected.store(false);
    authenticated.store(false);
    activeConnection.store(BLE_HS_CONN_HANDLE_NONE);
    session.fetch_add(1);
    disconnectedAt.store(millis());
    restartAdvertising.store(true);
  }
};
class Receiver : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &peer) override {
    if (!connected.load() || peer.getConnHandle() != activeConnection.load() ||
        !peer.isEncrypted() || !peer.isAuthenticated()) return;
    auto bytes = characteristic->getValue();
    if (frames.partial() && uint32_t(millis() - lastByteAt) >= 5000) frames.reset();
    lastByteAt = millis();
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (frames.push(uint8_t(bytes[i])) == OishiaBleFrames::Complete) {
        OishiaBleRequest request = {};
        request.session = session.load();
        strlcpy(request.json, frames.value(), sizeof(request.json));
        if (xQueueSend(requests, &request, 0) != pdTRUE) bleServer->disconnect(peer.getConnHandle());
      }
    }
  }
};
class Transmitter : public NimBLECharacteristicCallbacks {
  void onStatus(NimBLECharacteristic *, NimBLEConnInfo &peer, int code) override {
    if (peer.getConnHandle() == activeConnection.load() && code != 0) txFailed.store(true);
  }
};
Connections connectionCallbacks;
Receiver receiverCallbacks;
Transmitter transmitterCallbacks;
}

bool beginOishiaBluetooth(const char *name, uint32_t pairingPasskey) {
  passkey.store(pairingPasskey);
  requests = xQueueCreate(2, sizeof(OishiaBleRequest));
  if (!requests) return false;
  if (!NimBLEDevice::init(name)) { vQueueDelete(requests); requests = nullptr; return false; }
  NimBLEDevice::setMTU(185);
  // Display-only passkey pairing authenticates the encrypted link.
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(pairingPasskey);
  bleServer = NimBLEDevice::createServer();
  if (!bleServer) return false;
  bleServer->setCallbacks(&connectionCallbacks, false);
  bleServer->advertiseOnDisconnect(false);
  auto *service = bleServer->createService(SERVICE);
  if (!service) return false;
  tx = service->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ |
                                    NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN, 20);
  auto *rx = service->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE |
                                         NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN, 1536);
  if (!tx || !rx) return false;
  tx->setCallbacks(&transmitterCallbacks);
  rx->setCallbacks(&receiverCallbacks);
  if (!service->start() || !bleServer->start()) return false;
  auto *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE);
  advertising->setName(name);
  advertising->enableScanResponse(true);
  return advertising->start();
}

void allowOishiaPairing(bool allow) { pairingAllowed.store(allow); }
void authorizeOishiaBluetooth(uint32_t requestSession) {
  if (connected.load() && requestSession == session.load()) {
    lastAuthorizedAt.store(millis()); authenticated.store(true);
  }
}
bool resetOishiaBluetoothBonds(uint32_t pairingPasskey) {
  passkey.store(pairingPasskey);
  NimBLEDevice::setSecurityPasskey(pairingPasskey);
  outboundLength = 0;
  if (!bleServer) return true;
  if (connected.exchange(false)) {
    authenticated.store(false);
    session.fetch_add(1);
    xQueueReset(requests);
    bleServer->disconnect(activeConnection.load());
  }
  return NimBLEDevice::deleteAllBonds();
}

void updateOishiaBluetooth() {
  uint32_t now = millis();
  if (restartAdvertising.load() && uint32_t(now - disconnectedAt.load()) >= 500) {
    restartAdvertising.store(false);
    if (!connected.load()) NimBLEDevice::startAdvertising();
  }
  if (!connected.load()) { outboundLength = 0; return; }
  if ((!authenticated.load() && uint32_t(now - connectedAt.load()) >= 60000) ||
      (authenticated.load() && uint32_t(now - lastAuthorizedAt.load()) >= 120000) || txFailed.exchange(false)) {
    outboundLength = 0;
    bleServer->disconnect(activeConnection.load());
    return;
  }
  if (!outboundLength) return;
  if (outboundSession != session.load()) { outboundLength = 0; return; }
  if (uint32_t(now - outboundAt) >= 5000) { outboundLength = 0; bleServer->disconnect(activeConnection.load()); return; }
  if (uint32_t(now - lastPacketAt) < 10) return;
  auto peer = bleServer->getPeerInfoByHandle(activeConnection.load());
  if (!peer.isEncrypted() || !peer.isAuthenticated()) { outboundLength = 0; return; }
  size_t length = min(size_t(20), outboundLength - outboundOffset);
  // One packet per network tick; failure closes the connection instead of
  // ambiguously replaying part of a response.
  if (!tx->notify(reinterpret_cast<const uint8_t *>(outbound + outboundOffset), length, activeConnection.load())) {
    outboundLength = 0; bleServer->disconnect(activeConnection.load()); return;
  }
  lastPacketAt = now;
  outboundOffset += length;
  if (outboundOffset == outboundLength) outboundLength = 0;
}

bool receiveOishiaBleRequest(OishiaBleRequest &request) {
  if (!requests || outboundLength) return false;
  for (int i = 0; i < 4 && xQueueReceive(requests, &request, 0) == pdTRUE; ++i) {
    if (connected.load() && request.session == session.load()) return true;
  }
  return false;
}
void replyOishiaBluetooth(uint32_t requestSession, const char *json) {
  size_t length = strlen(json);
  if (!tx || !connected.load() || requestSession != session.load()) return;
  if (length + 1 > sizeof(outbound)) { bleServer->disconnect(activeConnection.load()); return; }
  memcpy(outbound, json, length); outbound[length++] = '\n';
  outboundOffset = 0; outboundLength = length;
  outboundSession = requestSession; outboundAt = millis();
}
bool oishiaBluetoothConnected() { return connected.load(); }
