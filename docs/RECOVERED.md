# What the original program does

Notes recovered from the disassembly of YAHTZEE v1.1 (David W. Buell, 1988,
TML Pascal). Procedure names (`Pxxxx`) and globals (`Gxxxx`) refer to
`YAHTZEE_disasm.s`. Items marked *confirmed* were read directly from the code;
items marked *inferred* are strong guesses still to be checked.

## Display

- **Screen mode** (confirmed, P8CB2): QuickDraw II starts with master SCB `$00`
  and a 160-byte line width, so the game runs in **320×200, 16 colours**.
- **Palette** (confirmed so far): no calls that change the colour table have
  been found, so the game uses the standard IIgs 320-mode palette. The
  start-up screen draws white (colour 15) on black (colour 0).
- **Fonts** (from the doc file): Art Deco 36 for the title, Shaston 16 for
  text, falling back to the system font (Shaston 8). These are Apple's
  fonts and can't be shipped, so the port will use look-alike bitmap fonts.

## Program structure (confirmed)

| Procedure | Role |
|---|---|
| P2450 | Main program |
| P8CB2 | Start up tools (loads `*/SYSTEM/TOOLS`, shows "One Moment Please...") |
| P8B11 | Draws off-screen graphics: dice, card grid, category names (inferred) |
| P9014 | Builds the four menus |
| P1AB8 | Opens the game window |
| P2262 | Main event loop (TaskMaster) |
| P8F81 | Shuts down tools |
| P288F | Score for a category |
| P48B5 | Computer: plan for one category (calls P325A–P4580) |
| P4B2B | Computer: choose plan to follow, or category to score |
| P20D6 | Computer: whole turn |
| P1384 | Score the chosen category and pass the turn |
| P10BF | End of game: series totals, winner message |
| P27DB | Upper/lower/bonus/grand totals |
| P1483 / P14E8 | Roll one die / roll all dice not held |
| P0F1C | "Does the current player have any open category?" |
| P50ED | Setup dialog (players, species, games per series) |
| P6029–P6E41 | The seven help pages |

## Data (confirmed)

- **Player record**: 65 bytes (`$41`) each, array starting at G03F3.
  G0012 is the number of players, G0014 the current player (1-based).
  Offsets: `$10` upper total, `$12` lower total, `$14` bonus, `$16` grand
  total, `$18` series total, `$1A`–`$26` one "open" flag per category,
  `$27`–`$40` the thirteen category scores (words). `$00`–`$0F` is most
  likely the name (inferred).
- **Turn state**: G000A counts rolls this turn (reset to 0 after the third
  roll), G000C is the category picked. G0010 counts games played and G000E
  is games per series.
- **Dice**: five Control Manager controls (handles at G0145), whose values are
  the faces. A **sorted copy** (G04F7, 4-byte stride) is used for scoring.

## Rules (confirmed)

