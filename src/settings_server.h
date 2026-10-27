#pragma once

// The device's own settings web page: HTML generation for GET /, the
// POST /save handler, and the small HTML/color-formatting helpers used
// only by that page.

#include "state.h"

void handleRoot();
void handleSave();
void setupWebServer();
