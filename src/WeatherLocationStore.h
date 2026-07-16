#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct WeatherLocation {
  std::string name;
  double lat = 0.0;
  double lon = 0.0;
};

// Singleton class for storing named weather locations on the SD card. Shared between the
// on-device WeatherActivity and the web UI's location-management page — one location is marked
// "default" and is what WeatherActivity fetches for.
class WeatherLocationStore : public PersistableStore<WeatherLocationStore> {
 private:
  std::vector<WeatherLocation> locations;
  int defaultIndex = -1;

  static constexpr size_t MAX_LOCATIONS = 10;

  WeatherLocationStore() = default;

  friend class PersistableStore<WeatherLocationStore>;

 public:
  static const char* getFilePath() { return "/.tinyrdr/weather.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool addLocation(const WeatherLocation& location);
  bool updateLocation(size_t index, const WeatherLocation& location);
  bool removeLocation(size_t index);
  void setDefault(size_t index);

  const std::vector<WeatherLocation>& getLocations() const { return locations; }
  const WeatherLocation* getDefault() const;
  int getDefaultIndex() const { return defaultIndex; }
  size_t getCount() const { return locations.size(); }
};

#define WEATHER_STORE WeatherLocationStore::getInstance()
