#include "scoring.h"
#include <algorithm>

namespace yahtzee {

Dice sorted(Dice d) { std::sort(d.begin(), d.end()); return d; }

int scoreCategory(int category, const Dice& dice)
{
    // Ported test-for-test from P288F. d1..d5 are the sorted dice.
    const Dice s = sorted(dice);
    const int d1 = s[0], d2 = s[1], d3 = s[2], d4 = s[3], d5 = s[4];
    auto sum = [&] { return d1 + d2 + d3 + d4 + d5; };

    switch (category) {
    case Ones: case Twos: case Threes: case Fours: case Fives: case Sixes: {
        int total = 0;
        for (int v : s) if (v == category) total += category;
        return total;
    }
    case ThreeOfAKind:
        return (d1 == d3 || d2 == d4 || d3 == d5) ? sum() : 0;
    case FourOfAKind:
        return (d1 == d4 || d2 == d5) ? sum() : 0;
    case FullHouse: {
        // A three and a distinct pair. Five of a kind does NOT qualify,
        // because the original requires the two groups to differ.
        const bool a = d1 == d3 && d4 == d5 && d3 != d4;
        const bool b = d1 == d2 && d3 == d5 && d2 != d3;
        return (a || b) ? 25 : 0;
    }
    case SmallStraight: {
        // The five patterns the original checks, covering every place the
        // fifth (out-of-run or duplicate) die can sit.
        auto n = [](int x, int y) { return x + 1 == y; };
        const bool hit = (n(d4, d5) && n(d3, d4) && n(d2, d3))
                      || (n(d4, d5) && n(d3, d4) && n(d1, d3))
                      || (n(d4, d5) && n(d2, d4) && n(d1, d2))
                      || (n(d3, d5) && n(d2, d3) && n(d1, d2))
                      || (n(d3, d4) && n(d2, d3) && n(d1, d2));
        return hit ? 30 : 0;
    }
    case LargeStraight:
        return (d1 + 1 == d2 && d2 + 1 == d3 && d3 + 1 == d4 && d4 + 1 == d5) ? 40 : 0;
    case Yahtzee:
        return d1 == d5 ? 50 : 0;
    case Chance:
        return sum();
    default:
        return 0;
    }
}

Totals computeTotals(const std::array<int, kCategoryCount>& scores, int yahtzeeBonus)
{
    Totals t;
    for (int i = 0; i < 6; ++i) t.upper += scores[i];
    for (int i = 6; i < kCategoryCount; ++i) t.lower += scores[i];
    t.yahtzeeBonus = yahtzeeBonus;
    t.lower += yahtzeeBonus;
    t.bonus = t.upper >= kUpperBonusThreshold ? kUpperBonus : 0;
    t.grand = t.upper + t.lower + t.bonus;
    return t;
}

bool isYahtzee(const Dice& d)
{
    return d[0] >= 1 && d[0] <= 6 && std::all_of(d.begin(), d.end(), [&](int v) { return v == d[0]; });
}

bool earnsYahtzeeBonus(const Dice& dice, int yahtzeeBox)
{
    return yahtzeeBox == 50 && isYahtzee(dice);
}

int scoreWithJoker(int category, const Dice& dice, int yahtzeeBox)
{
    if (yahtzeeBox != kYahtzeeBoxOpen && isYahtzee(dice)) {
        if (category == FullHouse) return 25;
        if (category == SmallStraight) return 30;
        if (category == LargeStraight) return 40;
    }
    return scoreCategory(category, dice);
}

} // namespace yahtzee
