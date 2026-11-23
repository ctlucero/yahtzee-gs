// Checks the ported scoring against an independent, count-based reference
// for all 7,776 possible rolls, plus spot checks of the original's quirks.
#include "../src/core/scoring.h"
#include <cstdio>
#include <set>

using namespace yahtzee;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static int reference(int cat, const Dice& d)
{
    int count[7] = {0}, sum = 0;
    for (int v : d) { ++count[v]; sum += v; }
    int maxCount = 0; bool has2 = false, has3 = false;
    for (int v = 1; v <= 6; ++v) {
        maxCount = count[v] > maxCount ? count[v] : maxCount;
        has2 |= count[v] == 2; has3 |= count[v] == 3;
    }
    auto run = [&](int lo, int len) {
        for (int v = lo; v < lo + len; ++v) if (!count[v]) return false;
        return true;
    };
    switch (cat) {
    case 1: case 2: case 3: case 4: case 5: case 6: return count[cat] * cat;
    case ThreeOfAKind: return maxCount >= 3 ? sum : 0;
    case FourOfAKind:  return maxCount >= 4 ? sum : 0;
    case FullHouse:    return (has3 && has2) ? 25 : 0;
    case SmallStraight: return (run(1, 4) || run(2, 4) || run(3, 4)) ? 30 : 0;
    case LargeStraight: return (run(1, 5) || run(2, 5)) ? 40 : 0;
    case Yahtzee:      return maxCount == 5 ? 50 : 0;
    case Chance:       return sum;
    }
    return -1;
}

int main()
{
    int combos = 0;
    for (int a = 1; a <= 6; ++a) for (int b = 1; b <= 6; ++b) for (int c = 1; c <= 6; ++c)
    for (int d = 1; d <= 6; ++d) for (int e = 1; e <= 6; ++e) {
        Dice dice{a, b, c, d, e};
        ++combos;
        for (int cat = 1; cat <= kCategoryCount; ++cat) {
            int got = scoreCategory(cat, dice), want = reference(cat, dice);
            CHECK(got == want, "cat %d dice %d%d%d%d%d: got %d want %d", cat, a, b, c, d, e, got, want);
        }
    }

    // Quirks of the original worth pinning down explicitly.
    CHECK(scoreCategory(FullHouse, {4, 4, 4, 4, 4}) == 0, "five of a kind is not a full house");
    CHECK(scoreCategory(SmallStraight, {1, 2, 3, 4, 5}) == 30, "large straight also scores small");
    CHECK(scoreCategory(FourOfAKind, {6, 6, 6, 6, 6}) == 30, "yahtzee counts as four of a kind");

    std::array<int, kCategoryCount> s{};
    s = {3, 6, 9, 12, 15, 18};                       // exactly 63 in the upper section
    CHECK(computeTotals(s).bonus == 35, "63 earns the bonus");
    s = {3, 6, 9, 12, 15, 17};                       // 62
    CHECK(computeTotals(s).bonus == 0, "62 does not");
    s = {3, 6, 9, 12, 15, 18, 20, 0, 25, 30, 40, 50, 22};
    Totals t = computeTotals(s);
    CHECK(t.upper == 63 && t.lower == 187 && t.grand == 285, "totals %d %d %d", t.upper, t.lower, t.grand);

    // Optional Yahtzee Bonus rule (port option).
    t = computeTotals(s, 200);
    CHECK(t.yahtzeeBonus == 200 && t.lower == 387 && t.grand == 485, "bonus totals %d %d", t.lower, t.grand);
    CHECK(isYahtzee({4, 4, 4, 4, 4}) && !isYahtzee({4, 4, 4, 4, 3}), "isYahtzee");
    CHECK(earnsYahtzeeBonus({2, 2, 2, 2, 2}, 50), "bonus with 50 in the box");
    CHECK(!earnsYahtzeeBonus({2, 2, 2, 2, 2}, 0), "no bonus with a zeroed box");
    CHECK(!earnsYahtzeeBonus({2, 2, 2, 2, 2}, kYahtzeeBoxOpen), "first Yahtzee is not a bonus");
    CHECK(!earnsYahtzeeBonus({2, 2, 2, 2, 3}, 50), "not a Yahtzee");
    const Dice five3 = {3, 3, 3, 3, 3};
    CHECK(scoreWithJoker(FullHouse, five3, 50) == 25, "joker full house");
    CHECK(scoreWithJoker(SmallStraight, five3, 0) == 30, "joker small straight (zeroed box)");
    CHECK(scoreWithJoker(LargeStraight, five3, 50) == 40, "joker large straight");
    CHECK(scoreWithJoker(FullHouse, five3, kYahtzeeBoxOpen) == 0, "no joker while the box is open");
    CHECK(scoreWithJoker(Threes, five3, 50) == 15 && scoreWithJoker(Chance, five3, 50) == 15, "others normal");
    for (int c = 1; c <= kCategoryCount; ++c)        // joker never changes a non-Yahtzee
        CHECK(scoreWithJoker(c, {1, 2, 3, 4, 6}, 50) == scoreCategory(c, {1, 2, 3, 4, 6}), "non-Yahtzee %d", c);

    std::printf("%d rolls x %d categories checked, %d failure(s)\n", combos, kCategoryCount, failures);
    return failures ? 1 : 0;
}
