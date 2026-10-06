# Oishia — Wi-Fi and Bluetooth robot


## First connection and login

1. Upload the sketch over USB. Serial diagnostics use **9600 baud**.
2. Join **OishiaPikku** using the fixed Wi-Fi password **27222222**. This password is deliberately unchanged. The OLED also shows the separate four-digit **PIN** and six-digit **BLE pair** code.
3. Open **http://192.168.4.1**. Use the four-digit PIN to log in within five minutes after power-on. Hold the physical button for five seconds to reopen login and setup at any time.
4. The PIN exchanges for a random 128-bit session token. Normal commands use that token, not the PIN. The browser keeps it only in the tab's session storage. Sessions expire after 30 minutes without an authenticated request, after eight hours total, on reboot, or after an access reset. Up to four sessions can coexist.
5. Choose **Find networks** or enter a hidden **2.4 GHz** network. Credentials are saved only after a successful connection; failed trials keep the previous network. Empty passwords explicitly select an open network.
6. On success, join your home Wi-Fi and open the numeric address on the OLED. The `oishia-xxxxxx.local` hostname is a convenience; numeric IP is the fallback. A new address needs a new login, so reopen the five-minute window if necessary.

Five incorrect PIN attempts trigger a one-minute cooldown shared across Wi-Fi and BLE. A successful PIN does not bypass an active cooldown. A five-second physical hold reopens login and clears the cooldown. Locking the dashboard revokes its token when the device is reachable; an interrupted logout still locks the page locally, and the device session expires normally.

Wi-Fi channel changes during provisioning may interrupt your phone's connection. Rejoin the hotspot to check the result. The hotspot closes after at least 60 seconds online; a manually opened hotspot remains for at least five minutes. Automatic network fallback does **not** reopen PIN login. Network scans return at most eight distinct networks; manual SSID entry remains available.

**Forget saved Wi-Fi** removes only the home network. It preserves PIN, mute preference, friendship, and Bluetooth bonds.

## Bluetooth

Bluetooth appears as **Oishia-XXXXXX**. Start the local controller on a Bluetooth-capable computer:

```sh
python3 tools/serve_bluetooth.py
```

Open **http://localhost:8080**, select Bluetooth, and enter the four-digit login PIN. During OS pairing, enter the separate **six-digit BLE pair code shown on the OLED**. Pairing completes before application request timeouts begin. Hold the button for five seconds to display the codes and enable new pairing.

NimBLE uses encrypted, authenticated Secure Connections passkey pairing. New peers may connect during the five-minute commissioning window; already-bonded peers can reconnect outside it. The device stores at most three bonds. An accepted connection must authenticate at the application level within 60 seconds and must make an authenticated request at least every two minutes afterward. A hidden browser tab stops polling and may therefore need to reconnect.

Old bonds from the previous Bluedroid firmware may need to be removed in the phone/computer's Bluetooth settings. If the bond slots are full, perform the physical access reset described below, then pair again.

