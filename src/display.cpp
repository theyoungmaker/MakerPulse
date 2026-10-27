#include "display.h"
#include "network.h"
#include <math.h>



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
  return deviceNickname.length() > 0 ? deviceNickname : "MakerPulse";
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


void drawFocusMenuOverlay(bool force) {
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


void drawCustomTimerEntryOverlay(bool force) {
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


void busEntryKeyRect(int r, int c, int& x, int& y, int& w, int& h) {
  x = BUS_ENTRY_X + 7 + c * (BUS_ENTRY_CELL_W + BUS_ENTRY_GAP);
  y = BUS_ENTRY_Y + 86 + r * (BUS_ENTRY_CELL_H + BUS_ENTRY_GAP);
  w = BUS_ENTRY_CELL_W;
  h = BUS_ENTRY_CELL_H;
}


void drawBusStopEntryOverlay(bool force) {
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


void drawTimerDoneOverlay(bool force) {
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
