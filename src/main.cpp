// MakerPulse firmware -- ESP32 "Cheap Yellow Display" desk companion.
// See CONTEXT.md for the settings-page/setup-site glossary and README.md
// for the build/flash/release process.

#include "config.h"
#include "state.h"
#include "network.h"
#include "display.h"
#include "settings_server.h"



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
