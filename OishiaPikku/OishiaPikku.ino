#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <Preferences.h>
#include "OishiaConnectivity.h"
#include "OishiaClock.h"
#include "OishiaTimer.h"
#include "OishiaMemory.h"
#include "OishiaLog.h"

// Keep these hardware settings in sync with the wiring.
#define BUTTON_PIN 27
#define PIR_PIN 26
#define BUZZER_PIN 25
#define LDR_PIN 34

#define MPU6500_ADDRESS_LOW 0x68
#define MPU6500_ADDRESS_HIGH 0x69

#define DHT_PIN 4
#define DHT_TYPE DHT11

#define OLED_SDA 21
#define OLED_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DHT dht(DHT_PIN, DHT_TYPE);

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

Preferences preferences;

enum PetState {
  PET_BOOT, PET_IDLE, PET_HAPPY, PET_LOVE, PET_SLEEP, PET_THINKING,
  PET_CURIOUS, PET_EXCITED, PET_SAD, PET_ANGRY, PET_GAME, PET_SENSOR, PET_GREETING
};

enum EyeStyle {
  EYE_NORMAL, EYE_HAPPY, EYE_LOVE, EYE_SLEEP, EYE_CURIOUS, EYE_SURPRISED,
  EYE_SAD, EYE_ANGRY, EYE_THINKING
};

enum SoundType {
  SOUND_NONE,
  SOUND_PET,
  SOUND_GREETING,
  SOUND_ALERT,
  SOUND_WAKE
};

bool displayReady = false;
bool sensorOK = false;
bool hadSensorReading = false;
bool ledState = false;
bool buttonReading = HIGH;
bool buttonStableState = HIGH;
bool pirWasHigh = false;
bool pendingButtonClick = false;
bool roomDark = false;
bool haveLightReading = false;
bool mpuReady = false;
bool haveMPUSample = false;
bool memoryReady = false;
bool memoryDirty = false;
bool memoryHealthy = true;
bool ownerResetHandled = false;
unsigned long loopNow = 0;
unsigned long lastMPUProbeAt = 0;
uint32_t maxLoopMicros = 0;
bool gameActive = false;
bool connectivityReady = false;
bool setupHoldHandled = false;
bool soundMuted = false;
bool remoteSleeping = false;
bool remoteReactionActive = false;
bool networkScreenVisible = false;
bool stopwatchVisible = false;
bool stopwatchRunning = false;
uint64_t stopwatchElapsedMs = 0;
uint32_t stopwatchLastTick = 0;
bool pirStuck = false;
bool displaySleeping = false;
bool sensorFailureLogged = false;
bool clockWasSynchronized = false;
OishiaNetworkInfo networkInfo = {};
OishiaHistory sensorHistory = {};
OishiaEventLog activityLog = {};
PetState remoteReactionState = PET_IDLE;
char remoteReactionMessage[22] = {};
unsigned long remoteReactionStartedAt = 0;
unsigned long remoteReactionDuration = 0;
unsigned long networkScreenStartedAt = 0;
unsigned long lastSnapshotAt = 0;
float temperature = 0.0f;
float humidity = 0.0f;
int lightLevel = 0;
float accelX = 0.0f;
float accelY = 0.0f;
float accelZ = 0.0f;
float previousAccelX = 0.0f;
float previousAccelY = 0.0f;
float previousAccelZ = 0.0f;
uint8_t mpuAddress = MPU6500_ADDRESS_LOW;
int8_t tiltDirection = 0;
int8_t previousTiltDirection = 0;
int happiness = 55;
int worry = 0;
int irritation = 0;
uint32_t totalPets = 0;
uint32_t totalVisits = 0;
uint8_t relationshipLevel = 1;
char relationshipMessage[22];
int starX = 64;
int starY = 8;
int catcherX = 50;
uint8_t gameScore = 0;
uint8_t gameMisses = 0;
uint8_t gameLevel = 1;
uint8_t gameLevelCatches = 0;
float gameBaselineX = 0.0f;
float gameBaselineY = 0.0f;
int8_t gameTiltAxis = 0;  // 0 = detect, 1 = MPU X, 2 = MPU Y

PetState petState = PET_BOOT;
unsigned long stateStartedAt = 0;
unsigned long stateDuration = 3400;
OishiaTimer nextSensorAt;
unsigned long lastSleepAt = 0;
OishiaTimer transitionUntil;
unsigned long lastSensorReadAt = 0;
unsigned long lastLEDToggleAt = 0;
unsigned long lastAnimationAt = 0;
unsigned long lastPupilMoveAt = 0;
OishiaTimer nextPupilMoveAt;
unsigned long blinkStartedAt = 0;
OishiaTimer nextBlinkAt;
unsigned long lastButtonChangeAt = 0;
unsigned long buttonPressedAt = 0;
OishiaTimer buttonClickDeadline;
unsigned long gameStartedAt = 0;
unsigned long lastGameMoveAt = 0;
unsigned long lastGameControlAt = 0;
OishiaTimer gameFlashUntil;
OishiaTimer gameLevelBannerUntil;
unsigned long lastMotionAt = 0;
unsigned long lastPresenceAt = 0;
unsigned long lastPirStatusAt = 0;
OishiaTimer motionCelebrationUntil;
OishiaTimer clockPreviewUntil;
unsigned long lastLightReadAt = 0;
unsigned long lastLightLogAt = 0;
unsigned long lastGyroReadAt = 0;
unsigned long lastShakeAt = 0;
unsigned long lastShakeSequenceAt = 0;
unsigned long darkSinceAt = 0;
unsigned long soundStepStartedAt = 0;
unsigned long lastMoodUpdateAt = 0;
unsigned long lastMemorySaveAt = 0;
unsigned long bootStartedAt = 0;
unsigned long pirHighSinceAt = 0;
unsigned long lastHistoryAt = 0;
unsigned long lastRoutineCheckAt = 0;
unsigned long lastDisplayCareAt = 0;
uint8_t appliedContrast = 0;
uint32_t lastRoutineStamp[3] = { UINT32_MAX, UINT32_MAX, UINT32_MAX };

const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long ERROR_BLINK_INTERVAL = 250;
const unsigned long ANIMATION_INTERVAL = 120;
const unsigned long BUTTON_DEBOUNCE_INTERVAL = 35;
const unsigned long BUTTON_LONG_PRESS_INTERVAL = 850;
const unsigned long DOUBLE_PRESS_INTERVAL = 350;
const uint32_t BUTTON_SETUP_HOLD_INTERVAL = 5000;
const uint32_t BUTTON_RESET_HOLD_INTERVAL = 12000;

const unsigned long PIR_STATUS_INTERVAL = 10000;
const unsigned long PIR_WARMUP_INTERVAL = 30000;
const unsigned long HISTORY_INTERVAL = 60000;
const unsigned long LIGHT_INTERVAL = 700;
const unsigned long LIGHT_LOG_INTERVAL = 10000;
const unsigned long GYRO_INTERVAL = 100;
const unsigned long SHAKE_COOLDOWN_INTERVAL = 3500;

const unsigned long MOOD_UPDATE_INTERVAL = 10000;
const unsigned long MEMORY_SAVE_INTERVAL = 30000;
const unsigned long GAME_CONTROL_INTERVAL = 80;
const unsigned long GAME_STAR_INTERVAL = 130;
const unsigned long GAME_LEVEL_BANNER_INTERVAL = 1100;
const unsigned long GAME_TOTAL_INTERVAL = 30000;
const uint8_t GAME_MAX_LEVEL = 3;
// Editable behavior defaults and names are defined in OishiaSettings.h.
OishiaSettings personalSettings;
char personalMessage[22] = {};

// 0 = waiting, 1 = closed, 2 = short open between a double blink, 3 = closed again.
uint8_t blinkStage = 0;
bool doubleBlink = false;
int8_t pupilX = 0;
int8_t pupilY = 0;
int8_t faceBob = 0;
uint8_t heartFrame = 0;
uint8_t sleepFrame = 0;
uint8_t moodFrame = 0;
uint8_t shakeStreak = 0;
uint8_t buttonClickCount = 0;
int lastMessageIndex = -1;
int lastArrivalMessageIndex = -1;
int lastPetResponseIndex = -1;
const char* activeMessage = 0;
SoundType soundType = SOUND_NONE;
uint8_t soundStep = 0;

const char* const petMessages[] = {
  "Love you!", "Hi {owner}!", "Miss you!", "{pet} is here",
  "Always together", "You are cute", "My favorite", "Love you {owner}",
  "Stay with me", "Our little world", "Forever us", "You make me smile",
  "With you, always", "Sending love", "Better together"
};
const uint8_t MESSAGE_COUNT = sizeof(petMessages) / sizeof(petMessages[0]);

const char* const arrivalMessages[] = {
  "I see you!", "Welcome back!", "Hi {owner}!", "You are here!"
};
const uint8_t ARRIVAL_MESSAGE_COUNT = sizeof(arrivalMessages) / sizeof(arrivalMessages[0]);

const char* const petResponses[] = {
  "That tickles!", "Hehe!", "Again!", "So nice!"
};
const uint8_t PET_RESPONSE_COUNT = sizeof(petResponses) / sizeof(petResponses[0]);

