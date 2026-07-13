#include "DiagnosticsActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
std::string formatUptime(unsigned long seconds) {
  const unsigned long hours = seconds / 3600;
  const unsigned long minutes = (seconds % 3600) / 60;
  char buf[24];
  snprintf(buf, sizeof(buf), "%luh %lum", hours, minutes);
  return buf;
}

std::string formatGigabytes(uint64_t usedBytes, uint64_t totalBytes) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f / %.1f GB", usedBytes / 1073741824.0, totalBytes / 1073741824.0);
  return buf;
}
}  // namespace

void DiagnosticsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void DiagnosticsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome(HomeMenuItem::TOOLS);
  }
}

void DiagnosticsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DIAGNOSTICS));

  char heapBuf[16];
  snprintf(heapBuf, sizeof(heapBuf), "%u KB", static_cast<unsigned>(ESP.getFreeHeap() / 1024));
  char battBuf[8];
  snprintf(battBuf, sizeof(battBuf), "%u%%", powerManager.getBatteryPercentage());

  const std::vector<std::pair<const char*, std::string>> rows = {
      {tr(STR_DIAG_BATTERY), battBuf},
      {tr(STR_DIAG_FREE_HEAP), heapBuf},
      {tr(STR_DIAG_UPTIME), formatUptime(millis() / 1000)},
      {tr(STR_DIAG_SD_USED), formatGigabytes(Storage.getUsedBytes(), Storage.getTotalBytes())},
      {tr(STR_DIAG_FIRMWARE), TINYRDR_VERSION},
  };

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(rows.size()), -1,
      [&rows](int index) { return std::string(rows[index].first); }, nullptr, nullptr,
      [&rows](int index) { return rows[index].second; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
