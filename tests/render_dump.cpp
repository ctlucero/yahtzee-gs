// Renders App states and dumps the raw 320x200 pixel indices for comparison
// with the emulator (tools/compare_renders.py).
#include "../src/screen/app.h"
#include <cstdio>
#include <string>

static void dump(gs::App& a, const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x) std::fputc(a.canvas().pixel(x, y), f);
    std::fclose(f);
}

int main(int argc, char** argv)
{
    std::string dir = argc > 1 ? argv[1] : ".";
    {
        gs::App s(1);
        s.command(503);                                   // Setup
        dump(s, dir + "/setup.raw");
        for (const char* name : {"Ann", "Bob"}) {
            for (int i = 0; i < 20; ++i) s.key(8, false);
            for (const char* p = name; *p; ++p) s.key(*p, false);
            s.key('\r', false);                            // Enroll (default button)
        }
        s.mouseDown(10 + 37, 19 + 136); s.mouseUp(10 + 37, 19 + 136);   // Computer
        for (const char* p = "Deep Dice"; *p; ++p) s.key(*p, false);
        dump(s, dir + "/setup2.raw");
        s.key('\r', false);
        s.key('\r', false);                                // 4th enrol -> Roster Full
        dump(s, dir + "/setup3.raw");
        s.key('\r', false);                                // Play is now the default
        dump(s, dir + "/setupgame.raw");
    }
    gs::App a(1);
    dump(a, dir + "/title.raw");
    a.command(501);                       // New Game
    dump(a, dir + "/newgame.raw");
    a.setDieValues({4, 3, 5, 5, 3});
    a.mouseDown(3 + 270, 15 + 170); a.mouseUp(3 + 270, 15 + 170);   // Roll
    dump(a, dir + "/roll1.raw");
    a.mouseDown(3 + 270, 15 + 20); a.mouseUp(3 + 270, 15 + 20);     // select die 1
    dump(a, dir + "/select.raw");
    a.mouseDown(3 + 30, 15 + 140); a.mouseUp(3 + 30, 15 + 140);     // select Chance
    dump(a, dir + "/category.raw");
    a.mouseDown(3 + 100, 15 + 170);                                  // hold the status bar
    dump(a, dir + "/possible.raw");
    a.mouseUp(3 + 100, 15 + 170);
    a.mouseDown(3 + 30, 15 + 170); a.mouseUp(3 + 30, 15 + 170);     // Score
    dump(a, dir + "/scored.raw");
    int guard = 0;
    while (a.busy() || guard < 2) { a.tick(); if (!a.busy()) ++guard; }
    dump(a, dir + "/aftercomputer.raw");
    a.command(400);                       // About
    dump(a, dir + "/about.raw");
    a.key('\r', false);
    a.command(701);                       // Instructions
    for (int page = 1; page <= 7; ++page) {
        dump(a, dir + "/instr" + std::to_string(page) + ".raw");
        a.mouseDown(6 + 240, 17 + 165); a.mouseUp(6 + 240, 17 + 165);   // Next Page
    }
    a.mouseDown(6 + 150, 17 + 165); a.mouseUp(6 + 150, 17 + 165);       // Done
    dump(a, dir + "/afterdialog.raw");
    std::printf("ok\n");
}
