#pragma once

// Compile-time configuration: pin assignments, display/touch calibration,
// layout geometry, and the small value types shared across modules.
// No mutable state and no function bodies live here.

#include <Arduino.h>
#include <TFT_eSPI.h>

#define FIRMWARE_VERSION "1.1.0"
#define SETUP_SITE_URL "theyoungmaker.github.io/MakerPulse"

// The Young Maker brand mark: a maker figure under three Wi-Fi arcs, reused
// as-is in the settings page header/footer. Colors sampled from their real
// logo asset and live site (theyoungmaker.com) rather than invented here.
#define YM_LOGO_SVG "<svg viewBox='0 0 100 130' xmlns='http://www.w3.org/2000/svg'><path d='M38 46a17 17 0 0 1 24 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><path d='M27 34a33 33 0 0 1 46 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><path d='M16 22a53 53 0 0 1 68 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><circle cx='50' cy='62' r='9' fill='#262220'/><path d='M27 86q23 20 46 0' fill='none' stroke='#262220' stroke-width='7' stroke-linecap='round'/><path d='M50 86L30 124' stroke='#262220' stroke-width='7' stroke-linecap='round'/><path d='M50 86L70 124' stroke='#262220' stroke-width='7' stroke-linecap='round'/></svg>"

#define TOUCH_CS  33
#define TOUCH_IRQ 36

#define FIRMWARE_VERSION "1.0.0"

#define SETUP_SITE_URL "theyoungmaker.github.io/MakerPulse"

#define YM_LOGO_SVG "<svg viewBox='0 0 100 130' xmlns='http://www.w3.org/2000/svg'><path d='M38 46a17 17 0 0 1 24 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><path d='M27 34a33 33 0 0 1 46 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><path d='M16 22a53 53 0 0 1 68 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><circle cx='50' cy='62' r='9' fill='#262220'/><path d='M27 86q23 20 46 0' fill='none' stroke='#262220' stroke-width='7' stroke-linecap='round'/><path d='M50 86L30 124' stroke='#262220' stroke-width='7' stroke-linecap='round'/><path d='M50 86L70 124' stroke='#262220' stroke-width='7' stroke-linecap='round'/></svg>"



const int ROT = 2;

const bool INV = false;
#define TOUCH_CS  33

#define TOUCH_IRQ 36


static const int T_SCK  = 25;

static const int T_MISO = 39;

static const int T_MOSI = 32;


static const int TOUCH_X_MIN = 562;

static const int TOUCH_X_MAX = 3604;

static const int TOUCH_Y_MIN = 544;

static const int TOUCH_Y_MAX = 3720;


static const bool TOUCH_SWAP_XY = false;

static const bool TOUCH_FLIP_X  = false;

static const bool TOUCH_FLIP_Y  = false;


const uint16_t COL_GREEN  = TFT_GREEN;

const uint16_t COL_YELLOW = 0xFFE0;

const uint16_t COL_RED    = TFT_RED;

const uint16_t COL_BLUE   = 0x041F;


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
}
;


const int HOME_SLOT_COUNT = 4;


// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_BUS = 2,
  PAGE_STATUS = 3
}
;


const unsigned long CLOCK_TICK_MS = 1000;

const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;


static const int HOURLY_COUNT = 4;

static const uint32_t KP_INTERVAL_SEC = 10 * 60;

static const uint32_t PSI_INTERVAL_SEC = 15 * 60;
#define LTA_FALLBACK_API_KEY "2jQlivifRK227X26Uk+bdA=="



struct BusStopConfig {
  String code = "";
  String label = "";
  String svcFilter = "";
}
;

static const int BUS_MAX_STOPS = 3;


struct BusStopNameInfo {
  String roadName = "";
  String description = "";
  bool valid = false;
  time_t lastAttempt = 0;
}
;

static const uint32_t BUS_NAME_RETRY_SEC = 10 * 60;


struct BusServiceInfo {
  String serviceNo = "";
  int eta1 = -1;
  int eta2 = -1;
  int eta3 = -1;
}
;

static const int BUS_MAX_SERVICES = 6;

static const uint32_t BUS_INTERVAL_SEC = 45;


// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;


const int BL_FULL = 255;

const int BL_DIM  = 18;

const int BL_OFF  = 0;

const int FLASH_BL_LOW = 20;

const int FLASH_BL_HIGH = 255;


static const char* busEntryKeys[4][3] = {
  {"1", "2", "3"},
  {"4", "5", "6"},
  {"7", "8", "9"},
  {"Del", "0", "OK"}
}
;

const int BUS_ENTRY_CELL_W = 58;

const int BUS_ENTRY_CELL_H = 30;

const int BUS_ENTRY_GAP = 6;


const int BUS_TAB_Y = 50;

const int BUS_TAB_H = 24;

const int BUS_TAB_W = 68;
