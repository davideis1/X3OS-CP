#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct KlondikeCard {
  uint8_t rank;  // 1-13 (A=1 ... J=11, Q=12, K=13)
  uint8_t suit;  // 0=Spades, 1=Hearts, 2=Diamonds, 3=Clubs
};

// Full-rules Klondike, including multi-card sequence moves. 13 piles addressed by a flat index
// (see kStockPile/kWastePile/kFoundationStart/kTableauStart): the cursor cycles through all of
// them with Left/Right (and Up/Down, when nothing is held). Confirm on a pile with nothing held
// picks up its top card; Up/Down while holding grows/shrinks the held group deeper into a
// tableau column's face-up run (single cards only from waste/foundations); Confirm again on a
// different pile attempts the drop.
class KlondikeActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  std::vector<KlondikeCard> stock;
  std::vector<KlondikeCard> waste;
  int foundationRank[4] = {0, 0, 0, 0};
  std::vector<KlondikeCard> tableau[7];
  int faceUpFrom[7] = {0, 0, 0, 0, 0, 0, 0};

  int cursor = 0;
  int pickedPile = -1;  // -1 = nothing held
  int heldCount = 1;
  bool solved = false;
  std::string message;  // transient "Invalid move" flash, cleared on the next input

  void deal();
  static bool isRed(uint8_t suit) { return suit == 1 || suit == 2; }
  static bool pileIsTableau(int pile) { return pile >= kTableauStart; }
  static bool pileIsFoundation(int pile) { return pile >= kFoundationStart && pile < kTableauStart; }
  static int tableauIndex(int pile) { return pile - kTableauStart; }
  static int foundationIndex(int pile) { return pile - kFoundationStart; }
  bool pileEmpty(int pile) const;
  KlondikeCard pileTopCard(int pile) const;  // caller must check !pileEmpty(pile) first
  int pileFaceUpCount(int pile) const;       // how many cards may be held starting from this pile
  bool canDrop(int fromPile, int count, int toPile) const;
  void performMove(int fromPile, int count, int toPile);
  void pickup();
  void attemptDrop(int target);
  void draw();
  void checkSolved();

 public:
  static constexpr int kStockPile = 0;
  static constexpr int kWastePile = 1;
  static constexpr int kFoundationStart = 2;
  static constexpr int kTableauStart = 6;
  static constexpr int kPileCount = 13;

  explicit KlondikeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Klondike", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
