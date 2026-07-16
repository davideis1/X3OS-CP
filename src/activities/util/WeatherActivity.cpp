#include "WeatherActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <SecureHttpClient.h>
#include <WiFi.h>

#include <cstdio>
#include <cstdlib>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "WeatherLocationStore.h"
#include "activities/ActivityResult.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Same wolfSSL TLS-handshake heap floor KOReaderSyncClient.cpp uses (duplicated rather than
// shared — one extra caller isn't worth extracting it from working code).
constexpr uint32_t MIN_HEAP_FOR_TLS = 55000;
constexpr uint32_t ENTER_CHANGE_LOCATION_MS = 700;

bool insufficientHeap() {
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
  if (freeHeap < MIN_HEAP_FOR_TLS || maxAllocHeap < MIN_HEAP_FOR_TLS) {
    LOG_ERR("WEATHER", "Insufficient heap for TLS handshake: %u free, %u max alloc (need %u)", freeHeap, maxAllocHeap,
            MIN_HEAP_FOR_TLS);
    return true;
  }
  return false;
}

const char* weatherCodeLabel(int code) {
  switch (code) {
    case 0:
      return tr(STR_WX_CLEAR);
    case 1:
    case 2:
    case 3:
      return tr(STR_WX_PARTLY_CLOUDY);
    case 45:
    case 48:
      return tr(STR_WX_FOG);
    case 51:
    case 53:
    case 55:
      return tr(STR_WX_DRIZZLE);
    case 61:
    case 63:
    case 65:
      return tr(STR_WX_RAIN);
    case 71:
    case 73:
    case 75:
      return tr(STR_WX_SNOW);
    case 80:
    case 81:
    case 82:
      return tr(STR_WX_SHOWERS);
    case 95:
    case 96:
    case 99:
      return tr(STR_WX_THUNDERSTORM);
    default:
      return tr(STR_WX_UNKNOWN);
  }
}
}  // namespace

void WeatherActivity::onEnter() {
  Activity::onEnter();
  const WeatherLocation* def = WEATHER_STORE.getDefault();
  hasLocation = def != nullptr;
  if (hasLocation) {
    latitude = def->lat;
    longitude = def->lon;
  }
  state = hasLocation ? WeatherState::READY : WeatherState::NEED_LOCATION;
  errorText.clear();
  shouldTearDownWifiOnExit = false;
  requestUpdate();
}

void WeatherActivity::onExit() {
  Activity::onExit();
  // Mirrors ClockSyncActivity/KOReaderSyncActivity's teardown: only reboot if this activity is
  // the one that brought Wi-Fi up. The silent restart clears heap fragmentation left by the TLS
  // session before the user is back at Home (silentRestart()'s target).
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void WeatherActivity::promptForLocation() {
  std::string initial;
  if (hasLocation) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.4f,%.4f", latitude, longitude);
    initial = buf;
  }

  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WEATHER_LOCATION_PROMPT), initial, 32),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          requestUpdate();
          return;
        }

        const auto& kb = std::get<KeyboardResult>(result.data);
        const size_t comma = kb.text.find(',');
        bool ok = false;
        double lat = 0.0;
        double lon = 0.0;
        if (comma != std::string::npos) {
          lat = strtod(kb.text.substr(0, comma).c_str(), nullptr);
          lon = strtod(kb.text.substr(comma + 1).c_str(), nullptr);
          ok = lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
        }

        if (!ok) {
          errorText = tr(STR_INVALID_LOCATION);
          state = hasLocation ? WeatherState::READY : WeatherState::NEED_LOCATION;
          requestUpdate();
          return;
        }

        // Upsert a fixed-name "Device" entry in the shared store, rather than a separate
        // standalone file — the web UI manages the rest of the list, this is just the one entry
        // the on-device keyboard can create.
        const auto& locations = WEATHER_STORE.getLocations();
        int deviceIdx = -1;
        for (size_t i = 0; i < locations.size(); i++) {
          if (locations[i].name == "Device") {
            deviceIdx = static_cast<int>(i);
            break;
          }
        }
        const WeatherLocation location{"Device", lat, lon};
        if (deviceIdx >= 0) {
          WEATHER_STORE.updateLocation(static_cast<size_t>(deviceIdx), location);
          WEATHER_STORE.setDefault(static_cast<size_t>(deviceIdx));
        } else {
          WEATHER_STORE.addLocation(location);
          WEATHER_STORE.setDefault(WEATHER_STORE.getCount() - 1);
        }

        latitude = lat;
        longitude = lon;
        hasLocation = true;
        errorText.clear();
        state = WeatherState::READY;
        requestUpdate();
      });
}

