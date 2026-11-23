// The computer player, recovered from the original program:
//   P48B5 and its nested procedures P325A..P4580  -> planCategory()
//   P4B2B                                         -> chooseCategory()
//   P20D6                                         -> the turn loop (see Game)
//
// The original evaluates every category after each roll. For each one it
// produces a "desirability" number and the dice it would reroll to chase
// that category. It then follows the plan of the most desirable open
// category, and after the last roll picks where to score with a few extra
// rules about the upper-section bonus.
#pragma once
#include "scoring.h"
#include <array>

namespace yahtzee {

struct Plan {
    int value = 0;                       // desirability (not points)
    std::array<bool, kDiceCount> reroll{};  // indexed by SORTED die position
};

// rerollsLeft: the original only asks "is there another roll?" (G000A <> 0),
// so any non-zero value behaves the same.
Plan planCategory(int category, const Dice& dice, int rerollsLeft);

struct ScoreCard {
    std::array<bool, kCategoryCount> open{};   // category i+1 still unscored
    std::array<int, kCategoryCount> score{};   // points in scored categories
};

using PlanTable = std::array<Plan, kCategoryCount>;
PlanTable planAll(const Dice& dice, int rerollsLeft);

// forScoring = false: pick the plan to follow for the next reroll (mode 1).
// forScoring = true:  pick the category to score now (mode 0).
// Returns a category number 1..13.
int chooseCategory(const Dice& dice, const ScoreCard& card,
                   const PlanTable& plans, bool forScoring);

} // namespace yahtzee
