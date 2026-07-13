#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct TodoItem {
  std::string text;
  bool done = false;
};

// Singleton class for storing the To-Do list on the SD card.
class TodoStore : public PersistableStore<TodoStore> {
 private:
  std::vector<TodoItem> items;

  static constexpr size_t MAX_ITEMS = 50;

  TodoStore() = default;

  friend class PersistableStore<TodoStore>;

 public:
  static const char* getFilePath() { return "/.tinyrdr/todo.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool addItem(const std::string& text);
  bool removeItem(size_t index);
  bool toggleItem(size_t index);

  const std::vector<TodoItem>& getItems() const { return items; }
  size_t getCount() const { return items.size(); }
};

#define TODO_STORE TodoStore::getInstance()
