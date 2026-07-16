#include "FiveCrownsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
std::string cardLabel(fivecrowns::Card c) {
  if (c.isJoker()) return "JOK";
  static const char* const kRankStr[14] = {"", "", "", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
  static const char kSuitChar[5] = {'T', 'H', 'C', 'S', 'D'};  // Stars='T', Hearts/Clubs/Spades/Diamonds
  return std::string(kRankStr[c.rank]) + kSuitChar[c.suit];
}

std::string wildRankLabel(uint8_t wildRank) {
  static const char* const kRankStr[14] = {"", "", "", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
  return std::string(kRankStr[wildRank]) + "s";
}
}  // namespace

void FiveCrownsActivity::startMatch() {
  handNumber = 1;
  human.score = 0;
  ai.score = 0;
  dealHand();
}

void FiveCrownsActivity::dealHand() {
  std::vector<fivecrowns::Card> full;
  full.reserve(116);
  for (int deckNum = 0; deckNum < 2; ++deckNum) {
    for (uint8_t suit = 0; suit < 5; ++suit) {
      for (uint8_t rank = 3; rank <= 13; ++rank) full.push_back(fivecrowns::Card{rank, suit});
    }
    for (int j = 0; j < 3; ++j) full.push_back(fivecrowns::Card{0, 0});
  }
  for (int i = static_cast<int>(full.size()) - 1; i > 0; --i) {
    const int j = random(i + 1);
    std::swap(full[i], full[j]);
  }

  const int size = handSize();
  int idx = 0;
  human.hand.assign(full.begin() + idx, full.begin() + idx + size);
  idx += size;
  ai.hand.assign(full.begin() + idx, full.begin() + idx + size);
  idx += size;
  discard.clear();
  discard.push_back(full[static_cast<size_t>(idx++)]);
  stock.assign(full.begin() + idx, full.end());

  goneOutPlayer = -1;
  turnOwner = 0;
  phase = TurnPhase::ChooseDrawSource;
  drawCursor = 0;
  handCursor = 0;
  message.clear();
  refreshHumanMelds();
}

void FiveCrownsActivity::refreshHumanMelds() { humanMelds = fivecrowns::findBestMelds(human.hand, wildRank()); }

bool FiveCrownsActivity::canGoOutDiscarding(const std::vector<fivecrowns::Card>& hand, int excludeIndex) const {
  if (excludeIndex < 0 || excludeIndex >= static_cast<int>(hand.size())) return false;
  std::vector<fivecrowns::Card> trial = hand;
  trial.erase(trial.begin() + excludeIndex);
  return fivecrowns::findBestMelds(trial, wildRank()).deadwood == 0;
}

void FiveCrownsActivity::humanDraw(bool fromDiscard) {
  if (fromDiscard) {
    if (discard.empty()) return;
    human.hand.push_back(discard.back());
    discard.pop_back();
  } else {
    if (stock.empty()) {
      if (discard.size() <= 1) return;  // nothing left to reshuffle into a new stock
      const fivecrowns::Card top = discard.back();
      discard.pop_back();
      stock = discard;
      discard.clear();
      discard.push_back(top);
      for (int i = static_cast<int>(stock.size()) - 1; i > 0; --i) {
        const int j = random(i + 1);
        std::swap(stock[i], stock[j]);
      }
    }
    if (stock.empty()) return;
    human.hand.push_back(stock.back());
    stock.pop_back();
  }
  handCursor = 0;
  phase = TurnPhase::ChooseDiscard;
  message.clear();
  refreshHumanMelds();
}

void FiveCrownsActivity::humanDiscard(int index) {
  if (index < 0 || index >= static_cast<int>(human.hand.size())) return;
  const fivecrowns::Card card = human.hand[static_cast<size_t>(index)];
  human.hand.erase(human.hand.begin() + index);
  discard.push_back(card);
  refreshHumanMelds();
  advanceAfterDiscard(0);
}

void FiveCrownsActivity::goOut(int owner, int discardIndex) {
  Player& p = playerFor(owner);
  const fivecrowns::Card card = p.hand[static_cast<size_t>(discardIndex)];
  p.hand.erase(p.hand.begin() + discardIndex);
  discard.push_back(card);
  if (owner == 0) refreshHumanMelds();

  if (goneOutPlayer == -1) {
    goneOutPlayer = owner;
    turnOwner = 1 - owner;
    phase = TurnPhase::FinalLap;
    message = owner == 0 ? tr(STR_YOU_WENT_OUT) : tr(STR_CPU_WENT_OUT);
  } else {
    // Second player to complete their hand during the final lap — score immediately rather than
    // starting another final-lap cycle.
    scoreHandAndAdvance();
  }
}

void FiveCrownsActivity::aiTakeTurn() {
  const uint8_t wr = wildRank();

  bool preferDiscard = false;
  if (!discard.empty()) {
    std::vector<fivecrowns::Card> withDiscard = ai.hand;
    withDiscard.push_back(discard.back());
    const int deadwoodWith = fivecrowns::findBestMelds(withDiscard, wr).deadwood;
    const int deadwoodWithout = fivecrowns::findBestMelds(ai.hand, wr).deadwood;
    preferDiscard = deadwoodWith < deadwoodWithout;
  }

  if (preferDiscard) {
    ai.hand.push_back(discard.back());
    discard.pop_back();
  } else {
    if (stock.empty() && discard.size() > 1) {
      const fivecrowns::Card top = discard.back();
      discard.pop_back();
      stock = discard;
      discard.clear();
      discard.push_back(top);
      for (int i = static_cast<int>(stock.size()) - 1; i > 0; --i) {
        const int j = random(i + 1);
        std::swap(stock[i], stock[j]);
      }
    }
    if (!stock.empty()) {
      ai.hand.push_back(stock.back());
      stock.pop_back();
    }
  }

  // Discard whichever card leaves the lowest deadwood; ties favor discarding the higher
  // point-value leftover card so the human doesn't get handed a cheap, useful discard.
  int bestIndex = 0;
  int bestDeadwood = -1;
  uint8_t bestDiscardValue = 0;
  for (int i = 0; i < static_cast<int>(ai.hand.size()); ++i) {
    std::vector<fivecrowns::Card> trial = ai.hand;
    const fivecrowns::Card removed = trial[static_cast<size_t>(i)];
    trial.erase(trial.begin() + i);
    const int deadwood = fivecrowns::findBestMelds(trial, wr).deadwood;
    const uint8_t removedValue = fivecrowns::cardPointValue(removed, wr);
    if (bestDeadwood < 0 || deadwood < bestDeadwood || (deadwood == bestDeadwood && removedValue > bestDiscardValue)) {
      bestDeadwood = deadwood;
      bestIndex = i;
      bestDiscardValue = removedValue;
    }
  }

  const fivecrowns::Card discardedCard = ai.hand[static_cast<size_t>(bestIndex)];
  char buf[64];
  if (bestDeadwood == 0) {
    goOut(1, bestIndex);
    snprintf(buf, sizeof(buf), tr(STR_CPU_WENT_OUT_FORMAT), cardLabel(discardedCard).c_str());
  } else {
    ai.hand.erase(ai.hand.begin() + bestIndex);
    discard.push_back(discardedCard);
    snprintf(buf, sizeof(buf), tr(STR_CPU_DISCARDED_FORMAT), cardLabel(discardedCard).c_str());
  }
  message = buf;
}

void FiveCrownsActivity::advanceAfterDiscard(int owner) {
  if (goneOutPlayer != -1) {
    scoreHandAndAdvance();
    return;
  }
  turnOwner = 1 - owner;
  message.clear();
  if (turnOwner == 0) {
    phase = TurnPhase::ChooseDrawSource;
    refreshHumanMelds();
  } else {
    phase = TurnPhase::OpponentTurn;
  }
}

void FiveCrownsActivity::scoreHandAndAdvance() {
  const auto scoreOf = [this](int owner) {
    if (owner == goneOutPlayer) return 0;
    return fivecrowns::findBestMelds(playerFor(owner).hand, wildRank()).deadwood;
  };
  lastHandHumanScore = scoreOf(0);
  lastHandAiScore = scoreOf(1);
  human.score += lastHandHumanScore;
  ai.score += lastHandAiScore;
  phase = TurnPhase::HandScoreSummary;
}

void FiveCrownsActivity::onEnter() {
  Activity::onEnter();
  startMatch();
  requestUpdate();
}

void FiveCrownsActivity::loop() {
  using Button = MappedInputManager::Button;

  switch (phase) {
    case TurnPhase::ChooseDrawSource: {
      if (mappedInput.wasReleased(Button::Back)) {
        onGoHome(HomeMenuItem::GAMES);
        return;
      }
      if (mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Right)) {
        drawCursor = 1 - drawCursor;
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(Button::Confirm)) {
        humanDraw(drawCursor == 1);
        requestUpdate();
        return;
      }
      break;
    }
    case TurnPhase::ChooseDiscard: {
      if (mappedInput.wasReleased(Button::Back)) {
        onGoHome(HomeMenuItem::GAMES);
        return;
      }
      if (mappedInput.wasReleased(Button::Confirm)) {
        if (canGoOutDiscarding(human.hand, handCursor)) {
          phase = TurnPhase::ConfirmGoOut;
        } else {
          humanDiscard(handCursor);
        }
        requestUpdate();
        return;
      }
      buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
        const int count = static_cast<int>(human.hand.size());
        handCursor = (handCursor - 1 + count) % count;
        requestUpdate();
      });
      buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
        const int count = static_cast<int>(human.hand.size());
        handCursor = (handCursor + 1) % count;
        requestUpdate();
      });
      break;
    }
    case TurnPhase::ConfirmGoOut: {
      if (mappedInput.wasReleased(Button::Confirm)) {
        goOut(0, handCursor);
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(Button::Back)) {
        phase = TurnPhase::ChooseDiscard;
        message.clear();
        requestUpdate();
        return;
      }
      break;
    }
    case TurnPhase::OpponentTurn: {
      if (message.empty()) {
        aiTakeTurn();
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(Button::Confirm) || mappedInput.wasReleased(Button::Back)) {
        advanceAfterDiscard(1);
        requestUpdate();
        return;
      }
      break;
    }
    case TurnPhase::FinalLap: {
      if (mappedInput.wasReleased(Button::Confirm) || mappedInput.wasReleased(Button::Back)) {
        message.clear();
        if (turnOwner == 0) {
          phase = TurnPhase::ChooseDrawSource;
          refreshHumanMelds();
        } else {
          phase = TurnPhase::OpponentTurn;
        }
        requestUpdate();
        return;
      }
      break;
    }
    case TurnPhase::HandScoreSummary: {
      if (mappedInput.wasReleased(Button::Back)) {
        onGoHome(HomeMenuItem::GAMES);
        return;
      }
      if (mappedInput.wasReleased(Button::Confirm)) {
        if (handNumber < 11) {
          handNumber++;
          dealHand();
        } else {
          phase = TurnPhase::MatchOver;
        }
        requestUpdate();
        return;
      }
      break;
    }
    case TurnPhase::MatchOver: {
      if (mappedInput.wasReleased(Button::Back)) {
        onGoHome(HomeMenuItem::GAMES);
        return;
      }
      if (mappedInput.wasReleased(Button::Confirm)) {
        startMatch();
        requestUpdate();
        return;
      }
      break;
    }
  }
}

void FiveCrownsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.clearScreen();

  char subtitle[48];
  snprintf(subtitle, sizeof(subtitle), "Hand %d/11 - Wild: %s", handNumber, wildRankLabel(wildRank()).c_str());
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FIVE_CROWNS), subtitle);

  const int margin = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  char scoreLine[64];
  snprintf(scoreLine, sizeof(scoreLine), "You: %d   CPU: %d (%d cards)", human.score, ai.score,
           static_cast<int>(ai.hand.size()));
  renderer.drawText(UI_10_FONT_ID, margin, y, scoreLine, true);
  y += lineHeight + metrics.verticalSpacing;

  // Discard + stock row
  const int pileBoxWidth = 90;
  const int pileBoxHeight = 36;
  const auto drawPileBox = [&](int x, bool isCursor, const std::string& label) {
    if (isCursor) {
      renderer.fillRect(x, y, pileBoxWidth, pileBoxHeight, true);
    } else {
      renderer.fillRect(x, y, pileBoxWidth, pileBoxHeight, false);
      renderer.drawRect(x, y, pileBoxWidth, pileBoxHeight, 1, true);
    }
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, label.c_str());
    renderer.drawText(UI_10_FONT_ID, x + (pileBoxWidth - tw) / 2, y + (pileBoxHeight - lineHeight) / 2, label.c_str(),
                      !isCursor);
  };
  const bool choosingDraw = phase == TurnPhase::ChooseDrawSource;
  char stockLabel[16];
  snprintf(stockLabel, sizeof(stockLabel), "Stock:%d", static_cast<int>(stock.size()));
  drawPileBox(margin, choosingDraw && drawCursor == 0, stockLabel);
  const std::string discardLabel = discard.empty() ? "-" : cardLabel(discard.back());
  drawPileBox(margin + pileBoxWidth + metrics.verticalSpacing, choosingDraw && drawCursor == 1, "Pile:" + discardLabel);
  y += pileBoxHeight + metrics.verticalSpacing * 2;

  // Hand row
  const int handCount = static_cast<int>(human.hand.size());
  if (handCount > 0) {
    const int colWidth = (pageWidth - margin * 2) / handCount;
    const int cardWidth = colWidth - 6;
    const int cardHeight = 36;
    const bool choosingDiscard = phase == TurnPhase::ChooseDiscard || phase == TurnPhase::ConfirmGoOut;
    for (int i = 0; i < handCount; ++i) {
      const int x = margin + i * colWidth;
      const bool isCursor = choosingDiscard && i == handCursor;
      const int groupId =
          (i < static_cast<int>(humanMelds.groupId.size())) ? humanMelds.groupId[static_cast<size_t>(i)] : -1;
      if (isCursor) {
        renderer.fillRect(x, y, cardWidth, cardHeight, true);
      } else {
        renderer.fillRect(x, y, cardWidth, cardHeight, false);
        renderer.drawRect(x, y, cardWidth, cardHeight, groupId >= 0 ? 2 : 1, true);
      }
      const std::string label = cardLabel(human.hand[static_cast<size_t>(i)]);
      const int tw = renderer.getTextWidth(UI_10_FONT_ID, label.c_str());
      renderer.drawText(UI_10_FONT_ID, x + (cardWidth - tw) / 2, y + (cardHeight - lineHeight) / 2, label.c_str(),
                        !isCursor);
      if (groupId >= 0 && !isCursor) {
        const char digit[2] = {static_cast<char>('1' + (groupId % 9)), '\0'};
        renderer.drawText(UI_10_FONT_ID, x + 2, y + 2, digit, true);
      }
    }
    y += cardHeight + metrics.verticalSpacing;
  }

  if (phase == TurnPhase::ConfirmGoOut) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_GO_OUT_PROMPT), true);
  } else if (phase == TurnPhase::HandScoreSummary) {
    char line1[48];
    snprintf(line1, sizeof(line1), "This hand — You: +%d  CPU: +%d", lastHandHumanScore, lastHandAiScore);
    renderer.drawCenteredText(UI_10_FONT_ID, y, line1, true);
    char line2[48];
    snprintf(line2, sizeof(line2), "Total — You: %d  CPU: %d", human.score, ai.score);
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineHeight + 4, line2, true);
  } else if (phase == TurnPhase::MatchOver) {
    const char* resultLabel =
        human.score == ai.score ? tr(STR_TIE_GAME) : (human.score < ai.score ? tr(STR_YOU_WIN) : tr(STR_CPU_WINS));
    renderer.drawCenteredText(UI_12_FONT_ID, y, resultLabel, true, EpdFontFamily::BOLD);
    char line2[48];
    snprintf(line2, sizeof(line2), "Final — You: %d  CPU: %d", human.score, ai.score);
    renderer.drawCenteredText(UI_10_FONT_ID, y + lineHeight + 8, line2, true);
  } else if (!message.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, message.c_str(), true);
  }

  const char* confirmLabel = tr(STR_SELECT);
  const char* backLabel = tr(STR_BACK);
  switch (phase) {
    case TurnPhase::ChooseDrawSource:
      confirmLabel = tr(STR_DRAW);
      break;
    case TurnPhase::ChooseDiscard:
      confirmLabel = tr(STR_DISCARD);
      break;
    case TurnPhase::ConfirmGoOut:
      confirmLabel = tr(STR_GO_OUT);
      backLabel = tr(STR_CANCEL);
      break;
    case TurnPhase::OpponentTurn:
    case TurnPhase::FinalLap:
      confirmLabel = tr(STR_SELECT);
      break;
    case TurnPhase::HandScoreSummary:
      confirmLabel = tr(STR_SELECT);
      break;
    case TurnPhase::MatchOver:
      confirmLabel = tr(STR_NEW_GAME);
      break;
  }

  if (phase == TurnPhase::ChooseDrawSource || phase == TurnPhase::ChooseDiscard) {
    const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
