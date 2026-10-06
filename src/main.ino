// MakerPulse V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <ImprovWiFiLibrary.h>
#include <math.h>

#define FIRMWARE_VERSION "1.0.0"
#define SETUP_SITE_URL "yourusername.github.io/MakerPulse" // TODO: update once GitHub Pages is live

// =========================================================
// WIFI
// =========================================================
// WiFi credentials are no longer compiled in. They are provisioned at
// runtime via the Improv Wi-Fi serial protocol (see setup()/loop() and
// onImprovWiFiConnectedCb below) and persisted through Preferences.
String storedWifiSsid = "";
String storedWifiPass = "";
bool deviceProvisioned = false; // true once a WiFi SSID has ever been saved

ImprovWiFi improvSerial(&Serial);

// =========================================================
// DISPLAY / TOUCH
// =========================================================
TFT_eSPI tft;

const int ROT = 2;
const bool INV = false;

#define TOUCH_CS  33
#define TOUCH_IRQ 36

static const int T_SCK  = 25;
static const int T_MISO = 39;
static const int T_MOSI = 32;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

static const int TOUCH_X_MIN = 562;
static const int TOUCH_X_MAX = 3604;
static const int TOUCH_Y_MIN = 544;
static const int TOUCH_Y_MAX = 3720;

static const bool TOUCH_SWAP_XY = false;
static const bool TOUCH_FLIP_X  = false;
static const bool TOUCH_FLIP_Y  = false;

// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);
Preferences prefs;

// =========================================================
// SPRITES
// =========================================================
TFT_eSprite sprClock = TFT_eSprite(&tft);
TFT_eSprite sprSmall = TFT_eSprite(&tft);

// =========================================================
// LOCATION
// =========================================================
float LAT = 1.3521f;
float LNG = 103.8198f;
String locationName = "Singapore";

// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0x08A3;
uint16_t COL_PANEL     = 0x1106;
uint16_t COL_PANEL_ALT = 0x18C7;
uint16_t COL_STROKE    = 0x31EC;
uint16_t COL_TEXT      = 0xEF7D;
uint16_t COL_DIM       = 0x94B2;
uint16_t COL_ACCENT    = 0x5EFA;

const uint16_t COL_GREEN  = TFT_GREEN;
const uint16_t COL_YELLOW = 0xFFE0;
const uint16_t COL_RED    = TFT_RED;
const uint16_t COL_BLUE   = 0x041F;

String textColorKey = "standard";
String unitKey = "metric"; // metric = C/mm, imperial = F/in
String regionFormatKey = "europe"; // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy
String timezoneKey = "singapore";

// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;
const int SCREEN_H = 320;
const int TOPBAR_H = 34;
const int NAV_H    = 44;

const int HOME_GRID_Y1 = 120;
const int HOME_GRID_Y2 = 198;
const int HOME_WIDGET_H = 70;

const int HOME_TIMER_X = 124;
const int HOME_TIMER_Y = HOME_GRID_Y1;
const int HOME_TIMER_W = 108;
const int HOME_TIMER_H = HOME_WIDGET_H;
const int TIMER_MENU_X = 20;
const int TIMER_MENU_Y = 68;
const int TIMER_MENU_W = 200;
const int TIMER_MENU_H = 194;
const int TIMER_DONE_X = 26;
const int TIMER_DONE_Y = 92;
const int TIMER_DONE_W = 188;
const int TIMER_DONE_H = 108;
const int BUS_ENTRY_X = 20;
const int BUS_ENTRY_Y = 44;
const int BUS_ENTRY_W = 200;
const int BUS_ENTRY_H = 226;
const int PAGE_ROW1_Y = 42;
const int PAGE_ROW2_Y = 120;
const int PAGE_ROW3_Y = 198;
const int PAGE_WIDGET_H = HOME_WIDGET_H;

const int WEATHER_NOW_Y = PAGE_ROW1_Y;
const int WEATHER_NOW_H = 88;
const int WEATHER_HOURLY_Y = 138;
const int WEATHER_HOURLY_H = 52;

// =========================================================
// PROFILE
// =========================================================
String buddyNickname = "";

enum HomeWidgetType {
  HOME_WIDGET_WEEK = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN,
  HOME_WIDGET_PSI,
  HOME_WIDGET_BUS
};

const int HOME_SLOT_COUNT = 4;
HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_WEEK,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR
};

String cacheHomeSlots[HOME_SLOT_COUNT];

// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_BUS = 2,
  PAGE_STATUS = 3
};

Page currentPage = PAGE_HOME;
Page lastDrawnPage = (Page)-1;

unsigned long lastClockTick = 0;
unsigned long lastDataTick  = 0;

const unsigned long CLOCK_TICK_MS = 1000;
const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

bool pageDirty = true;
bool dataDirty = true;

// cache
String cacheClock = "";
String cacheTemp = "";
String cacheRain = "";
String cacheWeek = "";
String cacheHomeEmpty1 = "";
String cacheHomeEmpty2 = "";
String cacheFocusTimer = "";
String cacheTimerMenu = "";
String cacheCustomTimer = "";
String cacheBusStopEntry = "";
String cacheTimerDone = "";
String cacheTimerDoneCountdown = "";
String cacheTimerDoneFlash = "";

String lastWifiText = "";
String lastSignalText = "";
String lastIpText = "";
String lastUptimeText = "";
String lastUvText = "";
String lastUvLevelText = "";
String lastPsiText = "";
String lastPsiLevelText = "";
String lastNowText = "";
String lastHourlyText = "";
String lastBusPageText = "";
String lastNetworkToggleText = "";

const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "week";
    case HOME_WIDGET_TIMER:   return "timer";
    case HOME_WIDGET_RAIN:    return "rain";
    case HOME_WIDGET_OUTDOOR: return "outdoor";
    case HOME_WIDGET_KP:      return "kp";
    case HOME_WIDGET_UV:      return "uv";
    case HOME_WIDGET_WIND:    return "wind";
    case HOME_WIDGET_SUN:     return "sun";
    case HOME_WIDGET_PSI:     return "psi";
    case HOME_WIDGET_BUS:     return "bus";
    default:                  return "week";
  }
}

const char* homeWidgetLabel(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "Week";
    case HOME_WIDGET_TIMER:   return "Timer";
    case HOME_WIDGET_RAIN:    return "Rain";
    case HOME_WIDGET_OUTDOOR: return "Outdoor";
    case HOME_WIDGET_KP:      return "KP index";
    case HOME_WIDGET_UV:      return "UV index";
    case HOME_WIDGET_WIND:    return "Wind";
    case HOME_WIDGET_SUN:     return "Sun event";
    case HOME_WIDGET_PSI:     return "PSI (haze)";
    case HOME_WIDGET_BUS:     return "Bus arrival";
    default:                  return "Week";
  }
}

HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "week") return HOME_WIDGET_WEEK;
  if (key == "timer") return HOME_WIDGET_TIMER;
  if (key == "rain") return HOME_WIDGET_RAIN;
  if (key == "outdoor") return HOME_WIDGET_OUTDOOR;
  if (key == "kp") return HOME_WIDGET_KP;
  if (key == "uv") return HOME_WIDGET_UV;
  if (key == "wind") return HOME_WIDGET_WIND;
  if (key == "sun") return HOME_WIDGET_SUN;
  if (key == "psi") return HOME_WIDGET_PSI;
  if (key == "bus") return HOME_WIDGET_BUS;
  return HOME_WIDGET_WEEK;
}

const char* homeSlotLabel(int slot) {
  switch (slot) {
    case 0: return "Top left";
    case 1: return "Top right";
    case 2: return "Bottom left";
    case 3: return "Bottom right";
    default: return "Slot";
  }
}

const char* timezonePosixByKey(const String& key) {
  if (key == "utc") return "UTC0";
  if (key == "atlantic_azores") return "AZOT1AZOST,M3.5.0/0,M10.5.0/1";
  if (key == "europe_west") return "WET0WEST,M3.5.0/1,M10.5.0";
  if (key == "uk") return "GMT0BST,M3.5.0/1,M10.5.0";
  if (key == "europe_central") return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if (key == "europe_east") return "EET-2EEST,M3.5.0/3,M10.5.0/4";
  if (key == "africa_south") return "SAST-2";
  if (key == "middle_east_gulf") return "GST-4";
  if (key == "india") return "IST-5:30";
  if (key == "thailand") return "ICT-7";
  if (key == "singapore") return "SGT-8";
  if (key == "china") return "CST-8";
  if (key == "us_eastern") return "EST5EDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_central") return "CST6CDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_mountain") return "MST7MDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_arizona") return "MST7";
  if (key == "us_pacific") return "PST8PDT,M3.2.0/2,M11.1.0/2";
  if (key == "alaska") return "AKST9AKDT,M3.2.0/2,M11.1.0/2";
  if (key == "hawaii") return "HST10";
  if (key == "canada_atlantic") return "AST4ADT,M3.2.0/2,M11.1.0/2";
  if (key == "brazil_east") return "BRT3";
  if (key == "argentina") return "ART3";
  if (key == "asia_tokyo") return "JST-9";
  if (key == "korea") return "KST-9";
  if (key == "australia_perth") return "AWST-8";
  if (key == "australia_darwin") return "ACST-9:30";
  if (key == "australia_sydney") return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  if (key == "new_zealand") return "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
  return "SGT-8";
}

const char* timezoneLabelByKey(const String& key) {
  if (key == "utc") return "UTC";
  if (key == "atlantic_azores") return "Azores";
  if (key == "europe_west") return "Western Europe";
  if (key == "uk") return "United Kingdom";
  if (key == "europe_central") return "Central Europe";
  if (key == "europe_east") return "Eastern Europe";
  if (key == "africa_south") return "South Africa";
  if (key == "middle_east_gulf") return "Gulf / UAE";
  if (key == "india") return "India";
  if (key == "thailand") return "Thailand / ICT";
  if (key == "singapore") return "Singapore";
  if (key == "china") return "China";
  if (key == "us_eastern") return "US Eastern";
  if (key == "us_central") return "US Central";
  if (key == "us_mountain") return "US Mountain";
  if (key == "us_arizona") return "US Arizona";
  if (key == "us_pacific") return "US Pacific";
  if (key == "alaska") return "Alaska";
  if (key == "hawaii") return "Hawaii";
  if (key == "canada_atlantic") return "Canada Atlantic";
  if (key == "brazil_east") return "Brazil East";
  if (key == "argentina") return "Argentina";
  if (key == "asia_tokyo") return "Japan";
  if (key == "korea") return "South Korea";
  if (key == "australia_perth") return "Australia West";
  if (key == "australia_darwin") return "Australia Central";
  if (key == "australia_sydney") return "Australia East";
  if (key == "new_zealand") return "New Zealand";
  return "Singapore";
}

String sanitizeTimezoneKey(const String& key) {
  const char* supported[] = {
    "utc", "atlantic_azores", "europe_west", "uk", "europe_central", "europe_east",
    "africa_south", "middle_east_gulf", "india", "thailand", "singapore", "china",
    "us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific",
    "alaska", "hawaii", "canada_atlantic", "brazil_east", "argentina",
    "asia_tokyo", "korea", "australia_perth", "australia_darwin",
    "australia_sydney", "new_zealand"
  };
  for (const char* supportedKey : supported) {
    if (key == supportedKey) return key;
  }
  return "singapore";
}

void applyDeviceTimezoneByKey(const String& key) {
  timezoneKey = sanitizeTimezoneKey(key);
  setenv("TZ", timezonePosixByKey(timezoneKey), 1);
  tzset();
}