void readSensor();
void loadMemory();
void saveMemory(unsigned long now);
void recordPet();
void recordVisit();
void updateRelationshipLevel();
void adjustMood(int happyChange, int worryChange, int irritationChange);
void updateMood(unsigned long now);
bool quietHoursActive();
void addActivity(const char *type, const char *text);
void updateHistory(unsigned long now);
void updateRoutines(unsigned long now);
void updateDisplayCare(unsigned long now);
void setupMPU6500();
bool writeMPURegister(uint8_t reg, uint8_t value);
bool readMPUAcceleration();
void updateGyro(unsigned long now);
void startGame(unsigned long now);
void updateGame(unsigned long now);
void updateButton(unsigned long now);
void updateMotion(unsigned long now);
void updateLight(unsigned long now);
void updateBuzzer(unsigned long now);
void updatePet(unsigned long now);
void updateAnimation(unsigned long now);
void updateBlink(unsigned long now);
void changeState(PetState nextState, unsigned long duration);
void chooseNextState(unsigned long now);
void setMessageForState();
const char* stateName(PetState state);
void drawPet();
void updateStopwatch(unsigned long now);
void drawStopwatch();
void drawBootScreen(unsigned long elapsed);
void drawSensorScreen();
void drawGameScreen();
void drawClockScreen();
bool clockScreenVisible(unsigned long now);
void drawFace(PetState state, int yOffset);
void drawMascotFace(EyeStyle style, PetState state, int yOffset);
void drawEyes(EyeStyle style, int yOffset);
void drawOpenEye(int x, int y, int w, int h, int lookX, int lookY);
void drawMouth(PetState state, int yOffset);
void drawCheeks(int yOffset);
void drawHeart(int x, int y, uint8_t size);
void drawSparkles(uint8_t frame);
void drawArrivalSparkles(uint8_t frame);
void drawWorryDrops(int yOffset, uint8_t frame);
void drawAngrySteam(uint8_t frame);
void drawCenteredText(const char* text, int y, uint8_t size);
void startSound(SoundType nextSound);
bool getSoundStep(SoundType type, uint8_t step, uint16_t* note, uint16_t* duration);
void updateConnectivity(unsigned long now);
bool applyRemoteReaction(unsigned long now);
void cancelRemoteReaction();
void drawNetworkScreen();

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(34));
  loadMemory();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LDR_PIN, INPUT);
  buttonReading = digitalRead(BUTTON_PIN);
  buttonStableState = buttonReading;
  buttonPressedAt = lastButtonChangeAt = millis();
  pirWasHigh = digitalRead(PIR_PIN) == HIGH;
  PetLog.print("Motion sensor at startup: ");
  PetLog.println(pirWasHigh ? "HIGH" : "LOW");
  dht.begin();

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeOut(25);
  setupMPU6500();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    PetLog.println("OLED not found");
  } else {
    displayReady = true;
    display.clearDisplay();
    display.display();
  }

  loopNow = millis();
  bootStartedAt = loopNow;
  if (pirWasHigh) pirHighSinceAt = loopNow;
  lastPresenceAt = loopNow;
  stateStartedAt = loopNow;
  nextBlinkAt.start(millis(), random(2500, 5501));
  nextPupilMoveAt.start(millis(), random(1300, 2801));
  nextSensorAt.start(millis(), random(15000, 23001));
  PetLog.println("Oishia + Sathu");
  PetLog.println("Pet state: BOOT");
  connectivityReady = beginConnectivity();
  if (!connectivityReady) PetLog.println("Oishia: Wi-Fi unavailable; continuing offline.");
  addActivity("system", "Oishia started");
}

void loop() {
  uint32_t loopStarted = micros();
  loopNow = millis();
  unsigned long now = loopNow;
  // Retire every timer even when its screen/state is inactive.
  nextSensorAt.running(now); transitionUntil.running(now); nextPupilMoveAt.running(now);
  nextBlinkAt.running(now); buttonClickDeadline.running(now); gameFlashUntil.running(now);
  gameLevelBannerUntil.running(now); motionCelebrationUntil.running(now);
  clockPreviewUntil.running(now);
  updateConnectivity(now);
  if (now - lastSensorReadAt >= SENSOR_INTERVAL) {
    lastSensorReadAt = now;
    readSensor();
  }
  updateMood(now);
  saveMemory(now);
  updateGyro(now);
  updateStopwatch(now);
  updateButton(now);
  updateMotion(now);
  updateLight(now);
  updateHistory(now);
  updateRoutines(now);
  updateDisplayCare(now);
  updateBuzzer(now);

  if (!stopwatchVisible && !applyRemoteReaction(now)) updatePet(now);
  if (now - lastAnimationAt >= ANIMATION_INTERVAL) {
    lastAnimationAt = now;
    updateAnimation(now);
    if (displayReady && !displaySleeping) drawPet();
  }
  maxLoopMicros = max(maxLoopMicros, uint32_t(micros() - loopStarted));
  delay(1);
}

void loadMemory() {
  if (!preferences.begin("aru", false)) {
    PetLog.println("Memory unavailable (session-only friendship)");
    return;
  }
  memoryReady = true;
  soundMuted = preferences.getBool("muted", false);
  totalPets = preferences.getUInt("pets", 0);
  totalVisits = preferences.getUInt("visits", 0);
  OishiaMemoryRecord record;
  if (preferences.getBytesLength("pet-v1") == sizeof(record) &&
      preferences.getBytes("pet-v1", &record, sizeof(record)) == sizeof(record) && record.valid()) {
    totalPets = record.pets; totalVisits = record.visits; soundMuted = record.muted;
  } else {
    // Migrate legacy values only after a successful atomic write.
    memoryDirty = true;
  }
  updateRelationshipLevel();
  PetLog.print("Oishia memory: ");
  PetLog.print(totalPets);
  PetLog.print(" pets, ");
  PetLog.print(totalVisits);
  PetLog.println(" visits");
}

void saveMemory(unsigned long now) {
  if (!memoryReady || !memoryDirty || now - lastMemorySaveAt < MEMORY_SAVE_INTERVAL) return;
  OishiaMemoryRecord record{1, totalPets, totalVisits, soundMuted ? 1u : 0u};
  memoryHealthy = saveOishiaMemory(preferences, record, memoryDirty);
  lastMemorySaveAt = now;  // Also back off failed writes; never retry every loop.
  PetLog.println(memoryHealthy ? "Oishia memory saved" : "Oishia memory write failed; retrying later");
}

void updateRelationshipLevel() {
  relationshipLevel = totalPets >= 100 ? 5 : uint8_t(totalPets / 25 + 1);
}

void recordPet() {
  totalPets++;
  addActivity("interaction", "Affection received");
  updateRelationshipLevel();
  memoryDirty = true;
  if (totalPets % 25 == 0) {
    snprintf(relationshipMessage, sizeof(relationshipMessage), "Friend level %u!", relationshipLevel);
    activeMessage = relationshipMessage;
  }
}

void recordVisit() {
  totalVisits++;
  memoryDirty = true;
  if (totalVisits % 10 == 0) {
    snprintf(relationshipMessage, sizeof(relationshipMessage), "Visit #%lu!", (unsigned long)totalVisits);
    activeMessage = relationshipMessage;
  }
}

void adjustMood(int happyChange, int worryChange, int irritationChange) {
  happiness = constrain(happiness + happyChange, 0, 100);
  worry = constrain(worry + worryChange, 0, 100);
  irritation = constrain(irritation + irritationChange, 0, 100);
}

void updateMood(unsigned long now) {
  if (now - lastMoodUpdateAt < MOOD_UPDATE_INTERVAL) return;
  lastMoodUpdateAt = now;

  // Emotions gently return to calm unless the environment keeps influencing Oishia.
  if (happiness < 55) happiness++;
  else if (happiness > 55) happiness--;
  if (worry > 0) worry--;
  if (irritation > 0) irritation--;

  if (!sensorOK) {
    adjustMood(-3, 15, 0);
  } else if (temperature * 10.0f > personalSettings.comfortableMaxTenths ||
             temperature * 10.0f < personalSettings.comfortableMinTenths ||
             humidity > personalSettings.comfortableMaxHumidity || humidity < personalSettings.comfortableMinHumidity) {
    adjustMood(-3, 3, 7);
  } else {
    adjustMood(2, -2, -2);
  }
}

bool quietHoursActive() {
  if (!personalSettings.quietHoursEnabled) return false;
  time_t current = time(nullptr);
  if (!oishiaClockTimeValid(current)) return false;
  struct tm local = {};
  localtime_r(&current, &local);
  uint16_t minute = uint16_t(local.tm_hour * 60 + local.tm_min);
  uint16_t start = uint16_t(personalSettings.quietStartHour * 60 + personalSettings.quietStartMinute);
  uint16_t end = uint16_t(personalSettings.quietEndHour * 60 + personalSettings.quietEndMinute);
  if (start == end) return false;
  return start < end ? minute >= start && minute < end : minute >= start || minute < end;
}

void addActivity(const char *type, const char *text) {
  uint8_t index;
  if (activityLog.count < OISHIA_EVENT_CAPACITY) {
    index = (activityLog.start + activityLog.count) % OISHIA_EVENT_CAPACITY;
    activityLog.count++;
  } else {
    index = activityLog.start;
    activityLog.start = (activityLog.start + 1) % OISHIA_EVENT_CAPACITY;
  }
  OishiaEvent &event = activityLog.events[index];
  event.uptimeSeconds = millis() / 1000u;
  time_t current = time(nullptr);
  event.epoch = oishiaClockTimeValid(current) ? uint32_t(current) : 0;
  snprintf(event.type, sizeof(event.type), "%s", type);
  snprintf(event.text, sizeof(event.text), "%s", text);
  publishOishiaEvents(activityLog);
}

void updateHistory(unsigned long now) {
  if (lastHistoryAt && now - lastHistoryAt < HISTORY_INTERVAL) return;
  if (!lastHistoryAt && now - bootStartedAt < 5000) return;
  lastHistoryAt = now;
  uint8_t index;
  if (sensorHistory.count < OISHIA_HISTORY_CAPACITY) {
    index = (sensorHistory.start + sensorHistory.count) % OISHIA_HISTORY_CAPACITY;
    sensorHistory.count++;
  } else {
    index = sensorHistory.start;
    sensorHistory.start = (sensorHistory.start + 1) % OISHIA_HISTORY_CAPACITY;
  }
  OishiaHistorySample &sample = sensorHistory.samples[index];
  sample = {};
  sample.uptimeSeconds = now / 1000u;
  time_t current = time(nullptr);
  sample.epoch = oishiaClockTimeValid(current) ? uint32_t(current) : 0;
  if (sensorOK) {
    sample.valid |= 1;
    sample.temperatureTenths = int16_t(roundf(temperature * 10.0f));
    sample.humidity = uint8_t(constrain(int(roundf(humidity)), 0, 100));
  }
  if (haveLightReading) { sample.valid |= 2; sample.light = uint16_t(constrain(lightLevel, 0, 4095)); }
  publishOishiaHistory(sensorHistory);
}

