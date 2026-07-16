#pragma once

#include <cstdint>
#include <vector>

namespace fivecrowns {

// A single card. rank 0 is the joker sentinel (suit is unused/ignored for jokers).
struct Card {
  uint8_t rank;  // 3-13 (3..10, J=11, Q=12, K=13); 0 = joker
  uint8_t suit;  // 0=Stars,1=Hearts,2=Clubs,3=Spades,4=Diamonds
  bool isJoker() const { return rank == 0; }
  bool isWild(uint8_t wildRank) const { return isJoker() || rank == wildRank; }
};

// Point value of a single card if left as deadwood. A wild-rank card always prices at 20, even if
// it was never used as a wild that hand (Five Crowns scoring rule).
uint8_t cardPointValue(Card c, uint8_t wildRank);

struct MeldResult {
  int deadwood = 0;
  // Parallel to the input `cards` vector: -1 if the card is unmelded (deadwood), else a 0-based
  // meld-group index (cards sharing a group id belong to the same book/run).
  std::vector<int> groupId;
};

// Finds the partition of `cards` into books (3+ same rank, any suit) and runs (3+ consecutive
// ranks, same suit) that minimizes total leftover (deadwood) point value, using jokers and the
// hand's current wild-rank cards as flexible substitutes for any missing card. Hand size is
// assumed small (<=16, so a usedMask fits in uint16_t) — Five Crowns hands never exceed 13 dealt
// + 1 just-drawn card.
MeldResult findBestMelds(const std::vector<Card>& cards, uint8_t wildRank);

}  // namespace fivecrowns