void appendTimezoneOptions(String& page, const String& selectedKey) {
  struct TimezoneGroup {
    const char* label;
    const char* keys[8];
    int count;
  };

  const TimezoneGroup groups[] = {
    {"Global", {"utc"}, 1},
    {"Europe", {"atlantic_azores", "europe_west", "uk", "europe_central", "europe_east"}, 5},
    {"Africa & Middle East", {"africa_south", "middle_east_gulf"}, 2},
    {"Asia", {"singapore", "india", "thailand", "china", "asia_tokyo", "korea"}, 6},
    {"North America", {"us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific", "alaska", "hawaii", "canada_atlantic"}, 8},
    {"South America", {"brazil_east", "argentina"}, 2},
    {"Australia & Oceania", {"australia_perth", "australia_darwin", "australia_sydney", "new_zealand"}, 4}
  };

  for (const TimezoneGroup& group : groups) {
    page += "<optgroup label='";
    page += group.label;
    page += "'>";
    for (int i = 0; i < group.count; i++) {
      const char* key = group.keys[i];
      page += "<option value='";
      page += key;
      page += "'";
      if (selectedKey == key) page += " selected";
      page += ">";
      page += timezoneLabelByKey(key);
      page += "</option>";
    }
    page += "</optgroup>";
  }
}

void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {8, 124, 8, 124};
  const int ys[HOME_SLOT_COUNT] = {HOME_GRID_Y1, HOME_GRID_Y1, HOME_GRID_Y2, HOME_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = 108;
  h = HOME_WIDGET_H;
}

void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN,
    HOME_WIDGET_PSI,
    HOME_WIDGET_BUS
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}

void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}

// Focus timer
bool focusMenuOpen = false;
bool focusTimerRunning = false;
bool focusTimerFinished = false;
unsigned long focusEndMs = 0;
unsigned long focusDurationSec = 0;
unsigned long focusRemainingSec = 0;
bool timerDoneDialogOpen = false;
unsigned long timerDoneDialogStartedMs = 0;
const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;
bool flashModeEnabled = false;
int timerPresetMin[6] = {1, 5, 10, 15, 25, 30};
bool customTimerEntryOpen = false;
int customTimerMinutes = 20;
bool wifiEnabled = true;
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartedMs = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Weather
static float tempC = NAN;
static float tempMinC = NAN;
static float tempMaxC = NAN;
static float precipMm = NAN;
static float windSpeedMs = NAN;
static float windDirectionDeg = NAN;
static float uvIndex = NAN;
static float apparentTempC = NAN;
static int weatherCode = -1;
static bool weatherIsDay = true;
static time_t lastWeatherFetch = 0;
static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;

static const int HOURLY_COUNT = 4;
static float hourlyTempC[HOURLY_COUNT];
static int hourlyCode[HOURLY_COUNT];
static bool hourlyIsDay[HOURLY_COUNT];
static int hourlyHour[HOURLY_COUNT];
static int hourlyFilled = 0;

// KP-index
static float kpIndex = NAN;
static time_t lastKpFetch = 0;
static const uint32_t KP_INTERVAL_SEC = 10 * 60;

// PSI (Singapore NEA haze index)
String psiRegion = "central";
static float psiValue = NAN;
static time_t lastPsiFetch = 0;
static const uint32_t PSI_INTERVAL_SEC = 15 * 60;

// Bus arrival (LTA DataMall)
String ltaApiKey = "";

// Shared fallback AccountKey so bus arrivals work out of the box even before
// a user has requested their own key. Never stored/shown via Preferences or
// the settings page -- ltaApiKey itself always reflects only what the user
// has entered, and effectiveLtaApiKey() is the only thing that falls back.
#define LTA_FALLBACK_API_KEY "2jQlivifRK227X26Uk+bdA=="

String effectiveLtaApiKey() {
  return ltaApiKey.length() > 0 ? ltaApiKey : String(LTA_FALLBACK_API_KEY);
}

struct BusStopConfig {
  String code = "";
  String label = "";
  String svcFilter = "";
};
static const int BUS_MAX_STOPS = 3;
static BusStopConfig busStops[BUS_MAX_STOPS];
static int busActiveStopIdx = 0;

struct BusStopNameInfo {
  String roadName = "";
  String description = "";
  bool valid = false;
  time_t lastAttempt = 0;
};
static BusStopNameInfo busStopNames[BUS_MAX_STOPS];
static const uint32_t BUS_NAME_RETRY_SEC = 10 * 60;

struct BusServiceInfo {
  String serviceNo = "";
  int eta1 = -1;
  int eta2 = -1;
  int eta3 = -1;
};
static const int BUS_MAX_SERVICES = 6;
static BusServiceInfo busServicesByStop[BUS_MAX_STOPS][BUS_MAX_SERVICES];
static int busServiceCountByStop[BUS_MAX_STOPS] = {0, 0, 0};
static bool busDataValidByStop[BUS_MAX_STOPS] = {false, false, false};
static time_t lastBusFetchByStop[BUS_MAX_STOPS] = {0, 0, 0};
static int busServicePage[BUS_MAX_STOPS] = {0, 0, 0};
static const uint32_t BUS_INTERVAL_SEC = 45;

static String busStopDisplayName(int idx) {
  if (idx < 0 || idx >= BUS_MAX_STOPS) return "";
  if (busStops[idx].label.length() > 0) return busStops[idx].label;
  if (busStopNames[idx].valid) {
    if (busStopNames[idx].description.length() > 0) return busStopNames[idx].description;
    if (busStopNames[idx].roadName.length() > 0) return busStopNames[idx].roadName;
  }
  return "";
}

bool busStopEntryOpen = false;
int busStopEntryTargetIdx = -1;
String busStopEntryDigits = "";

// Sunrise / Sunset
static int sunriseMin = -1;
static int sunsetMin  = -1;
static int lastSunYmd = -1;
static time_t lastSyncTime = 0;

// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;

bool sleepDimmed = false;
bool sleepOff = false;
bool manualDimMode = false;

unsigned long lastInteractionMs = 0;

int sleepIntervalMin = 10;
int sleepOffDelaySec = 60;

const int BL_FULL = 255;
const int BL_DIM  = 18;
const int BL_OFF  = 0;
const int FLASH_BL_LOW = 20;
const int FLASH_BL_HIGH = 255;

void wakeDisplay(bool clearManualMode = true);

int sanitizeTimerMinutes(int value);

// =========================================================
// HELPERS
// =========================================================
static int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}

static int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}

static int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}

static String wifiStatusText() {
  if (!wifiEnabled) return "Disabled";
  return WiFi.status() == WL_CONNECTED ? "Online" : "Offline";
}

static String signalText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-- dBm";
  return String(WiFi.RSSI()) + " dBm";
}

static String ipText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-";
  return WiFi.localIP().toString();
}

static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}

static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}

static String formatDateParts(const struct tm& tmValue) {
  char buf[32];
  strftime(buf, sizeof(buf), useUsRegionFormat() ? "%a %m/%d/%Y" : "%a %d.%m.%Y", &tmValue);
  return String(buf);
}

static String formatMinuteOfDay(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  if (useUsRegionFormat()) {
    int hour24 = minOfDay / 60;
    int minute = minOfDay % 60;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
    return String(buf);
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}

static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}

static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}

static String tempRangeText() {
  return "H:" + formatDisplayTemp(tempMaxC) + "  L:" + formatDisplayTemp(tempMinC);
}

static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}

static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}

static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}

static String kpText() {
  return isnan(kpIndex) ? "Kp --" : "Kp " + String(kpIndex, 1);
}

static String kpLevelText() {
  if (isnan(kpIndex)) return "--";
  if (kpIndex < 3.0f) return "Low";
  if (kpIndex < 5.0f) return "Medium";
  if (kpIndex < 7.0f) return "High";
  return "Extreme";
}

String sanitizePsiRegion(const String& region) {
  if (region == "north" || region == "south" || region == "east" || region == "west" || region == "central") {
    return region;
  }
  return "central";
}

static String psiText() {
  return isnan(psiValue) ? "PSI --" : "PSI " + String((int)roundf(psiValue));
}

static String psiLevelText() {
  if (isnan(psiValue)) return "--";
  if (psiValue <= 50.0f) return "Good";
  if (psiValue <= 100.0f) return "Moderate";
  if (psiValue <= 200.0f) return "Unhealthy";
  if (psiValue <= 300.0f) return "V.Unhealthy";
  return "Hazardous";
}

static String busEtaText() {
  if (busStops[0].code.length() == 0) return "No stop";
  if (!busDataValidByStop[0]) return "--";
  if (busServiceCountByStop[0] == 0) return "No bus";
  int eta1 = busServicesByStop[0][0].eta1;
  return eta1 <= 0 ? "Due" : String(eta1) + " min";
}

static String busDetailText() {
  if (busStops[0].code.length() == 0) return "Set in settings";
  if (!busDataValidByStop[0] || busServiceCountByStop[0] == 0) return "--";
  String label = "Bus " + busServicesByStop[0][0].serviceNo;
  int eta2 = busServicesByStop[0][0].eta2;
  if (eta2 >= 0) {
    label += eta2 <= 0 ? " +due" : " +" + String(eta2) + "m";
  }
  return label;
}

static String uvText() {
  return isnan(uvIndex) ? "UV --" : "UV " + String(uvIndex, 1);
}

static String uvLevelText() {
  if (isnan(uvIndex)) return "--";
  if (uvIndex < 3.0f) return "Low";
  if (uvIndex < 6.0f) return "Moderate";
  if (uvIndex < 8.0f) return "High";
  if (uvIndex < 11.0f) return "Very High";
  return "Extreme";
}

static String weatherConditionText(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mainly Clear";
    case 2: return "Cloudy";
    case 3: return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Fz Drizzle";
    case 61:
    case 63:
    case 65: return "Rain";
    case 66:
    case 67: return "Fz Rain";
    case 71:
    case 73:
    case 75: return "Snow";
    case 77: return "Snow Grains";
    case 80:
    case 81:
    case 82: return "Showers";
    case 85:
    case 86: return "Snow Showers";
    case 95: return "T-Storm";
    case 96:
    case 99: return "Storm+Hail";
    default: return "--";
  }
}

static String apparentTempText() {
  if (isnan(apparentTempC)) return "";
  return "Feels " + formatDisplayTemp(apparentTempC);
}

static uint16_t statusColor() {
  if (textColorKey != "standard") return COL_TEXT;
  if (!wifiEnabled) return COL_YELLOW;
  return WiFi.status() == WL_CONNECTED ? COL_GREEN : COL_RED;
}

static String uptimeText() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long days = seconds / 86400UL;
  seconds %= 86400UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;

  if (days > 0) return String(days) + "d " + String(hours) + "h";
  if (hours > 0) return String(hours) + "h " + String(minutes) + "m";
  return String(minutes) + "m";
}

static String nextSunLabel() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "Sun";
  if (nowMin < sunriseMin) return "Sunrise";
  if (nowMin < sunsetMin) return "Sunset";
  return "Sunrise";
}

static String nextSunTimeText() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "--:--";
  if (nowMin < sunriseMin) return formatMinuteOfDay(sunriseMin);
  if (nowMin < sunsetMin) return formatMinuteOfDay(sunsetMin);
  return formatMinuteOfDay(sunriseMin);
}

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}

static String themePreviewCss(const String& key) {
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}

static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

static String focusHintText() {
  if (focusTimerFinished) return "Tap to reset";
  if (focusTimerRunning) return String((focusDurationSec / 60UL)) + " min session";
  return "Tap to start";
}

static String formatElapsedText(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  if (minutes == 0) return "< 1 minute elapsed";
  if (minutes == 1) return "1 minute elapsed";
  return String(minutes) + " minutes elapsed";
}

static String lastSyncText() {
  if (lastSyncTime <= 0) return "Sync --:--";

  struct tm tmSync;
  localtime_r(&lastSyncTime, &tmSync);
  return "Sync " + formatClockParts(tmSync, false);
}

static String weekNumberText() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tmNow);
  return String(buf);
}

static String timerDoneCountdownText() {
  if (!timerDoneDialogOpen) return "";

  unsigned long elapsedMs = millis() - timerDoneDialogStartedMs;
  unsigned long remainingMs = (elapsedMs >= TIMER_DONE_DIALOG_MS) ? 0 : (TIMER_DONE_DIALOG_MS - elapsedMs);
  unsigned long remainingSec = (remainingMs + 999UL) / 1000UL;
  return String("Auto close in ") + String(remainingSec) + "s";
}