void updateRoutines(unsigned long now) {
  if (now - lastRoutineCheckAt < 1000) return;
  lastRoutineCheckAt = now;
  time_t current = time(nullptr);
  if (!oishiaClockTimeValid(current)) { clockWasSynchronized = false; return; }
  if (!clockWasSynchronized) { clockWasSynchronized = true; addActivity("clock", "Time synchronized"); }
  struct tm local = {};
  localtime_r(&current, &local);
  uint32_t stamp = uint32_t(local.tm_year + 1900) * 600000u + uint32_t(local.tm_yday) * 1440u +
                   uint32_t(local.tm_hour) * 60u + uint32_t(local.tm_min);
  for (uint8_t i = 0; i < 3; ++i) {
    const OishiaRoutine &routine = personalSettings.routines[i];
    if (!routine.enabled || routine.hour != local.tm_hour || routine.minute != local.tm_min ||
        !(routine.weekdays & (1u << local.tm_wday)) || lastRoutineStamp[i] == stamp) continue;
    lastRoutineStamp[i] = stamp;
    cancelRemoteReaction();
    if (routine.action == ROUTINE_SLEEP) {
      remoteSleeping = true; remoteReactionActive = true; remoteReactionState = PET_SLEEP;
      remoteReactionStartedAt = now; remoteReactionDuration = 5000;
      snprintf(remoteReactionMessage, sizeof(remoteReactionMessage), "%s", routine.message);
      changeState(PET_SLEEP, 60000); activeMessage = remoteReactionMessage;
    } else if (routine.action == ROUTINE_WAKE) {
      changeState(PET_HAPPY, 8000); activeMessage = routine.message; startSound(SOUND_WAKE);
    } else if (routine.action == ROUTINE_CLOCK) {
      clockPreviewUntil.start(now, 60000);
    } else {
      changeState(routine.action == ROUTINE_GREETING ? PET_GREETING : PET_HAPPY, 8000);
      activeMessage = routine.message;
      startSound(routine.action == ROUTINE_GREETING ? SOUND_GREETING : SOUND_PET);
    }
    addActivity("routine", routine.message);
  }
}

void updateDisplayCare(unsigned long now) {
  if (!displayReady || now - lastDisplayCareAt < 1000) return;
  lastDisplayCareAt = now;
  bool quiet = quietHoursActive();
  bool shouldSleep = personalSettings.displayOffQuiet && quiet && !networkScreenVisible && !stopwatchVisible &&
                     now - lastPresenceAt >= 60000;
  if (shouldSleep != displaySleeping) {
    displaySleeping = shouldSleep;
    display.ssd1306_command(displaySleeping ? SSD1306_DISPLAYOFF : SSD1306_DISPLAYON);
    if (!displaySleeping) addActivity("display", "Display woke");
  }
  uint8_t contrast = (quiet || roomDark) ? personalSettings.displayNightContrast : personalSettings.displayDayContrast;
  if (!displaySleeping && contrast != appliedContrast) {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(contrast);
    appliedContrast = contrast;
  }
}

void setupMPU6500() {
  const uint8_t addresses[] = { MPU6500_ADDRESS_LOW, MPU6500_ADDRESS_HIGH };
  for (uint8_t i = 0; i < sizeof(addresses); i++) {
    mpuAddress = addresses[i];
    Wire.beginTransmission(mpuAddress);
    if (Wire.endTransmission() != 0) continue;

    // Wake the MPU-6500 and use its +-2g accelerometer range.
    if (writeMPURegister(0x6B, 0x00) && writeMPURegister(0x1C, 0x00)) {
      mpuReady = true;
      haveMPUSample = false;
      tiltDirection = previousTiltDirection = 0;
      PetLog.print("MPU-6500 ready at 0x");
      PetLog.println(mpuAddress, HEX);
      return;
    }
  }
  PetLog.println("MPU-6500 not found (tilt reactions disabled)");
}

bool writeMPURegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readMPUAcceleration() {
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddress, (uint8_t)6) != 6) return false;

  int16_t rawX = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t rawY = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t rawZ = ((int16_t)Wire.read() << 8) | Wire.read();
  accelX = rawX / 16384.0f;
  accelY = rawY / 16384.0f;
  accelZ = rawZ / 16384.0f;
  return true;
}

void updateGyro(unsigned long now) {
  if (!mpuReady) {
    if (now - lastMPUProbeAt >= 5000) { lastMPUProbeAt = now; setupMPU6500(); }
    return;
  }
  if (now - lastGyroReadAt < GYRO_INTERVAL) return;
  lastGyroReadAt = now;
  if (!readMPUAcceleration()) {
    mpuReady = false;
    haveMPUSample = false;
    tiltDirection = previousTiltDirection = 0;
    lastMPUProbeAt = now;
    if (gameActive) { changeState(PET_CURIOUS, 3200); activeMessage = "Tilt unavailable"; }
    PetLog.println("MPU-6500 read failed (tilt reactions disabled)");
    return;
  }

  if (accelX > 0.35f) tiltDirection = 1;
  else if (accelX < -0.35f) tiltDirection = -1;
  else tiltDirection = 0;

  if (tiltDirection != previousTiltDirection) {
    previousTiltDirection = tiltDirection;
    if (tiltDirection == 0) PetLog.println("Gyro: level");
    else PetLog.println(tiltDirection > 0 ? "Gyro: tilted right" : "Gyro: tilted left");
  }

  if (!haveMPUSample) {
    previousAccelX = accelX;
    previousAccelY = accelY;
    previousAccelZ = accelZ;
    haveMPUSample = true;
    return;
  }

  float movement = fabs(accelX - previousAccelX) +
                   fabs(accelY - previousAccelY) +
                   fabs(accelZ - previousAccelZ);
  previousAccelX = accelX;
  previousAccelY = accelY;
  previousAccelZ = accelZ;

  if (movement >= 0.85f && now - lastShakeAt >= SHAKE_COOLDOWN_INTERVAL) {
    lastShakeAt = now;
    motionCelebrationUntil.start(now, 3600);
    if (now - lastShakeSequenceAt <= 15000) shakeStreak++;
    else shakeStreak = 1;
    lastShakeSequenceAt = now;
    if (petState == PET_SLEEP) {
      changeState(PET_HAPPY, 4200);
      activeMessage = "Up we go!";
      adjustMood(4, -2, -4);
      startSound(SOUND_WAKE);
      PetLog.println("Gyro: wake movement");
    } else if (shakeStreak >= 3) {
      shakeStreak = 0;
      changeState(PET_ANGRY, 4600);
      activeMessage = "Easy, please!";
      adjustMood(-8, 2, 50);
      startSound(SOUND_ALERT);
      PetLog.println("Gyro: too much shaking");
    } else {
      changeState(PET_EXCITED, 3600);
      activeMessage = "Wheee!";
      adjustMood(3, 0, 4);
      startSound(SOUND_PET);
      PetLog.println("Gyro: shake detected");
    }
  }
}

void startGame(unsigned long now) {
  if (!mpuReady || !haveMPUSample) {
    changeState(PET_CURIOUS, 3200);
    activeMessage = "Tilt unavailable";
    return;
  }
  gameActive = true;
  gameScore = 0;
  gameMisses = 0;
  gameLevel = 1;
  gameLevelCatches = 0;
  catcherX = 50;
  starX = random(12, 116);
  starY = 9;
  gameBaselineX = accelX;
  gameBaselineY = accelY;
  gameTiltAxis = 0;
  gameStartedAt = now;
  lastGameMoveAt = now;
  lastGameControlAt = now;
  gameFlashUntil.stop();
  gameLevelBannerUntil.stop();
  changeState(PET_GAME, GAME_TOTAL_INTERVAL);
  startSound(SOUND_GREETING);
  PetLog.println("Game: catch the star");
}

void updateGame(unsigned long now) {
  if (!gameActive) return;

  // Pause briefly on a level-up so the player can see the Star champ banner.
  if (gameLevelBannerUntil.running(now)) return;

  if (now - lastGameControlAt >= GAME_CONTROL_INTERVAL) {
    lastGameControlAt = now;
    float tiltX = accelX - gameBaselineX;
    float tiltY = accelY - gameBaselineY;
    if (gameTiltAxis == 0 && (fabs(tiltX) > 0.12f || fabs(tiltY) > 0.12f)) {
      gameTiltAxis = fabs(tiltX) >= fabs(tiltY) ? 1 : 2;
      PetLog.println(gameTiltAxis == 1 ? "Game control: MPU X axis" : "Game control: MPU Y axis");
    }
    float tilt = gameTiltAxis == 2 ? tiltY : tiltX;
    if (personalSettings.tiltInvert) tilt = -tilt;
    if (fabs(tilt) > 0.10f) {
      int step = constrain((int)(fabs(tilt) * 12.0f), 3, 9);
      if (tilt < 0) catcherX -= step;
      else catcherX += step;
    }
    catcherX = constrain(catcherX, 5, 95);
  }

  unsigned long starInterval = GAME_STAR_INTERVAL - (gameLevel - 1) * 25;
  if (now - lastGameMoveAt >= starInterval) {
    lastGameMoveAt = now;
    starY += 1;
    if (starY >= 46) {
      if (starX >= catcherX - 2 && starX <= catcherX + 30) {
        gameScore++;
        gameLevelCatches++;
        gameFlashUntil.start(now, 300);
        adjustMood(6, -3, -3);
        startSound(SOUND_PET);

        // Each level needs two catches, then carries on a little faster.
        if (gameLevelCatches >= 2 && gameLevel < GAME_MAX_LEVEL) {
          gameLevel++;
          gameLevelCatches = 0;
          gameLevelBannerUntil.start(now, GAME_LEVEL_BANNER_INTERVAL);
          startSound(SOUND_GREETING);
          PetLog.print("Game level: ");
          PetLog.println(gameLevel);
        }
      } else {
        gameMisses++;
        adjustMood(-1, 1, 0);
      }
      starX = random(12, 116);
      starY = 9;
    }
  }

  if (now - gameStartedAt >= GAME_TOTAL_INTERVAL) {
    gameActive = false;
    if (gameScore >= 2) {
      changeState(PET_HAPPY, 4200);
      activeMessage = "Star champ!";
      recordPet();
    } else {
      changeState(PET_CURIOUS, 3600);
      activeMessage = "Nice try!";
    }
    PetLog.print("Game score: ");
    PetLog.println(gameScore);
  }
}