void WeatherActivity::startFetch() {
  state = WeatherState::FETCHING;
  errorText.clear();
  requestUpdate();

  if (WiFi.status() == WL_CONNECTED) {
    performFetch();
    return;
  }

  shouldTearDownWifiOnExit = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             state = WeatherState::ERROR;
                             errorText = tr(STR_WEATHER_ERROR);
                             requestUpdate();
                             return;
                           }
                           performFetch();
                         });
}

void WeatherActivity::performFetch() {
  if (insufficientHeap()) {
    state = WeatherState::ERROR;
    errorText = tr(STR_WEATHER_ERROR);
    requestUpdate();
    return;
  }

  char urlBuf[160];
  snprintf(urlBuf, sizeof(urlBuf),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true", latitude,
           longitude);

  freeink::SecureHttpClient http;
  http.setInsecure();
  bool success = false;

  if (http.begin(urlBuf)) {
    const int httpCode = http.GET();
    if (httpCode == 200) {
      JsonDocument doc;
      const DeserializationError error = deserializeJson(doc, http.getString().c_str());
      if (!error) {
        lastTemperature = doc["current_weather"]["temperature"].as<float>();
        lastWindspeed = doc["current_weather"]["windspeed"].as<float>();
        lastWeatherCode = doc["current_weather"]["weathercode"].as<int>();
        success = true;
      } else {
        LOG_ERR("WEATHER", "JSON parse failed: %s", error.c_str());
      }
    } else {
      LOG_ERR("WEATHER", "HTTP GET failed: %d", httpCode);
    }
    http.end();
  } else {
    LOG_ERR("WEATHER", "Bad URL: %s", urlBuf);
  }

  if (!success) {
    errorText = tr(STR_WEATHER_ERROR);
  }
  state = success ? WeatherState::RESULT : WeatherState::ERROR;
  requestUpdate();
}

void WeatherActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    onGoHome(HomeMenuItem::WEATHER);
    return;
  }

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (state == WeatherState::NEED_LOCATION) {
      promptForLocation();
    } else if (state != WeatherState::FETCHING) {
      startFetch();
    }
    return;
  }

  if (state != WeatherState::FETCHING && state != WeatherState::NEED_LOCATION &&
      mappedInput.isPressed(Button::Confirm) && mappedInput.getHeldTime() > ENTER_CHANGE_LOCATION_MS) {
    promptForLocation();
  }
}

void WeatherActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WEATHER));

  const int centerY = pageHeight / 2 - 40;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID) + 8;

  switch (state) {
    case WeatherState::NEED_LOCATION:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_WEATHER_LOCATION_PROMPT), true);
      break;
    case WeatherState::READY:
      renderer.drawCenteredText(UI_12_FONT_ID, centerY, tr(STR_CONNECT_AND_FETCH), true, EpdFontFamily::BOLD);
      if (!errorText.empty()) {
        renderer.drawCenteredText(UI_10_FONT_ID, centerY + lineHeight, errorText.c_str(), true);
      }
      break;
    case WeatherState::FETCHING:
      renderer.drawCenteredText(UI_12_FONT_ID, centerY, tr(STR_FETCHING_WEATHER), true, EpdFontFamily::BOLD);
      break;
    case WeatherState::RESULT: {
      char tempBuf[32];
      snprintf(tempBuf, sizeof(tempBuf), "%.0f C", lastTemperature);
      renderer.drawCenteredText(UI_12_FONT_ID, centerY, tempBuf, true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + lineHeight, weatherCodeLabel(lastWeatherCode), true);
      char windBuf[32];
      snprintf(windBuf, sizeof(windBuf), "%s %.0f km/h", tr(STR_WIND_LABEL), lastWindspeed);
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + lineHeight * 2, windBuf, true);
      break;
    }
    case WeatherState::ERROR:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY, errorText.c_str(), true);
      break;
  }

  if (state != WeatherState::NEED_LOCATION && state != WeatherState::FETCHING) {
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - metrics.buttonHintsHeight - 40, tr(STR_CHANGE_LOCATION_HINT),
                              true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
