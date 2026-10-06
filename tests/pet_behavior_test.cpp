// Compile the actual sketch with hardware adapters replaced at the boundary.
#include "../learning/learning.ino"
#include <cassert>
#include <iostream>
bool beginConnectivity() { return false; }
void publishPetSnapshot(const OishiaPetSnapshot &) {}
void publishOishiaHistory(const OishiaHistory &) {}
void publishOishiaEvents(const OishiaEventLog &) {}
bool receiveOishiaCommand(OishiaCommand &) { return false; }
void getNetworkInfo(OishiaNetworkInfo &) {}
void requestNetworkSetup() {}
void requestOwnerReset() {}
OishiaSettings queuedSettings;
bool settingsQueued = false;
bool receiveOishiaSettings(OishiaSettings &out) { if(!settingsQueued)return false;out=queuedSettings;settingsQueued=false;return true; }
int main() {
  assert(validOishiaSettings(personalSettings));
  assert(!validOishiaName("",0)); assert(!validOishiaName(" Space",6));
  assert(!validOishiaName("abcdefghijklz",13)); assert(!validOishiaName("A\0B",3));
  OishiaSettings custom; strcpy(custom.petName,"Pikku"); strcpy(custom.ownerName,"Alex");
  custom.autoSleep=0; custom.tiltInvert=1;
  strcpy(custom.timezone,"America/New_York"); custom.clockAfterSeconds=300;
  assert(saveOishiaSettings(preferences,custom));
  auto loaded=loadOishiaSettings(preferences); assert(!strcmp(loaded.petName,"Pikku") && loaded.tiltInvert);
  preferences.fail=true; strcpy(custom.petName,"Unsaved"); assert(!saveOishiaSettings(preferences,custom));
  preferences.fail=false; assert(!strcmp(loadOishiaSettings(preferences).petName,"Pikku"));
  custom.brightThreshold=custom.darkThreshold; assert(!validOishiaSettings(custom));
  custom=OishiaSettings{}; custom.sleepAfterSeconds=4; assert(!validOishiaSettings(custom));
  custom=OishiaSettings{}; strcpy(custom.timezone,"Unknown/Nowhere"); assert(!validOishiaSettings(custom));
  custom=OishiaSettings{}; custom.clockAfterSeconds=9; assert(!validOishiaSettings(custom));
  custom=OishiaSettings{};
  assert(!shouldShowOishiaClock(custom,119999,0,false));
  assert(shouldShowOishiaClock(custom,120000,0,false));
  assert(!shouldShowOishiaClock(custom,120000,0,true));
  custom.clockWhenAway=0; assert(!shouldShowOishiaClock(custom,120000,0,false));
  assert(oishiaClockTimeValid(1704067200) && !oishiaClockTimeValid(1000));
  assert(validOishiaBrowserEpoch(1704067200u) && validOishiaBrowserEpoch(4102444800u));
  assert(!validOishiaBrowserEpoch(1704067199u) && !validOishiaBrowserEpoch(4102444801u));
  petState=PET_BOOT; lastPresenceAt=0; pirWasHigh=false; fakePir=HIGH;
  updateMotion(70000); assert(lastPresenceAt==70000 && pirWasHigh);
  fakePir=LOW; updateMotion(70001); assert(lastPresenceAt==70000 && !pirWasHigh);
  petState=PET_IDLE; remoteSleeping=true; clockPreviewUntil.start(70001,30000);
  assert(clockScreenVisible(70001));
  clockPreviewUntil.stop(); remoteSleeping=false;
  Preferences invalidStore;
  OishiaSettingsV2 invalidV2={}; invalidV2.version=999;
  invalidStore.putBytes("personal-v2",&invalidV2,sizeof(invalidV2)); assert(!strcmp(loadOishiaSettings(invalidStore).petName,"Oishia"));
  Preferences legacyStore;
  OishiaSettingsV1 legacy={1,"Mochi","Jamie",1,0,700,1200,45,20};
  legacyStore.putBytes("personal-v1",&legacy,sizeof(legacy));
  auto migrated=loadOishiaSettings(legacyStore);
  assert(!strcmp(migrated.petName,"Mochi") && !strcmp(migrated.ownerName,"Jamie"));
  assert(!strcmp(migrated.timezone,"Asia/Tokyo") && migrated.clockWhenAway && migrated.clockAfterSeconds==120);
  char greeting[22]; personalizeOishiaMessage(greeting,sizeof(greeting),"Hi {owner}!",loaded); assert(!strcmp(greeting,"Hi Alex!"));
  strcpy(loaded.ownerName,"%s <&>"); personalizeOishiaMessage(greeting,sizeof(greeting),"Love you {owner}",loaded); assert(!strcmp(greeting,"Love you %s <&>"));
  sensorOK = true; roomDark = false;
  loopNow = 1000; fakeMillis = 1005;
  changeState(PET_EXCITED, 3200);
  assert(stateStartedAt == loopNow); // Hardware work crossed a millisecond.
  updatePet(loopNow); assert(petState == PET_EXCITED);
  updatePet(loopNow + 3199); assert(petState == PET_EXCITED);

  loadMemory(); assert(totalPets == 42 && totalVisits == 17 && memoryDirty);
  preferences.fail = true;
  saveMemory(30000); assert(memoryDirty && !memoryHealthy);
  int writes = preferences.writes;
  saveMemory(30001); assert(preferences.writes == writes); // Backoff.
  preferences.fail = false;
  saveMemory(60000); assert(!memoryDirty && memoryHealthy);
  totalPets = totalVisits = 0;
  loadMemory(); assert(totalPets == 42 && totalVisits == 17);

  loopNow = fakeMillis = 70000;
  mpuReady = haveMPUSample = true; Wire.fail = true;
  gameActive = true; petState = PET_GAME;
  updateGyro(loopNow);
  assert(!mpuReady && !haveMPUSample && !gameActive && petState != PET_GAME);
  Wire.fail = false;
  updateGyro(loopNow + 4999); assert(!mpuReady);
  updateGyro(loopNow + 5000); assert(mpuReady && !haveMPUSample);
  updateGyro(loopNow + 5100); assert(haveMPUSample);
  mpuReady = false; startGame(loopNow); assert(!gameActive);

  connectivityReady=true; settingsQueued=true; queuedSettings=loaded;
  personalSettings.autoSleep=1; remoteSleeping=true; remoteReactionState=PET_SLEEP; petState=PET_SLEEP;
  updateConnectivity(loopNow); assert(!strcmp(personalSettings.petName,"Pikku")); assert(remoteSleeping && petState==PET_SLEEP);
  connectivityReady=false;
  remoteSleeping = true; remoteReactionState = PET_SLEEP; petState = PET_SLEEP;
  changeState(PET_EXCITED, 3000); assert(petState == PET_SLEEP);
  cancelRemoteReaction(); assert(!remoteSleeping);
  // Actual blink and button schedulers use the tested 32-bit duration timer.
  petState = PET_IDLE; blinkStage = 0; nextBlinkAt.start(0xfffffff0,32);
  updateBlink(15); assert(blinkStage == 0);
  updateBlink(16); assert(blinkStage == 1);
  pendingButtonClick=true; buttonClickCount=1; buttonClickDeadline.start(0xfffffff0,32);
  loopNow = 15; updateButton(loopNow); assert(pendingButtonClick);
  loopNow = 16; updateButton(loopNow); assert(!pendingButtonClick && petState == PET_EXCITED);
  // A second tap can remain held past the first tap's 350 ms click window.
  auto buttonAt = [](uint32_t at, int reading) {
    loopNow = fakeMillis = at; fakeButton = reading; updateButton(at);
  };
  petState = PET_IDLE;
  const uint32_t petsBeforeTaps = totalPets;
  buttonAt(1000, LOW); buttonAt(1035, LOW);
  buttonAt(1100, HIGH); buttonAt(1135, HIGH);
  assert(pendingButtonClick && buttonClickCount == 1);
  buttonAt(1250, LOW); buttonAt(1285, LOW);
  buttonAt(1600, LOW);
  assert(totalPets == petsBeforeTaps && petState == PET_IDLE);
  buttonAt(1650, HIGH); buttonAt(1685, HIGH);
  assert(pendingButtonClick && buttonClickCount == 2);
  buttonAt(2035, HIGH);
  assert(petState == PET_SENSOR && !pendingButtonClick && totalPets == petsBeforeTaps);
  // A tap followed by a long hold must not also trigger the pending single tap.
  petState = PET_IDLE;
  buttonAt(3000, LOW); buttonAt(3035, LOW);
  buttonAt(3100, HIGH); buttonAt(3135, HIGH);
  buttonAt(3250, LOW); buttonAt(3285, LOW);
  buttonAt(4000, LOW);
  assert(petState == PET_IDLE && totalPets == petsBeforeTaps);
  buttonAt(4200, HIGH); buttonAt(4235, HIGH);
  assert(petState == PET_SLEEP && !pendingButtonClick && totalPets == petsBeforeTaps);
  remoteSleeping=false; remoteReactionActive=false; personalSettings.autoSleep=0;
  roomDark=true; darkSinceAt=0; temperature=10; nextSensorAt.start(100000,99999);
  loopNow=100000; chooseNextState(loopNow); assert(petState!=PET_SLEEP);
  personalSettings.autoSleep=1; personalSettings.sleepAfterSeconds=60; darkSinceAt=99990;
  temperature=24; lastSleepAt=100000; chooseNextState(loopNow); assert(petState!=PET_SLEEP);
  loopNow=160000; chooseNextState(loopNow); assert(petState==PET_SLEEP);
  std::cout << "Actual pet transitions, button/blink rollover, persistence retries, MPU recovery, and remote sleep passed.\n";
}