static String homeTitleText() {
  return buddyNickname.length() > 0 ? buddyNickname : "MakerPulse";
}

int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}

void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusMenuOpen = false;
  customTimerEntryOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheCustomTimer = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void startFocusTimer(unsigned long minutes) {
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  customTimerEntryOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheCustomTimer = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);
  pageDirty = true;
}

void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay();
}

void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusTimerFinished = true;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}

void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}

void setBacklight(int value) {
  value = constrain(value, 0, 255);
  analogWrite(BACKLIGHT_PIN, value);
}

void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(BL_FULL);
  pageDirty = true;
}

void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}

void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  pageDirty = true;
}

void toggleSleepMode() {
  if (manualDimMode) {
    wakeDisplay();
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}

void handleAutoSleep() {
  if (focusMenuOpen || timerDoneDialogOpen) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;
  unsigned long offAfterMs = dimAfterMs + ((unsigned long)sleepOffDelaySec * 1000UL);

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  if (sleepDimmed && !sleepOff && now - lastInteractionMs > offAfterMs) {
    enterSleepOff();
  }
}

// =========================================================
// THEME / SETTINGS
// =========================================================
void applyThemeByKey(const String& accentKey, const String& bgKey) {
  if (accentKey == "standard")    COL_ACCENT = 0xEF7D;
  else if (accentKey == "cyan")   COL_ACCENT = 0x5EFA;
  else if (accentKey == "ice")    COL_ACCENT = 0xEFFF;
  else if (accentKey == "white")  COL_ACCENT = TFT_WHITE;
  else if (accentKey == "mint")   COL_ACCENT = 0x07F0;
  else if (accentKey == "green")  COL_ACCENT = TFT_GREEN;
  else if (accentKey == "blue")   COL_ACCENT = 0x3D9F;
  else if (accentKey == "purple") COL_ACCENT = 0xA2F5;
  else if (accentKey == "pink")   COL_ACCENT = 0xF97F;
  else if (accentKey == "orange") COL_ACCENT = 0xFD20;
  else if (accentKey == "amber")  COL_ACCENT = 0xFEA0;
  else if (accentKey == "red")    COL_ACCENT = TFT_RED;
  else                            COL_ACCENT = 0x5EFA;

  if (bgKey == "slate") {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  } else if (bgKey == "deep") {
    COL_BG = 0x0000; COL_PANEL = 0x0841; COL_PANEL_ALT = 0x1082; COL_STROKE = 0x2945;
  } else if (bgKey == "nordic") {
    COL_BG = 0x0864; COL_PANEL = 0x10C6; COL_PANEL_ALT = 0x1908; COL_STROKE = 0x3A2D;
  } else if (bgKey == "forest") {
    COL_BG = 0x0208; COL_PANEL = 0x0ACB; COL_PANEL_ALT = 0x134D; COL_STROKE = 0x2D72;
  } else if (bgKey == "coffee") {
    COL_BG = 0x18A3; COL_PANEL = 0x2945; COL_PANEL_ALT = 0x39C7; COL_STROKE = 0x5A89;
  } else if (bgKey == "soft") {
    COL_BG = 0x10A2; COL_PANEL = 0x1924; COL_PANEL_ALT = 0x2145; COL_STROKE = 0x3A49;
  } else if (bgKey == "midnight") {
    COL_BG = 0x0008; COL_PANEL = 0x0011; COL_PANEL_ALT = 0x0018; COL_STROKE = 0x3A7F;
  } else if (bgKey == "graphite") {
    COL_BG = 0x1082; COL_PANEL = 0x18C3; COL_PANEL_ALT = 0x2104; COL_STROKE = 0x4208;
  } else if (bgKey == "garnet") {
    COL_BG = 0x1004; COL_PANEL = 0x1886; COL_PANEL_ALT = 0x20E8; COL_STROKE = 0x41AC;
  } else if (bgKey == "ochre") {
    COL_BG = 0x20E1; COL_PANEL = 0x3184; COL_PANEL_ALT = 0x4226; COL_STROKE = 0x632B;
  } else {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  }
}

void applyTextColorByKey(const String& key) {
  textColorKey = key;

  if (key == "standard") {
    COL_TEXT = 0xEF7D; COL_DIM  = 0x94B2;
  } else if (key == "white") {
    COL_TEXT = TFT_WHITE; COL_DIM = 0xBDF7;
  } else if (key == "ice") {
    COL_TEXT = 0xEFFF; COL_DIM = 0x9D7F;
  } else if (key == "mint") {
    COL_TEXT = 0x07F0; COL_DIM = 0x05EC;
  } else if (key == "orange") {
    COL_TEXT = 0xFD20; COL_DIM = 0xBA26;
  } else if (key == "amber") {
    COL_TEXT = 0xFEA0; COL_DIM = 0xBCE0;
  } else if (key == "green") {
    COL_TEXT = TFT_GREEN; COL_DIM = 0x86E8;
  } else if (key == "cyan") {
    COL_TEXT = 0x5EFA; COL_DIM = 0x3D96;
  } else if (key == "blue") {
    COL_TEXT = 0x3D9F; COL_DIM = 0x22B1;
  } else if (key == "purple") {
    COL_TEXT = 0xA2F5; COL_DIM = 0x79ED;
  } else if (key == "red") {
    COL_TEXT = TFT_RED; COL_DIM = 0xB9E7;
  } else if (key == "pink") {
    COL_TEXT = 0xF97F; COL_DIM = 0xC2F1;
  } else {
    COL_TEXT = 0xEF7D;
    COL_DIM  = 0x94B2;
    textColorKey = "standard";
  }
}

void loadStoredSettings() {
  prefs.begin("makerpulse", false);

  storedWifiSsid    = prefs.getString("wifiSsid", "");
  storedWifiPass    = prefs.getString("wifiPass", "");
  deviceProvisioned = storedWifiSsid.length() > 0;

  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");

  buddyNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Singapore");
  LAT              = prefs.getFloat("lat", 1.3521f);
  LNG              = prefs.getFloat("lng", 103.8198f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  timezoneKey      = sanitizeTimezoneKey(prefs.getString("tz", "singapore"));
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);
  psiRegion        = sanitizePsiRegion(prefs.getString("psiRegion", "central"));
  ltaApiKey        = prefs.getString("ltaKey", "");

  String legacyBusStop = prefs.getString("busStop", "");
  String legacyBusSvc  = prefs.getString("busSvc", "");
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String codeKey  = String("busCode") + String(i);
    String labelKey = String("busLabel") + String(i);
    String svcKey   = String("busSvc") + String(i);
    String nameKey  = String("busName") + String(i);
    String roadKey  = String("busRoad") + String(i);

    busStops[i].code      = prefs.getString(codeKey.c_str(), i == 0 ? legacyBusStop : "");
    busStops[i].label     = prefs.getString(labelKey.c_str(), "");
    busStops[i].svcFilter = prefs.getString(svcKey.c_str(), i == 0 ? legacyBusSvc : "");

    busStopNames[i].roadName    = prefs.getString(roadKey.c_str(), "");
    busStopNames[i].description = prefs.getString(nameKey.c_str(), "");
    busStopNames[i].valid = busStopNames[i].description.length() > 0 || busStopNames[i].roadName.length() > 0;
  }

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
  }

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  buddyNickname.trim();
  applyThemeByKey(accent, bg);
  applyTextColorByKey(txt);
  applyDeviceTimezoneByKey(timezoneKey);
}

void resetDataCaches() {
  tempC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  kpIndex = NAN;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  lastKpFetch = 0;
  dataDirty = true;
  pageDirty = true;
}

// =========================================================
// TOUCH
// =========================================================
bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_FLIP_X)  x = (SCREEN_W - 1) - x;
  if (TOUCH_FLIP_Y)  y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}

bool touchNewPress(int& tx, int& ty) {
  static bool wasDown = false;
  static unsigned long lastPressMs = 0;

  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !wasDown && (now - lastPressMs > 220)) {
    fire = true;
    lastPressMs = now;
    tx = x;
    ty = y;
  }

  wasDown = down;
  return fire;
}

// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  return true;
}

void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);

  if ((sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) dataDirty = true;
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,apparent_temperature,wind_speed_10m,wind_direction_10m,uv_index,weather_code,is_day" +
               "&hourly=precipitation,temperature_2m,weather_code,is_day" +
               "&daily=temperature_2m_max,temperature_2m_min" +
               "&forecast_days=1&timezone=auto&wind_speed_unit=ms";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) return false;

  tempC = doc["current"]["temperature_2m"] | NAN;
  apparentTempC = doc["current"]["apparent_temperature"] | NAN;
  windSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  windDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  uvIndex = doc["current"]["uv_index"] | NAN;
  weatherCode = doc["current"]["weather_code"] | -1;
  weatherIsDay = (doc["current"]["is_day"] | 1) == 1;
  tempMaxC = NAN;
  tempMinC = NAN;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  if (maxTemps && !maxTemps.isNull() && maxTemps.size() > 0) tempMaxC = maxTemps[0] | NAN;
  if (minTemps && !minTemps.isNull() && minTemps.size() > 0) tempMinC = minTemps[0] | NAN;

  JsonArray times = doc["hourly"]["time"];
  JsonArray precs = doc["hourly"]["precipitation"];
  JsonArray hourlyTemps = doc["hourly"]["temperature_2m"];
  JsonArray hourlyCodes = doc["hourly"]["weather_code"];
  JsonArray hourlyIsDayArr = doc["hourly"]["is_day"];

  hourlyFilled = 0;

  if (times && precs) {
    time_t nowT = time(nullptr);
    struct tm tmNow;
    localtime_r(&nowT, &tmNow);

    char key[20];
    strftime(key, sizeof(key), "%Y-%m-%dT%H:00", &tmNow);

    int idx = -1;
    for (int i = 0; i < (int)times.size(); i++) {
      const char* t = times[i];
      if (t && String(t).startsWith(key)) {
        idx = i;
        break;
      }
    }
    if (idx < 0) idx = 0;
    precipMm = precs[idx] | NAN;

    if (hourlyTemps && hourlyCodes && hourlyIsDayArr) {
      for (int i = 0; i < HOURLY_COUNT; i++) {
        int srcIdx = idx + i;
        if (srcIdx >= (int)times.size()) break;

        const char* t = times[srcIdx];
        int hourVal = (t && strlen(t) >= 13) ? String(t).substring(11, 13).toInt() : -1;

        hourlyTempC[hourlyFilled] = hourlyTemps[srcIdx] | NAN;
        hourlyCode[hourlyFilled] = hourlyCodes[srcIdx] | -1;
        hourlyIsDay[hourlyFilled] = (hourlyIsDayArr[srcIdx] | 1) == 1;
        hourlyHour[hourlyFilled] = hourVal;
        hourlyFilled++;
      }
    }
  }

  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  return true;
}

void ensureWeather() {
  time_t nowT = time(nullptr);
  if ((isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || isnan(precipMm) || isnan(windSpeedMs) || isnan(windDirectionDeg) || isnan(uvIndex) ||
       (nowT - lastWeatherFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchWeather()) dataDirty = true;
  }
}

bool fetchKpIndex() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int lastRow = body.lastIndexOf('[');
  if (lastRow < 0) return false;

  int firstComma = body.indexOf(',', lastRow);
  if (firstComma < 0) return false;

  int q1 = body.indexOf('"', firstComma);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;

  String kpStrLocal = body.substring(q1 + 1, q2);
  kpIndex = kpStrLocal.toFloat();

  lastKpFetch = time(nullptr);
  lastSyncTime = lastKpFetch;
  return true;
}

void ensureKpIndex() {
  time_t nowT = time(nullptr);
  if ((isnan(kpIndex) || (nowT - lastKpFetch) > KP_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchKpIndex()) dataDirty = true;
  }
}

