#include "network.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>



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


void connectWiFi(bool waitForConnection) {
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
