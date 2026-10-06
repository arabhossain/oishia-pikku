#pragma once
#include <Arduino.h>

// One encrypted BLE client. RX: writes, TX: notifications, standard UART UUIDs.
// Requests contain their own device key; no authorization survives a reconnect.
struct OishiaBleRequest { uint32_t session; char json[1537]; };
bool beginOishiaBluetooth(const char *name, uint32_t pairingPasskey);
void allowOishiaPairing(bool allow);
void authorizeOishiaBluetooth(uint32_t session);
bool resetOishiaBluetoothBonds(uint32_t pairingPasskey);
void updateOishiaBluetooth();
bool receiveOishiaBleRequest(OishiaBleRequest &request);
void replyOishiaBluetooth(uint32_t session, const char *json);
bool oishiaBluetoothConnected();