bool fetchPsi() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://api.data.gov.sg/v1/environment/psi")) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, body)) return false;

  JsonArray items = doc["items"];
  if (!items || items.size() == 0) return false;

  JsonObject readings = items[0]["readings"]["psi_twenty_four_hourly"];
  if (!readings) return false;

  psiValue = readings[psiRegion.c_str()] | NAN;

  lastPsiFetch = time(nullptr);
  lastSyncTime = lastPsiFetch;
  return true;
}

void ensurePsi() {
  time_t nowT = time(nullptr);
  if ((isnan(psiValue) || (nowT - lastPsiFetch) > PSI_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchPsi()) dataDirty = true;
  }
}

static int parseBusEtaMin(const char* arrivalStr, time_t nowT, bool& ok) {
  ok = false;
  if (!arrivalStr || strlen(arrivalStr) < 19) return -1;
  struct tm tmArrival = {};
  if (strptime(arrivalStr, "%Y-%m-%dT%H:%M:%S", &tmArrival) == nullptr) return -1;
  time_t arrivalT = mktime(&tmArrival);
  long secondsAway = (long)(arrivalT - nowT);
  ok = true;
  return (int)((secondsAway < 0 ? 0 : secondsAway) / 60);
}

bool fetchBusArrival(int idx) {
  if (idx < 0 || idx >= BUS_MAX_STOPS) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (busStops[idx].code.length() == 0) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://datamall2.mytransport.sg/ltaodataservice/v3/BusArrival?BusStopCode=" + busStops[idx].code;
  if (busStops[idx].svcFilter.length() > 0) url += "&ServiceNo=" + busStops[idx].svcFilter;

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("AccountKey", effectiveLtaApiKey());
  http.addHeader("accept", "application/json");

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) return false;

  JsonArray services = doc["Services"];
  time_t nowT = time(nullptr);

  BusServiceInfo* slots = busServicesByStop[idx];
  for (int i = 0; i < BUS_MAX_SERVICES; i++) {
    slots[i] = BusServiceInfo();
  }
  int& count = busServiceCountByStop[idx];
  count = 0;

  if (services) {
    for (JsonObject svc : services) {
      const char* a1 = svc["NextBus"]["EstimatedArrival"] | "";
      bool ok1;
      int e1 = parseBusEtaMin(a1, nowT, ok1);
      if (!ok1) continue;

      bool ok2, ok3;
      const char* a2 = svc["NextBus2"]["EstimatedArrival"] | "";
      const char* a3 = svc["NextBus3"]["EstimatedArrival"] | "";
      int e2 = parseBusEtaMin(a2, nowT, ok2);
      int e3 = parseBusEtaMin(a3, nowT, ok3);

      BusServiceInfo info;
      info.serviceNo = svc["ServiceNo"] | "";
      info.eta1 = e1;
      info.eta2 = ok2 ? e2 : -1;
      info.eta3 = ok3 ? e3 : -1;

      int insertAt = (count < BUS_MAX_SERVICES) ? count : -1;
      for (int j = 0; j < count; j++) {
        if (info.eta1 < slots[j].eta1) { insertAt = j; break; }
      }
      if (insertAt >= 0) {
        int shiftEnd = (count < BUS_MAX_SERVICES) ? count : BUS_MAX_SERVICES - 1;
        for (int j = shiftEnd; j > insertAt; j--) slots[j] = slots[j - 1];
        slots[insertAt] = info;
        if (count < BUS_MAX_SERVICES) count++;
      }
    }
  }

  busDataValidByStop[idx] = true;
  lastBusFetchByStop[idx] = time(nullptr);
  lastSyncTime = lastBusFetchByStop[idx];
  return true;
}

bool fetchBusStopName(int idx) {
  if (idx < 0 || idx >= BUS_MAX_STOPS) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (busStops[idx].code.length() == 0) return false;

  busStopNames[idx].lastAttempt = time(nullptr);

  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://datamall2.mytransport.sg/ltaodataservice/BusStops?BusStopCode=" + busStops[idx].code;

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("AccountKey", effectiveLtaApiKey());
  http.addHeader("accept", "application/json");

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, body)) return false;

  JsonArray values = doc["value"];
  if (!values) return false;

  for (JsonObject stop : values) {
    const char* stopCode = stop["BusStopCode"] | "";
    if (busStops[idx].code != stopCode) continue;

    busStopNames[idx].roadName = stop["RoadName"] | "";
    busStopNames[idx].description = stop["Description"] | "";
    busStopNames[idx].valid = true;

    prefs.putString((String("busRoad") + String(idx)).c_str(), busStopNames[idx].roadName);
    prefs.putString((String("busName") + String(idx)).c_str(), busStopNames[idx].description);
    return true;
  }

  return false;
}

void ensureBusArrival() {
  if (WiFi.status() != WL_CONNECTED) return;

  time_t nowT = time(nullptr);

  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStops[i].code.length() == 0) continue;
    if (busStopNames[i].valid) continue;
    if ((nowT - busStopNames[i].lastAttempt) < (time_t)BUS_NAME_RETRY_SEC) continue;

    if (fetchBusStopName(i)) dataDirty = true;
    return;
  }

  int staleIdx = -1;
  time_t staleFetch = 0;
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStops[i].code.length() == 0) continue;
    bool due = !busDataValidByStop[i] || (nowT - lastBusFetchByStop[i]) > (time_t)BUS_INTERVAL_SEC;
    if (!due) continue;
    if (staleIdx < 0 || lastBusFetchByStop[i] < staleFetch) {
      staleIdx = i;
      staleFetch = lastBusFetchByStop[i];
    }
  }

  if (staleIdx >= 0) {
    if (fetchBusArrival(staleIdx)) dataDirty = true;
  }
}

// =========================================================
// DRAW HELPERS
// =========================================================
void drawCard(int x, int y, int w, int h, bool accent = false) {
  tft.fillRoundRect(x, y, w, h, 10, COL_PANEL);
  tft.drawRoundRect(x, y, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void drawTopBar(const String& title) {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, TOPBAR_H - 1, SCREEN_W, COL_STROKE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString(title, 10, 9, 2);

  const int bs = 25;
  const int bx = SCREEN_W - bs - 6;
  const int by = 4;

  uint16_t bg = (sleepDimmed || sleepOff) ? COL_ACCENT : COL_PANEL;
  uint16_t fg = (sleepDimmed || sleepOff) ? TFT_BLACK : COL_TEXT;

  tft.fillRoundRect(bx, by, bs, bs, 7, bg);
  tft.drawRoundRect(bx, by, bs, bs, 7, COL_ACCENT);

  const int cx = bx + 12;
  const int cy = by + 12;

  tft.drawCircle(cx, cy + 1, 6, fg);
  tft.drawFastHLine(cx - 4, cy - 5, 9, bg);
  tft.drawFastVLine(cx, cy - 7, 6, fg);
}

void drawNavBar() {
  const int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, y, SCREEN_W, COL_STROKE);

  const int btnW = SCREEN_W / 4;
  const char* names[4] = {"Home", "Weather", "Bus", "Status"};

  for (int i = 0; i < 4; i++) {
    int bx = i * btnW;
    bool active = ((int)currentPage == i);

    uint16_t bg = active ? COL_ACCENT : COL_PANEL;
    uint16_t fg = active ? TFT_BLACK : COL_TEXT;

    tft.fillRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, bg);
    tft.drawRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, active ? COL_ACCENT : COL_STROKE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(names[i], bx + btnW / 2, y + NAV_H / 2, 1);
  }

  tft.setTextDatum(TL_DATUM);
}

void makeSpriteCard(TFT_eSprite& spr, int w, int h, bool accent = false) {
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(COL_BG);
  spr.fillRoundRect(0, 0, w, h, 10, COL_PANEL);
  spr.drawRoundRect(0, 0, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void pushSpriteAndDelete(TFT_eSprite& spr, int x, int y) {
  spr.pushSprite(x, y, COL_BG);
  spr.deleteSprite();
}

void drawCleanSunIcon(TFT_eSPI& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 4, c);
  spr.drawLine(cx, cy - 9, cx, cy - 7, c);
  spr.drawLine(cx, cy + 7, cx, cy + 9, c);
  spr.drawLine(cx - 9, cy, cx - 7, cy, c);
  spr.drawLine(cx + 7, cy, cx + 9, cy, c);
  spr.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, c);
  spr.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, c);
  spr.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, c);
  spr.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, c);
}

void drawMoonIcon(TFT_eSPI& spr, int cx, int cy, uint16_t c, uint16_t bg = COL_PANEL) {
  spr.fillCircle(cx, cy, 6, c);
  spr.fillCircle(cx + 4, cy - 2, 6, bg);
}

void drawCloudGlyph(TFT_eSPI& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx - 7, cy + 2, 7, c);
  spr.fillCircle(cx + 2, cy - 4, 9, c);
  spr.fillCircle(cx + 10, cy + 2, 6, c);
  spr.fillRoundRect(cx - 12, cy, 30, 10, 5, c);
}

void drawConditionIcon(TFT_eSPI& spr, int cx, int cy, int code, bool isDay, uint16_t accent, uint16_t dim, uint16_t bg = COL_PANEL) {
  bool clearOrMainlyClear = (code == 0 || code == 1);
  bool fog = (code == 45 || code == 48);
  bool thunder = (code == 95 || code == 96 || code == 99);
  bool rain = (code >= 61 && code <= 67) || (code >= 51 && code <= 57) || (code >= 80 && code <= 82);
  bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;

  if (clearOrMainlyClear) {
    if (isDay) drawCleanSunIcon(spr, cx, cy, accent);
    else drawMoonIcon(spr, cx, cy, accent, bg);
    if (code == 1) spr.fillCircle(cx + 9, cy + 9, 5, dim);
    return;
  }

  if (thunder) {
    drawCloudGlyph(spr, cx, cy - 4, dim);
    spr.drawLine(cx - 2, cy + 8, cx + 3, cy + 8, accent);
    spr.drawLine(cx + 3, cy + 8, cx - 1, cy + 14, accent);
    spr.drawLine(cx - 1, cy + 14, cx + 4, cy + 14, accent);
    return;
  }

  if (rain) {
    drawCloudGlyph(spr, cx, cy - 4, dim);
    for (int i = -1; i <= 1; i++) {
      spr.drawLine(cx + i * 7, cy + 9, cx + i * 7 - 2, cy + 15, accent);
    }
    return;
  }

  if (snow) {
    drawCloudGlyph(spr, cx, cy - 4, dim);
    for (int i = -1; i <= 1; i++) {
      spr.fillCircle(cx + i * 7, cy + 12, 1, accent);
    }
    return;
  }

  if (fog) {
    for (int i = 0; i < 3; i++) {
      spr.drawFastHLine(cx - 12, cy - 4 + i * 5, 24, dim);
    }
    return;
  }

  drawCloudGlyph(spr, cx, cy, dim);
}

int drawWrappedTextLimited(int x, int y, int maxW, const String& text, int font, uint16_t fg, uint16_t bg, int maxLines) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);

  const int lineH = tft.fontHeight(font) + 2;
  String line = "";
  String word = "";
  int linesDrawn = 0;

  auto flushLine = [&]() {
    if (linesDrawn >= maxLines) return;
    if (line.length() > 0) tft.drawString(line, x, y, font);
    y += lineH;
    line = "";
    linesDrawn++;
  };

  auto placeWordOnEmptyLine = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    while (tft.textWidth(word, font) > maxW && word.length() > 1) {
      int cut = word.length();
      while (cut > 1 && tft.textWidth(word.substring(0, cut), font) > maxW) cut--;
      if (linesDrawn >= maxLines) return;
      tft.drawString(word.substring(0, cut), x, y, font);
      y += lineH;
      linesDrawn++;
      word = word.substring(cut);
    }

    if (linesDrawn < maxLines) {
      line = word;
      word = "";
    }
  };

  auto flushWord = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    if (line.length() == 0) {
      placeWordOnEmptyLine();
      return;
    }

    String candidate = line + " " + word;
    if (tft.textWidth(candidate, font) <= maxW) {
      line = candidate;
      word = "";
      return;
    }

    flushLine();
    placeWordOnEmptyLine();
  };

  for (int i = 0; i < (int)text.length(); i++) {
    if (linesDrawn >= maxLines) break;
    char c = text[i];

    if (c == '\n') {
      flushWord();
      flushLine();
      continue;
    }

    if (c == ' ') {
      flushWord();
      continue;
    }

    word += c;
  }

  if (linesDrawn < maxLines) {
    flushWord();
    if (line.length() > 0) flushLine();
  }

  return y;
}

