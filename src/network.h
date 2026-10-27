#pragma once

// WiFi connection management, Improv Wi-Fi provisioning, and every outbound
// HTTP fetch (weather, PSI, KP-index, sunrise/sunset, LTA bus arrivals).

#include "state.h"

void beginWiFiConnect();
void connectWiFi(bool waitForConnection = true);
void updateWiFiConnectionState();
void setWifiEnabled(bool enabled);

bool fetchSunriseSunset();
void ensureSunTimesForToday();
bool fetchWeather();
void ensureWeather();
bool fetchKpIndex();
void ensureKpIndex();
bool fetchPsi();
void ensurePsi();
bool fetchBusArrival(int idx);
bool fetchBusStopName(int idx);
void ensureBusArrival();
