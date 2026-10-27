#pragma once

// Touch input and everything drawn to the TFT: page chrome, sprites/icons,
// the four main pages and their overlays, and the readout-formatting
// helpers used only for what appears on screen.

#include "state.h"

bool readTouchXY(int& sx, int& sy);
bool touchNewPress(int& tx, int& ty);

void setBacklight(int value);
void wakeDisplay(bool clearManualMode = true);
void enterSleepDim();
void enterSleepOff();
void toggleSleepMode();
void handleAutoSleep();

void drawCurrentPageFull();
void updateCurrentPageDynamic();
void drawTimerDoneOverlay(bool force = false);
void drawCustomTimerEntryOverlay(bool force = false);
void drawFocusMenuOverlay(bool force = false);
void drawBusStopEntryOverlay(bool force = false);

bool handleHomeTouch(int x, int y);
bool handleStatusTouch(int x, int y);
bool handleBusTouch(int x, int y);
bool handleTimerDoneDialogTouch(int x, int y);
void handleNavTouch(int x, int y);