void updateButton(unsigned long now) {
  bool reading = digitalRead(BUTTON_PIN);
  // Wait for the next press to finish before deciding how many taps were made.
  if (pendingButtonClick && reading == HIGH && buttonStableState == HIGH &&
      buttonClickDeadline.due(now)) {
    pendingButtonClick = false;
    if (stopwatchVisible) {
      if (buttonClickCount == 1) {
        stopwatchRunning = !stopwatchRunning;
        stopwatchLastTick = uint32_t(now);
      } else if (buttonClickCount == 2) {
        stopwatchRunning = false;
        stopwatchElapsedMs = 0;
      }
    } else if (buttonClickCount >= 4) {
      stopwatchVisible = true;
      stopwatchRunning = false;
      stopwatchLastTick = uint32_t(now);
      gameActive = false;
      cancelRemoteReaction();
      clockPreviewUntil.stop();
      networkScreenVisible = false;
      changeState(PET_IDLE, 5000);
    } else if (buttonClickCount == 1) {
      int response = random(0, PET_RESPONSE_COUNT);
      if (PET_RESPONSE_COUNT > 1 && response == lastPetResponseIndex) {
        response = (response + 1) % PET_RESPONSE_COUNT;
      }
      lastPetResponseIndex = response;
      changeState(PET_EXCITED, 3200);
      activeMessage = petResponses[response];
      recordPet();
      adjustMood(12, -6, -10);
      startSound(SOUND_PET);
    } else if (buttonClickCount == 2) {
      changeState(PET_SENSOR, 5200);
      startSound(SOUND_PET);
    } else if (buttonClickCount == 3) {
      startGame(now);
    }
    buttonClickCount = 0;
  }

  if (reading == LOW) lastPresenceAt = now;
  if (reading != buttonReading) {
    buttonReading = reading;
    lastButtonChangeAt = now;
  }

  if (!stopwatchVisible && reading == LOW && buttonStableState == LOW && !setupHoldHandled &&
      uint32_t(now - buttonPressedAt) >= BUTTON_SETUP_HOLD_INTERVAL) {
    setupHoldHandled = true;
    pendingButtonClick = false;
    buttonClickCount = 0;
    requestNetworkSetup();
    networkScreenVisible = true;
    networkScreenStartedAt = now;
  }

  if (!stopwatchVisible && reading == LOW && buttonStableState == LOW && !ownerResetHandled &&
      uint32_t(now - buttonPressedAt) >= BUTTON_RESET_HOLD_INTERVAL) {
    ownerResetHandled = true;
    requestOwnerReset();
    networkScreenVisible = true;
    networkScreenStartedAt = now;
  }

  if (uint32_t(now - lastButtonChangeAt) < BUTTON_DEBOUNCE_INTERVAL || reading == buttonStableState) {
    return;
  }

  buttonStableState = reading;
  if (buttonStableState == LOW) {
    buttonPressedAt = now;
    setupHoldHandled = false;
    ownerResetHandled = false;
    return;
  }

  if (setupHoldHandled) return;
  cancelRemoteReaction();
  networkScreenVisible = false;

  uint32_t pressLength = uint32_t(now - buttonPressedAt);
  if (pressLength >= BUTTON_LONG_PRESS_INTERVAL) {
    pendingButtonClick = false;
    buttonClickCount = 0;
    if (stopwatchVisible) {
      stopwatchRunning = false;
      stopwatchVisible = false;
      changeState(PET_IDLE, 5000);
    } else if (petState == PET_GAME) {
      startGame(now);
      PetLog.println("Game reset");
    } else if (petState == PET_SLEEP) {
      changeState(PET_HAPPY, 4200);
      activeMessage = "I'm awake!";
      adjustMood(5, -3, -3);
      startSound(SOUND_WAKE);
    } else {
      changeState(PET_SLEEP, 7000);
      activeMessage = "Cozy time";
      adjustMood(2, -4, -4);
      startSound(SOUND_PET);
    }
    return;
  }

  buttonClickCount++;
  pendingButtonClick = true;
  buttonClickDeadline.start(now, DOUBLE_PRESS_INTERVAL);
}

void updateMotion(unsigned long now) {
  bool motionHigh = digitalRead(PIR_PIN) == HIGH;
  bool warming = now - bootStartedAt < PIR_WARMUP_INTERVAL;
  if (motionHigh && !pirWasHigh) pirHighSinceAt = now;
  if (!motionHigh) { pirHighSinceAt = 0; pirStuck = false; }
  if (motionHigh && pirHighSinceAt && !pirStuck &&
      now - pirHighSinceAt >= uint32_t(personalSettings.pirStuckSeconds) * 1000u) {
    pirStuck = true;
    addActivity("sensor", "PIR stuck HIGH");
  }
  if (motionHigh && !pirStuck) lastPresenceAt = now;
  if (motionHigh != pirWasHigh) {
    PetLog.println(motionHigh ? "Motion: detected" : "Motion: clear");
  }
  if (now - lastPirStatusAt >= PIR_STATUS_INTERVAL) {
    lastPirStatusAt = now;
    PetLog.print("Motion sensor: ");
    PetLog.println(motionHigh ? "HIGH" : "LOW");
  }
  if (motionHigh && !pirWasHigh && !warming && !pirStuck &&
      now - lastMotionAt >= personalSettings.motionCooldownSeconds * 1000u) {
    lastMotionAt = now;
    if (petState != PET_BOOT && petState != PET_GREETING) {
      int message = random(0, ARRIVAL_MESSAGE_COUNT);
      if (ARRIVAL_MESSAGE_COUNT > 1 && message == lastArrivalMessageIndex) {
        message = (message + 1) % ARRIVAL_MESSAGE_COUNT;
      }
      lastArrivalMessageIndex = message;
      motionCelebrationUntil.start(now, 4800);
      changeState(PET_EXCITED, 4800);
      activeMessage = arrivalMessages[message];
      recordVisit();
      adjustMood(7, -4, -4);
      startSound(SOUND_GREETING);
      addActivity("presence", "Arrival detected");
    }
  }
  pirWasHigh = motionHigh;
}

void updateLight(unsigned long now) {
  if (now - lastLightReadAt < LIGHT_INTERVAL) {
    return;
  }
  lastLightReadAt = now;
  int rawLight = analogRead(LDR_PIN);
  if (!haveLightReading) {
    lightLevel = rawLight;
    haveLightReading = true;
  } else {
    // Smooth the LDR so a passing shadow does not make Oishia jump to sleep.
    lightLevel = (lightLevel * 3 + rawLight) / 4;
  }

  bool wasDark = roomDark;
  if (!roomDark && lightLevel < int(personalSettings.darkThreshold)) {
    roomDark = true;
    darkSinceAt = now;
  } else if (roomDark && lightLevel > int(personalSettings.brightThreshold)) {
    roomDark = false;
    if (petState == PET_SLEEP) {
      changeState(PET_HAPPY, 4500);
      activeMessage = "Good morning!";
      startSound(SOUND_WAKE);
    }
  }

  if (wasDark != roomDark) {
    PetLog.print("Light: ");
    PetLog.print(lightLevel);
    PetLog.println(roomDark ? " (dark)" : " (bright)");
  }
  if (now - lastLightLogAt >= LIGHT_LOG_INTERVAL) {
    lastLightLogAt = now;
    PetLog.print("Light level: ");
    PetLog.println(lightLevel);
  }
}

void startSound(SoundType nextSound) {
  noTone(BUZZER_PIN);
  if (soundMuted || remoteSleeping || personalSettings.soundProfile == 0 || quietHoursActive()) {
    soundType = SOUND_NONE;
    return;
  }
  soundType = nextSound;
  soundStep = 0;
  soundStepStartedAt = 0;
}

bool getSoundStep(SoundType type, uint8_t step, uint16_t* note, uint16_t* duration) {
  static const uint16_t petNotes[] = { 880, 1175 };
  static const uint16_t petDurations[] = { 80, 110 };
  static const uint16_t greetingNotes[] = { 784, 1047, 1319 };
  static const uint16_t greetingDurations[] = { 75, 75, 120 };
  static const uint16_t alertNotes[] = { 330, 0, 330 };
  static const uint16_t alertDurations[] = { 110, 70, 140 };
  static const uint16_t wakeNotes[] = { 523, 659, 784 };
  static const uint16_t wakeDurations[] = { 80, 80, 120 };

  const uint16_t* notes = 0;
  const uint16_t* durations = 0;
  uint8_t count = 0;
  if (type == SOUND_PET) {
    notes = petNotes; durations = petDurations; count = 2;
  } else if (type == SOUND_GREETING) {
    notes = greetingNotes; durations = greetingDurations; count = 3;
  } else if (type == SOUND_ALERT) {
    notes = alertNotes; durations = alertDurations; count = 3;
  } else if (type == SOUND_WAKE) {
    notes = wakeNotes; durations = wakeDurations; count = 3;
  }
  if (step >= count) return false;
  *note = notes[step];
  *duration = durations[step];
  return true;
}

void updateBuzzer(unsigned long now) {
  if (soundType == SOUND_NONE) return;

  uint16_t note;
  uint16_t duration;
  if (!getSoundStep(soundType, soundStep, &note, &duration)) {
    noTone(BUZZER_PIN);
    soundType = SOUND_NONE;
    return;
  }

  if (soundStepStartedAt == 0 || now - soundStepStartedAt >= duration) {
    if (soundStepStartedAt != 0) soundStep++;
    if (!getSoundStep(soundType, soundStep, &note, &duration)) {
      noTone(BUZZER_PIN);
      soundType = SOUND_NONE;
      return;
    }
    soundStepStartedAt = now;
    if (personalSettings.soundProfile == 2 && note) note = uint16_t(note * 1.12f);
    if (note == 0) noTone(BUZZER_PIN);
    else tone(BUZZER_PIN, note);
  }
}

void readSensor() {
  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();
  if (isnan(newTemperature) || isnan(newHumidity)) {
    bool wasSensorOK = sensorOK;
    sensorOK = false;
    adjustMood(-8, 45, 0);
    PetLog.println("Failed to read sensor");
    if (hadSensorReading && wasSensorOK) startSound(SOUND_ALERT);
    if (petState != PET_BOOT && petState != PET_GREETING && petState != PET_SAD) {
      changeState(PET_SAD, 4500);
    }
    if (!sensorFailureLogged) { sensorFailureLogged = true; addActivity("sensor", "Climate read failed"); }
    return;
  }

  float oldTemperature = temperature;
  float oldHumidity = humidity;
  bool wasSleeping = petState == PET_SLEEP;
  temperature = newTemperature;
  humidity = newHumidity;
  sensorOK = true;
  if (sensorFailureLogged) { sensorFailureLogged = false; addActivity("sensor", "Climate recovered"); }
  PetLog.print("Temperature: ");
  PetLog.print(temperature, 1);
  PetLog.print(" C | Humidity: ");
  PetLog.print(humidity, 0);
  PetLog.println(" %");

  if (hadSensorReading && wasSleeping &&
      (fabs(temperature - oldTemperature) >= 2.0f || fabs(humidity - oldHumidity) >= 10.0f)) {
    changeState(PET_HAPPY, random(3200, 5001));
  }
  hadSensorReading = true;
}