// =========================================================
// HOME SPRITES
// =========================================================
void drawClockCardSprite(bool force = false) {
  const int x = 8, y = PAGE_ROW1_Y, w = 224, h = HOME_WIDGET_H;

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, true);
  String dateBuf = formatDateParts(tmNow);

  String sr = formatMinuteOfDay(sunriseMin);
  String ss = formatMinuteOfDay(sunsetMin);

  String combined = timeBuf + "|" + dateBuf + "|" + sr + "|" + ss + "|" +
                    String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  makeSpriteCard(sprClock, w, h, true);

  sprClock.setTextDatum(TL_DATUM);

  sprClock.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String clockMain = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String clockSuffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    sprClock.drawString(clockMain, 10, 11, 4);
    if (clockSuffix.length() > 0) {
      int suffixX = 10 + sprClock.textWidth(clockMain, 4) + 4;
      sprClock.drawString(clockSuffix, suffixX, 18, 2);
    }
  } else {
    sprClock.drawString(timeBuf, 10, 11, 4);
  }

  sprClock.setTextColor(COL_DIM, COL_PANEL);
  sprClock.drawString(dateBuf, 10, 45, 2);

  drawCleanSunIcon(sprClock, 151, 22, COL_ACCENT);
  drawMoonIcon(sprClock, 151, 50, COL_ACCENT);

  sprClock.setTextColor(COL_ACCENT, COL_PANEL);
  sprClock.drawString(sr, 165, 15, 2);
  sprClock.drawString(ss, 165, 43, 2);

  pushSpriteAndDelete(sprClock, x, y);
}

void drawMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 31, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 55, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawWeatherStyleMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 28, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 52, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawSunEventWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String label = nextSunLabel();
  String value = nextSunTimeText();
  String combined = label + "|" + value + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = value.lastIndexOf(' ');
    String mainValue = splitAt > 0 ? value.substring(0, splitAt) : value;
    String suffix = splitAt > 0 ? value.substring(splitAt + 1) : "";
    sprSmall.drawString(mainValue, 10, 30, 4);
    if (suffix.length() > 0) {
      int suffixX = 10 + sprSmall.textWidth(mainValue, 4) + 3;
      sprSmall.drawString(suffix, suffixX, 35, 2);
    }
  } else {
    sprSmall.drawString(value, 10, 30, 4);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawPlaceholderSprite(int x, int y, int w, int h, const char* label, String& cache, bool force = false) {
  String combined = String(label) + "|" + String(COL_PANEL) + "|" + String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_STROKE, COL_PANEL);
  sprSmall.drawString("Empty", 10, 31, 2);

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawFocusTimerWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusMenuOpen ? 1 : 0) +
                    "|" + String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString("Timer", 10, 8, 2);

  if (focusMenuOpen) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString("Select", 10, 22, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("duration", 10, 44, 2);
  } else {
    sprSmall.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
    sprSmall.drawString(value, 10, 24, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString(hint, 10, 49, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawHomeSlotWidget(int slot, bool force = false) {
  int x, y, w, h;
  getHomeSlotRect(slot, x, y, w, h);

  switch (homeWidgetSlots[slot]) {
    case HOME_WIDGET_WEEK:
      drawMetricSprite(x, y, w, h, "Week", weekNumberText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_TIMER:
      drawFocusTimerWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_RAIN:
      drawWeatherStyleMetricSprite(x, y, w, h, "Rain", rainText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_OUTDOOR:
      drawWeatherStyleMetricSprite(x, y, w, h, "Outdoor", tempText(), cacheHomeSlots[slot], force, tempRangeText());
      break;
    case HOME_WIDGET_KP:
      drawWeatherStyleMetricSprite(x, y, w, h, "KP index", kpText(), cacheHomeSlots[slot], force, kpLevelText());
      break;
    case HOME_WIDGET_UV:
      drawWeatherStyleMetricSprite(x, y, w, h, "UV index", uvText(), cacheHomeSlots[slot], force, uvLevelText());
      break;
    case HOME_WIDGET_WIND:
      drawWeatherStyleMetricSprite(x, y, w, h, "Wind", windText(), cacheHomeSlots[slot], force, windDirectionText());
      break;
    case HOME_WIDGET_SUN:
      drawSunEventWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_PSI:
      drawWeatherStyleMetricSprite(x, y, w, h, "PSI", psiText(), cacheHomeSlots[slot], force, psiLevelText());
      break;
    case HOME_WIDGET_BUS:
      drawWeatherStyleMetricSprite(x, y, w, h, "Bus", busEtaText(), cacheHomeSlots[slot], force, busDetailText());
      break;
  }
}

void drawFocusMenuOverlay(bool force = false) {
  String combined = String(focusTimerRunning ? 1 : 0) + "|" + String(focusTimerFinished ? 1 : 0) +
                    "|" + String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheTimerMenu) return;
  cacheTimerMenu = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Start timer", TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Choose a session length", TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;

  String labels[6];
  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    labels[i] = String(timerPresetMin[i]) + " min";
  }

  for (int i = 0; i < 6; i++) {
    tft.fillRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_PANEL);
    tft.drawRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(labels[i].c_str(), xs[i] + btnW / 2, ys[i] + 7, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionW = 152;
  const int actionX = TIMER_MENU_X + 24;
  const char* actionLabel = focusTimerRunning ? "Stop" : (focusTimerFinished ? "Reset" : "Custom time");
  uint16_t actionColor = focusTimerRunning ? COL_RED : COL_ACCENT;

  tft.fillRoundRect(actionX, actionY - 4, actionW, 26, 8, COL_PANEL);
  tft.drawRoundRect(actionX, actionY - 4, actionW, 26, 8, actionColor);
  tft.setTextColor(actionColor, COL_PANEL);
  tft.drawCentreString(actionLabel, actionX + actionW / 2, actionY + 4, 2);
}

void drawCustomTimerEntryOverlay(bool force = false) {
  String combined = String(customTimerMinutes) + "|" + String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheCustomTimer) return;
  cacheCustomTimer = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Custom timer", TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Set your own session length", TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  String valueLabel = String(customTimerMinutes) + " min";
  tft.fillRect(TIMER_MENU_X + 14, TIMER_MENU_Y + 52, TIMER_MENU_W - 28, 30, COL_PANEL_ALT);
  tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
  tft.drawCentreString(valueLabel.c_str(), TIMER_MENU_X + TIMER_MENU_W / 2, TIMER_MENU_Y + 56, 4);

  const int stepBtnW = 40;
  const int stepBtnH = 30;
  const int stepY = TIMER_MENU_Y + 96;
  const int stepGap = 4;
  const int stepXs[4] = {
    TIMER_MENU_X + 14,
    TIMER_MENU_X + 14 + (stepBtnW + stepGap),
    TIMER_MENU_X + 14 + 2 * (stepBtnW + stepGap),
    TIMER_MENU_X + 14 + 3 * (stepBtnW + stepGap)
  };
  const char* stepLabels[4] = {"-5", "-1", "+1", "+5"};

  for (int i = 0; i < 4; i++) {
    tft.fillRoundRect(stepXs[i], stepY, stepBtnW, stepBtnH, 8, COL_PANEL);
    tft.drawRoundRect(stepXs[i], stepY, stepBtnW, stepBtnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(stepLabels[i], stepXs[i] + stepBtnW / 2, stepY + 8, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionH = 26;
  const int cancelX = TIMER_MENU_X + 14;
  const int startX = TIMER_MENU_X + 106;
  const int actionBtnW = 80;

  tft.fillRoundRect(cancelX, actionY - 4, actionBtnW, actionH, 8, COL_PANEL);
  tft.drawRoundRect(cancelX, actionY - 4, actionBtnW, actionH, 8, COL_STROKE);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawCentreString("Cancel", cancelX + actionBtnW / 2, actionY + 4, 2);

  tft.fillRoundRect(startX, actionY - 4, actionBtnW, actionH, 8, COL_PANEL);
  tft.drawRoundRect(startX, actionY - 4, actionBtnW, actionH, 8, COL_ACCENT);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawCentreString("Start", startX + actionBtnW / 2, actionY + 4, 2);
}

const char* busEntryKeys[4][3] = {
  {"1", "2", "3"},
  {"4", "5", "6"},
  {"7", "8", "9"},
  {"Del", "0", "OK"}
};
const int BUS_ENTRY_CELL_W = 58;
const int BUS_ENTRY_CELL_H = 30;
const int BUS_ENTRY_GAP = 6;

void busEntryKeyRect(int r, int c, int& x, int& y, int& w, int& h) {
  x = BUS_ENTRY_X + 7 + c * (BUS_ENTRY_CELL_W + BUS_ENTRY_GAP);
  y = BUS_ENTRY_Y + 86 + r * (BUS_ENTRY_CELL_H + BUS_ENTRY_GAP);
  w = BUS_ENTRY_CELL_W;
  h = BUS_ENTRY_CELL_H;
}

void drawBusStopEntryOverlay(bool force = false) {
  String combined = busStopEntryDigits + "|" + String(busStopEntryTargetIdx) + "|" +
                     String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheBusStopEntry) return;
  cacheBusStopEntry = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(BUS_ENTRY_X, BUS_ENTRY_Y, BUS_ENTRY_W, BUS_ENTRY_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(BUS_ENTRY_X, BUS_ENTRY_Y, BUS_ENTRY_W, BUS_ENTRY_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Bus stop code", BUS_ENTRY_X + 14, BUS_ENTRY_Y + 10, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Enter the code, then OK", BUS_ENTRY_X + 14, BUS_ENTRY_Y + 30, 1);

  String displayDigits = busStopEntryDigits.length() > 0 ? busStopEntryDigits : "-----";
  tft.fillRect(BUS_ENTRY_X + 14, BUS_ENTRY_Y + 48, BUS_ENTRY_W - 28, 30, COL_PANEL_ALT);
  tft.setTextColor(busStopEntryDigits.length() > 0 ? COL_ACCENT : COL_DIM, COL_PANEL_ALT);
  tft.drawCentreString(displayDigits.c_str(), BUS_ENTRY_X + BUS_ENTRY_W / 2, BUS_ENTRY_Y + 52, 4);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx, by, bw, bh;
      busEntryKeyRect(r, c, bx, by, bw, bh);
      bool isOk = (r == 3 && c == 2);
      bool isDel = (r == 3 && c == 0);
      uint16_t btnColor = isOk ? COL_ACCENT : (isDel ? COL_RED : COL_STROKE);

      tft.fillRoundRect(bx, by, bw, bh, 8, COL_PANEL);
      tft.drawRoundRect(bx, by, bw, bh, 8, btnColor);
      tft.setTextColor(isOk ? COL_ACCENT : (isDel ? COL_RED : COL_TEXT), COL_PANEL);
      tft.drawCentreString(busEntryKeys[r][c], bx + bw / 2, by + 7, 2);
    }
  }
}

void drawTimerDoneOverlay(bool force = false) {
  if (!timerDoneDialogOpen) return;

  String elapsed = formatElapsedText(focusDurationSec);
  String countdown = timerDoneCountdownText();
  bool flashOn = flashModeEnabled && ((millis() / 300UL) % 2UL == 0);
  setBacklight(flashModeEnabled ? (flashOn ? FLASH_BL_HIGH : FLASH_BL_LOW) : BL_FULL);
  String combined = elapsed + "|" + String(COL_PANEL_ALT) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);
  String flashKey = String(flashOn ? 1 : 0);
  if (force || combined != cacheTimerDone || flashKey != cacheTimerDoneFlash) {
    cacheTimerDone = combined;
    cacheTimerDoneFlash = flashKey;
    cacheTimerDoneCountdown = "";

    uint16_t backdrop = flashOn ? COL_ACCENT : COL_BG;
    uint16_t panelBorder = flashOn ? TFT_WHITE : COL_ACCENT;
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, backdrop);
    tft.fillRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, COL_PANEL_ALT);
    tft.drawRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, panelBorder);

    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString("Timer complete", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 14, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
    tft.drawCentreString(elapsed, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 42, 2);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap anywhere to acknowledge", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 68, 1);
  }

  if (force || countdown != cacheTimerDoneCountdown) {
    cacheTimerDoneCountdown = countdown;
    tft.fillRect(TIMER_DONE_X + 28, TIMER_DONE_Y + 82, TIMER_DONE_W - 56, 12, COL_PANEL_ALT);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(countdown.c_str(), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 82, 1);
  }
}

// =========================================================
// PAGES
// =========================================================
void drawHomePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(homeTitleText());
  drawNavBar();

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawClockCardSprite(true);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, true);
  }
  if (focusMenuOpen) drawFocusMenuOverlay(true);
  if (customTimerEntryOpen) drawCustomTimerEntryOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateHomeDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (customTimerEntryOpen) {
    drawCustomTimerEntryOverlay(false);
    return;
  }

  if (focusMenuOpen) {
    drawFocusMenuOverlay(false);
    return;
  }

  drawClockCardSprite(false);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, false);
  }
}

