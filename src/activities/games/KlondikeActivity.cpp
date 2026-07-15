#include "KlondikeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
std::string cardLabel(KlondikeCard c) {
  static const char* const kRankStr[14] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
  static const char kSuitChar[4] = {'S', 'H', 'D', 'C'};
  return std::string(kRankStr[c.rank]) + kSuitChar[c.suit];
}
}  // namespace

void KlondikeActivity::deal() {
  std::vector<KlondikeCard> full;
  full.reserve(52);
  for (uint8_t suit = 0; suit < 4; ++suit) {
    for (uint8_t rank = 1; rank <= 13; ++rank) {
      full.push_back(KlondikeCard{rank, suit});
    }
  }
  for (int i = static_cast<int>(full.size()) - 1; i > 0; --i) {
    const int j = random(i + 1);
    std::swap(full[i], full[j]);
  }

  int idx = 0;
  for (int col = 0; col < 7; ++col) {
    tableau[col].clear();
    for (int k = 0; k <= col; ++k) {
      tableau[col].push_back(full[static_cast<size_t>(idx++)]);
    }
    faceUpFrom[col] = col;  // only the last card (index == col) starts face-up
  }
  stock.assign(full.begin() + idx, full.end());
  waste.clear();
  for (int& r : foundationRank) r = 0;

  cursor = 0;
  pickedPile = -1;
  heldCount = 1;
  solved = false;
  message.clear();
}

void KlondikeActivity::onEnter() {
  Activity::onEnter();
  deal();
  requestUpdate();
}

bool KlondikeActivity::pileEmpty(int pile) const {
  if (pile == kStockPile) return stock.empty();
  if (pile == kWastePile) return waste.empty();
  if (pileIsFoundation(pile)) return foundationRank[foundationIndex(pile)] == 0;
  return tableau[tableauIndex(pile)].empty();
}

KlondikeCard KlondikeActivity::pileTopCard(int pile) const {
  if (pile == kStockPile) return stock.back();
  if (pile == kWastePile) return waste.back();
  if (pileIsFoundation(pile)) {
    const int suit = foundationIndex(pile);
    return KlondikeCard{static_cast<uint8_t>(foundationRank[suit]), static_cast<uint8_t>(suit)};
  }
  return tableau[tableauIndex(pile)].back();
}

int KlondikeActivity::pileFaceUpCount(int pile) const {
  if (pile == kWastePile) return waste.empty() ? 0 : 1;
  if (pileIsFoundation(pile)) return pileEmpty(pile) ? 0 : 1;
  if (pileIsTableau(pile)) {
    const int col = tableauIndex(pile);
    return static_cast<int>(tableau[col].size()) - faceUpFrom[col];
  }
  return 0;  // stock is never held from
}

bool KlondikeActivity::canDrop(int fromPile, int count, int toPile) const {
  if (toPile == fromPile) return false;  // handled as cancel by the caller, not a drop
  if (toPile == kStockPile || toPile == kWastePile) return false;

  KlondikeCard movingCard;
  if (count == 1) {
    movingCard = pileTopCard(fromPile);
  } else {
    if (!pileIsTableau(fromPile)) return false;  // sequences only ever come from a tableau column
    const auto& col = tableau[tableauIndex(fromPile)];
    movingCard = col[col.size() - static_cast<size_t>(count)];
  }

  if (pileIsFoundation(toPile)) {
    if (count > 1) return false;  // only single cards go to foundations
    const int suit = foundationIndex(toPile);
    return movingCard.suit == suit && movingCard.rank == foundationRank[suit] + 1;
  }

  // toPile is a tableau column
  const auto& destCol = tableau[tableauIndex(toPile)];
  if (destCol.empty()) {
    return movingCard.rank == 13;  // King only onto an empty column
  }
  const KlondikeCard destTop = destCol.back();
  return isRed(movingCard.suit) != isRed(destTop.suit) && movingCard.rank == destTop.rank - 1;
}

void KlondikeActivity::performMove(int fromPile, int count, int toPile) {
  std::vector<KlondikeCard> moving;
  if (fromPile == kWastePile) {
    moving.push_back(waste.back());
    waste.pop_back();
  } else if (pileIsFoundation(fromPile)) {
    const int suit = foundationIndex(fromPile);
    moving.push_back(KlondikeCard{static_cast<uint8_t>(foundationRank[suit]), static_cast<uint8_t>(suit)});
    foundationRank[suit]--;
  } else {
    const int col = tableauIndex(fromPile);
    auto& colVec = tableau[col];
    moving.assign(colVec.end() - count, colVec.end());
    colVec.resize(colVec.size() - static_cast<size_t>(count));
    if (faceUpFrom[col] >= static_cast<int>(colVec.size())) {
      faceUpFrom[col] = colVec.empty() ? 0 : static_cast<int>(colVec.size()) - 1;
    }
  }

  if (pileIsFoundation(toPile)) {
    foundationRank[foundationIndex(toPile)] = moving[0].rank;
  } else {
    auto& destCol = tableau[tableauIndex(toPile)];
    destCol.insert(destCol.end(), moving.begin(), moving.end());
  }
}

void KlondikeActivity::draw() {
  if (!stock.empty()) {
    waste.push_back(stock.back());
    stock.pop_back();
  } else if (!waste.empty()) {
    stock.assign(waste.rbegin(), waste.rend());
    waste.clear();
  }
}

void KlondikeActivity::pickup() {
  if (cursor == kStockPile) {
    draw();
    return;
  }
  if (pileEmpty(cursor)) return;
  pickedPile = cursor;
  heldCount = 1;
}

