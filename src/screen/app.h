// The game screen and its event handling, ported procedure by procedure
// from the original program (procedure names in comments refer to
// docs/RECOVERED.md and the disassembly). Everything here is plain C++;
// the Qt layer only shows the canvas and forwards mouse, keys and ticks.
#pragma once
#include "canvas.h"
#include "../core/computer.h"
#include "../core/scoring.h"
#include <array>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace gs {

// A Control Manager control as the original uses it.
struct Control {
    enum Kind { Die, Category, Button } kind = Button;
    Rect rect;
    int value = 0;      // die face / category number
    int hilite = 0;     // 0 active, 0xFF inactive, or the part code while pressed
    int refCon = 0;     // the original keeps "selected" here
    bool exists = false;
};

// Player record (65 bytes in the original, at G03F3).
struct PlayerRecord {
    std::string name;
    int species = 1;                         // offset $0E: 1 human, 0 computer
    std::array<bool, 13> open{};             // $1A..
    std::array<int, 13> score{};             // $27..
    int upper = 0, lower = 0, bonus = 0, grand = 0;   // $10, $12, $14, $16
    int series = 0;                          // $18
    int yahtzeeBonus = 0;                    // port option: bonus points earned
};

// The port's own options (Game > Options...), not in the original.
struct Options {
    bool yahtzeeBonus = false;   // 100 per extra Yahtzee, joker placement
    bool modernRoll = false;     // dice from the OS entropy source
};

// A modal dialog in the style of the IIgs Dialog Manager.
// Button styles are the Control Manager's ctlFlag bits: 0 rounded, single
// outline; 3 square with a drop shadow.
struct DialogButton { Rect rect; std::string label; int item = 0; bool visible = true;
                      int style = 0; bool enabled = true; };
// Radio buttons (family > 0) and check boxes (family 0).
struct DialogCheck { Rect rect; std::string title; int item = 0; int family = 0; bool on = false;
                     bool enabled = true; };
struct Dialog {
    enum Kind { None, About, Instructions, Setup, Options } kind = None;
    Rect frame;                       // content rectangle, global coordinates
    int background = 1;               // solid erase colour
    std::vector<DialogButton> buttons;
    int page = 1;
    int pressed = -1;                 // button being tracked
    bool pressedInside = false;
    std::vector<uint8_t> savedScreen;
    int defaultItem = 1;              // chosen by Return
    // Setup only
    std::vector<DialogCheck> checks;
    Rect editRect;
    std::string editText;
    bool editSelected = true, editEnabled = true, caretOn = true;
    int caret = 0, blink = 0;
    std::array<std::pair<std::string, int>, 4> roster{};   // name, species
    int enrolled = 0;
    int pressedCheck = -1;
};

struct MenuItem { std::string text; int id; bool enabled = true; char key = 0; };
struct Menu { std::string title; int x = 0, width = 0; std::vector<MenuItem> items; };

class App {
public:
    explicit App(unsigned seed = std::random_device{}());

    Canvas& canvas() { return cv_; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }
    bool quitRequested() const { return quit_; }

    // Input in screen coordinates (0..319, 0..199).
    void mouseDown(int x, int y);
    void mouseMove(int x, int y);
    void mouseUp(int x, int y);
    void key(char c, bool command);
    void tick();                                    // 60 per second

    // Menu commands (also reachable from the Qt layer).
    void command(int itemId);

    // The port's options. The Qt layer loads them at start-up and saves
    // them when onOptionsChanged fires.
    const Options& options() const { return opt_; }
    void setOptions(const Options& o);
    std::function<void(const Options&)> onOptionsChanged;

    // --- test hooks ------------------------------------------------------
    PlayerRecord& record(int i) { return rec_[i]; }
    int playerCount() const { return nPlayers_; }
    void setPlayerCount(int n) { nPlayers_ = n; }
    int currentPlayer() const { return current_; }
    int rollNumber() const { return rollNo_; }
    const Control& die(int i) const { return ctl_[kDie0 + i]; }
    void setDieValues(const std::array<int, 5>& v);   // for tests: override the next roll
    int drawDie();                                   // one die under the current options
    bool busy() const { return !queue_.empty(); }
    bool dialogOpen() const { return dialog_.kind != Dialog::None; }
    int dialogPage() const { return dialog_.page; }
    bool gameInProgress() const { return gameInProgress_; }
    bool gameOver() const { return gameOver_; }

    // Control ids
    enum { kDie0 = 0, kCat0 = 5, kStatus = 18, kRoll = 19, kScore = 20, kControlCount = 21 };

private:
    // ----- drawing helpers (P4E28, P4E62, P4EAF, P4FA3, P5051, ...) -----
    void windowPort();
    void screenPort();
    void drawAt(int x, int y, const std::string& s);
    void drawCentered(int cx, int y, const std::string& s);
    void shadowText(bool center, const std::string& s, int x, int y, int shadow, int main, int bg);
    void shadowLine(int shadow, int main, int x, int y, int width, bool center);
    void panel(int left, int top, int right, int bottom, int ow, int oh, int fill, int frame);
    void useFont(int size);                          // 8, 16 or 32 (system font scaled)

