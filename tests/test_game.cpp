// Plays complete games between computer players to check the game flow
// and to report how strong the recovered 1988 computer player is.
#include "../src/core/game.h"
#include <cstdio>

using namespace yahtzee;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// Drive one computer turn exactly like P20D6: roll, ask for rerolls,
// repeat until it is satisfied or out of rolls, then score.
static void computerTurn(Game& g)
{
    g.roll();
    while (g.rollNumber() < kRollsPerTurn) {
        auto choice = g.computerRerollChoice();
        bool any = false;
        for (int i = 0; i < kDiceCount; ++i)
            if (choice[i]) { g.toggleSelected(i); any = true; }
        if (!any) break;
        g.roll();
    }
    g.score(g.computerScoringChoice());
}

int main()
{
    Game g(12345);
    Player a; a.name = "A"; a.computer = true;
    Player b; b.name = "B"; b.computer = true;
    g.setPlayers({a, b});
    g.setGamesPerSeries(6);

    // Basic turn rules.
    CHECK(g.canRoll(), "can roll at start of turn");
    CHECK(!g.canScore(Chance), "cannot score before rolling");
    g.roll();
    CHECK(g.rollNumber() == 1, "first roll");
    CHECK(!g.canRoll(), "roll is dimmed until a die is selected");
    g.toggleSelected(0);
    CHECK(g.canRoll(), "roll enabled once a die is selected");
    g.roll(); g.toggleSelected(1); g.roll();
    CHECK(g.rollNumber() == 3 && !g.canRoll() && !g.canSelect(), "three rolls per turn");
    g.score(Chance);
    CHECK(g.currentPlayer() == 1, "turn passes after scoring");
    CHECK(!g.player(0).card.open[Chance - 1], "chance used");

    // Full computer games.
    const int games = 2000;
    long total = 0; int yahtzees = 0, bonuses = 0, best = 0, worst = 1000;
    for (int n = 0; n < games; ++n) {
        g.newGame();
        int turns = 0;
        while (!g.gameOver()) { computerTurn(g); ++turns; }
        CHECK(turns == 26, "26 turns in a two-player game, got %d", turns);
        for (int p = 0; p < 2; ++p) {
            const Player& pl = g.player(p);
            for (bool open : pl.card.open) CHECK(!open, "card full at game end");
            total += pl.totals.grand;
            best = pl.totals.grand > best ? pl.totals.grand : best;
            worst = pl.totals.grand < worst ? pl.totals.grand : worst;
            yahtzees += pl.card.score[Yahtzee - 1] == 50;
            bonuses += pl.totals.bonus > 0;
        }
    }
    std::printf("computer player over %d games: average %.1f (range %d-%d), "
                "Yahtzee in %.0f%%, upper bonus in %.0f%%\n",
                2 * games, double(total) / (2 * games), worst, best,
                100.0 * yahtzees / (2 * games), 100.0 * bonuses / (2 * games));

    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
