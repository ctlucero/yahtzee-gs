// Plays whole series through the App (screen logic) with every player set
// to the computer, clicking buttons the way a person would between games.
#include "../src/screen/app.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++failures; std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void click(gs::App& a, int lx, int ly) { a.mouseDown(lx + 3, ly + 15); a.mouseUp(lx + 3, ly + 15); }
static void runUntilIdle(gs::App& a) { for (int i = 0; i < 200000 && (a.busy() || i < 3); ++i) a.tick(); }

int main(int argc, char** argv)
{
    gs::App a(7);
    // About and Instructions: open, page through, close.
    a.command(400);
    CHECK(a.dialogOpen(), "about opens");
    a.key('\r', false);
    CHECK(!a.dialogOpen(), "return closes about");
    a.command(701);
    for (int i = 0; i < 10; ++i) { a.mouseDown(6 + 240, 17 + 165); a.mouseUp(6 + 240, 17 + 165); }
    CHECK(a.dialogPage() == 7, "seven instruction pages, got %d", a.dialogPage());
    a.mouseDown(6 + 50, 17 + 165); a.mouseUp(6 + 50, 17 + 165);
    CHECK(a.dialogPage() == 6, "last page goes back");
    a.mouseDown(6 + 150, 17 + 165); a.mouseUp(6 + 150, 17 + 165);
    CHECK(!a.dialogOpen(), "done closes instructions");

    // Setup: enrol a human and a computer, two games a series, then Play.
    {
        gs::App s(3);
        s.command(503);
        for (const char* name : {"Ann", "Hal"}) {
            for (int i = 0; i < 20; ++i) s.key(8, false);
            for (const char* p = name; *p; ++p) s.key(*p, false);
            s.key('\r', false);
        }
        s.mouseDown(10 + 180, 19 + 90); s.mouseUp(10 + 180, 19 + 90);      // "2 " games
        s.mouseDown(10 + 239 + 10, 19 + 150); s.mouseUp(10 + 239 + 10, 19 + 150);   // Play
        CHECK(!s.dialogOpen() && s.gameInProgress(), "play starts a game");
        CHECK(s.playerCount() == 2, "two players, got %d", s.playerCount());
        CHECK(s.record(0).name == "Ann" && s.record(0).species == 1, "Ann is human");
        CHECK(s.record(1).name == "Hal" && s.record(1).species == 0, "Hal is the computer");
    }

    // Options dialog: Game > Options..., tick both, OK.
    {
        gs::App o(4);
        CHECK(!o.options().yahtzeeBonus && !o.options().modernRoll, "options default off");
        int saved = 0;
        o.onOptionsChanged = [&](const gs::Options&) { ++saved; };
        o.command(504);
        CHECK(o.dialogOpen(), "options opens");
        o.mouseDown(40 + 20, 36 + 50); o.mouseUp(40 + 20, 36 + 50);
        o.mouseDown(40 + 20, 36 + 78); o.mouseUp(40 + 20, 36 + 78);
        o.key('\r', false);
        CHECK(!o.dialogOpen() && o.options().yahtzeeBonus && o.options().modernRoll && saved == 1, "options set");
        o.command(504);
        o.mouseDown(40 + 120, 36 + 117); o.mouseUp(40 + 120, 36 + 117);   // Cancel
        CHECK(!o.dialogOpen() && o.options().yahtzeeBonus, "cancel keeps options");
        // During a game the Yahtzee Bonus box can't be changed.
        o.command(501);
        o.command(504);
        o.mouseDown(40 + 20, 36 + 50); o.mouseUp(40 + 20, 36 + 50);
        o.key('\r', false);
        CHECK(o.options().yahtzeeBonus, "bonus option locked during a game");
    }

    // Modern Roll: dice from the OS, every face about 1/6 (chi-square, 5 dof).
    for (bool modern : {false, true}) {
        gs::App m(11);
        gs::Options opt; opt.modernRoll = modern; m.setOptions(opt);
        int n[7] = {};
        const int N = 60000;
        for (int i = 0; i < N; ++i) { const int v = m.drawDie(); CHECK(v >= 1 && v <= 6, "die %d", v); ++n[v]; }
        double chi = 0;
        for (int f = 1; f <= 6; ++f) chi += (n[f] - N / 6.0) * (n[f] - N / 6.0) / (N / 6.0);
        CHECK(chi < 20.5, "%s roll chi-square %.1f", modern ? "modern" : "classic", chi);   // p < 0.001
    }

    // Redraw: a total that gets narrower (33 -> 51) leaves no old digits.
    {
        gs::App r(5);
        r.command(501);
        auto turn = [&](std::array<int, 5> d, int cat) {
            r.setDieValues(d);
            click(r, 270, 170);
            click(r, 30, (cat <= 6 ? 7 + 9 * (cat - 1) : 81 + 9 * (cat - 7)) + 4);
            click(r, 30, 170);
            runUntilIdle(r);
        };
        turn({5, 5, 5, 6, 6}, 5); turn({3, 3, 3, 3, 1}, 3); turn({2, 2, 2, 1, 1}, 2);
        turn({6, 6, 6, 2, 3}, 6);
        CHECK(r.record(0).upper == 51, "upper 51");
        // Player 1's column ends at x 170 (local); "51" is 13 px wide.
        int stray = 0;
        for (int y = 61; y < 68; ++y)
            for (int x = 140; x < 170 - 13; ++x) stray += r.canvas().pixel(x + 3, y + 15) != 10;
        CHECK(stray == 0, "old digits left beside the upper total: %d pixels", stray);
    }

    // Yahtzee Bonus through the screen: 50, then two more Yahtzees placed
    // with the joker, then a Yahtzee in Ones.
    {
        gs::App y(5);
        gs::Options opt; opt.yahtzeeBonus = true; y.setOptions(opt);
        y.command(501);
        auto top = [](int cat) { return cat <= 6 ? 6 + 9 * (cat - 1) : 77 + 9 * (cat - 7); };
        auto turn = [&](std::array<int, 5> d, int cat) {
            y.setDieValues(d);
            click(y, 270, 170);
            click(y, 30, top(cat) + 4);
            click(y, 30, 170);
            runUntilIdle(y);
        };
        turn({5, 5, 5, 5, 5}, 12);
        CHECK(y.record(0).yahtzeeBonus == 0 && y.record(0).score[11] == 50, "first Yahtzee scores 50");
        turn({4, 4, 4, 4, 4}, 11);
        turn({6, 6, 6, 6, 6}, 9);
        turn({1, 1, 1, 1, 1}, 1);
        const auto& p = y.record(0);
        CHECK(p.score[10] == 40 && p.score[8] == 25 && p.score[0] == 5, "joker scores %d %d %d", p.score[10], p.score[8], p.score[0]);
        CHECK(p.yahtzeeBonus == 300 && p.lower == 415 && p.grand == 420, "bonus %d lower %d grand %d", p.yahtzeeBonus, p.lower, p.grand);
        // A whole computer game with the option: totals stay consistent.
        gs::App c(21);
        c.setOptions(opt);
        c.setPlayerCount(4);
        for (int i = 0; i < 4; ++i) c.record(i).species = 0;
        int bonuses = 0;
        for (int game = 0; game < 30; ++game) {
            c.command(501);
            int guard = 0;
            while (!c.gameOver() && guard++ < 400) runUntilIdle(c);
            for (int i = 0; i < 4; ++i) {
                const auto& q = c.record(i);
                int lower = q.yahtzeeBonus;
                for (int k = 6; k < 13; ++k) lower += q.score[k];
                CHECK(q.lower == lower && q.grand == q.upper + q.lower + q.bonus, "computer totals");
                CHECK(q.yahtzeeBonus % 100 == 0 && (q.yahtzeeBonus == 0 || q.score[11] == 50), "bonus needs 50");
                bonuses += q.yahtzeeBonus / 100;
            }
            c.command(501);                                    // Quit Game
        }
        std::printf("computer earned %d Yahtzee bonuses in 120 cards\n", bonuses);
    }

    for (int p = 0; p < 2; ++p) a.record(p).species = 0;     // computer vs computer
    a.command(501);
    for (int game = 1; game <= 3; ++game) {
        int guard = 0;
        while (!a.gameOver() && guard++ < 100) runUntilIdle(a);
        CHECK(a.gameOver(), "game %d finished", game);
        for (int p = 0; p < 2; ++p)
            for (bool o : a.record(p).open) CHECK(!o, "card full");
        click(a, 270, 170);                       // "View" standings
        if (argc > 1 && game == 3) {
            FILE* f = std::fopen(argv[1], "wb");
            for (int y = 0; y < 200; ++y) for (int x = 0; x < 320; ++x) std::fputc(a.canvas().pixel(x, y), f);
            std::fclose(f);
        }
        if (game < 3) click(a, 270, 170);         // "Play" next game
    }
    click(a, 100, 100);                           // click to play again -> new series
    CHECK(a.gameInProgress() && !a.gameOver(), "new series started");
    std::printf("series totals were %d and %d; %d failure(s)\n",
                a.record(0).series, a.record(1).series, failures);
    return failures ? 1 : 0;
}
