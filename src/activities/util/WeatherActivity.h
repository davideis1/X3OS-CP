#pragma once

#include <string>

#include "activities/Activity.h"

enum class WeatherState { NEED_LOCATION, READY, FETCHING, RESULT, ERROR };

// Fetched on-demand: join Wi-Fi (reusing WifiSelectionActivity, same as every other network
// feature in this codebase), one HTTPS GET to Open-Meteo (no API key needed), show current
// conditions. No caching across boots beyond the saved location. Locations are managed in
// WeatherLocationStore (shared with the web UI's location-management page); this activity always
// fetches for whichever one is marked default there.
class WeatherActivity final : public Activity {
  WeatherState state = WeatherState::NEED_LOCATION;
  bool shouldTearDownWifiOnExit = false;

  double latitude = 0.0;
  double longitude = 0.0;
  bool hasLocation = false;

  float lastTemperature = 0.0f;
  float lastWindspeed = 0.0f;
  int lastWeatherCode = 0;
  std::string errorText;  // set for ERROR state and the transient "invalid location" flash

  void promptForLocation();
  void startFetch();
  void performFetch();  // blocking HTTPS GET + JSON parse; called once Wi-Fi is connected

 public:
  explicit WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Weather", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
