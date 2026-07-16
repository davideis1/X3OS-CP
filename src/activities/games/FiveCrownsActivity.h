#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "FiveCrownsMelds.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Five Crowns vs. 1 AI opponent, full 11-hand match (hand N deals N+2 cards, wild rank rotates
// 3s..Kings). Each turn: draw (stock or discard) then discard one card, or go out once the
// computed grouping covers the whole hand minus the discard. The human's hand is auto-grouped
// into books/runs each turn via FiveCrownsMelds (see FiveCrownsMelds.h) rather than requiring
// manual drag-and-drop meld-building.
class FiveCrownsActivity final : public Activity {
 public:
  enum class TurnPhase {
    ChooseDrawSource,
    ChooseDiscard,
    ConfirmGoOut,
    OpponentTurn,
    FinalLap,
    HandScoreSummary,
    MatchOver
  };

 private:
  struct Player {
    std::vector<fivecrowns::Card> hand;
    int score = 0;
  };

  ButtonNavigator buttonNavigator;

  Player human;
  Player ai;
  std::vector<fivecrowns::Card> stock;
  std::vector<fivecrowns::Card> discard;  // .back() = top of pile
  int handNumber = 1;                     // 1..11
  int turnOwner = 0;                      // 0 = human, 1 = ai
  TurnPhase phase = TurnPhase::ChooseDrawSource;
  int goneOutPlayer = -1;  // -1 = nobody yet this hand
  int drawCursor = 0;      // 0 = stock, 1 = discard (ChooseDrawSource phase)
  int handCursor = 0;      // linear index over the active player's hand (ChooseDiscard phase)
  fivecrowns::MeldResult humanMelds;
  std::string message;         // transient flash, cleared on the next input
  std::string opponentFlash;   // what the AI just did, shown during OpponentTurn
  int lastHandHumanScore = 0;  // deltas shown on the HandScoreSummary screen
  int lastHandAiScore = 0;

  [[nodiscard]] uint8_t wildRank() const { return static_cast<uint8_t>(handNumber + 2); }
  [[nodiscard]] int handSize() const { return handNumber + 2; }
  Player& playerFor(int owner) { return owner == 0 ? human : ai; }

  void startMatch();
  void dealHand();
  void refreshHumanMelds();
  // True if `hand` minus the card at `excludeIndex` is fully covered by humanMelds/aiMelds-style
  // grouping (i.e. going out is legal after discarding that card).
  bool canGoOutDiscarding(const std::vector<fivecrowns::Card>& hand, int excludeIndex) const;

  void humanDraw(bool fromDiscard);
  void humanDiscard(int index);
  void goOut(int owner, int discardIndex);
  void aiTakeTurn();
  void advanceAfterDiscard(int owner);
  void scoreHandAndAdvance();

 public:
  explicit FiveCrownsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FiveCrowns", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