void updatePet(unsigned long now) {
  if (petState == PET_BOOT && now - stateStartedAt >= stateDuration) {
    changeState(PET_GREETING, 2500);
    return;
  }
  if (petState == PET_GREETING && now - stateStartedAt >= stateDuration) {
    changeState(PET_HAPPY, random(3500, 5601));
    return;
  }
  if (!sensorOK && petState != PET_BOOT && petState != PET_GREETING && petState != PET_SAD) {
    changeState(PET_SAD, 4500);
    return;
  }
  if (petState == PET_GAME) {
    updateGame(now);
    return;
  }
  if (personalSettings.autoSleep && roomDark && now - darkSinceAt >= personalSettings.sleepAfterSeconds * 1000u &&
      petState != PET_SLEEP && petState != PET_SENSOR) {
    changeState(PET_SLEEP, 8000);
    activeMessage = "Good night";
    return;
  }
  if (personalSettings.autoSleep && roomDark && petState == PET_SLEEP && now - stateStartedAt >= stateDuration) {
    // Remain asleep in sustained darkness without repeatedly announcing a new state.
    stateStartedAt = now;
    stateDuration = 8000;
    return;
  }
  if (now - stateStartedAt >= stateDuration) chooseNextState(now);
}

void chooseNextState(unsigned long now) {
  if (!sensorOK) {
    changeState(PET_SAD, 4500);
    return;
  }
  if (personalSettings.autoSleep && roomDark && now - darkSinceAt >= personalSettings.sleepAfterSeconds * 1000u) {
    changeState(PET_SLEEP, 8000);
    activeMessage = "Good night";
    return;
  }
  if (petState == PET_SENSOR) {
    nextSensorAt.start(now, random(15000, 26001));
  } else if (nextSensorAt.due(now)) {
    changeState(PET_SENSOR, random(4600, 5601));
    return;
  }
  if (personalSettings.autoSleep && now - lastSleepAt > 60000 &&
      (temperature * 10.0f < personalSettings.comfortableMinTenths || random(0, 7) == 0)) {
    lastSleepAt = now;
    changeState(PET_SLEEP, random(5000, 7601));
    return;
  }
  if (worry >= 55) {
    changeState(PET_SAD, random(3200, 5001));
    return;
  }
  if (irritation >= 55) {
    changeState(PET_ANGRY, random(3600, 5601));
    return;
  }
  if (temperature * 10.0f >= personalSettings.comfortableMaxTenths + 20 ||
      humidity >= personalSettings.comfortableMaxHumidity + 4) {
    changeState(PET_ANGRY, random(3600, 5601));
    activeMessage = "Too stuffy!";
    return;
  }
  if (temperature * 10.0f > personalSettings.comfortableMaxTenths ||
      humidity > personalSettings.comfortableMaxHumidity ||
      temperature * 10.0f < personalSettings.comfortableMinTenths ||
      humidity < personalSettings.comfortableMinHumidity) {
    changeState(PET_SAD, random(3200, 5001));
    activeMessage = temperature * 10.0f < personalSettings.comfortableMinTenths ? "Brrr..." : "Phew...";
    return;
  }
  if (happiness >= 70) {
    const PetState cheerfulChoices[] = { PET_HAPPY, PET_HAPPY, PET_LOVE, PET_LOVE, PET_EXCITED };
    changeState(cheerfulChoices[random(0, sizeof(cheerfulChoices) / sizeof(cheerfulChoices[0]))], random(3400, 6201));
    return;
  }
  const PetState choices[] = {
    PET_IDLE, PET_HAPPY, PET_HAPPY, PET_LOVE, PET_LOVE,
    PET_THINKING, PET_CURIOUS, PET_EXCITED
  };
  changeState(choices[random(0, sizeof(choices) / sizeof(choices[0]))], random(3000, 7001));
}

void changeState(PetState nextState, unsigned long duration) {
  // Remote sleep/reactions take priority without repeatedly restarting their
  // animation whenever a sensor tries to change the mood.
  if ((remoteSleeping || (remoteReactionActive &&
       loopNow - remoteReactionStartedAt < remoteReactionDuration)) &&
      nextState != remoteReactionState) return;
  if (nextState != PET_GAME) gameActive = false;
  petState = nextState;
  stateStartedAt = loopNow;
  stateDuration = duration;
  transitionUntil.start(stateStartedAt, 140);
  setMessageForState();
  PetLog.print("Pet state: ");
  PetLog.println(stateName(petState));
}

void setMessageForState() {
  activeMessage = 0;
  if (petState == PET_GREETING) {
    if (relationshipLevel >= 5) activeMessage = "My favorite!";
    else if (relationshipLevel >= 3) activeMessage = "Hi, bestie!";
    else activeMessage = "Hi {owner}!";
  }
  else if (petState == PET_LOVE || petState == PET_EXCITED) {
    int index = random(0, MESSAGE_COUNT);
    if (MESSAGE_COUNT > 1 && index == lastMessageIndex) index = (index + 1) % MESSAGE_COUNT;
    lastMessageIndex = index;
    activeMessage = petMessages[index];
  } else if (petState == PET_THINKING) activeMessage = "Hmm...";
  else if (petState == PET_SLEEP) activeMessage = "Cozy time";
  else if (petState == PET_SAD) activeMessage = sensorOK ? "I'm worried..." : "I'm confused...";
  else if (petState == PET_ANGRY) activeMessage = "Hey, easy!";
  else if (petState == PET_GAME) activeMessage = "Catch stars!";
  else if (petState == PET_HAPPY && random(0, 3) == 0) activeMessage = "{pet} is here";
}

void updateAnimation(unsigned long now) {
  updateBlink(now);
  if (nextPupilMoveAt.due(now)) {
    pupilX = random(-1, 2);
    pupilY = random(-1, 2);
    nextPupilMoveAt.start(now, random(1500, 3701));
  }
  if (petState == PET_ANGRY) {
    faceBob = ((now / 130) % 2 == 0) ? -1 : 1;
  } else if (petState == PET_SAD) {
    faceBob = ((now / 520) % 2 == 0) ? 0 : 1;
  } else if (motionCelebrationUntil.running(now)) {
    faceBob = (int8_t)((now / 120) % 3) - 1;
  } else {
    faceBob = ((now / 700) % 2 == 0) ? 0 : 1;
  }
  heartFrame = (now / 280) % 4;
  sleepFrame = (now / 500) % 3;
  moodFrame = (now / 180) % 4;
}

void updateBlink(unsigned long now) {
  if (petState == PET_SLEEP) {
    blinkStage = 0;
    return;
  }
  if (blinkStage == 0 && nextBlinkAt.due(now)) {
    blinkStage = 1;
    blinkStartedAt = now;
    doubleBlink = random(0, 8) == 0;
  } else if (blinkStage == 1 && now - blinkStartedAt >= 170) {
    if (doubleBlink) {
      blinkStage = 2;
      blinkStartedAt = now;
    } else {
      blinkStage = 0;
      nextBlinkAt.start(now, random(2500, 5501));
    }
  } else if (blinkStage == 2 && now - blinkStartedAt >= 90) {
    blinkStage = 3;
    blinkStartedAt = now;
  } else if (blinkStage == 3 && now - blinkStartedAt >= 170) {
    blinkStage = 0;
    nextBlinkAt.start(now, random(2500, 5501));
  }
}

const char* stateName(PetState state) {
  switch (state) {
    case PET_BOOT: return "BOOT";
    case PET_IDLE: return "IDLE";
    case PET_HAPPY: return "HAPPY";
    case PET_LOVE: return "LOVE";
    case PET_SLEEP: return "SLEEP";
    case PET_THINKING: return "THINKING";
    case PET_CURIOUS: return "CURIOUS";
    case PET_EXCITED: return "EXCITED";
    case PET_SAD: return "SAD";
    case PET_ANGRY: return "ANGRY";
    case PET_GAME: return "GAME";
    case PET_SENSOR: return "SENSOR";
    case PET_GREETING: return "GREETING";
  }
  return "UNKNOWN";
}

void drawPet() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  if (stopwatchVisible) drawStopwatch();
  else if (networkScreenVisible && petState != PET_BOOT) drawNetworkScreen();
  else if (petState == PET_BOOT) drawBootScreen(loopNow - stateStartedAt);
  else if (petState == PET_GAME) drawGameScreen();
  else if (petState == PET_SENSOR) drawSensorScreen();
  else if (clockScreenVisible(loopNow)) drawClockScreen();
  else {
    drawFace(petState, faceBob);
    if (activeMessage) {
      if (activeMessage == remoteReactionMessage) drawCenteredText(activeMessage, 55, 1);
      else { personalizeOishiaMessage(personalMessage, sizeof(personalMessage), activeMessage, personalSettings); drawCenteredText(personalMessage, 55, 1); }
    }
  }
  display.display();
}

void updateStopwatch(unsigned long now) {
  const uint32_t tick = uint32_t(now);
  if (stopwatchRunning) stopwatchElapsedMs += uint32_t(tick - stopwatchLastTick);
  stopwatchLastTick = tick;
}

void drawStopwatch() {
  const uint32_t hours = uint32_t(stopwatchElapsedMs / 3600000u);
  const unsigned minutes = unsigned((stopwatchElapsedMs / 60000u) % 60u);
  const unsigned seconds = unsigned((stopwatchElapsedMs / 1000u) % 60u);
  const unsigned tenths = unsigned((stopwatchElapsedMs / 100u) % 10u);
  char elapsedText[24];
  char detailText[22];
  snprintf(elapsedText, sizeof(elapsedText), "%02lu:%02u:%02u", (unsigned long)hours, minutes, seconds);
  snprintf(detailText, sizeof(detailText), ".%u  %s", tenths, stopwatchRunning ? "RUNNING" : "PAUSED");
  drawCenteredText("STOPWATCH", 1, 1);
  drawCenteredText(elapsedText, 16, hours < 100 ? 2 : 1);
  drawCenteredText(detailText, 34, 1);
  drawCenteredText(stopwatchRunning ? "Tap: pause" : "Tap: start", 45, 1);
  drawCenteredText("2x:reset Hold:exit", 55, 1);
}