void KlondikeActivity::attemptDrop(int target) {
  if (target == pickedPile) {
    pickedPile = -1;
    heldCount = 1;
    message.clear();
    return;
  }
  if (!canDrop(pickedPile, heldCount, target)) {
    message = tr(STR_INVALID_MOVE);
    return;
  }
  performMove(pickedPile, heldCount, target);
  pickedPile = -1;
  heldCount = 1;
  message.clear();
  checkSolved();
}

void KlondikeActivity::checkSolved() {
  solved = foundationRank[0] == 13 && foundationRank[1] == 13 && foundationRank[2] == 13 && foundationRank[3] == 13;
}

void KlondikeActivity::loop() {
  using Button = MappedInputManager::Button;

  if (solved) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      deal();
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(Button::Back)) {
      onGoHome(HomeMenuItem::GAMES);
    }
    return;
  }

  if (mappedInput.wasReleased(Button::Back)) {
    if (pickedPile != -1) {
      pickedPile = -1;
      heldCount = 1;
      message.clear();
      requestUpdate();
    } else {
      onGoHome(HomeMenuItem::GAMES);
    }
    return;
  }

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (pickedPile == -1) {
      pickup();
    } else {
      attemptDrop(cursor);
    }
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
    cursor = (cursor - 1 + kPileCount) % kPileCount;
    message.clear();
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
    cursor = (cursor + 1) % kPileCount;
    message.clear();
    requestUpdate();
  });

  if (pickedPile != -1) {
    buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
      if (heldCount < pileFaceUpCount(pickedPile)) {
        heldCount++;
        requestUpdate();
      }
    });
    buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
      if (heldCount > 1) {
        heldCount--;
        requestUpdate();
      }
    });
  } else {
    buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
      cursor = (cursor - 1 + kPileCount) % kPileCount;
      requestUpdate();
    });
    buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
      cursor = (cursor + 1) % kPileCount;
      requestUpdate();
    });
  }
}

void KlondikeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_KLONDIKE));

  const int margin = metrics.contentSidePadding;
  const int colWidth = (pageWidth - margin * 2) / 7;
  const int cardWidth = colWidth - 6;
  const int cardHeight = 36;
  const int topRowY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  const auto drawBox = [&](int x, int y, int h, bool held, bool cursorHere) {
    if (held) {
      renderer.fillRect(x, y, cardWidth, h, true);
    } else {
      renderer.fillRect(x, y, cardWidth, h, false);
      renderer.drawRect(x, y, cardWidth, h, cursorHere ? 2 : 1, true);
    }
  };

  // Top row: Stock, Waste, [gap], Foundation x4 (S,H,D,C)
  constexpr int kTopSlotForPile[6] = {0, 1, 3, 4, 5, 6};
  for (int pile = 0; pile <= 5; ++pile) {
    const int x = margin + kTopSlotForPile[pile] * colWidth;
    const bool isCursor = cursor == pile;
    const bool isHeld = pickedPile == pile;
    drawBox(x, topRowY, cardHeight, isHeld, isCursor);

    if (pile == kStockPile) {
      const std::string countText = std::to_string(stock.size());
      const int tw = renderer.getTextWidth(UI_10_FONT_ID, countText.c_str());
      renderer.drawText(UI_10_FONT_ID, x + (cardWidth - tw) / 2, topRowY + (cardHeight - lineHeight) / 2,
                        countText.c_str(), true);
    } else if (!pileEmpty(pile)) {
      const std::string label = cardLabel(pileTopCard(pile));
      const int tw = renderer.getTextWidth(UI_10_FONT_ID, label.c_str());
      renderer.drawText(UI_10_FONT_ID, x + (cardWidth - tw) / 2, topRowY + (cardHeight - lineHeight) / 2, label.c_str(),
                        !isHeld);
    }
  }

  // Tableau
  const int tableauTop = topRowY + cardHeight + metrics.verticalSpacing * 2;
  constexpr int faceDownHeight = 10;
  constexpr int faceUpOffset = 22;

  for (int col = 0; col < 7; ++col) {
    const int pile = KlondikeActivity::kTableauStart + col;
    const int x = margin + col * colWidth;
    const auto& colCards = tableau[col];
    const bool colCursor = cursor == pile;
    int y = tableauTop;

    if (colCards.empty()) {
      drawBox(x, tableauTop, cardHeight, false, colCursor);
      continue;
    }

    for (int i = 0; i < static_cast<int>(colCards.size()); ++i) {
      const bool faceUp = i >= faceUpFrom[col];
      const bool isTopCard = i == static_cast<int>(colCards.size()) - 1;
      const bool isHeld = pickedPile == pile && i >= static_cast<int>(colCards.size()) - heldCount;

      if (!faceUp) {
        renderer.drawRect(x, y, cardWidth, faceDownHeight, 1, true);
        y += faceDownHeight;
        continue;
      }

      const int thisHeight = isTopCard ? cardHeight : faceUpOffset;
      drawBox(x, y, thisHeight, isHeld, isTopCard && colCursor);
      const std::string label = cardLabel(colCards[static_cast<size_t>(i)]);
      renderer.drawText(UI_10_FONT_ID, x + 4, y + 3, label.c_str(), !isHeld);
      y += faceUpOffset;
    }
  }

  if (solved) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight - metrics.buttonHintsHeight - 60, tr(STR_SOLVED), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - metrics.buttonHintsHeight - 30, tr(STR_NEW_GAME), true);
  } else if (!message.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - metrics.buttonHintsHeight - 30, message.c_str(), true);
  }

  const char* confirmLabel = pickedPile == -1 ? tr(STR_SELECT) : tr(STR_DROP);
  const char* backLabel = pickedPile == -1 ? tr(STR_BACK) : tr(STR_CANCEL);
  const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, tr(STR_DIR_UP), tr(STR_DIR_DOWN));

  renderer.displayBuffer();
}
