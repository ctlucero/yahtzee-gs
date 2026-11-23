// Yahtzee for KDE Plasma - a port of YAHTZEE v1.1 for the Apple IIgs
// Original (c) 1988 David W. Buell, "Distribute freely but do not sell".
//
// Scoring rules, recovered from procedures P288F (category score) and
// P27DB (totals) of the original TML Pascal program.
#pragma once
#include <array>

namespace yahtzee {

// Category numbers match the original program (1..13).
enum Category : int {
    Ones = 1, Twos, Threes, Fours, Fives, Sixes,
    ThreeOfAKind, FourOfAKind, FullHouse,
    SmallStraight, LargeStraight, Yahtzee, Chance
};
constexpr int kCategoryCount = 13;
constexpr int kDiceCount = 5;

using Dice = std::array<int, kDiceCount>;   // face values 1..6

// The original sorts a copy of the dice (global G04F7) before scoring,
// and its tests depend on that order. scoreCategory() sorts internally.
Dice sorted(Dice d);

// Points the given dice would score in a category (P288F).
int scoreCategory(int category, const Dice& dice);

constexpr int kUpperBonusThreshold = 63;   // original tests "upper > 62"
constexpr int kUpperBonus = 35;

struct Totals {
    int upper = 0;    // player record offset $10
    int lower = 0;    // player record offset $12 (includes any Yahtzee bonus)
    int bonus = 0;    // player record offset $14
    int grand = 0;    // player record offset $16
    int yahtzeeBonus = 0;   // port option, not in the original
};

// scores[i] is the score for category i+1; unscored categories count 0
// (P27DB simply adds all thirteen fields). There is no Yahtzee bonus or
// joker rule in the original; yahtzeeBonus (points) is the port's optional
// rule and is 0 when the option is off.
Totals computeTotals(const std::array<int, kCategoryCount>& scores, int yahtzeeBonus = 0);

// ---- Optional "Yahtzee Bonus" rule (a port option, off by default) -------
// Each further Yahtzee scores a 100-point bonus, provided the Yahtzee box
// already holds 50. The roll may be placed in any open box. Once the
// Yahtzee box is filled (50 or 0), a Yahtzee placed in Full House, Small
// Straight or Large Straight scores that box's full value (the "joker").
constexpr int kYahtzeeBonus = 100;
constexpr int kYahtzeeBoxOpen = -1;

bool isYahtzee(const Dice& dice);

// yahtzeeBox: the Yahtzee box's score, or kYahtzeeBoxOpen if unscored.
bool earnsYahtzeeBonus(const Dice& dice, int yahtzeeBox);
int scoreWithJoker(int category, const Dice& dice, int yahtzeeBox);

} // namespace yahtzee
