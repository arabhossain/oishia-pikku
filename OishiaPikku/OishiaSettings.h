#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "OishiaTimeZones.h"

struct OishiaSettingsV1 {
  uint32_t version; char petName[13]; char ownerName[13]; uint8_t tiltInvert; uint8_t autoSleep;
  uint32_t darkThreshold; uint32_t brightThreshold; uint32_t sleepAfterSeconds; uint32_t motionCooldownSeconds;
};
static_assert(sizeof(OishiaSettingsV1) == 48, "Legacy v1 layout changed");
struct OishiaSettingsV2 {
  uint32_t version; char petName[13]; char ownerName[13]; uint8_t tiltInvert; uint8_t autoSleep;
  uint32_t darkThreshold; uint32_t brightThreshold; uint32_t sleepAfterSeconds; uint32_t motionCooldownSeconds;
  char timezone[32]; uint8_t clockWhenAway; uint8_t clock24Hour; uint32_t clockAfterSeconds;
};
static_assert(sizeof(OishiaSettingsV2) == 88, "Legacy v2 layout changed");

enum OishiaRoutineAction : uint8_t { ROUTINE_REMINDER, ROUTINE_GREETING, ROUTINE_SLEEP, ROUTINE_WAKE, ROUTINE_CLOCK };
struct OishiaRoutine {
  uint8_t enabled=0, action=ROUTINE_REMINDER, hour=8, minute=0, weekdays=0x7f;
  char message[22]="Good morning!";
};
struct OishiaSettings {
  uint32_t version=3;
  char petName[13]="Oishia", ownerName[13]="Sathu";
  uint8_t tiltInvert=0, autoSleep=1;
  uint32_t darkThreshold=800, brightThreshold=1100, sleepAfterSeconds=30, motionCooldownSeconds=10;
  char timezone[32]="Asia/Tokyo";
  uint8_t clockWhenAway=1, clock24Hour=1;
  uint32_t clockAfterSeconds=120;
  uint8_t portraitEnabled=1, clockStyle=0, clockShowSeconds=1;
  uint8_t displayDayContrast=180, displayNightContrast=35, displayOffQuiet=0;
  uint8_t soundProfile=1, quietHoursEnabled=0;
  uint8_t quietStartHour=22, quietStartMinute=0, quietEndHour=7, quietEndMinute=0;
  int16_t comfortableMinTenths=180, comfortableMaxTenths=290;
  uint8_t comfortableMinHumidity=30, comfortableMaxHumidity=78;
  uint16_t pirStuckSeconds=300;
  OishiaRoutine routines[3];
};
static_assert(sizeof(OishiaSettings)<=256,"Settings record exceeded bounded NVS size");