bool clockScreenVisible(unsigned long now) {
  bool screenBlocked = stopwatchVisible || networkScreenVisible || petState == PET_BOOT || petState == PET_GAME || petState == PET_SENSOR;
  if (screenBlocked) return false;
  if (clockPreviewUntil.running(now)) return true;
  bool blocked = remoteSleeping || remoteReactionActive;
  return shouldShowOishiaClock(personalSettings, now, lastPresenceAt, blocked);
}

void drawClockScreen() {
  time_t clockNow = time(nullptr);
  const char *city = strrchr(personalSettings.timezone, '/');
  city = city ? city + 1 : personalSettings.timezone;
  char cityText[22];
  snprintf(cityText, sizeof(cityText), "%.21s", city);
  for (char *c = cityText; *c; ++c) if (*c == '_') *c = ' ';

  if (!oishiaClockTimeValid(clockNow)) {
    drawCenteredText("CLOCK", 4, 1);
    drawCenteredText("Waiting for time", 22, 1);
    drawCenteredText("Connect home Wi-Fi", 36, 1);
    drawCenteredText(cityText, 54, 1);
    return;
  }

  struct tm local = {};
  localtime_r(&clockNow, &local);
  char dateText[18];
  char timeText[12];
  char detailText[12];
  strftime(dateText, sizeof(dateText), "%a, %b %d", &local);
  const char *format24 = personalSettings.clockShowSeconds ? "%H:%M:%S" : "%H:%M";
  const char *format12 = personalSettings.clockShowSeconds ? "%I:%M:%S" : "%I:%M";
  if (personalSettings.clock24Hour) {
    strftime(timeText, sizeof(timeText), format24, &local);
    detailText[0] = 0;
  } else {
    strftime(timeText, sizeof(timeText), format12, &local);
    if (timeText[0] == '0') memmove(timeText, timeText + 1, strlen(timeText));
    strftime(detailText, sizeof(detailText), "%p", &local);
  }
  int8_t shift = int8_t((clockNow / 60) % 3) - 1;
  if (personalSettings.clockStyle == 1) {
    drawCenteredText(timeText, 18 + shift, 2);
    drawCenteredText(*detailText ? detailText : cityText, 44 + shift, 1);
  } else if (personalSettings.clockStyle == 2 && personalSettings.portraitEnabled) {
    drawMascotFace(EYE_HAPPY, PET_HAPPY, -4 + shift);
    display.fillRect(0, 49, 128, 15, SSD1306_BLACK);
    drawCenteredText(timeText, 52 + shift, 1);
  } else {
    drawCenteredText(dateText, 2 + shift, 1);
    drawCenteredText(timeText, 18 + shift, 2);
    if (*detailText) drawCenteredText(detailText, 42 + shift, 1);
    drawCenteredText(cityText, 55 + shift, 1);
  }
}

void cancelRemoteReaction() {
  remoteSleeping = false;
  remoteReactionActive = false;
}

bool applyRemoteReaction(unsigned long now) {
  if (!remoteSleeping && !remoteReactionActive) return false;
  if (!remoteSleeping && now - remoteReactionStartedAt >= remoteReactionDuration) {
    remoteReactionActive = false;
    return false;
  }
  if (petState != remoteReactionState) changeState(remoteReactionState, remoteReactionDuration);
  activeMessage = remoteReactionMessage;
  return true;
}

void updateConnectivity(unsigned long now) {
  if (!connectivityReady) return;
  OishiaSettings incoming;
  if (receiveOishiaSettings(incoming)) {
    bool disableSleep = personalSettings.autoSleep && !incoming.autoSleep;
    personalSettings = incoming;
    darkSinceAt = now;
    if (disableSleep && petState == PET_SLEEP && !remoteSleeping) changeState(PET_HAPPY, 4200);
  }
  bool wasSetup = networkInfo.setupActive;
  bool wasConnected = networkInfo.connected;
  getNetworkInfo(networkInfo);
  if ((!wasSetup && networkInfo.setupActive) || (!wasConnected && networkInfo.connected)) {
    networkScreenVisible = true;
    networkScreenStartedAt = now;
  }
  if (networkScreenVisible && now - networkScreenStartedAt >= 45000) networkScreenVisible = false;

  OishiaCommand command;
  // One command per loop; only this task owns pet state and the hardware.
  if (receiveOishiaCommand(command)) {
    networkScreenVisible = false;
    if (command.type == OishiaCommandType::Mute || command.type == OishiaCommandType::Unmute) {
      bool nextMuted = command.type == OishiaCommandType::Mute;
      if (nextMuted != soundMuted) memoryDirty = true;
      soundMuted = nextMuted;
      if (soundMuted) { noTone(BUZZER_PIN); soundType = SOUND_NONE; }
    } else if (command.type == OishiaCommandType::PreviewClock) {
      cancelRemoteReaction();
      clockPreviewUntil.start(now, 30000);
    } else if (command.type == OishiaCommandType::SetTime) {
      struct timeval browserTime = {time_t(command.epoch), 0};
      settimeofday(&browserTime, nullptr);
      addActivity("clock", "Dashboard set time");
    } else if (command.type == OishiaCommandType::ClearEvents) {
      activityLog = {};
      publishOishiaEvents(activityLog);
    } else {
      cancelRemoteReaction();
      remoteReactionActive = true;
      remoteReactionStartedAt = now;
      remoteReactionDuration = 5000;
      remoteReactionState = PET_HAPPY;
      if (command.type == OishiaCommandType::Love) {
        remoteReactionState = PET_LOVE;
        snprintf(remoteReactionMessage, sizeof(remoteReactionMessage), "Love you too!");
        recordPet();
        adjustMood(12, -6, -10);
        startSound(SOUND_PET);
      } else if (command.type == OishiaCommandType::Sleep) {
        remoteSleeping = true;
        remoteReactionState = PET_SLEEP;
        snprintf(remoteReactionMessage, sizeof(remoteReactionMessage), "Cozy time");
        noTone(BUZZER_PIN);
        soundType = SOUND_NONE;
        addActivity("control", "Sleep requested");
      } else if (command.type == OishiaCommandType::Wake) {
        remoteReactionDuration = 8000;
        snprintf(remoteReactionMessage, sizeof(remoteReactionMessage), "I'm awake!");
        startSound(SOUND_WAKE);
        addActivity("control", "Wake requested");
      } else if (command.type == OishiaCommandType::Message) {
        snprintf(remoteReactionMessage, sizeof(remoteReactionMessage), "%s", command.message);
        startSound(SOUND_PET);
      }
      changeState(remoteReactionState, remoteReactionDuration);
      activeMessage = remoteReactionMessage;
    }
  }

  if (now - lastSnapshotAt >= 500) {
    lastSnapshotAt = now;
    OishiaPetSnapshot snapshot = {};
    snprintf(snapshot.mood, sizeof(snapshot.mood), "%s", stateName(petState));
    if (activeMessage == remoteReactionMessage) snprintf(snapshot.message, sizeof(snapshot.message), "%s", remoteReactionMessage);
    else personalizeOishiaMessage(snapshot.message, sizeof(snapshot.message), activeMessage ? activeMessage : "", personalSettings);
    snapshot.pets = totalPets;
    snapshot.visits = totalVisits;
    snapshot.friendship = relationshipLevel;
    snapshot.happiness = happiness;
    snapshot.sleeping = petState == PET_SLEEP;
    snapshot.muted = soundMuted;
    snapshot.memoryHealthy = memoryReady && memoryHealthy;
    snapshot.memoryPending = memoryDirty;
    snapshot.tiltReady = mpuReady && haveMPUSample;
    snapshot.maxLoopMicros = maxLoopMicros;
    snapshot.climateValid = sensorOK;
    snapshot.lightValid = haveLightReading;
    snapshot.motionDetected = pirWasHigh;
    snapshot.clockVisible = clockScreenVisible(now);
    snapshot.quietHours = quietHoursActive();
    snapshot.displayOff = displaySleeping;
    snapshot.pirWarming = now - bootStartedAt < PIR_WARMUP_INTERVAL;
    snapshot.pirStuck = pirStuck;
    snapshot.presenceQuietSeconds = uint32_t(now - lastPresenceAt) / 1000u;
    snapshot.dark = roomDark;
    snapshot.temperature = temperature;
    snapshot.humidity = humidity;
    snapshot.light = lightLevel;
    publishPetSnapshot(snapshot);
  }
}

void drawNetworkScreen() {
  display.setTextSize(1);
  if (!connectivityReady) {
    drawCenteredText("Wi-Fi unavailable", 12, 1);
    drawCenteredText("Restart to retry", 28, 1);
    drawCenteredText("Tap to return", 55, 1);
    return;
  }
  drawCenteredText(networkInfo.setupActive ? "Connect to pet" : "Your pet is online", 0, 1);
  drawCenteredText(!networkInfo.ready ? "Starting Wi-Fi..." :
                  (networkInfo.setupActive ? networkInfo.hotspot : "On your home Wi-Fi"), 12, 1);
  char keyLine[20];
  snprintf(keyLine, sizeof(keyLine), "PIN: %s", networkInfo.accessKey);
  drawCenteredText(keyLine, 24, 1);
  if (networkInfo.setupActive) {
    snprintf(keyLine, sizeof(keyLine), "WiFi: %s", networkInfo.hotspotPassword);
    drawCenteredText(keyLine, 36, 1);
  }
  drawCenteredText(networkInfo.setupActive ? "192.168.4.1" : networkInfo.ip, 46, 1);
  snprintf(keyLine, sizeof(keyLine), "BLE pair: %s", networkInfo.pairingPasskey);
  drawCenteredText(networkInfo.bluetoothReady ? keyLine : "Tap to return", 55, 1);
}