- A die is `Random mod 6 + 1` using QuickDraw's Random. The port uses
  `std::mt19937` seeded from the system at start-up (the original's seed
  can't be matched), or the OS random source per die with Modern Roll.
- Categories 1–13 in the usual order; the scoring for each is ported exactly
  in `src/core/scoring.cpp` and verified against all 7,776 rolls.
- Full house requires the triple and pair to be different values: a Yahtzee
  does **not** score 25 as a full house.
- Upper-section bonus: 35 points when the upper total is 63 or more.
- **No Yahtzee bonus and no joker rule.** Totals are just the sum of the
  thirteen boxes plus the upper bonus. (The port adds both as an option,
  Game ▸ Options…, off by default; see the README.)

## Turn and game flow (confirmed)

- You select the dice to **reroll** (not to keep). Roll is dimmed until at
  least one die is selected. Three rolls per turn; you can score after any.
- After scoring, play passes to the next player. The game ends when the last
  player has no open boxes. Each player's grand total is added to a series
  total; the top grand total wins ("Wins!" or "Winners Tied!", or "You Win!"
  in a one-player game).

## The computer player (confirmed, verified against the original)

Ported in `src/core/computer.cpp`. Verified by running the original 65816
code in an emulator (`tools/emu.py`) and comparing every one of 6,552 plans
(all rolls × categories × last-roll-or-not) and 5,000 random category
choices: `tests/data/original_vectors.txt`, checked by `test_computer`.

After each roll it rates every category with a "desirability" number and the
dice it would reroll to chase it (P48B5):

- **Upper boxes**: 5 per matching die; reroll the rest.
- **3/4 of a kind, Yahtzee**: rated high when already made; otherwise build on
  the best triple, the highest pair, or failing that a single die of 4+.
- **Full house**: made = 28; otherwise chase from a triple, two pairs or a pair.
- **Straights**: build up from the low dice if the middle die is 3 or less,
  otherwise down from the high die.
- **Chance**: never targeted; dice under 4 are marked for reroll.

It follows the plan of the highest-rated open category (ties go to the later
category) and stops early when that plan rerolls nothing. To score (P4B2B):
the highest-rated open category, except that a weak upper-section choice
(rating under 15) is replaced by the upper box that keeps the 63-point bonus
on pace (three of each number), or else the one that loses least ground; and
if the best rating is still under 11 it dumps the roll in the open box
nearest the bottom of the card (Chance first, then Yahtzee, and so on up to
3 of a kind).

With the port's Yahtzee Bonus option on, the planner is unchanged except
that a Yahtzee rolled after the Yahtzee box is filled is kept (no more
rolls) and placed in the open box worth most under the joker rule, the
matching upper box winning ties (`App::jokerChoice`).

In 4,000 simulated games it averages about 226 points, with a Yahtzee in
about a quarter of games and the upper bonus in about two thirds.

## Still to recover (next stage)
Nothing major; the Edit menu exists only for desk accessories.

## Dialogs (confirmed)

- About (P71EB): modal dialog (72,23)–(248,190) on cream (colour 1), default
  button " Okay " at (53,146)–(123,160); "YAHTZEE" in Shaston 16 and the
  credits in Shaston 8, embossed in olive (12) on dark green (4), with the
  distribution terms in orange. The port omits the three lines with the
  author's 1988 mailing address and phone number.
- Instructions (P5BCB, P708A, P5F3E, P5D97): modal dialog (6,17)–(314,196)
  on green, panel (8,5)–(299,156), buttons Last Page / Done / Next Page.
  Seven pages; each has a bold embossed Shaston 16 title with an underline,
  and lines of Shaston 8 drawn in XOR mode with inline codes: '^' bold
  orange, '<' plain cream, '>' olive, '~' white. Last Page is hidden on
  page 1 and Next Page on page 7.

## Screen (confirmed by running the original's drawing code)

The emulator in `tools/qdemu.py` runs the original's own drawing procedures
against a QuickDraw II / Control Manager imitation, which gives reference
pictures of the real screen. The C++ port matches them pixel for pixel
(`tools/compare_renders.py`).

- Palette 7 (P865D) for scanlines 13–199: `000 FFD 55F 009 032 B44 088 F00
  0FB FB0 022 DD1 690 A55 666 FFF`; the menu bar keeps the standard palette.
- Window (P1AB8): frameless, content (3,15)–(317,198), erase pattern solid
  colour 4 (dark green). Content is drawn by P04B8.
- Panels (P0021): rounded rects (12,3)–(245,164) and (255,3)–(302,164),
  fill 10, 2×1 frame in 12. Title panel (18,59)–(296,124) with "Yahtzee!".
- Controls (P7C4E): five dice (262, 7+31i, 36×30), thirteen category
  labels (24..244, 9 px rows from 7 and 81), and buttons Score (12..59),
  status bar (69..245) and Roll (255..302), all at y 167..180.
- Dice (P754D): pip shadow 3, highlight 2, body 1 (cream) / 6 selected /
  5 pressed, edge 0. Categories (P77D8): 8 normal, 7 selected, 11 pressed,
  6 unavailable. Buttons (P7938): white with black text, inverted when
  pressed, checkerboard-dimmed when unavailable, hidden (colour 4) when off.
- Score columns (P2725): right edge at 108 + p·(125 div players); current
  player yellow (11), others grey (14); winning total red (7).
- Totals are redrawn in place in copy mode. Shaston's digits share one
  width, so that was enough in the original; the port's GS8 font has a
  narrow '1', so the port clears the number cell first (`drawTotal`).
  Otherwise a total that becomes narrower, such as 33 → 51, left part of
  the old number showing ("351").
- Banners (P02C8): letters stacked vertically in the dice panel, Shaston 16,
  embossed (shadow colour 8, letter colour 6).
- Menus: Apple (About Yahtzee), File (Quit), Edit, Game (New Game ↔ Quit
  Game, Setup… [disabled during a game], Instructions).
- Fonts: the original wants Art Deco 36 for the title and falls back to
  Shaston 32 when it isn't installed; the port takes that fallback using its
  own GS8 font scaled 4×.

- Setup (P50ED): modal dialog (10,19)–(310,194) on cream. White panels
  "Players" (6,12)–(149,169) and "Options" (156,55)–(294,119) with tab
  labels, "Setup" in bold Shaston 16 at (182,27). Items: 1 Enroll (square
  with shadow, the default), 2 Cancel, 3 Play, 4 name edit line (13 chars),
  5/6 Human/Computer radios, 7 View Standings check box, 8–13 games per
  series radios 1–6. Each Enroll adds the typed name and species to the
  roster (computers marked with a red apple, character $14), then loads the
  next player's current name and species. After four, the field reads
  "Roster Full", the species radios are disabled, Enroll is removed and
  Play becomes the default. Play copies the roster, the games per series
  and the standings option, sets the player count to the number enrolled
  (unchanged if none) and starts a new game; Cancel discards everything.
- Button styles (ctlFlag): About, Instructions and Enroll use 3 (square with
  drop shadow); Cancel and Play use 0 (rounded).