Web Bluetooth needs HTTPS or localhost and a supported browser/platform. The ESP32's ordinary HTTP page supports Wi-Fi control; use the local controller or a native BLE client for Bluetooth. See [Chrome's Web Bluetooth documentation](https://developer.chrome.com/docs/capabilities/bluetooth).

## Physical controls and sensors

| Input | Behavior |
| --- | --- |
| Short tap / double tap / triple tap / four taps | Pet reaction / sensor screen / tilt game / stopwatch. |
| Ordinary long press | Sleep/wake, or restart the game. |
| Five-second hold | Show connection codes; open setup, PIN login, and new pairing for five minutes. Release here to keep access credentials. |
| Twelve-second hold | Revoke all sessions, rotate the login PIN and BLE pairing code, and clear Bluetooth bonds. Keep holding beyond the five-second setup screen. Wi-Fi credentials, fixed hotspot password, friendship, and mute are retained. |
| PIR, GPIO 26 | Motion readings, welcome reactions, hearts, and sparkles. It does not measure distance or prove that a stationary person is present. |
| MPU-6500, I2C | Tilt/shake reactions and game control. Transient failures trigger five-second reprobes and reset sampling baselines. The game reports “Tilt unavailable” until valid samples return. |
| LDR, GPIO 34 | Room-light readings and automatic dark/bright reactions. |
| DHT11, GPIO 4 | Temperature, humidity, and climate moods. |

**Stopwatch:** tap the physical button four times to open it on the OLED. Tap once to start or pause, twice to reset to zero (paused), and hold for at least 0.85 seconds then release to exit. The display shows hours, minutes, seconds, and tenths, and stays on until you exit. Exiting pauses the stopwatch and keeps its time until reset or reboot. While the stopwatch is open, holding the button only exits; it does not trigger network setup or access reset.

The five/twelve-second holds consume the release so it does not also trigger an ordinary long-press action. Access-reset storage/bond failures are exposed as `settingsResult: access_reset_error`; in that case the displayed PIN may be session-only and the previous PIN can return after reboot.

Remote **Cozy sleep** stays active until Wake, love, a note, or ordinary physical interaction. Sensor reactions do not override it. **Tiny note** accepts 1–21 printable ASCII characters; the OLED's built-in font cannot display arbitrary Unicode. Mute stops sound immediately. Friendship and mute changes are coalesced into one versioned NVS record and committed at most once every 30 seconds; power loss before that commit can lose recent changes. Failed writes remain pending and retry after 30 seconds. The dashboard reports pending/failed persistence.

Pet memory stays in the legacy `aru` namespace and network settings in `aru-net`. Existing `pets`, `visits`, and `muted` values migrate to the atomic `pet-v1` record on the first successful save. Legacy keys remain for recovery; downgrading to older firmware can show older counters/preferences.

## Names and preferences

Open **Names & preferences** in the unlocked dashboard, over either Wi-Fi or Bluetooth. The original hard-coded values are the factory defaults:

| Setting | Default | Allowed values |
| --- | --- | --- |
| Companion name | Oishia | 1–12 printable ASCII characters, no leading/trailing spaces |
| Your name | Sathu | Same limit; used in greetings |
| Clock when away | On | Replaces the normal pet face after the PIR has stayed quiet |
| No motion before clock | 120 seconds | 10–86400 seconds |
| Time format | 24-hour | 24-hour or 12-hour |
| Seconds / clock layout | On / large | Seconds on/off; large, minimal, or pet layout |
| Detailed pet face | On | Oishia mascot or simple eyes and mouth, mirrored in the dashboard |
| Timezone | Asia/Tokyo | One of the worldwide city zones listed in the dashboard |
| Sound profile | Gentle | Silent, gentle, or playful; manual mute remains available |
| Quiet hours | Off, 22:00–07:00 | Local start/end; suppresses automatic sound |
| OLED contrast | Day 180 / night 35 | Day 5–255; night 1–255; optional quiet-hour display-off |
| Automatic sleep | On | On/off, including occasional automatic naps; manual/remote sleep still works |
| Darkness before sleep | 30 seconds | 5–3600 seconds |
| Motion greeting cooldown | 10 seconds | 1–300 seconds |
| PIR stuck warning | 300 seconds | 60–3600 seconds continuously HIGH |
| Reverse tilt game direction | Off | On/off |
| Dark light threshold | 800 | 0–4094 |
| Bright light threshold | 1100 | 1–4095, strictly greater than dark |
| Comfort temperature | 18.0–29.0 °C | −10.0–60.0 °C, minimum below maximum |
| Comfort humidity | 30–78% | 0–100%, minimum below maximum |
| Daily routines | Three disabled slots | Local time, every day/weekdays/weekends, action, 1–21 character message |

Names appear in the dashboard, OLED boot screen, and personalized greetings; both names also appear above the sensor readings (shortened to nine characters each in that compact header). Names are text, not HTML or format strings. The OLED font limits names to English/ASCII characters. The product label, Wi-Fi SSID **OishiaPikku**, Bluetooth discovery name **Oishia-XXXXXX**, mDNS hostname, hardware pins, and fixed password **27222222** remain stable.

The clock takes UTC time from three Google Public NTP endpoints after home Wi-Fi reaches the internet and applies the saved timezone locally. No API key or Google account is needed. All configured endpoints use the same leap-smear policy. When NTP is unavailable, an authenticated dashboard automatically supplies its browser clock; this also makes time work while the phone is connected directly to Oishia's internet-less setup hotspot. Tokyo is the factory default; **Use this device's zone** copies the browser timezone when that city is present in the compact list. The dashboard reports the time source and remaining quiet-time delay. **Preview clock for 30 seconds** displays it immediately, even when motion is currently detected or remote sleep is active. Ordinary motion or a button press immediately returns an automatically activated clock to the pet screen. Setup codes, boot, sensor readings, and the game take priority over the clock. If neither source has synchronized since power-on, the away screen says that it is waiting for time. The ESP32 has no battery-backed real-time clock in this design, so every power cycle needs NTP or a dashboard visit. The PIR detects movement rather than continuous occupancy; a motionless person can therefore be treated as away after the chosen delay.

**Save preferences** validates and atomically commits the complete record before applying it. Failed saves retain the previous values and leave your edits in the form. A successful save persists across restart, Forget Wi-Fi, and access resets. Changes do not wake remote sleep or reset friendship. Saved light settings restart the darkness delay; turning automatic sleep off exits an ordinary sleep state. Live polling preserves unsaved edits. **Discard edits** reloads the latest saved values. **Restore defaults** fills the form; it changes the device only after Save preferences.

The record is versioned as `personal-v3` in `aru-net`; valid `personal-v1` and `personal-v2` records migrate in memory, retaining their existing values while new controls take factory defaults. Absent or invalid records use factory defaults. This is separate from friendship/mute storage.

API: authenticated `POST /api/settings` (or BLE `op: "settings"`) accepts the complete settings object returned by `GET /api/status`; partial records are rejected. Successful responses contain the saved `settings`. Writes are limited to one changed record every two seconds, and identical saves do not write flash. The dashboard's versioned JSON export contains this complete object without credentials or private counters; import fills the form and still passes through firmware validation before save.