    void drawTitle();                                // P007E
    void drawPanels();                               // P0021
    void clearCard();                                // P0779
    void drawTotalsLabels();                         // P019B
    // Score-card rows (top y of each 9-pixel text row). The original layout
    // is used unless the Yahtzee Bonus option adds its extra row.
    bool bonusLayout() const { return layoutBonus_; }
    int categoryTop(int cat) const;
    int rowUpperTotal() const;
    int rowBonus() const;
    int rowYahtzeeBonus() const;
    int rowLowerTotal() const;
    int rowGrandTotal() const;
    void drawTotal(int x, int row, int v, const char* label);
    void drawNumber(int x, int y, int v);            // P2DA9
    int columnX(int player) const;                   // P2725 (1-based player)
    int topGrand() const;                            // P26B6
    void drawColumn(int player, bool current);       // P2E2D
    void banner(const std::string& s, bool draw);    // P02C8
    void setStatus(const std::string& s);            // P064D
    void statusRoll(int n);                          // P0693
    void drawContent();                              // P04B8
    void drawMenuBar();
    void drawMenu(int m);

    // ----- controls ------------------------------------------------------
    void createControls();                           // P7C4E
    void killControls();
    void drawControl(int id);                        // CDEF drawCtl messages
    void drawDie(const Control& c);                  // P754D
    void drawCategory(const Control& c);             // P77D8
    void drawButton(int id, const Control& c);       // P7938
    void setCtl(int id, int hilite, int refCon);     // P251E
    void hiliteControl(int id, int hilite);
    void setValue(int id, int v);
    int hitControl(int lx, int ly) const;

    // ----- game flow -------------------------------------------------------
    void newOrQuitGame();                            // P18FD
    void defaultPlayers(bool names);                 // P7F24
    void rollDice();                                 // P15C0
    void rollOne(int i);                             // P1483
    void rollSelected();                             // P14E8
    void sortDice();                                 // tail of P15C0
    void categoriesInactive(int p);                  // P261C
    void categoriesActive(int p);                    // P2583
    void diceInactive();                             // P2761
    void syncCategories(int prev, int cur);          // P2CF0
    void scoreCategory();                            // P1384
    void nextTurn(int prev, int cur);                // P0F92
    void gameOverScreen();                           // P10BF
    void standings();                                // P09CC
    void playNext();                                 // P07C3
    void buttonHit(int id);                          // P1C67
    void dieHit(int id);                             // P1E8B
    void categoryHit(int id);                        // P1F95
    void showPossibilities(bool on);                 // P1DF0 / P1D50
    bool anyOpen(int p) const;                       // P0F1C (checks player p)
    void totals(PlayerRecord& r);                    // P27DB
    int yahtzeeBox(const PlayerRecord& r) const;
    int potential(int cat) const;                    // what the dice would score in cat
    int jokerChoice() const;                         // computer's box for a joker roll

    // ----- the computer (P20D6), run as timed steps ------------------------
    void computerTurn();
    void computerRollStep();
    void flash(int id, std::function<void()> then);  // P2FF2
    void later(int ticks, std::function<void()> fn);
    yahtzee::Dice diceValues() const;

    // ----- dialogs (P71EB About, P5BCB/P708A Instructions) -------------
    void openAbout();
    void openInstructions();
    void openSetup();                                   // P50ED
    void openOptions();
    void drawOptionsFrame();
    void optionsItemHit(int item);
    void drawSetupFrame();
    void drawCheck(const DialogCheck& c, bool pressed);
    void drawEditLine();
    void setupItemHit(int item);
    void setupKey(char c);
    void openDialog(Dialog::Kind kind, const Rect& frame, int background);
    void closeDialog();
    void drawDialog();
    void dialogPort();
    void drawDialogButton(const DialogButton& b, bool pressed);
    void drawAboutText();
    void drawInstructionsPage();
    void dialogItemHit(int item);
    bool dialogMouseDown(int x, int y);
    bool dialogMouseMove(int x, int y);
    bool dialogMouseUp(int x, int y);

    // ----- menus -------------------------------------------------------
    int menuAt(int x) const;
    int itemAt(int m, int x, int y) const;
    void closeMenu();
    void setItemEnabled(int id, bool on);
    void setItemText(int id, const std::string& s);

    Canvas cv_;
    std::mt19937 rng_;
    Options opt_;
    bool layoutBonus_ = false;   // controls were laid out with the bonus row
    bool dirty_ = true, quit_ = false;

    std::array<Control, kControlCount> ctl_{};
    std::array<PlayerRecord, 4> rec_{};
    std::array<std::pair<int, int>, 5> sorted_{};   // G04F7: value, die number (1-based)
    std::string rollLabel_ = "Roll";                 // G029E
    std::string status_;                             // G02A4
    std::string bannerText_;                         // G02C3

    int nPlayers_ = 2;           // G0012
    int current_ = 1;            // G0014 (1-based)
    int rollNo_ = 0;             // G000A
    int selectedCat_ = 0;        // G000C
    int gamesPlayed_ = 0;        // G0010
    int gamesPerSeries_ = 3;     // G000E
    bool showStandings_ = true;  // G0003
    bool standingsShown_ = false;// G0004
    bool computerTurnFlag_ = false; // G0005
    bool gameInProgress_ = false;// G0007
    bool gameOver_ = false;      // G0008
    bool seriesOver_ = true;     // G0009

    // tracking
    int tracking_ = -1;          // control being pressed
    bool trackingInside_ = false;
    int menuOpen_ = -1, menuItem_ = -1;
    std::vector<Menu> menus_;
    Dialog dialog_;

    struct Step { int wait; std::function<void()> fn; };
    std::deque<Step> queue_;
    int waitTicks_ = 0;
    std::array<int, 5> forced_{};  // test hook: next roll values
    bool haveForced_ = false;
};

} // namespace gs