void drawBootScreen(unsigned long elapsed) {
  if (elapsed < 700) {
    display.drawPixel(48, 27, SSD1306_WHITE);
    display.drawPixel(80, 27, SSD1306_WHITE);
  } else if (elapsed < 1400) {
    display.drawPixel(42, 27, SSD1306_WHITE);
    drawHeart(59, 23, 12);
    display.drawPixel(86, 27, SSD1306_WHITE);
  } else if (elapsed < 2200) {
    drawCenteredText("HELLO", 25, 1);
    drawHeart(59, 39, 10);
  } else {
    drawCenteredText(personalSettings.petName, 16, 1);
    drawHeart(59, 28, 11);
    drawCenteredText(personalSettings.ownerName, 43, 1);
  }
}

void drawSensorScreen() {
  display.setTextSize(1);
  char names[22];
  snprintf(names, sizeof(names), "%.9s + %.9s", personalSettings.petName, personalSettings.ownerName);
  drawCenteredText(names, 0, 1);

  if (!sensorOK) {
    drawFace(PET_SAD, -2);
    drawCenteredText("Sensor?", 55, 1);
    return;
  }

  // A tiny face makes the readings feel like part of Oishia's world.
  display.fillCircle(60, 13, 2, SSD1306_WHITE);
  display.fillCircle(68, 13, 2, SSD1306_WHITE);
  display.drawLine(61, 18, 64, 20, SSD1306_WHITE);
  display.drawLine(64, 20, 67, 18, SSD1306_WHITE);
  display.setCursor(4, 23);
  display.print("Temp");
  display.setCursor(74, 23);
  display.print("Humidity");

  char temperatureText[12];
  char humidityText[8];
  snprintf(temperatureText, sizeof(temperatureText), "%.1fC", temperature);
  snprintf(humidityText, sizeof(humidityText), "%.0f%%", humidity);
  display.setTextSize(2);
  display.setCursor(2, 35);
  display.print(temperatureText);
  display.setCursor(72, 35);
  display.print(humidityText);
  drawHeart(59, 53, heartFrame == 0 ? 11 : 9);
}

void drawGameScreen() {
  if (gameLevelBannerUntil.running(loopNow)) {
    drawCenteredText("Star champ!", 17, 1);
    drawHeart(58, 29, 12);
    char levelText[10];
    snprintf(levelText, sizeof(levelText), "Level %u", gameLevel);
    drawCenteredText(levelText, 45, 1);
    return;
  }

  display.setTextSize(1);
  display.setCursor(3, 0);
  display.print("L");
  display.print(gameLevel);
  display.print(" Catch!");
  display.setCursor(102, 0);
  display.print(gameScore);

  // Falling sparkle.
  display.drawPixel(starX, starY - 2, SSD1306_WHITE);
  display.drawPixel(starX - 2, starY, SSD1306_WHITE);
  display.drawPixel(starX, starY, SSD1306_WHITE);
  display.drawPixel(starX + 2, starY, SSD1306_WHITE);
  display.drawPixel(starX, starY + 2, SSD1306_WHITE);

  // Tiny Oishia catches the star by tilting the device left and right.
  display.drawRoundRect(catcherX, 47, 28, 14, 6, SSD1306_WHITE);
  display.fillCircle(catcherX + 9, 53, 3, SSD1306_WHITE);
  display.fillCircle(catcherX + 19, 53, 3, SSD1306_WHITE);
  display.drawLine(catcherX + 11, 57, catcherX + 14, 59, SSD1306_WHITE);
  display.drawLine(catcherX + 14, 59, catcherX + 17, 57, SSD1306_WHITE);
  display.drawLine(catcherX - 4, 52, catcherX, 55, SSD1306_WHITE);
  display.drawLine(catcherX + 27, 55, catcherX + 31, 52, SSD1306_WHITE);

  if (gameFlashUntil.running(loopNow)) {
    drawHeart(catcherX + 9, 35, 10);
  }
}

void drawFace(PetState state, int yOffset) {
  // Let the eyes float on black; reserve the bottom rows for messages.
  unsigned long now = loopNow;
  bool reactionBurst = now - stateStartedAt < 1800;
  if (state == PET_LOVE || state == PET_EXCITED || state == PET_GREETING) {
    if (reactionBurst) {
      drawHeart(8, 7 + (heartFrame == 0 ? 0 : 1), heartFrame == 0 ? 11 : 9);
      drawHeart(110, 8, heartFrame == 2 ? 11 : 9);
    }
    if (reactionBurst && motionCelebrationUntil.running(now)) {
      drawArrivalSparkles(heartFrame);
    }
  } else if (state == PET_THINKING || state == PET_CURIOUS) {
    if (reactionBurst) drawSparkles(heartFrame);
  } else if (state == PET_SAD) {
    drawWorryDrops(yOffset, moodFrame);
  } else if (state == PET_ANGRY) {
    drawAngrySteam(moodFrame);
  }

  EyeStyle style = EYE_NORMAL;
  switch (state) {
    case PET_HAPPY: case PET_GREETING: style = EYE_HAPPY; break;
    case PET_LOVE: style = EYE_LOVE; break;
    case PET_SLEEP: style = EYE_SLEEP; break;
    case PET_THINKING: style = EYE_THINKING; break;
    case PET_CURIOUS: style = EYE_CURIOUS; break;
    case PET_EXCITED: style = EYE_SURPRISED; break;
    case PET_SAD: style = EYE_SAD; break;
    case PET_ANGRY: style = EYE_ANGRY; break;
    default: break;
  }
  if (transitionUntil.running(now) || blinkStage == 1 || blinkStage == 3) style = EYE_SLEEP;
  if (personalSettings.portraitEnabled) drawMascotFace(style, state, yOffset);
  else {
    drawEyes(style, yOffset);
    drawMouth(state, yOffset);
    drawCheeks(yOffset);
  }

  if (state == PET_SLEEP) {
    display.setTextSize(1);
    display.setCursor(101, 19 - sleepFrame * 3);
    display.print("z");
    if (sleepFrame > 0) {
      display.setCursor(110, 11 - sleepFrame * 2);
      display.print("z");
    }
  }
}

void drawMascotFace(EyeStyle style, PetState state, int yOffset) {
  const int y = yOffset;

  // A bold bear-cat silhouette stays readable on a real 128x64 OLED. Body,
  // tail, ears and paws are drawn first so the face panel remains uncluttered.
  display.fillCircle(98, 43 + y, 9, SSD1306_WHITE);
  display.fillCircle(102, 39 + y, 4, SSD1306_BLACK);
  display.fillRoundRect(39, 38 + y, 50, 15, 7, SSD1306_WHITE);
  display.fillCircle(45, 47 + y, 8, SSD1306_WHITE);
  display.fillCircle(83, 47 + y, 8, SSD1306_WHITE);
  display.fillCircle(35, 10 + y, 12, SSD1306_WHITE);
  display.fillCircle(93, 10 + y, 12, SSD1306_WHITE);
  display.fillCircle(35, 10 + y, 5, SSD1306_BLACK);
  display.fillCircle(93, 10 + y, 5, SSD1306_BLACK);
  display.fillRoundRect(22, 4 + y, 84, 46, 20, SSD1306_WHITE);
  display.fillTriangle(58, 5 + y, 63, 0 + y, 66, 6 + y, SSD1306_WHITE);
  display.fillTriangle(64, 6 + y, 70, 1 + y, 72, 8 + y, SSD1306_WHITE);
  display.fillRoundRect(29, 10 + y, 70, 34, 15, SSD1306_BLACK);

  const int leftEye = 48;
  const int rightEye = 80;
  const int eyeY = 24 + y;
  if (style == EYE_HAPPY) {
    display.drawLine(leftEye - 7, eyeY + 2, leftEye, eyeY - 3, SSD1306_WHITE);
    display.drawLine(leftEye, eyeY - 3, leftEye + 7, eyeY + 2, SSD1306_WHITE);
    display.drawLine(rightEye - 7, eyeY + 2, rightEye, eyeY - 3, SSD1306_WHITE);
    display.drawLine(rightEye, eyeY - 3, rightEye + 7, eyeY + 2, SSD1306_WHITE);
  } else if (style == EYE_SLEEP) {
    display.fillRoundRect(leftEye - 7, eyeY, 14, 2, 1, SSD1306_WHITE);
    display.fillRoundRect(rightEye - 7, eyeY, 14, 2, 1, SSD1306_WHITE);
  } else if (style == EYE_LOVE) {
    drawHeart(leftEye - 6, eyeY - 6, 12);
    drawHeart(rightEye - 6, eyeY - 6, 12);
  } else {
    uint8_t leftRadius = style == EYE_CURIOUS ? 8 : 7;
    uint8_t rightRadius = style == EYE_CURIOUS ? 5 : 7;
    display.fillCircle(leftEye, eyeY, leftRadius, SSD1306_WHITE);
    display.fillCircle(rightEye, eyeY, rightRadius, SSD1306_WHITE);
    int lookX = constrain(int(pupilX) + int(tiltDirection), -1, 1);
    int lookY = style == EYE_THINKING ? -1 : constrain(int(pupilY), -1, 1);
    uint8_t pupilRadius = style == EYE_SURPRISED ? 2 : 3;
    display.fillCircle(leftEye + lookX * 2, eyeY + lookY * 2, pupilRadius, SSD1306_BLACK);
    display.fillCircle(rightEye + lookX * 2, eyeY + lookY * 2, pupilRadius, SSD1306_BLACK);
    display.drawPixel(leftEye + lookX * 2 - 1, eyeY + lookY * 2 - 1, SSD1306_WHITE);
    display.drawPixel(rightEye + lookX * 2 - 1, eyeY + lookY * 2 - 1, SSD1306_WHITE);
    if (style == EYE_SAD) {
      display.drawLine(leftEye - 7, eyeY - 9, leftEye + 4, eyeY - 6, SSD1306_WHITE);
      display.drawLine(rightEye - 4, eyeY - 6, rightEye + 7, eyeY - 9, SSD1306_WHITE);
    } else if (style == EYE_ANGRY) {
      display.drawLine(leftEye - 7, eyeY - 7, leftEye + 4, eyeY - 10, SSD1306_WHITE);
      display.drawLine(rightEye - 4, eyeY - 10, rightEye + 7, eyeY - 7, SSD1306_WHITE);
    }
  }

  // Tiny muzzle and cheeks mirror the dashboard mascot without fine detail.
  display.fillTriangle(61, 32 + y, 67, 32 + y, 64, 35 + y, SSD1306_WHITE);
  display.drawLine(64, 35 + y, 64, 37 + y, SSD1306_WHITE);
  if (state == PET_SAD) {
    display.drawLine(58, 41 + y, 64, 37 + y, SSD1306_WHITE);
    display.drawLine(64, 37 + y, 70, 41 + y, SSD1306_WHITE);
  } else if (state == PET_ANGRY) {
    display.drawLine(58, 39 + y, 70, 39 + y, SSD1306_WHITE);
  } else if (state == PET_EXCITED) {
    display.drawCircle(64, 40 + y, 3, SSD1306_WHITE);
  } else {
    display.drawLine(57, 37 + y, 61, 40 + y, SSD1306_WHITE);
    display.drawLine(61, 40 + y, 64, 37 + y, SSD1306_WHITE);
    display.drawLine(64, 37 + y, 67, 40 + y, SSD1306_WHITE);
    display.drawLine(67, 40 + y, 71, 37 + y, SSD1306_WHITE);
  }
  display.drawPixel(35, 35 + y, SSD1306_WHITE);
  display.drawPixel(38, 36 + y, SSD1306_WHITE);
  display.drawPixel(90, 35 + y, SSD1306_WHITE);
  display.drawPixel(93, 36 + y, SSD1306_WHITE);

  // Paws and a small dark heart identify Oishia even when the face is asleep.
  display.drawLine(43, 47 + y, 46, 50 + y, SSD1306_BLACK);
  display.drawLine(85, 47 + y, 82, 50 + y, SSD1306_BLACK);
  display.fillCircle(61, 47 + y, 2, SSD1306_BLACK);
  display.fillCircle(67, 47 + y, 2, SSD1306_BLACK);
  display.fillTriangle(59, 47 + y, 69, 47 + y, 64, 52 + y, SSD1306_BLACK);
}