inline bool validOishiaName(const char *name,size_t length){
  if(!name||!length||length>12||name[0]==' '||name[length-1]==' ')return false;
  for(size_t i=0;i<length;++i)if(uint8_t(name[i])<32||uint8_t(name[i])>126)return false;
  return true;
}
inline bool validOishiaMessage(const char *text,size_t capacity){
  const char *end=static_cast<const char *>(memchr(text,0,capacity));
  if(!end||end==text||end-text>21||text[0]==' '||end[-1]==' ')return false;
  for(const char *p=text;p<end;++p)if(uint8_t(*p)<32||uint8_t(*p)>126)return false;
  return true;
}
inline bool validOishiaRoutine(const OishiaRoutine &r){
  return r.enabled<=1&&r.action<=ROUTINE_CLOCK&&r.hour<24&&r.minute<60&&r.weekdays&&!(r.weekdays&0x80)&&validOishiaMessage(r.message,sizeof(r.message));
}
inline bool validOishiaSettings(const OishiaSettings &s){
  const char *p=static_cast<const char *>(memchr(s.petName,0,sizeof(s.petName)));
  const char *o=static_cast<const char *>(memchr(s.ownerName,0,sizeof(s.ownerName)));
  const char *z=static_cast<const char *>(memchr(s.timezone,0,sizeof(s.timezone)));
  bool rv=true;for(const auto &r:s.routines)rv&=validOishiaRoutine(r);
  return s.version==3&&p&&o&&z&&validOishiaName(s.petName,p-s.petName)&&validOishiaName(s.ownerName,o-s.ownerName)&&
    validOishiaTimeZone(s.timezone)&&s.tiltInvert<=1&&s.autoSleep<=1&&s.clockWhenAway<=1&&s.clock24Hour<=1&&
    s.portraitEnabled<=1&&s.clockStyle<=2&&s.clockShowSeconds<=1&&s.displayDayContrast>=5&&s.displayNightContrast>=1&&
    s.displayOffQuiet<=1&&s.soundProfile<=2&&s.quietHoursEnabled<=1&&s.quietStartHour<24&&s.quietStartMinute<60&&
    s.quietEndHour<24&&s.quietEndMinute<60&&s.darkThreshold<=4094&&s.brightThreshold<=4095&&s.brightThreshold>s.darkThreshold&&
    s.sleepAfterSeconds>=5&&s.sleepAfterSeconds<=3600&&s.motionCooldownSeconds>=1&&s.motionCooldownSeconds<=300&&
    s.clockAfterSeconds>=10&&s.clockAfterSeconds<=86400&&s.comfortableMinTenths>=-100&&s.comfortableMaxTenths<=600&&
    s.comfortableMinTenths<s.comfortableMaxTenths&&s.comfortableMinHumidity<=95&&s.comfortableMaxHumidity<=100&&
    s.comfortableMinHumidity<s.comfortableMaxHumidity&&s.pirStuckSeconds>=60&&s.pirStuckSeconds<=3600&&rv;
}
inline bool validLegacyBase(const OishiaSettingsV1 &s){
  const char *p=static_cast<const char *>(memchr(s.petName,0,sizeof(s.petName))),*o=static_cast<const char *>(memchr(s.ownerName,0,sizeof(s.ownerName)));
  return p&&o&&validOishiaName(s.petName,p-s.petName)&&validOishiaName(s.ownerName,o-s.ownerName)&&s.tiltInvert<=1&&s.autoSleep<=1&&
    s.darkThreshold<=4094&&s.brightThreshold<=4095&&s.brightThreshold>s.darkThreshold&&s.sleepAfterSeconds>=5&&
    s.sleepAfterSeconds<=3600&&s.motionCooldownSeconds>=1&&s.motionCooldownSeconds<=300;
}
inline void migrateOishiaBase(OishiaSettings &out,const OishiaSettingsV1 &old){
  memcpy(out.petName,old.petName,sizeof(out.petName));memcpy(out.ownerName,old.ownerName,sizeof(out.ownerName));out.tiltInvert=old.tiltInvert;
  out.autoSleep=old.autoSleep;out.darkThreshold=old.darkThreshold;out.brightThreshold=old.brightThreshold;
  out.sleepAfterSeconds=old.sleepAfterSeconds;out.motionCooldownSeconds=old.motionCooldownSeconds;
}
template<class Store>bool saveOishiaSettings(Store &store,const OishiaSettings &s){return validOishiaSettings(s)&&store.putBytes("personal-v3",&s,sizeof(s))==sizeof(s);}
template<class Store>OishiaSettings loadOishiaSettings(Store &store){
  OishiaSettings out;if(store.getBytesLength("personal-v3")==sizeof(out)&&store.getBytes("personal-v3",&out,sizeof(out))==sizeof(out)&&validOishiaSettings(out))return out;
  OishiaSettingsV2 v2={};if(store.getBytesLength("personal-v2")==sizeof(v2)&&store.getBytes("personal-v2",&v2,sizeof(v2))==sizeof(v2)){
    OishiaSettingsV1 base={};base.version=v2.version;memcpy(base.petName,v2.petName,13);memcpy(base.ownerName,v2.ownerName,13);base.tiltInvert=v2.tiltInvert;base.autoSleep=v2.autoSleep;
    base.darkThreshold=v2.darkThreshold;base.brightThreshold=v2.brightThreshold;base.sleepAfterSeconds=v2.sleepAfterSeconds;base.motionCooldownSeconds=v2.motionCooldownSeconds;
    if(v2.version==2&&validLegacyBase(base)&&memchr(v2.timezone,0,32)&&validOishiaTimeZone(v2.timezone)&&v2.clockWhenAway<=1&&v2.clock24Hour<=1&&v2.clockAfterSeconds>=10&&v2.clockAfterSeconds<=86400){migrateOishiaBase(out,base);strcpy(out.timezone,v2.timezone);out.clockWhenAway=v2.clockWhenAway;out.clock24Hour=v2.clock24Hour;out.clockAfterSeconds=v2.clockAfterSeconds;return out;}
  }
  OishiaSettingsV1 v1={};if(store.getBytesLength("personal-v1")==sizeof(v1)&&store.getBytes("personal-v1",&v1,sizeof(v1))==sizeof(v1)&&v1.version==1&&validLegacyBase(v1))migrateOishiaBase(out,v1);return out;
}
inline void personalizeOishiaMessage(char *out,size_t cap,const char *text,const OishiaSettings &s){
  const char *m=strstr(text,"{owner}"),*name=s.ownerName;size_t n=7;if(!m){m=strstr(text,"{pet}");name=s.petName;n=5;}if(m)snprintf(out,cap,"%.*s%s%s",int(m-text),text,name,m+n);else snprintf(out,cap,"%s",text);
}