void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Weather");
  drawNavBar();

  drawCard(8, WEATHER_NOW_Y, 224, WEATHER_NOW_H, true);
  drawCard(8, WEATHER_HOURLY_Y, 224, WEATHER_HOURLY_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;

  lastNowText = "";
  lastHourlyText = "";
  lastUvText = "";
  lastUvLevelText = "";
  lastPsiText = "";
  lastPsiLevelText = "";
}

static String hourLabel12(int hour24) {
  if (hour24 < 0) return "--";
  int h12 = hour24 % 12;
  if (h12 == 0) h12 = 12;
  return String(h12) + (hour24 >= 12 ? "P" : "A");
}

void updateWeatherDynamic() {
  String t = tempText();
  String cond = weatherConditionText(weatherCode);
  String feels = apparentTempText();
  String tr = tempRangeText();
  String r = "Rain " + rainText();
  String nowCombined = t + "|" + cond + "|" + feels + "|" + tr + "|" + r + "|" +
                        String(weatherCode) + "|" + String(weatherIsDay ? 1 : 0) + "|" +
                        String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (nowCombined != lastNowText) {
    tft.fillRect(18, WEATHER_NOW_Y + 8, 204, WEATHER_NOW_H - 16, COL_PANEL);

    drawConditionIcon(tft, 40, WEATHER_NOW_Y + 44, weatherCode, weatherIsDay, COL_ACCENT, COL_DIM);

    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(t, 74, WEATHER_NOW_Y + 8, 4);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(cond, 74, WEATHER_NOW_Y + 36, 2);
    if (feels.length() > 0) tft.drawString(feels, 74, WEATHER_NOW_Y + 56, 1);

    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(tr, 150, WEATHER_NOW_Y + 12, 1);
    tft.drawString(r, 150, WEATHER_NOW_Y + 28, 1);

    lastNowText = nowCombined;
  }

  String hourlyCombined = String(hourlyFilled);
  for (int i = 0; i < hourlyFilled; i++) {
    hourlyCombined += "|" + String(hourlyHour[i]) + ":" + String((int)roundf(hourlyTempC[i])) +
                       ":" + String(hourlyCode[i]) + ":" + String(hourlyIsDay[i] ? 1 : 0);
  }
  hourlyCombined += "|" + String(useUsRegionFormat() ? 1 : 0) + "|" + String(COL_ACCENT);

  if (hourlyCombined != lastHourlyText) {
    tft.fillRect(18, WEATHER_HOURLY_Y + 2, 204, WEATHER_HOURLY_H - 6, COL_PANEL);

    if (hourlyFilled == 0) {
      tft.setTextColor(COL_DIM, COL_PANEL);
      tft.drawCentreString("No hourly data yet", 120, WEATHER_HOURLY_Y + 22, 2);
    } else {
      const int colW = 204 / HOURLY_COUNT;
      for (int i = 0; i < hourlyFilled; i++) {
        int colCx = 18 + colW * i + colW / 2;

        String hLabel = useUsRegionFormat() ? hourLabel12(hourlyHour[i]) : (String(hourlyHour[i]) + "h");
        tft.setTextColor(COL_DIM, COL_PANEL);
        tft.drawCentreString(hLabel, colCx, WEATHER_HOURLY_Y + 4, 1);

        drawConditionIcon(tft, colCx, WEATHER_HOURLY_Y + 26, hourlyCode[i], hourlyIsDay[i], COL_ACCENT, COL_DIM);

        String hTemp = formatDisplayTemp(hourlyTempC[i]);
        tft.setTextColor(COL_TEXT, COL_PANEL);
        tft.drawCentreString(hTemp, colCx, WEATHER_HOURLY_Y + 38, 1);
      }
    }

    lastHourlyText = hourlyCombined;
  }

  String u = uvText();
  String ul = uvLevelText();
  if (u != lastUvText || ul != lastUvLevelText || dataDirty) {
    tft.fillRect(18, PAGE_ROW3_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("UV index", 18, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(u, 18, PAGE_ROW3_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(ul, 18, PAGE_ROW3_Y + 52, 1);
    lastUvText = u;
    lastUvLevelText = ul;
  }

  String k = psiText();
  String kl = psiLevelText();
  if (k != lastPsiText || kl != lastPsiLevelText || dataDirty) {
    tft.fillRect(134, PAGE_ROW3_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("PSI (haze)", 134, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(k, 134, PAGE_ROW3_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(kl, 134, PAGE_ROW3_Y + 52, 1);
    lastPsiText = k;
    lastPsiLevelText = kl;
  }
}

void drawBusPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Bus");
  drawNavBar();

  drawCard(8, 42, 224, 226, true);

  pageDirty = false;
  lastDrawnPage = PAGE_BUS;
  lastBusPageText = "";
}

const int BUS_TAB_Y = 50;
const int BUS_TAB_H = 24;
const int BUS_TAB_W = 68;

void updateBusDynamic() {
  int active = busActiveStopIdx;
  if (active < 0 || active >= BUS_MAX_STOPS) active = 0;

  String combined = String(active);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    combined += "|" + busStops[i].code;
  }
  combined += "|" + busStopDisplayName(active) + "|" + String(busDataValidByStop[active] ? 1 : 0) +
              "|" + String(busServiceCountByStop[active]) + "|" + String(busServicePage[active]);
  for (int i = 0; i < busServiceCountByStop[active]; i++) {
    combined += "|" + busServicesByStop[active][i].serviceNo + ":" +
                String(busServicesByStop[active][i].eta1) + ":" +
                String(busServicesByStop[active][i].eta2) + ":" +
                String(busServicesByStop[active][i].eta3);
  }

  if (combined == lastBusPageText && !dataDirty) return;
  lastBusPageText = combined;

  tft.fillRect(18, BUS_TAB_Y, 204, 216, COL_PANEL);

  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    int tabX = 18 + i * BUS_TAB_W;
    bool configured = busStops[i].code.length() > 0;
    bool isActive = configured && (i == active);
    uint16_t bg = isActive ? COL_ACCENT : COL_PANEL_ALT;
    uint16_t fg = isActive ? TFT_BLACK : (configured ? COL_TEXT : COL_DIM);

    tft.fillRoundRect(tabX + 2, BUS_TAB_Y, BUS_TAB_W - 4, BUS_TAB_H, 8, bg);
    tft.drawRoundRect(tabX + 2, BUS_TAB_Y, BUS_TAB_W - 4, BUS_TAB_H, 8, isActive ? COL_ACCENT : COL_STROKE);
    tft.setTextColor(fg, bg);
    String tabLabel = configured ? busStops[i].code : "+";
    tft.drawCentreString(tabLabel.c_str(), tabX + BUS_TAB_W / 2, BUS_TAB_Y + 5, configured ? 1 : 2);
  }

  bool activeConfigured = busStops[active].code.length() > 0;
  if (!activeConfigured) {
    drawWrappedTextLimited(18, 86, 198, "Tap + above to add a bus stop.", 2, COL_DIM, COL_PANEL, 4);
    return;
  }

  String name = busStopDisplayName(active);
  String nameLine = name.length() > 0 ? (name + " " + busStops[active].code) : ("Stop " + busStops[active].code);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(nameLine, 18, 80, 2);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Tap this tab again to edit", 18, 98, 1);

  if (!busDataValidByStop[active]) {
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Fetching bus times...", 18, 116, 2);
    return;
  }

  int total = busServiceCountByStop[active];
  if (total == 0) {
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("No buses currently in service.", 18, 116, 2);
    return;
  }

  const int perPage = 3;
  int pageCount = (total + perPage - 1) / perPage;
  if (busServicePage[active] >= pageCount) busServicePage[active] = 0;
  int page = busServicePage[active];
  int startIdx = page * perPage;

  for (int slot = 0; slot < perPage; slot++) {
    int i = startIdx + slot;
    if (i >= total) break;
    int rowY = 112 + slot * 44;
    BusServiceInfo& svc = busServicesByStop[active][i];

    tft.fillRoundRect(18, rowY, 46, 26, 8, COL_PANEL_ALT);
    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString(svc.serviceNo.c_str(), 41, rowY + 6, 2);

    String etaText = svc.eta1 <= 0 ? "Due" : String(svc.eta1) + " min";
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(etaText, 74, rowY + 2, 4);

    String nextText = "";
    if (svc.eta2 >= 0) {
      nextText += "then ";
      nextText += (svc.eta2 <= 0) ? "due" : (String(svc.eta2) + "m");
    }
    if (svc.eta3 >= 0) {
      nextText += (nextText.length() > 0 ? ", " : "then ");
      nextText += (svc.eta3 <= 0) ? "due" : (String(svc.eta3) + "m");
    }
    if (nextText.length() > 0) {
      tft.setTextColor(COL_ACCENT, COL_PANEL);
      tft.drawString(nextText, 74, rowY + 30, 1);
    }
  }

  if (pageCount > 1) {
    String moreLabel = "More (" + String(page + 1) + "/" + String(pageCount) + ")";
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawCentreString(moreLabel.c_str(), 120, 250, 2);
  }
}

void drawStatusPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Status");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_STATUS;

  lastWifiText = "";
  lastSignalText = "";
  lastIpText = "";
  lastUptimeText = "";
  lastNetworkToggleText = "";
}

void updateStatusDynamic() {
  String w = wifiStatusText();
  if (w != lastWifiText) {
    tft.fillRect(18, PAGE_ROW1_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("WiFi", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(statusColor(), COL_PANEL);
    tft.drawString(w, 18, PAGE_ROW1_Y + 32, 2);
    lastWifiText = w;
  }

  String s = signalText();
  if (s != lastSignalText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Signal", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(s, 134, PAGE_ROW1_Y + 30, 4);
    lastSignalText = s;
  }

  String ip = ipText();
  if (ip != lastIpText) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 200, 18, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("IP address", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(ip, 18, PAGE_ROW2_Y + 30, 2);
    lastIpText = ip;
  }

  String up = uptimeText();
  String upCombined = up + "|" + lastSyncText();
  if (upCombined != lastUptimeText) {
    tft.fillRect(18, PAGE_ROW3_Y + 26, 88, 26, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Uptime", 18, PAGE_ROW3_Y + 10, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(up, 18, PAGE_ROW3_Y + 26, 2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(lastSyncText(), 18, PAGE_ROW3_Y + 42, 1);
    lastUptimeText = upCombined;
  }

  String networkLabel = wifiEnabled ? "Enabled" : "Disabled";
  if (networkLabel != lastNetworkToggleText) {
    uint16_t btnBg = wifiEnabled ? COL_ACCENT : COL_PANEL_ALT;
    uint16_t btnFg = wifiEnabled ? TFT_BLACK : COL_TEXT;
    uint16_t btnStroke = wifiEnabled ? COL_ACCENT : COL_STROKE;

    tft.fillRect(132, PAGE_ROW3_Y + 8, 92, 42, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Network", 134, PAGE_ROW3_Y + 10, 2);
    tft.fillRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnBg);
    tft.drawRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnStroke);
    tft.setTextColor(btnFg, btnBg);
    tft.drawCentreString(networkLabel.c_str(), 178, PAGE_ROW3_Y + 32, 2);
    lastNetworkToggleText = networkLabel;
  }
}

void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_BUS:     drawBusPageFull(); break;
    case PAGE_STATUS:  drawStatusPageFull(); break;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) drawFocusMenuOverlay(true);
  if (customTimerEntryOpen && currentPage == PAGE_HOME) drawCustomTimerEntryOverlay(true);
  if (busStopEntryOpen && currentPage == PAGE_BUS) drawBusStopEntryOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateCurrentPageDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (customTimerEntryOpen && currentPage == PAGE_HOME) {
    drawCustomTimerEntryOverlay(false);
    return;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) {
    drawFocusMenuOverlay(false);
    return;
  }

  if (busStopEntryOpen && currentPage == PAGE_BUS) {
    drawBusStopEntryOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_BUS:     updateBusDynamic(); break;
    case PAGE_STATUS:  updateStatusDynamic(); break;
  }
}

bool handleBusStopEntryTouch(int x, int y) {
  if (!busStopEntryOpen) return false;

  if (x < BUS_ENTRY_X || x >= BUS_ENTRY_X + BUS_ENTRY_W || y < BUS_ENTRY_Y || y >= BUS_ENTRY_Y + BUS_ENTRY_H) {
    busStopEntryOpen = false;
    pageDirty = true;
    return true;
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx, by, bw, bh;
      busEntryKeyRect(r, c, bx, by, bw, bh);
      if (x < bx || x >= bx + bw || y < by || y >= by + bh) continue;

      const char* key = busEntryKeys[r][c];

      if (strcmp(key, "Del") == 0) {
        if (busStopEntryDigits.length() > 0) busStopEntryDigits.remove(busStopEntryDigits.length() - 1);
        cacheBusStopEntry = "";
        pageDirty = true;
        return true;
      }

      if (strcmp(key, "OK") == 0) {
        int idx = busStopEntryTargetIdx;
        if (idx >= 0 && idx < BUS_MAX_STOPS) {
          String newCode = busStopEntryDigits;
          bool codeChanged = (busStops[idx].code != newCode);
          busStops[idx].code = newCode;
          if (newCode.length() == 0) {
            busStops[idx].label = "";
            busStops[idx].svcFilter = "";
          }

          if (codeChanged) {
            busStopNames[idx] = BusStopNameInfo();
            busDataValidByStop[idx] = false;
            busServiceCountByStop[idx] = 0;
            lastBusFetchByStop[idx] = 0;
            busServicePage[idx] = 0;
          }

          prefs.putString((String("busCode") + String(idx)).c_str(), busStops[idx].code);
          prefs.putString((String("busLabel") + String(idx)).c_str(), busStops[idx].label);
          prefs.putString((String("busName") + String(idx)).c_str(), "");
          prefs.putString((String("busRoad") + String(idx)).c_str(), "");

          if (newCode.length() > 0) {
            busActiveStopIdx = idx;
          } else if (idx == busActiveStopIdx) {
            for (int j = 0; j < BUS_MAX_STOPS; j++) {
              if (busStops[j].code.length() > 0) { busActiveStopIdx = j; break; }
            }
          }
        }

        busStopEntryOpen = false;
        cacheBusStopEntry = "";
        lastBusPageText = "";
        pageDirty = true;
        return true;
      }

      if (busStopEntryDigits.length() < 6) {
        busStopEntryDigits += key;
        cacheBusStopEntry = "";
        pageDirty = true;
      }
      return true;
    }
  }

  return true;
}

bool handleCustomTimerEntryTouch(int x, int y) {
  if (!customTimerEntryOpen) return false;

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    customTimerEntryOpen = false;
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  const int stepBtnW = 40;
  const int stepBtnH = 30;
  const int stepY = TIMER_MENU_Y + 96;
  const int stepGap = 4;
  const int stepXs[4] = {
    TIMER_MENU_X + 14,
    TIMER_MENU_X + 14 + (stepBtnW + stepGap),
    TIMER_MENU_X + 14 + 2 * (stepBtnW + stepGap),
    TIMER_MENU_X + 14 + 3 * (stepBtnW + stepGap)
  };
  const int stepDeltas[4] = {-5, -1, 1, 5};

  for (int i = 0; i < 4; i++) {
    if (x >= stepXs[i] && x < stepXs[i] + stepBtnW && y >= stepY && y < stepY + stepBtnH) {
      customTimerMinutes = sanitizeTimerMinutes(customTimerMinutes + stepDeltas[i]);
      cacheCustomTimer = "";
      pageDirty = true;
      return true;
    }
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionH = 26;
  const int cancelX = TIMER_MENU_X + 14;
  const int startX = TIMER_MENU_X + 106;
  const int actionBtnW = 80;

  if (x >= cancelX && x < cancelX + actionBtnW && y >= actionY - 4 && y < actionY - 4 + actionH) {
    customTimerEntryOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  if (x >= startX && x < startX + actionBtnW && y >= actionY - 4 && y < actionY - 4 + actionH) {
    startFocusTimer(customTimerMinutes);
    customTimerEntryOpen = false;
    pageDirty = true;
    return true;
  }

  return true;
}

bool handleFocusMenuTouch(int x, int y) {
  if (!focusMenuOpen) return false;

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  struct ButtonHit {
    int x;
    int y;
    int w;
    int h;
    int minutes;
  };

  const ButtonHit buttons[] = {
    {34, 124, 74, 28, timerPresetMin[0]},
    {132, 124, 74, 28, timerPresetMin[1]},
    {34, 158, 74, 28, timerPresetMin[2]},
    {132, 158, 74, 28, timerPresetMin[3]},
    {34, 192, 74, 28, timerPresetMin[4]},
    {132, 192, 74, 28, timerPresetMin[5]}
  };

  for (const ButtonHit& btn : buttons) {
    if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
      startFocusTimer(btn.minutes);
      pageDirty = true;
      return true;
    }
  }

  if (x >= 44 && x < 196 && y >= 224 && y < 250) {
    if (focusTimerRunning || focusTimerFinished) {
      resetFocusTimer();
    } else {
      customTimerEntryOpen = true;
      cacheCustomTimer = "";
    }
    pageDirty = true;
    return true;
  }

  return true;
}

bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;

  if (customTimerEntryOpen) return handleCustomTimerEntryTouch(x, y);
  if (focusMenuOpen) return handleFocusMenuTouch(x, y);

  for (int slot = 0; slot < HOME_SLOT_COUNT; slot++) {
    if (homeWidgetSlots[slot] != HOME_WIDGET_TIMER) continue;

    int slotX, slotY, slotW, slotH;
    getHomeSlotRect(slot, slotX, slotY, slotW, slotH);

    if (x >= slotX && x < slotX + slotW && y >= slotY && y < slotY + slotH) {
      if (focusTimerFinished) {
        resetFocusTimer();
      } else {
        focusMenuOpen = true;
        cacheFocusTimer = "";
        clearHomeSlotCaches();
        cacheTimerMenu = "";
      }
      pageDirty = true;
      return true;
    }
  }

  return false;
}

bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (!timerDoneDialogOpen) return false;
  dismissTimerDoneDialog();
  return true;
}

