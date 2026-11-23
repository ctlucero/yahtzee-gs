// Compares the ported computer player with vectors produced by running the
// ORIGINAL 65816 code (P48B5 and P4B2B) in an emulator. See tests/data/.
#include "../src/core/computer.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace yahtzee;

static Dice parseDice(const std::string& s)
{
    Dice d{};
    for (int i = 0; i < kDiceCount; ++i) d[i] = s[i] - '0';
    return d;
}

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "tests/data/original_vectors.txt";
    std::ifstream in(path);
    if (!in) { std::printf("cannot open %s\n", path); return 2; }

    int plans = 0, choices = 0, failures = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        char kind; std::string dice;
        ss >> kind >> dice;
        if (kind == 'P') {
            int cat, rerolls, value; std::string flags;
            ss >> cat >> rerolls >> value >> flags;
            Plan p = planCategory(cat, parseDice(dice), rerolls);
            bool ok = p.value == value;
            for (int i = 0; i < kDiceCount; ++i) ok &= p.reroll[i] == (flags[i] == '1');
            if (!ok && ++failures <= 10)
                std::printf("plan mismatch: %s\n", line.c_str());
            ++plans;
        } else if (kind == 'C') {
            int mode, mask, expected;
            ss >> mode >> mask;
            ScoreCard card;
            PlanTable table;
            for (int c = 0; c < kCategoryCount; ++c) { card.open[c] = mask >> c & 1; ss >> card.score[c]; }
            for (int c = 0; c < kCategoryCount; ++c) ss >> table[c].value;
            ss >> expected;
            int got = chooseCategory(parseDice(dice), card, table, mode == 0);
            if (got != expected && ++failures <= 10)
                std::printf("choice mismatch (got %d): %s\n", got, line.c_str());
            ++choices;
        }
    }
    std::printf("%d plans and %d category choices checked against the original, %d failure(s)\n",
                plans, choices, failures);
    return failures || !plans || !choices ? 1 : 0;
}
