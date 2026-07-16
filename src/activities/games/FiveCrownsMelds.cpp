#include "FiveCrownsMelds.h"

#include <algorithm>
#include <array>
#include <functional>
#include <unordered_map>
#include <utility>

namespace fivecrowns {

namespace {
constexpr uint8_t kRankPoints[14] = {0, 0, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
constexpr uint8_t kJokerPoints = 50;
constexpr uint8_t kWildPoints = 20;

// A candidate book or run: which non-wild hand indices it uses (bitmask) and how many wild cards
// are needed to complete it to a valid meld.
struct Candidate {
  uint16_t nonWildMask = 0;
  int wildsNeeded = 0;
};

// How a usedMask state was reached during the reachability search below, so the winning
// combination can be reconstructed by walking parents back to the empty mask.
struct ReachState {
  int wildsUsed = 0;
  uint16_t prevMask = 0;
  int candidateIndex = -1;  // -1 only for the root (mask 0)
};
}  // namespace

uint8_t cardPointValue(Card c, uint8_t wildRank) {
  if (c.isJoker()) return kJokerPoints;
  if (c.rank == wildRank) return kWildPoints;
  return kRankPoints[c.rank];
}

MeldResult findBestMelds(const std::vector<Card>& cards, uint8_t wildRank) {
  const int n = static_cast<int>(cards.size());
  MeldResult result;
  result.groupId.assign(static_cast<size_t>(n), -1);
  if (n == 0) return result;

  std::vector<int> wildIndices;
  std::vector<int> naturalIndices;
  wildIndices.reserve(static_cast<size_t>(n));
  naturalIndices.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    if (cards[static_cast<size_t>(i)].isWild(wildRank)) {
      wildIndices.push_back(i);
    } else {
      naturalIndices.push_back(i);
    }
  }
  const int totalWilds = static_cast<int>(wildIndices.size());

  // Deadwood contribution from leftover wild cards once `used` of them (the highest-value ones)
  // are considered melded: suffix[k] = sum of the (totalWilds-k) lowest-value wilds remaining.
  // Using more wilds in melds never increases deadwood, so for a fixed non-wild coverage, more
  // wildsUsed is always at least as good.
  std::vector<uint8_t> wildValues;
  wildValues.reserve(wildIndices.size());
  for (int idx : wildIndices) wildValues.push_back(cardPointValue(cards[static_cast<size_t>(idx)], wildRank));
  std::sort(wildValues.begin(), wildValues.end(), std::greater<uint8_t>());
  std::vector<int> suffix(static_cast<size_t>(totalWilds) + 1, 0);
  for (int k = totalWilds - 1; k >= 0; --k) {
    suffix[static_cast<size_t>(k)] = suffix[static_cast<size_t>(k) + 1] + wildValues[static_cast<size_t>(k)];
  }

  // --- Candidate generation ---
  std::vector<Candidate> candidates;

  // Books: all non-wild cards sharing a rank (wild-rank cards were already classified as wild).
  std::array<std::vector<int>, 14> byRank;
  for (int idx : naturalIndices) byRank[cards[static_cast<size_t>(idx)].rank].push_back(idx);
  for (int rank = 3; rank <= 13; ++rank) {
    const auto& group = byRank[static_cast<size_t>(rank)];
    if (group.size() < 2) continue;
    Candidate c;
    for (int idx : group) c.nonWildMask |= static_cast<uint16_t>(1u << idx);
    c.wildsNeeded = std::max(0, 3 - static_cast<int>(group.size()));
    if (c.wildsNeeded <= totalWilds) candidates.push_back(c);
  }

  // Runs: all contiguous subranges of the present (rank, suit) cards, per suit.
  for (int suit = 0; suit < 5; ++suit) {
    std::vector<std::pair<int, int>> present;  // (rank, handIndex)
    for (int idx : naturalIndices) {
      const Card& c = cards[static_cast<size_t>(idx)];
      if (c.suit == suit) present.emplace_back(c.rank, idx);
    }
    std::sort(present.begin(), present.end());
    const int m = static_cast<int>(present.size());
    for (int i = 0; i < m; ++i) {
      for (int j = i; j < m; ++j) {
        const int startRank = present[static_cast<size_t>(i)].first;
        const int endRank = present[static_cast<size_t>(j)].first;
        const int length = endRank - startRank + 1;
        if (length < 3) continue;
        int uniqueRanks = 1;
        for (int k = i + 1; k <= j; ++k) {
          if (present[static_cast<size_t>(k)].first != present[static_cast<size_t>(k) - 1].first) uniqueRanks++;
        }
        const int wildsNeeded = length - uniqueRanks;
        if (wildsNeeded < 0 || wildsNeeded > totalWilds) continue;
        Candidate c;
        c.wildsNeeded = wildsNeeded;
        for (int k = i; k <= j; ++k) {
          c.nonWildMask |= static_cast<uint16_t>(1u << present[static_cast<size_t>(k)].second);
        }
        candidates.push_back(c);
      }
    }
  }

  // --- Reachability search: which usedMask states can be built from disjoint candidates, and
  // the max cumulative wilds spent to reach each (see the suffix[] comment above for why more
  // wilds used is always at least as good for a fixed mask). ---
  std::unordered_map<uint16_t, ReachState> stateForMask;
  stateForMask[0] = ReachState{};
  std::vector<uint16_t> frontier{0};
  while (!frontier.empty()) {
    std::vector<uint16_t> nextFrontier;
    for (uint16_t mask : frontier) {
      const int wildsUsed = stateForMask[mask].wildsUsed;
      for (size_t ci = 0; ci < candidates.size(); ++ci) {
        const Candidate& cand = candidates[ci];
        if (mask & cand.nonWildMask) continue;  // overlaps a card already used
        const int newWilds = wildsUsed + cand.wildsNeeded;
        if (newWilds > totalWilds) continue;
        const uint16_t newMask = static_cast<uint16_t>(mask | cand.nonWildMask);
        auto it = stateForMask.find(newMask);
        if (it == stateForMask.end() || newWilds > it->second.wildsUsed) {
          stateForMask[newMask] = ReachState{newWilds, mask, static_cast<int>(ci)};
          nextFrontier.push_back(newMask);
        }
      }
    }
    frontier = std::move(nextFrontier);
  }

  // --- Pick the best (mask, wildsUsed) combination ---
  int bestDeadwood = -1;
  uint16_t bestMask = 0;
  for (const auto& [mask, state] : stateForMask) {
    int nonWildDeadwood = 0;
    for (int idx : naturalIndices) {
      if (!(mask & static_cast<uint16_t>(1u << idx))) {
        nonWildDeadwood += cardPointValue(cards[static_cast<size_t>(idx)], wildRank);
      }
    }
    const int deadwood = nonWildDeadwood + suffix[static_cast<size_t>(state.wildsUsed)];
    if (bestDeadwood < 0 || deadwood < bestDeadwood) {
      bestDeadwood = deadwood;
      bestMask = mask;
    }
  }
  result.deadwood = bestDeadwood;

  // Reconstruct the winning combination by walking parents back to the empty mask.
  int groupId = 0;
  uint16_t mask = bestMask;
  while (mask != 0) {
    const ReachState& state = stateForMask[mask];
    const Candidate& cand = candidates[static_cast<size_t>(state.candidateIndex)];
    for (int idx = 0; idx < n; ++idx) {
      if (cand.nonWildMask & static_cast<uint16_t>(1u << idx)) result.groupId[static_cast<size_t>(idx)] = groupId;
    }
    groupId++;
    mask = state.prevMask;
  }

  return result;
}

}  // namespace fivecrowns