bool handleStatusTouch(int x, int y) {
  if (currentPage != PAGE_STATUS) return false;

  if (x >= 124 && x < 232 && y >= 198 && y < 268) {
    setWifiEnabled(!wifiEnabled);
    return true;
  }

  return false;
}

bool handleBusTouch(int x, int y) {
  if (currentPage != PAGE_BUS) return false;

  if (busStopEntryOpen) return handleBusStopEntryTouch(x, y);

  int active = busActiveStopIdx;
  if (active < 0 || active >= BUS_MAX_STOPS) active = 0;

  if (y >= BUS_TAB_Y && y < BUS_TAB_Y + BUS_TAB_H) {
    int i = (x - 18) / BUS_TAB_W;
    if (i >= 0 && i < BUS_MAX_STOPS) {
      bool configured = busStops[i].code.length() > 0;
      if (!configured) {
        busStopEntryOpen = true;
        busStopEntryTargetIdx = i;
        busStopEntryDigits = "";
        cacheBusStopEntry = "";
      } else if (i == active) {
        busStopEntryOpen = true;
        busStopEntryTargetIdx = i;
        busStopEntryDigits = busStops[i].code;
        cacheBusStopEntry = "";
      } else {
        busActiveStopIdx = i;
        lastBusPageText = "";
      }
      pageDirty = true;
      return true;
    }
  }

  int total = busServiceCountByStop[active];
  int pageCount = (total + 2) / 3;
  if (pageCount > 1 && x >= 20 && x < 220 && y >= 242 && y < 268) {
    busServicePage[active] = (busServicePage[active] + 1) % pageCount;
    lastBusPageText = "";
    pageDirty = true;
    return true;
  }

  return false;
}

// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  if (y < SCREEN_H - NAV_H) return;

  int btnW = SCREEN_W / 4;
  int idx = x / btnW;
  if (idx < 0 || idx > 3) return;

  Page newPage = (Page)idx;
  if (newPage != currentPage) {
    currentPage = newPage;
    pageDirty = true;
  }
}

// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String tz     = sanitizeTimezoneKey(prefs.getString("tz", "singapore"));
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(21000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>MakerPulse</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:linear-gradient(180deg,#0b1018 0%,#111827 100%);color:#edf2f7;font-family:system-ui,sans-serif;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid #243244;border-radius:20px;background:linear-gradient(135deg,#111927 0%,#172235 100%);box-shadow:0 10px 30px rgba(0,0,0,.22);}";
  page += ".hero h1{font-size:30px;margin:0 0 8px 0;}";
  page += ".hero p{margin:0;color:#a9b7c9;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#0b1220;border:1px solid #334155;color:#dbe7f5;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#171b22;border:1px solid #2d3748;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#a0aec0;font-weight:600;}";
  page += "input,select{width:100%;border-radius:12px;border:1px solid #334155;background:#0b1220;color:#edf2f7;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "button{margin-top:18px;background:#38bdf8;border:none;color:#001018;padding:13px 18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #334155;border-radius:12px;background:#0b1220;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<h1>MakerPulse</h1>";
  page += "<p>Shape MakerPulse into your own desk companion with widgets, notes, colors, and smart daily tools.</p>";
  page += "<div class='ip'>ESP IP: ";
  page += WiFi.localIP().toString();
  page += "</div></div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Theme and color</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Colors and visual style for the display.</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Accent</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Text</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Theme</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Settings</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>Buddy nickname</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">Never</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 minute</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 minutes</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 minutes</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 minutes</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 hour</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Sleep dims the screen first, then turns it fully off after 60 seconds.</div></div>";
  page += "<div><label class='label'>Measurement system</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Time zone</label><select name='tz'>";
  appendTimezoneOptions(page, tz);
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>Choose the six quick timers shown in the popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>Alert behavior</span><label style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Location</span><div class='settings-desc'>Used for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>Latitude</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>Longitude</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Example Singapore: latitude 1.3521, longitude 103.8198.</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Air quality (PSI)</span><div class='settings-desc'>NEA haze reading shown on the Weather page and as a Home widget. No API key needed.</div><div class='grid'>";
  page += "<div><label class='label'>PSI region</label><select name='psiRegion'>";
  page += "<option value='central'" + String(psiRegion=="central"?" selected":"") + ">Central</option>";
  page += "<option value='north'"   + String(psiRegion=="north"?" selected":"")   + ">North</option>";
  page += "<option value='south'"   + String(psiRegion=="south"?" selected":"")   + ">South</option>";
  page += "<option value='east'"    + String(psiRegion=="east"?" selected":"")    + ">East</option>";
  page += "<option value='west'"    + String(psiRegion=="west"?" selected":"")    + ">West</option>";
  page += "</select></div>";
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Bus arrival</span><div class='settings-desc'>Shows next bus times for up to " + String(BUS_MAX_STOPS) + " stops, via LTA DataMall. MakerPulse works out of the box using a shared default key, but shared keys can hit rate limits &mdash; for reliable results, request your own free AccountKey at <a href='https://datamall.lta.gov.sg/content/datamall/en/request-for-api.html' target='_blank'>datamall.lta.gov.sg</a> and enter it below. Find each 5-digit bus stop code on the physical bus stop sign (you can also enter a code directly on the device from the Bus tab). The stop name is looked up automatically once a code is saved; use the override field if you'd rather set your own. Leave service number blank to show the soonest bus of any route.</div>";
  page += "<div><label class='label'>LTA AccountKey</label><input name='ltaKey' value='" + htmlEscape(ltaApiKey) + "'></div>";
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String autoName = busStopDisplayName(i);
    page += "<div class='timer-slot' style='margin-top:12px;'>";
    page += "<div class='timer-slot-head'>Stop " + String(i + 1) + (autoName.length() > 0 ? (" - " + htmlEscape(autoName)) : "") + "</div>";
    page += "<div class='grid-3'>";
    page += "<div><label class='label'>Bus stop code</label><input name='busCode" + String(i) + "' maxlength='10' value='" + htmlEscape(busStops[i].code) + "'></div>";
    page += "<div><label class='label'>Name override</label><input name='busLabel" + String(i) + "' maxlength='30' value='" + htmlEscape(busStops[i].label) + "'></div>";
    page += "<div><label class='label'>Service number</label><input name='busSvc" + String(i) + "' maxlength='10' value='" + htmlEscape(busStops[i].svcFilter) + "'></div>";
    page += "</div></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widget Customization</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Choose which widgets appear in the four Home slots below the clock card.</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i]);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>Save to MakerPulse</button>";
  page += "</div></div></form>";
  page += "<script>";
  page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Deep black',nordic:'Nordic blue',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'Ochre'}};";
  page += "var panelStorageKey='makerpulse-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newTz     = server.hasArg("tz") ? server.arg("tz") : timezoneKey;
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  String newPsiRegion = server.hasArg("psiRegion") ? server.arg("psiRegion") : psiRegion;
  String newLtaKey    = server.hasArg("ltaKey") ? server.arg("ltaKey") : ltaApiKey;

  BusStopConfig newBusStops[BUS_MAX_STOPS];
  bool busStopCodeChanged[BUS_MAX_STOPS];
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String codeKey  = String("busCode") + String(i);
    String labelKey = String("busLabel") + String(i);
    String svcKey   = String("busSvc") + String(i);

    newBusStops[i].code      = server.hasArg(codeKey) ? server.arg(codeKey) : busStops[i].code;
    newBusStops[i].label     = server.hasArg(labelKey) ? server.arg(labelKey) : busStops[i].label;
    newBusStops[i].svcFilter = server.hasArg(svcKey) ? server.arg(svcKey) : busStops[i].svcFilter;

    newBusStops[i].code.trim();
    newBusStops[i].label.trim();
    newBusStops[i].svcFilter.trim();

    if (newBusStops[i].code.length() > 10) newBusStops[i].code = newBusStops[i].code.substring(0, 10);
    if (newBusStops[i].label.length() > 30) newBusStops[i].label = newBusStops[i].label.substring(0, 30);
    if (newBusStops[i].svcFilter.length() > 10) newBusStops[i].svcFilter = newBusStops[i].svcFilter.substring(0, 10);

    busStopCodeChanged[i] = (newBusStops[i].code != busStops[i].code);
  }

  newLoc.trim();
  newNickname.trim();
  newPsiRegion.trim();
  newLtaKey.trim();

  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  newTz = sanitizeTimezoneKey(newTz);
  newPsiRegion = sanitizePsiRegion(newPsiRegion);

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);
  bool psiRegionChanged = (newPsiRegion != psiRegion);
  bool busSettingsChanged = (newLtaKey != ltaApiKey);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStopCodeChanged[i] ||
        newBusStops[i].label != busStops[i].label ||
        newBusStops[i].svcFilter != busStops[i].svcFilter) {
      busSettingsChanged = true;
    }
  }

  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  timezoneKey = newTz;
  flashModeEnabled = newFlashMode;
  psiRegion = newPsiRegion;
  ltaApiKey = newLtaKey;
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    busStops[i] = newBusStops[i];
    if (busStopCodeChanged[i]) {
      busStopNames[i] = BusStopNameInfo();
      busDataValidByStop[i] = false;
      busServiceCountByStop[i] = 0;
      lastBusFetchByStop[i] = 0;
      busServicePage[i] = 0;
    }
  }
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("tz", timezoneKey);
  prefs.putString("nickname", buddyNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);
  prefs.putString("psiRegion", psiRegion);
  prefs.putString("ltaKey", ltaApiKey);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    prefs.putString((String("busCode") + String(i)).c_str(), busStops[i].code);
    prefs.putString((String("busLabel") + String(i)).c_str(), busStops[i].label);
    prefs.putString((String("busSvc") + String(i)).c_str(), busStops[i].svcFilter);
    if (busStopCodeChanged[i]) {
      prefs.putString((String("busName") + String(i)).c_str(), "");
      prefs.putString((String("busRoad") + String(i)).c_str(), "");
    }
  }
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  applyDeviceTimezoneByKey(timezoneKey);
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastNowText = "";
  lastHourlyText = "";
  lastPsiText = "";
  lastPsiLevelText = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();
  if (psiRegionChanged) {
    psiValue = NAN;
    lastPsiFetch = 0;
  }
  if (busSettingsChanged) {
    lastBusPageText = "";
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 1700000000 && millis() - t0 < 10000) {
    delay(200);
    now = time(nullptr);
  }
}

void beginWiFiConnect() {
  if (!wifiEnabled || storedWifiSsid.length() == 0) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(storedWifiSsid.c_str(), storedWifiPass.c_str());
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}

void connectWiFi(bool waitForConnection = true) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }
  wifiConnectInProgress = false;
}

void updateWiFiConnectionState() {
  if (!wifiEnabled || !wifiConnectInProgress) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress = false;
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensurePsi();
    ensureBusArrival();
    dataDirty = true;
    pageDirty = true;
    return;
  }

  if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectInProgress = false;
    pageDirty = true;
  }
}

void setWifiEnabled(bool enabled) {
  wifiEnabled = enabled;
  prefs.putBool("wifiEnabled", wifiEnabled);

  if (!wifiEnabled) {
    wifiConnectInProgress = false;
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  } else {
    beginWiFiConnect();
  }

  dataDirty = true;
  pageDirty = true;
}

void drawSetupScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("MakerPulse", SCREEN_W / 2, 60, 4);
  tft.setTextColor(0x5EFA, TFT_BLACK);
  tft.drawCentreString("Set up your device at:", SCREEN_W / 2, 140, 2);
  tft.drawCentreString(SETUP_SITE_URL, SCREEN_W / 2, 164, 2);
  tft.setTextColor(0x94B2, TFT_BLACK);
  tft.drawCentreString("Connect via USB and", SCREEN_W / 2, 220, 1);
  tft.drawCentreString("follow the on-screen steps", SCREEN_W / 2, 236, 1);
}

void finishBootAfterWifi() {
  tft.drawString("Syncing time...", 10, 58, 2);
  configTzTime(
    "SGT-8",
    "pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com"
  );
  waitForNtpTime();

  ensureSunTimesForToday();
  ensureWeather();
  ensureKpIndex();
  ensurePsi();
  ensureBusArrival();

  setupWebServer();

  pageDirty = true;
  dataDirty = true;

  drawCurrentPageFull();
  updateCurrentPageDynamic();

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("MakerPulse web: http://");
  Serial.println(WiFi.localIP());
}

void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
  storedWifiSsid = String(ssid);
  storedWifiPass = String(password);
  prefs.putString("wifiSsid", storedWifiSsid);
  prefs.putString("wifiPass", storedWifiPass);
  wifiEnabled = true;
  prefs.putBool("wifiEnabled", true);

  bool wasProvisioned = deviceProvisioned;
  deviceProvisioned = true;

  if (!wasProvisioned) {
    finishBootAfterWifi();
  }
  pageDirty = true;
  dataDirty = true;
}

void onImprovWiFiErrorCb(ImprovTypes::Error err) {
  // Improv reports errors back over serial to the website itself; nothing
  // additional required on-device.
}

void setup() {
  Serial.begin(115200);
  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "MakerPulse",
                              FIRMWARE_VERSION, "MakerPulse", "http://{LOCAL_IPV4}");
  improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
  improvSerial.onImprovError(onImprovWiFiErrorCb);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BL_FULL);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  setBacklight(BL_FULL);
  lastInteractionMs = millis();

  delay(200);

  tft.init();
  tft.setRotation(ROT);
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  ts.setRotation(ROT);

  if (!deviceProvisioned) {
    drawSetupScreen();
    return; // skip WiFi connect / NTP / webserver / first page draw entirely
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Booting MakerPulse...", 10, 10, 2);
  tft.drawString("Connecting WiFi...", 10, 34, 2);
  connectWiFi(true);

  finishBootAfterWifi();
}

void loop() {
  improvSerial.handleSerial();

  if (!deviceProvisioned) {
    delay(10);
    return; // Improv is the only thing running; TFT just shows the setup screen
  }

  server.handleClient();
  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneDialogState();
  handleAutoSleep();

  int tx = 0, ty = 0;
  if (touchNewPress(tx, ty)) {
    lastInteractionMs = millis();

    if (sleepOff) {
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
      } else {
        wakeDisplay();
      }
      return;
    }

    if (handleTimerDoneDialogTouch(tx, ty)) {
      return;
    }

    if (tx >= SCREEN_W - 36 && ty <= TOPBAR_H) {
      toggleSleepMode();
      pageDirty = true;
    } else {
      if (sleepDimmed) {
        if (!manualDimMode) {
          wakeDisplay();
        } else {
          if (!handleHomeTouch(tx, ty) && !handleStatusTouch(tx, ty) && !handleBusTouch(tx, ty)) {
            handleNavTouch(tx, ty);
          }
        }
      } else {
        if (!handleHomeTouch(tx, ty) && !handleStatusTouch(tx, ty) && !handleBusTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    }
  }

  if (millis() - lastDataTick >= DATA_TICK_MS) {
    lastDataTick = millis();
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensurePsi();
    ensureBusArrival();
  }

  if (pageDirty || lastDrawnPage != currentPage) {
    bool anyOverlayOpen = focusMenuOpen || customTimerEntryOpen || timerDoneDialogOpen || busStopEntryOpen;
    if (lastDrawnPage != currentPage || !anyOverlayOpen) {
      drawCurrentPageFull();
    } else if (timerDoneDialogOpen) {
      drawTimerDoneOverlay(true);
    } else if (customTimerEntryOpen && currentPage == PAGE_HOME) {
      drawCustomTimerEntryOverlay(true);
    } else if (focusMenuOpen && currentPage == PAGE_HOME) {
      drawFocusMenuOverlay(true);
    } else if (busStopEntryOpen && currentPage == PAGE_BUS) {
      drawBusStopEntryOverlay(true);
    }
    updateCurrentPageDynamic();
    pageDirty = false;
    dataDirty = false;
  }

  if (millis() - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick = millis();
    updateCurrentPageDynamic();
    dataDirty = false;
  }

  delay(10);
}
