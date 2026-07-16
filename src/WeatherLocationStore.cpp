#include "WeatherLocationStore.h"

#include <Logging.h>

#include <algorithm>

void WeatherLocationStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["locations"].to<JsonArray>();
  for (const auto& location : locations) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = location.name;
    obj["lat"] = location.lat;
    obj["lon"] = location.lon;
  }
  doc["defaultIndex"] = defaultIndex;
}

bool WeatherLocationStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'locations' key (treat as empty list); only a JSON parse error is
  // fatal.
  locations.clear();
  JsonArrayConst arr = doc["locations"].as<JsonArrayConst>();
  locations.reserve(std::min(arr.size(), MAX_LOCATIONS));

  for (JsonObjectConst obj : arr) {
    if (locations.size() >= WeatherLocationStore::MAX_LOCATIONS) break;
    WeatherLocation location;
    location.name = obj["name"] | "";
    location.lat = obj["lat"] | 0.0;
    location.lon = obj["lon"] | 0.0;
    locations.push_back(std::move(location));
  }

  defaultIndex = doc["defaultIndex"] | -1;
  if (defaultIndex < 0 || defaultIndex >= static_cast<int>(locations.size())) {
    defaultIndex = -1;
  }

  LOG_DBG("WXLOC", "Loaded %zu weather location(s) from file, default=%d", locations.size(), defaultIndex);
  return true;
}

bool WeatherLocationStore::addLocation(const WeatherLocation& location) {
  if (locations.size() >= MAX_LOCATIONS) {
    LOG_DBG("WXLOC", "Cannot add more locations, limit of %zu reached", MAX_LOCATIONS);
    return false;
  }

  locations.push_back(location);
  if (defaultIndex < 0) {
    defaultIndex = static_cast<int>(locations.size()) - 1;
  }
  LOG_DBG("WXLOC", "Added location: %s", location.name.c_str());
  return saveToFile();
}

bool WeatherLocationStore::updateLocation(size_t index, const WeatherLocation& location) {
  if (index >= locations.size()) {
    return false;
  }

  locations[index] = location;
  LOG_DBG("WXLOC", "Updated location at index %zu: %s", index, location.name.c_str());
  return saveToFile();
}

bool WeatherLocationStore::removeLocation(size_t index) {
  if (index >= locations.size()) {
    return false;
  }

  locations.erase(locations.begin() + static_cast<ptrdiff_t>(index));

  const int removedIdx = static_cast<int>(index);
  if (defaultIndex == removedIdx) {
    defaultIndex = -1;
  } else if (defaultIndex > removedIdx) {
    defaultIndex--;
  }

  LOG_DBG("WXLOC", "Removed location at index %zu", index);
  return saveToFile();
}

void WeatherLocationStore::setDefault(size_t index) {
  if (index >= locations.size()) {
    return;
  }
  defaultIndex = static_cast<int>(index);
  saveToFile();
}

const WeatherLocation* WeatherLocationStore::getDefault() const {
  if (defaultIndex < 0 || defaultIndex >= static_cast<int>(locations.size())) {
    return nullptr;
  }
  return &locations[static_cast<size_t>(defaultIndex)];
}