void drawEyes(EyeStyle style, int yOffset) {
  int y = 16 + yOffset;
  const int left = 29;
  const int right = 75;
  if (style == EYE_NORMAL) {
    int lookX = tiltDirection == 0 ? pupilX : tiltDirection;
    drawOpenEye(left, y, 24, 24, lookX, pupilY);
    drawOpenEye(right, y, 24, 24, lookX, pupilY);
  } else if (style == EYE_HAPPY) {
    // Thick, rounded smile arches stay readable on the tiny OLED.
    for (uint8_t eye = 0; eye < 2; ++eye) {
      int x = eye == 0 ? left : right;
      display.fillRoundRect(x, y + 6, 24, 20, 11, SSD1306_WHITE);
      display.fillRoundRect(x + 3, y + 10, 18, 18, 8, SSD1306_BLACK);
      display.fillRect(x, y + 17, 24, 12, SSD1306_BLACK);
    }
  } else if (style == EYE_LOVE) {
    drawHeart(left + 2, y + 3, 20);
    drawHeart(right + 2, y + 3, 20);
  } else if (style == EYE_SLEEP) {
    display.fillRoundRect(left + 2, y + 13, 20, 3, 1, SSD1306_WHITE);
    display.fillRoundRect(right + 2, y + 13, 20, 3, 1, SSD1306_WHITE);
  } else if (style == EYE_CURIOUS) {
    drawOpenEye(left + 2, y + 3, 20, 21, 1, -1);
    drawOpenEye(right - 1, y - 2, 26, 27, 1, -1);
  } else if (style == EYE_SURPRISED) {
    display.fillCircle(left + 12, y + 12, 12, SSD1306_WHITE);
    display.fillCircle(right + 12, y + 12, 12, SSD1306_WHITE);
    display.fillCircle(left + 12, y + 13, 4, SSD1306_BLACK);
    display.fillCircle(right + 12, y + 13, 4, SSD1306_BLACK);
  } else if (style == EYE_SAD) {
    display.drawLine(left + 1, y + 4, left + 22, y, SSD1306_WHITE);
    display.drawLine(right + 1, y, right + 22, y + 4, SSD1306_WHITE);
    drawOpenEye(left + 1, y + 5, 22, 19, 0, 1);
    drawOpenEye(right + 1, y + 5, 22, 19, 0, 1);
  } else if (style == EYE_ANGRY) {
    // Eyebrows point inward and the smaller eyes give Oishia a comic grumpy look.
    display.drawLine(left + 1, y + 2, left + 22, y + 7, SSD1306_WHITE);
    display.drawLine(right + 1, y + 7, right + 22, y + 2, SSD1306_WHITE);
    drawOpenEye(left + 1, y + 9, 22, 15, 0, 1);
    drawOpenEye(right + 1, y + 9, 22, 15, 0, 1);
  } else { // EYE_THINKING
    drawOpenEye(left, y, 24, 24, 0, -1);
    drawOpenEye(right, y, 24, 24, 0, -1);
    display.drawLine(left + 3, y - 3, left + 19, y - 5, SSD1306_WHITE);
  }
}

void drawOpenEye(int x, int y, int w, int h, int lookX, int lookY) {
  display.fillRoundRect(x, y, w, h, w / 3, SSD1306_WHITE);
  int pupilRadius = h / 5;
  int pupilXPos = x + w / 2 + lookX * 2;
  int pupilYPos = y + h / 2 + lookY * 2;
  display.fillCircle(pupilXPos, pupilYPos, pupilRadius, SSD1306_BLACK);
  display.drawPixel(pupilXPos - 1, pupilYPos - 1, SSD1306_WHITE);
}

void drawMouth(PetState state, int yOffset) {
  int y = 42 + yOffset;
  if (state == PET_LOVE) drawHeart(59, y - 3, 11);
  else if (state == PET_SLEEP) display.drawLine(59, y, 69, y, SSD1306_WHITE);
  else if (state == PET_EXCITED) {
    display.fillCircle(64, y + 2, 6, SSD1306_WHITE);
    display.fillCircle(64, y + 1, 3, SSD1306_BLACK);
  } else if (state == PET_SAD) {
    display.drawLine(58, y + 4, 64, y, SSD1306_WHITE);
    display.drawLine(64, y, 70, y + 4, SSD1306_WHITE);
  } else if (state == PET_ANGRY) {
    display.drawLine(56, y + 3, 61, y + 1, SSD1306_WHITE);
    display.drawLine(61, y + 1, 66, y + 4, SSD1306_WHITE);
    display.drawLine(66, y + 4, 72, y + 1, SSD1306_WHITE);
  } else if (state == PET_THINKING) {
    display.drawLine(59, y + 3, 68, y + 3, SSD1306_WHITE);
    display.drawPixel(70, y + 2, SSD1306_WHITE);
  } else if (state == PET_CURIOUS) display.drawCircle(64, y + 2, 4, SSD1306_WHITE);
  else {
    display.drawLine(59, y + 1, 61, y + 4, SSD1306_WHITE);
    display.drawLine(61, y + 4, 66, y + 4, SSD1306_WHITE);
    display.drawLine(66, y + 4, 68, y + 1, SSD1306_WHITE);
  }
}

void drawCheeks(int yOffset) {
  int y = 39 + yOffset;
  display.fillRoundRect(20, y, 7, 3, 1, SSD1306_WHITE);
  display.fillRoundRect(101, y, 7, 3, 1, SSD1306_WHITE);
}

void drawHeart(int x, int y, uint8_t size) {
  int radius = size / 4;
  int half = size / 2;
  display.fillCircle(x + radius + 1, y + radius + 1, radius, SSD1306_WHITE);
  display.fillCircle(x + size - radius - 1, y + radius + 1, radius, SSD1306_WHITE);
  display.fillTriangle(x, y + radius + 1, x + size - 1, y + radius + 1, x + half, y + size - 1, SSD1306_WHITE);
}

void drawSparkles(uint8_t frame) {
  display.drawPixel(18, 15, SSD1306_WHITE);
  display.drawPixel(17, 16, SSD1306_WHITE);
  display.drawPixel(19, 16, SSD1306_WHITE);
  display.drawPixel(18, 17, SSD1306_WHITE);
  if (frame & 1) {
    display.drawPixel(109, 29, SSD1306_WHITE);
    display.drawPixel(108, 30, SSD1306_WHITE);
    display.drawPixel(110, 30, SSD1306_WHITE);
    display.drawPixel(109, 31, SSD1306_WHITE);
  }
}

void drawArrivalSparkles(uint8_t frame) {
  // A little burst around Oishia makes a PIR arrival obvious at a glance.
  display.drawLine(4, 25, 9, 25, SSD1306_WHITE);
  display.drawLine(6, 23, 6, 27, SSD1306_WHITE);
  display.drawPixel(120, 31, SSD1306_WHITE);
  display.drawPixel(119, 30, SSD1306_WHITE);
  display.drawPixel(121, 30, SSD1306_WHITE);
  display.drawPixel(120, 29, SSD1306_WHITE);
  if (frame & 1) {
    display.drawPixel(16, 44, SSD1306_WHITE);
    display.drawPixel(17, 43, SSD1306_WHITE);
    display.drawPixel(111, 45, SSD1306_WHITE);
    display.drawPixel(112, 44, SSD1306_WHITE);
  }
}

void drawWorryDrops(int yOffset, uint8_t frame) {
  int dropY = 36 + yOffset + frame;
  display.drawPixel(25, dropY, SSD1306_WHITE);
  display.drawPixel(24, dropY + 1, SSD1306_WHITE);
  display.drawPixel(25, dropY + 2, SSD1306_WHITE);
  display.drawPixel(103, dropY + (frame & 1), SSD1306_WHITE);
  display.drawPixel(104, dropY + 1 + (frame & 1), SSD1306_WHITE);
}

void drawAngrySteam(uint8_t frame) {
  int leftX = 18 + (frame & 1);
  int rightX = 107 - (frame & 1);
  display.drawPixel(leftX, 12, SSD1306_WHITE);
  display.drawPixel(leftX + 1, 10, SSD1306_WHITE);
  display.drawPixel(leftX, 8, SSD1306_WHITE);
  display.drawPixel(rightX, 12, SSD1306_WHITE);
  display.drawPixel(rightX - 1, 10, SSD1306_WHITE);
  display.drawPixel(rightX, 8, SSD1306_WHITE);
}

void drawCenteredText(const char* text, int y, uint8_t size) {
  if (!text || !text[0]) return;
  int16_t x1, y1;
  uint16_t width, height;
  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  int x = ((int)SCREEN_WIDTH - (int)width) / 2;
  display.setCursor(x < 0 ? 0 : x, y);
  display.print(text);
}
