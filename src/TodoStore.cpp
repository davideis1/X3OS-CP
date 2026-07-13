#include "TodoStore.h"

#include <Logging.h>

#include <algorithm>
#include <utility>

void TodoStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["items"].to<JsonArray>();
  for (const auto& item : items) {
    JsonObject obj = arr.add<JsonObject>();
    obj["text"] = item.text;
    obj["done"] = item.done;
  }
}

bool TodoStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'items' key (treat as empty list); only a JSON parse error is fatal.
  items.clear();
  JsonArrayConst arr = doc["items"].as<JsonArrayConst>();
  items.reserve(std::min(arr.size(), MAX_ITEMS));

  for (JsonObjectConst obj : arr) {
    if (items.size() >= TodoStore::MAX_ITEMS) break;
    TodoItem item;
    item.text = obj["text"] | "";
    item.done = obj["done"] | false;
    items.push_back(std::move(item));
  }

  LOG_DBG("TODO", "Loaded %zu to-do items from file", items.size());
  return true;
}

bool TodoStore::addItem(const std::string& text) {
  if (items.size() >= MAX_ITEMS) {
    LOG_DBG("TODO", "Cannot add more items, limit of %zu reached", MAX_ITEMS);
    return false;
  }

  items.push_back(TodoItem{text, false});
  LOG_DBG("TODO", "Added item: %s", text.c_str());
  return saveToFile();
}

bool TodoStore::removeItem(size_t index) {
  if (index >= items.size()) {
    return false;
  }

  items.erase(items.begin() + static_cast<ptrdiff_t>(index));
  LOG_DBG("TODO", "Removed item at index %zu", index);
  return saveToFile();
}

bool TodoStore::toggleItem(size_t index) {
  if (index >= items.size()) {
    return false;
  }

  items[index].done = !items[index].done;
  return saveToFile();
}
