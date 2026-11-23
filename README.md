# Yahtzee for KDE Plasma

A native Linux port of **YAHTZEE v1.1** for the Apple IIgs (David W. Buell, 1988,
written in TML Pascal; "Distribute freely but do not sell"), recovered from
the original program and rebuilt in C++ with a thin Qt 6 shell. It keeps the
original's look: the 320×200 IIgs screen with the game's own palette. 

## How to play

The default is the original's: **Player #1 (you) against Player #2 (the
computer)**, three games to a series. To change it, choose **Game ▸ Setup…**
when no game is running: type a name, pick Human or Computer, press **Enroll**
(or Return) for each player in turn, set the options, then **Play**.

Choose **Game ▸ New Game** (Ctrl+G). Click **Roll**. Click the dice you want
to **reroll** (they turn teal), then Roll again — up to three rolls. Click a
category name and then **Score**. Hold the mouse on the long bar at the
bottom to see what each open category would score. Ctrl+Q quits. It's Yahtzee.

## Options

**Game ▸ Options…** has two extras that the 1988 original does not. Both are
off by default, so the game plays exactly as the original until you turn
them on, and both are remembered between runs.

- **Yahtzee Bonus**: once your Yahtzee box holds 50, every further Yahtzee
  earns 100 bonus points and can be scored in any open box.
- **Modern Roll**: every die comes straight from the operating system's
  random source (`getrandom`, the same pool as `/dev/urandom`) instead of
  the game's own seeded generator, with no rounding bias between faces.
  This can be switched at any time.


## Build (needs Qt 6 development packages)

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/yahtzee-gs


