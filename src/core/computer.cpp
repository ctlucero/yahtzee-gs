#include "computer.h"

namespace yahtzee {

namespace {

// d[0..4] are the sorted dice (the original's d1..d5).
struct Sorted {
    int d1, d2, d3, d4, d5;
    explicit Sorted(const Dice& s) : d1(s[0]), d2(s[1]), d3(s[2]), d4(s[3]), d5(s[4]) {}
};

bool in(int v, int a, int b) { return v == a || v == b; }      // v IN [a, b]
bool inRange(int v, int lo, int hi) { return v >= lo && v <= hi; } // v IN [lo..hi]

void mark(Plan& p, bool b1, bool b2, bool b3, bool b4, bool b5)  // P3105
{
    const bool b[5] = {b1, b2, b3, b4, b5};
    for (int i = 0; i < 5; ++i) if (b[i]) p.reroll[i] = true;
}

// P31D5: the value of the highest pair, or 0.
int highestPair(const Sorted& s)
{
    if (s.d5 == s.d4) return s.d5;
    if (s.d4 == s.d3) return s.d4;
    if (s.d3 == s.d2) return s.d3;
    if (s.d2 == s.d1) return s.d2;
    return 0;
}

// Keep the highest pair, reroll the rest (shared by several planners).
void markKeepHighestPair(Plan& p, const Sorted& s)
{
    mark(p, s.d1 != s.d2 || in(s.d4, s.d3, s.d5),
            !in(s.d2, s.d1, s.d3) || in(s.d4, s.d3, s.d5),
            !in(s.d3, s.d2, s.d4) || s.d4 == s.d5,
            !in(s.d4, s.d3, s.d5),
            s.d5 != s.d4);
}

// Nothing to build on: keep the highest die if it is 4 or more.
void markKeepHighDie(Plan& p, const Sorted& s)
{
    if (s.d5 >= 4) p.value = 2;
    mark(p, true, true, true, true, s.d5 <= 3);
}

// Keep a three of a kind, reroll the other two (used by several planners).
void markKeepTriple(Plan& p, const Sorted& s)
{
    mark(p, s.d1 != s.d3, s.d3 == s.d5, false, s.d1 == s.d3, s.d5 != s.d3);
}

void planThreeOfAKind(Plan& p, const Dice& dice, const Sorted& s, bool more) // P325A
{
    const int pts = scoreCategory(ThreeOfAKind, dice);
    if (pts > 0) {
        // Have it: reroll the two odd dice only if they are low.
        mark(p, s.d1 != s.d3 && s.d1 < 4, s.d3 == s.d5 && s.d2 < 4, false,
                s.d1 == s.d3 && s.d4 < 4, s.d5 != s.d3 && s.d5 < 4);
        p.value = pts > 27 ? 16 : 12;
    } else if (more) {
        const int pair = highestPair(s);
        if (pair > 0) {
            markKeepHighestPair(p, s);
            p.value = pair == 6 ? 6 : pair == 5 ? 4 : 3;
        } else {
            markKeepHighDie(p, s);
        }
    }
}

void planFourOfAKind(Plan& p, const Dice& dice, const Sorted& s, bool more) // P367E
{
    const int pts = scoreCategory(FourOfAKind, dice);
    if (pts > 0) {
        mark(p, s.d1 != s.d2 && s.d1 < 4, false, false, false, s.d4 != s.d5 && s.d5 < 4);
        p.value = pts > 24 ? 22 : 13;
    } else if (more) {
        if (scoreCategory(ThreeOfAKind, dice) > 0) {
            markKeepTriple(p, s);
            p.value = 17;
        } else {
            const int pair = highestPair(s);
            if (pair > 0) {
                markKeepHighestPair(p, s);
                p.value = (pair == 5 || pair == 6) ? 4 : 3;
            } else {
                markKeepHighDie(p, s);
            }
        }
    }
}

void planFullHouse(Plan& p, const Dice& dice, const Sorted& s, bool more) // P3AE3
{
    if (scoreCategory(FullHouse, dice) > 0) {
        p.value = 28;
    } else if (more) {
        if (scoreCategory(ThreeOfAKind, dice) > 0) {
            mark(p, s.d1 != s.d3, s.d3 == s.d5 && s.d2 < 4, false,
                    s.d1 == s.d3, s.d3 != s.d5 && s.d5 < 4);
            p.value = 13;
        } else if (in(s.d2, s.d1, s.d3) && in(s.d4, s.d3, s.d5)) {
            // Two pairs: reroll the odd die.
            mark(p, s.d1 != s.d2, false, !in(s.d3, s.d2, s.d4), false, s.d5 != s.d4);
            p.value = 8;
        } else {
            if (highestPair(s) > 0) p.value = 3;
            // Reroll every die that isn't part of a pair.
            mark(p, s.d1 != s.d2, !in(s.d2, s.d1, s.d3), !in(s.d3, s.d2, s.d4),
                    !in(s.d4, s.d3, s.d5), s.d5 != s.d4);
        }
    }
}

void planSmallStraight(Plan& p, const Dice& dice, const Sorted& s, bool more) // P3E60
{
    if (scoreCategory(SmallStraight, dice) > 0) {
        p.value = 11;
    } else if (more) {
        if (s.d3 <= 3) {
            // Build upward from the lowest die.
            mark(p, false,
                    s.d1 + 1 != s.d2 || s.d2 == s.d3,
                    !inRange(s.d3, s.d1 + 1, s.d1 + 2) || s.d3 == s.d4,
                    !inRange(s.d4, s.d1 + 1, s.d1 + 3) || s.d4 == s.d5,
                    !inRange(s.d5, s.d1 + 1, s.d1 + 3));
        } else {
            // Build downward from the highest die.
            mark(p, !inRange(s.d1, s.d5 - 3, s.d5 - 1),
                    !inRange(s.d2, s.d5 - 3, s.d5 - 1) || s.d2 == s.d1,
                    !inRange(s.d3, s.d5 - 2, s.d5 - 1) || s.d3 == s.d2,
                    s.d5 - 1 != s.d4 || s.d4 == s.d3,
                    false);
        }
        p.value = 2;
    }
}

void planLargeStraight(Plan& p, const Dice& dice, const Sorted& s, bool more) // P4156
{
    if (scoreCategory(LargeStraight, dice) > 0) {
        p.value = 29;
    } else if (more) {
        if (scoreCategory(SmallStraight, dice) > 0) {
            // Four in a row already: reroll the duplicate, or else the end
            // die that doesn't continue the run.
            const int d[5] = {s.d1, s.d2, s.d3, s.d4, s.d5};
            bool found = false;
            for (int i = 0; i < 4; ++i)
                if (d[i] == d[i + 1]) { p.reroll[i] = true; found = true; }
            if (!found) mark(p, s.d2 - 1 != s.d1, false, false, false, s.d4 + 1 != s.d5);
            p.value = 21;
        } else if (s.d3 <= 3) {
            mark(p, false,
                    s.d1 + 1 != s.d2 || s.d2 == s.d3,
                    !inRange(s.d3, s.d1 + 1, s.d1 + 2) || s.d3 == s.d4,
                    !inRange(s.d4, s.d1 + 1, s.d1 + 3) || s.d4 == s.d5,
                    !inRange(s.d5, s.d1 + 1, s.d1 + 4));
            p.value = 1;
        } else {
            mark(p, !inRange(s.d1, s.d5 - 4, s.d5 - 1),
                    !inRange(s.d2, s.d5 - 4, s.d5 - 1) || s.d2 == s.d1,
                    !inRange(s.d3, s.d5 - 4, s.d5 - 1) || s.d3 == s.d2,
                    s.d5 - 1 != s.d4 || s.d4 == s.d3,
                    false);
            p.value = 1;
        }
    }
}

void planYahtzee(Plan& p, const Dice& dice, const Sorted& s, bool more) // P4580
{
    if (scoreCategory(Yahtzee, dice) == 50) {
        p.value = 30;
    } else if (more) {
        if (scoreCategory(FourOfAKind, dice) > 0) {
            mark(p, s.d1 != s.d2, false, false, false, s.d4 != s.d5);
            p.value = 23;
        } else if (scoreCategory(ThreeOfAKind, dice) > 0) {
            markKeepTriple(p, s);
            p.value = 18;
        } else {
            if (highestPair(s) > 0) p.value = 4;
            markKeepHighestPair(p, s);
        }
    }
}

} // namespace

Plan planCategory(int category, const Dice& dice, int rerollsLeft)        // P48B5
{
    const Dice sd = sorted(dice);
    const Sorted s(sd);
    const bool more = rerollsLeft != 0;
    Plan p;
    switch (category) {
    case Ones: case Twos: case Threes: case Fours: case Fives: case Sixes:
        // 5 points per matching die; reroll the others (even on the last roll,
        // where the flags are simply never used).
        for (int i = 0; i < 5; ++i) {
            if (sd[i] == category) p.value += 5;
            else p.reroll[i] = true;
        }
        break;
    case ThreeOfAKind:  planThreeOfAKind(p, dice, s, more); break;
    case FourOfAKind:   planFourOfAKind(p, dice, s, more); break;
    case FullHouse:     planFullHouse(p, dice, s, more); break;
    case SmallStraight: planSmallStraight(p, dice, s, more); break;
    case LargeStraight: planLargeStraight(p, dice, s, more); break;
    case Yahtzee:       planYahtzee(p, dice, s, more); break;
    case Chance:
        // Never "desirable" in itself; reroll anything below 4.
        for (int i = 0; i < 5; ++i) if (sd[i] < 4) p.reroll[i] = true;
        break;
    }
    return p;
}

PlanTable planAll(const Dice& dice, int rerollsLeft)
{
    PlanTable t;
    for (int c = 1; c <= kCategoryCount; ++c) t[c - 1] = planCategory(c, dice, rerollsLeft);
    return t;
}

int chooseCategory(const Dice& dice, const ScoreCard& card,
                   const PlanTable& plans, bool forScoring)               // P4B2B
{
    auto open = [&](int c) { return card.open[c - 1]; };

    // The most desirable open category; ties go to the later one.
    int best = 0, bestValue = 0;
    for (int c = 1; c <= kCategoryCount; ++c)
        if (open(c) && plans[c - 1].value >= bestValue) { bestValue = plans[c - 1].value; best = c; }
    if (!forScoring) return best;

    // How far ahead (+) or behind (-) of bonus pace (three of each) the
    // scored upper boxes are.
    int pace = 0;
    for (int c = 1; c <= 6; ++c)
        if (!open(c)) pace += card.score[c - 1] - 3 * c;

    // A weak upper-section choice: prefer an upper box that keeps the bonus
    // on pace, otherwise the one that loses the least ground.
    bool settled = false;
    if (best >= 1 && best <= 6 && bestValue < 15) {
        int leastLoss = 32767;
        for (int c = 1; c <= 6; ++c) {
            if (!open(c)) continue;
            const int pts = scoreCategory(c, dice);
            if (pts - 3 * c + pace >= 0) { best = c; settled = true; break; }
            if (3 * c - pts < leastLoss) {
                leastLoss = 3 * c - pts;
                bestValue = plans[c - 1].value;
                best = c;
            }
        }
    }

    // Still nothing worthwhile: dump it in the last open lower box,
    // checking Chance first, then Yahtzee, and so on up the card.
    if (bestValue < 11 && !settled) {
        for (int c = Chance; c >= ThreeOfAKind; --c)
            if (open(c)) { best = c; break; }
    }
    return best;
}

} // namespace yahtzee
