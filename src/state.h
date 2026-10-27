#pragma once

// Device-wide mutable state: hardware object instances, persisted settings,
// and the small pure helpers (lookups, sanitizers, cache resets) that act
// directly on that state. Every other module reaches shared state through
// the externs declared here.

#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <ImprovWiFiLibrary.h>

extern String storedWifiSsid;
extern String storedWifiPass;
extern bool deviceProvisioned;
extern ImprovWiFi improvSerial;

extern TFT_eSPI tft;
extern SPIClass touchSPI;
extern XPT2046_Touchscreen ts;

extern WebServer server;
extern Preferences prefs;

extern TFT_eSprite sprClock;
extern TFT_eSprite sprSmall;

extern float LAT;
extern float LNG;
extern String locationName;

extern uint16_t COL_BG, COL_PANEL, COL_PANEL_ALT, COL_STROKE, COL_TEXT, COL_DIM, COL_ACCENT;
extern const uint16_t COL_GREEN, COL_YELLOW, COL_RED, COL_BLUE;

extern String textColorKey;
extern String unitKey;
extern String regionFormatKey;
extern String timezoneKey;

extern String deviceNickname;
extern HomeWidgetType homeWidgetSlots[];
extern String cacheHomeSlots[];

extern Page currentPage;
extern Page lastDrawnPage;
extern unsigned long lastClockTick;
extern unsigned long lastDataTick;
extern bool pageDirty;
extern bool dataDirty;

extern String cacheClock, cacheTemp, cacheRain, cacheWeek, cacheHomeEmpty1, cacheHomeEmpty2;
extern String cacheFocusTimer, cacheTimerMenu, cacheCustomTimer, cacheBusStopEntry;
extern String cacheTimerDone, cacheTimerDoneCountdown, cacheTimerDoneFlash;

extern String lastWifiText, lastSignalText, lastIpText, lastUptimeText;
extern String lastUvText, lastUvLevelText, lastPsiText, lastPsiLevelText;
extern String lastNowText, lastHourlyText, lastBusPageText, lastNetworkToggleText;

const char* homeWidgetKey(HomeWidgetType type);
const char* homeWidgetLabel(HomeWidgetType type);
HomeWidgetType homeWidgetFromKey(const String& key);
const char* homeSlotLabel(int slot);
const char* timezonePosixByKey(const String& key);
const char* timezoneLabelByKey(const String& key);
String sanitizeTimezoneKey(const String& key);
void applyDeviceTimezoneByKey(const String& key);
void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h);
void clearHomeSlotCaches();

extern bool focusMenuOpen, focusTimerRunning, focusTimerFinished;
extern unsigned long focusEndMs, focusDurationSec, focusRemainingSec;
extern bool timerDoneDialogOpen;
extern unsigned long timerDoneDialogStartedMs;
extern bool flashModeEnabled;
extern int timerPresetMin[6];
extern bool customTimerEntryOpen;
extern int customTimerMinutes;
extern bool wifiEnabled, wifiConnectInProgress;
extern unsigned long wifiConnectStartedMs;

extern float tempC, tempMinC, tempMaxC, precipMm, windSpeedMs, windDirectionDeg, uvIndex, apparentTempC;
extern int weatherCode;
extern bool weatherIsDay;
extern time_t lastWeatherFetch;
extern float hourlyTempC[HOURLY_COUNT];
extern int hourlyCode[HOURLY_COUNT];
extern bool hourlyIsDay[HOURLY_COUNT];
extern int hourlyHour[HOURLY_COUNT];
extern int hourlyFilled;

extern float kpIndex;
extern time_t lastKpFetch;

extern String psiRegion;
extern float psiValue;
extern time_t lastPsiFetch;

extern String ltaApiKey;
String effectiveLtaApiKey();

extern BusStopConfig busStops[BUS_MAX_STOPS];
extern int busActiveStopIdx;
extern BusStopNameInfo busStopNames[BUS_MAX_STOPS];
extern BusServiceInfo busServicesByStop[BUS_MAX_STOPS][BUS_MAX_SERVICES];
extern int busServiceCountByStop[BUS_MAX_STOPS];
extern bool busDataValidByStop[BUS_MAX_STOPS];
extern time_t lastBusFetchByStop[BUS_MAX_STOPS];
extern int busServicePage[BUS_MAX_STOPS];
String busStopDisplayName(int idx);

extern bool busStopEntryOpen;
extern int busStopEntryTargetIdx;
extern String busStopEntryDigits;

extern int sunriseMin, sunsetMin, lastSunYmd;
extern time_t lastSyncTime;

extern bool sleepDimmed, sleepOff, manualDimMode;
extern unsigned long lastInteractionMs;
extern int sleepIntervalMin, sleepOffDelaySec;

int ymdFromLocal(time_t t);
int minutesFromLocalEpoch(time_t t);
int minutesNowLocal();
String sanitizePsiRegion(const String& region);
int sanitizeTimerMinutes(int value);

void resetFocusTimer();
void startFocusTimer(unsigned long minutes);
void dismissTimerDoneDialog();
void openTimerDoneDialog();
void updateFocusTimerState();
void updateTimerDoneDialogState();

void applyThemeByKey(const String& accentKey, const String& bgKey);
void applyTextColorByKey(const String& key);
uint16_t colorFromHex565(const String& hex);

void loadStoredSettings();
void resetDataCaches();
