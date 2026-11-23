#include "app.h"
#include <algorithm>
#include <cctype>
#ifdef __linux__
#include <sys/random.h>
#endif

namespace gs {

namespace {

#include "dice_shapes.inc"

const char* const kCategoryNames[13] = {
    "Ones", "Twos", "Threes", "Fours", "Fives", "Sixes", "3 of a kind",
    "4 of a kind", "Full House", "Sm Straight", "Lg Straight", "Yahtzee", "Chance"};

// The game's own colour table (P865D), installed as palette 7.
const std::array<uint16_t, 16> kGamePalette = {
    0x000, 0xFFD, 0x55F, 0x009, 0x032, 0xB44, 0x088, 0xF00,
    0x0FB, 0xFB0, 0x022, 0xDD1, 0x690, 0xA55, 0x666, 0xFFF};

// The seven Instructions pages (P6029..P6E41), text and baselines exactly as in
// the original. Inside the text: '^' bold orange, '<' plain cream,
// '>' olive, '~' white (P5D97).
struct InstructionLine { int y; const char* text; };
struct InstructionPage { const char* title; std::vector<InstructionLine> lines; };
const std::vector<InstructionPage>& instructionPages()
{
    static const std::vector<InstructionPage> pages = {
    {"YAHTZEE", {
        {48, "   ^YAHTZEE<  is a popular game for up "},
        {58, "to four players.  Using strategy and"},
        {68, "luck, each player scores the value"},
        {78, "of his five dice in one of thirteen"},
        {88, "categories."},
        {102, "   Begin by rolling all five dice, and,"},
        {112, "if you like, reroll any or all of the"},
        {122, "dice twice more before selecting a"},
        {132, "category for scoring.  Continue until"},
        {142, "all categories have been scored."}}},
    {"Scoring", {
        {48, "<   After each turn, the player must"},
        {58, "choose a category, even if it scores"},
        {68, "a zero.  Each of the categories has"},
        {78, "a rule for scoring. They are:"},
        {92, "^   Ones< - sum of all the 'ones'"},
        {102, "^   Twos< - sum of all the 'twos'"},
        {112, "^   Threes< - sum of all the 'threes'"},
        {122, "^   Fours< - sum of all the 'fours'"},
        {132, "^   Fives< - sum of all the 'fives'"},
        {142, "^   Sixes< - sum of all the 'sixes'"}}},
    {"Scoring", {
        {48, "^   3 of a kind< - Sum of all the dice"},
        {58, "^   4 of a kind< - Sum of all the dice"},
        {68, "^   Full House< - 25 points"},
        {78, ">         (3 of a kind plus 1 pair)"},
        {88, "^   Small Straight< - 30 points"},
        {98, ">         (Four dice in sequence)"},
        {108, "^   Large Straight< - 40 points"},
        {118, ">         (Five dice in sequence)"},
        {128, "^   Yahtzee< - 50 points"},
        {138, ">         (Five of a kind)"},
        {148, "^   Chance< - Sum of all the dice"}}},
    {"Scoring", {
        {48, "<   A 35 point bonus is awarded if the"},
        {58, "Upper Section totals 63 or more.  This"},
        {68, "is most easily achieved by rolling a"},
        {78, "three of a kind for each of the first"},
        {88, "six categories."},
        {102, "   If you're not sure of the scoring"},
        {112, "potential of your roll, click on the"},
        {122, "bar at the bottom of the screen and"},
        {132, "the possibilities will be shown until"},
        {142, "you release the mouse button."}}},
    {"Operation", {
        {48, "<   You select a die to be rerolled by"},
        {58, "clicking on it.  The die will change"},
        {68, "color when selected.  Similarly,"},
        {78, "click on the name of the category in"},
        {88, "which you wish to score."},
        {102, "<   If rerolling of dice is allowed, the"},
        {112, "Roll button is dimmed until at least"},
        {122, "one die is selected, at which time it"},
        {132, "appears normal and is activated.  The"},
        {142, "Score button works in a similar way."}}},
    {"Players", {
        {48, "<   The default configuration pits you"},
        {58, "against the computer, but up to four"},
        {68, "players of any species may play at a"},
        {78, "time."},
        {92, "   Just select ~Setup...< from the ~Game<"},
        {102, "menu and enter the player names and"},
        {112, "species you wish.  'Enroll' must be"},
        {122, "pressed to add a player to the roster."},
        {132, "'Cancel' will preserve the last set"},
        {142, "of contestants entered."}}},
    {"Options", {
        {48, "   <The default number of games in a"},
        {58, "series is three.  You may alter this"},
        {68, "by choosing ~Setup...< from the ~Game"},
        {78, "<menu.  One to six games may make up"},
        {88, "a series."},
        {102, "   At the conclusion of every game,"},
        {112, "each player's cumulative score is "},
        {122, "available for viewing.  If you do not "},
        {132, "desire this feature, it may be turned "},
        {142, "off from the ~Setup...< box."}}}
    };
    return pages;
}

constexpr int kInactive = 0xFF;
constexpr int kMenuBarHeight = 13;

// Menu item ids from the original menu definitions.
enum { idAbout = 400, idQuit = 402, idUndo = 250, idCut = 251, idCopy = 252, idPaste = 253,
       idClear = 254, idNewGame = 501, idSetup = 503, idInstructions = 701,
       idOptions = 504 /* the port's own */ };

// Modern Roll: one die straight from the operating system's entropy source
// (getrandom, the same pool as /dev/urandom), so there is no seeded sequence
// to settle into a pattern. Bytes 252..255 are thrown away so that each face
// gets exactly 42 of the 252 kept values (no modulo bias).
int osDie()
{
    for (;;) {
        unsigned char b = 0;
#ifdef __linux__
        if (getrandom(&b, 1, 0) != 1)
#endif
        {
            static std::random_device rd;
            b = static_cast<unsigned char>(rd() & 0xFF);
        }
        if (b < 252) return b % 6 + 1;
    }
}

} // namespace

// =================================================================== setup

App::App(unsigned seed) : rng_(seed)
{
    for (int y = 0; y < Canvas::H; ++y) cv_.setSCB(y, y < kMenuBarHeight ? 0 : 7);
    cv_.setColorTable(7, kGamePalette);
    cv_.clear(0);

    menus_ = {
        {"@", 0, 0, {{"About Yahtzee", idAbout}, {"-", 0, false}}},
        {"File", 0, 0, {{"Quit", idQuit, true, 'Q'}}},
        {"Edit", 0, 0, {{"Undo", idUndo, false, 'Z'}, {"-", 0, false}, {"Cut", idCut, false, 'X'},
                        {"Copy", idCopy, false, 'C'}, {"Paste", idPaste, false, 'V'},
                        {"Clear", idClear, false}}},
        {"Game", 0, 0, {{"New Game", idNewGame, true, 'G'}, {"Setup...", idSetup},
                        {"Options...", idOptions}, {"-", 0, false},
                        {"Instructions", idInstructions, true, 'I'}}},
    };

    defaultPlayers(true);                           // P7F24(1) during start-up
    drawMenuBar();
    drawContent();
}

void App::windowPort()
{
    cv_.setPort(3, 15, Rect{0, 0, 183, 314});
    cv_.setBackPattern(4);
}

void App::screenPort() { cv_.setPort(0, 0, Rect{0, 0, Canvas::H, Canvas::W}); }

void App::useFont(int size) { cv_.setFont(&systemFont(), size / 8); }

// ========================================================= drawing helpers

void App::drawAt(int x, int y, const std::string& s)            // P4E28
{
    cv_.moveTo(x, y);
    cv_.drawString(s);
    dirty_ = true;
}

void App::drawCentered(int cx, int y, const std::string& s)     // P4E62
{
    drawAt(cx - cv_.stringWidth(s) / 2, y, s);
}

void App::shadowText(bool center, const std::string& s, int x, int y,
                     int shadow, int main, int bg)                // P4EAF
{
    if (center) x -= cv_.stringWidth(s) / 2;
    cv_.moveTo(x, y);
    for (unsigned char c : s) {
        cv_.setForeColor(shadow ^ bg);
        cv_.setTextMode(modeForeXOR);
        cv_.drawChar(c);
        cv_.move(-cv_.charWidth(c) - 1, -1);
        cv_.setForeColor(main);
        cv_.setTextMode(modeForeCopy);
        cv_.drawChar(c);
        cv_.move(1, 1);
    }
    dirty_ = true;
}

void App::shadowLine(int shadow, int main, int x, int y, int width, bool center)  // P4FA3
{
    const int pw = cv_.penWidth(), ph = cv_.penHeight();
    cv_.setPenSize(1, 2);
    cv_.setSolidPenPat(shadow);
    if (center) x -= width / 2;
    cv_.moveTo(x, y);
    cv_.line(width, 0);
    cv_.move(-1, -1);
    cv_.setSolidPenPat(main);
    cv_.line(-width, 0);
    cv_.setPenSize(pw, ph);
}

void App::panel(int left, int top, int right, int bottom, int ow, int oh,
                int fill, int frame)                              // P5051
{
    const int pw = cv_.penWidth(), ph = cv_.penHeight();
    const Rect r = makeRect(left, top, right, bottom);
    cv_.setPenSize(2, 1);
    cv_.setSolidPenPat(fill);
    cv_.paintRRect(r, ow, oh);
    cv_.setSolidPenPat(frame);
    cv_.frameRRect(r, ow, oh);
    cv_.setPenSize(pw, ph);
    dirty_ = true;
}

void App::drawTitle()                                            // P007E
{
    windowPort();
    cv_.eraseRect(makeRect(0, 0, 314, 183));
    dirty_ = true;
    if (gameInProgress_ || gameOver_) return;
    panel(18, 59, 296, 124, 13, 13, 10, 12);
    // The original asks for Art Deco 36 and falls back to Shaston 32 when it
    // isn't installed; the port takes the fallback (the system font, scaled).
    useFont(32);
    const int ascent = systemFont().ascent() * cv_.fontScale();
    shadowText(true, "Yahtzee!", 158, 91 + ascent / 2, 8, 6, 10);
    useFont(8);
}

void App::drawPanels()                                           // P0021
{
    windowPort();
    panel(12, 3, 245, 164, 13, 13, 10, 12);
    panel(255, 3, 302, 164, 13, 13, 10, 12);
}

void App::clearCard()                                            // P0779
{
    cv_.setSolidPenPat(10);
    cv_.paintRect(makeRect(24, 4, 233, 163));
    dirty_ = true;
}

void App::drawTotalsLabels()                                     // P019B
{
    windowPort();
    cv_.setTextMode(modeForeCopy);
    cv_.setForeColor(13);
    drawAt(24, rowUpperTotal() + 7, "Upper Total");
    drawAt(24, rowBonus() + 7, "Bonus");
    if (layoutBonus_) drawAt(24, rowYahtzeeBonus() + 7, "Yahtzee Bonus");
    drawAt(24, rowLowerTotal() + 7, "Lower Total");
    cv_.setForeColor(9);
    drawAt(24, rowGrandTotal() + 7, "Grand Total");
}

// Row positions. The original layout (Yahtzee Bonus off) is exactly the
// original's: categories in 9-pixel rows from 7 and 81, totals below each
// section. With the bonus row the card needs one more row than the panel
// holds at 9 pixels, so the categories move up a pixel or four and the
// rows below Sixes and Chance use an 8-pixel pitch (their labels have no
// descenders that could touch the row below).
int App::categoryTop(int cat) const
{
    if (!layoutBonus_) return cat <= 6 ? 7 + 9 * (cat - 1) : 81 + 9 * (cat - 7);
    return cat <= 6 ? 6 + 9 * (cat - 1) : 77 + 9 * (cat - 7);
}
int App::rowUpperTotal() const { return categoryTop(6) + 9; }
int App::rowBonus() const { return rowUpperTotal() + (layoutBonus_ ? 8 : 9); }
int App::rowYahtzeeBonus() const { return categoryTop(13) + 8; }
int App::rowLowerTotal() const { return layoutBonus_ ? rowYahtzeeBonus() + 8 : categoryTop(13) + 9; }
int App::rowGrandTotal() const { return rowLowerTotal() + (layoutBonus_ ? 8 : 9); }

void App::drawTotal(int x, int row, int v, const char* label)
{
    // Totals are redrawn in place in copy mode, which only repaints the
    // background under the new string. Shaston's digits all have one width,
    // so the original never needed more; the port's font has a narrow '1',
    // so a shorter new number (33 -> 51, 158 -> 191) left the edge of the
    // old one showing. Clear the number cell (the digits' 8 rows, at least
    // as wide as the widest likely number) and draw the digits over it.
    // On rows that were already background this gives exactly the
    // original's pixels, and it never paints into the row below, which the
    // bonus layout's 8-pixel rows need.
    // The cell never reaches back into the row's label (in a four-player
    // game the first column sits close to "Yahtzee Bonus").
    int w = std::min(30, 125 / nPlayers_ - 3);
    w = std::min(w, x - (24 + cv_.stringWidth(label) + 1));
    w = std::max(w, cv_.stringWidth(std::to_string(v)));
    cv_.setSolidPenPat(10);
    cv_.paintRect(makeRect(x - w, row, x, row + 8));
    const int mode = cv_.textMode();
    cv_.setTextMode(modeForeCopy);
    drawNumber(x, row, v);
    cv_.setTextMode(mode);
}

void App::drawNumber(int x, int y, int v)                        // P2DA9
{
    cv_.setBackColor(10);
    const std::string s = std::to_string(v);
    drawAt(x - cv_.stringWidth(s), y + 7, s);
}

int App::columnX(int player) const { return 108 + player * (125 / nPlayers_); }  // P2725

int App::topGrand() const                                        // P26B6
{
    int top = 0;
    for (int p = 0; p < nPlayers_; ++p) top = std::max(top, rec_[p].grand);
    return top;
}

void App::drawColumn(int player, bool current)                   // P2E2D
{
    const PlayerRecord& r = rec_[player - 1];
    const int x = columnX(player);
    cv_.setForeColor(current ? 11 : 14);
    cv_.setTextMode(modeForeCopy);
    for (int c = 0; c < 13; ++c)
        if (!r.open[c]) drawNumber(x, ctl_[kCat0 + c].rect.top, r.score[c]);
    cv_.setTextMode(modeCopy);
    drawTotal(x, rowUpperTotal(), r.upper, "Upper Total");
    if (r.bonus != 0) drawTotal(x, rowBonus(), r.bonus, "Bonus");
    if (layoutBonus_ && r.yahtzeeBonus != 0) drawTotal(x, rowYahtzeeBonus(), r.yahtzeeBonus, "Yahtzee Bonus");
    drawTotal(x, rowLowerTotal(), r.lower, "Lower Total");
    if (r.grand == topGrand() && gameOver_) cv_.setForeColor(7);
    drawTotal(x, rowGrandTotal(), r.grand, "Grand Total");
}

void App::banner(const std::string& s, bool draw)                // P02C8
{
    if (s.empty()) return;
    windowPort();
    cv_.setSolidPenPat(10);
    cv_.paintRect(makeRect(259, 5, 294, 162));
    dirty_ = true;
    bannerText_.clear();
    if (!draw) return;
    useFont(16);
    int total = 0;
    for (char c : s) total += c == ' ' ? 9 : 17;
    int y = 85 - total / 2 + 14;
    for (char c : s) {
        shadowText(true, std::string(1, c), 279, y, 8, 6, 10);
        y += c == ' ' ? 9 : 17;
    }
    useFont(8);
    bannerText_ = s;
}

void App::setStatus(const std::string& s)                        // P064D
{
    status_ = s.substr(0, 31);
    drawControl(kStatus);
}

void App::statusRoll(int n)                                      // P0693
{
    std::string s = "Roll #" + std::to_string(n);
    if (nPlayers_ > 1) s = rec_[current_ - 1].name + " - " + s;
    setStatus(s);
}

void App::drawContent()                                          // P04B8
{
    drawTitle();
    if (!gameInProgress_) return;
    drawPanels();
    if (standingsShown_) { standings(); return; }
    for (int c = 0; c < 13; ++c)
        if (rec_[current_ - 1].open[c]) ctl_[kCat0 + c].hilite = 0;
    for (int i = 0; i < kControlCount; ++i)
        if (ctl_[i].exists && ctl_[i].kind != Control::Button) drawControl(i);
    for (int i = kStatus; i <= kScore; ++i) drawControl(i);
    if (ctl_[kScore].refCon != 1) categoriesInactive(current_);
    drawTotalsLabels();
    for (int p = 1; p <= nPlayers_; ++p)
        drawColumn(p, (p == current_ && !gameOver_) || nPlayers_ == 1);
    banner(bannerText_, true);
}

// ================================================================ controls

void App::createControls()                                       // P7C4E
{
    windowPort();
    for (int i = 0; i < 5; ++i) {
        Control& c = ctl_[kDie0 + i];
        c = Control{};
        c.kind = Control::Die;
        c.rect = makeRect(262, 7 + 31 * i, 298, 37 + 31 * i);
        c.exists = true;
        drawControl(kDie0 + i);
    }
    for (int cat = 13; cat >= 1; --cat) {
        Control& c = ctl_[kCat0 + cat - 1];
        c = Control{};
        c.kind = Control::Category;
        const int top = categoryTop(cat);
        c.rect = makeRect(24, top, 244, top + 9);
        c.value = cat;
        c.exists = true;
        drawControl(kCat0 + cat - 1);
    }
    const Rect rects[3] = {makeRect(69, 167, 245, 180), makeRect(255, 167, 302, 180),
                           makeRect(12, 167, 59, 180)};
    for (int i = 0; i < 3; ++i) {
        Control& c = ctl_[kStatus + i];
        c = Control{};
        c.kind = Control::Button;
        c.rect = rects[i];
        c.exists = true;
    }
}

void App::killControls()
{
    for (auto& c : ctl_) c.exists = false;
}

void App::drawControl(int id)
{
    const Control& c = ctl_[id];
    if (!c.exists) return;
    windowPort();
    switch (c.kind) {
    case Control::Die: drawDie(c); break;
    case Control::Category: drawCategory(c); break;
    case Control::Button: drawButton(id, c); break;
    }
    dirty_ = true;
}

void App::drawDie(const Control& c)                              // P754D
{
    const int v = c.value;
    const auto& shape = kDiceShapes[v];
    auto paint = [&](char which, int color) {
        cv_.setSolidPenPat(color);
        std::vector<std::pair<int, int>> pts;
        for (int y = 0; y < 30; ++y)
            for (int x = 0; x < 36; ++x)
                if (shape[y][x] == which) pts.push_back({c.rect.left + x, c.rect.top + y});
        cv_.paintPoints(pts);
    };
    if (v > 0) { paint('S', 3); paint('H', 2); }
    int body;
    if (v == 0) body = 10;
    else if (c.hilite == 0x21) body = 5;
    else if (c.refCon == 0) body = 1;
    else body = 6;
    paint('b', body);
    paint('E', v > 0 ? 0 : 10);
}

void App::drawCategory(const Control& c)                         // P77D8
{
    int color;
    if (c.hilite == 0x22) color = 11;
    else if (c.hilite == kInactive) color = 6;
    else if (c.refCon == 0) color = 8;
    else color = 7;
    cv_.setTextMode(modeForeCopy);
    cv_.setForeColor(color);
    drawAt(c.rect.left, c.rect.top + 7, kCategoryNames[c.value - 1]);
}

void App::drawButton(int id, const Control& c)                   // P7938
{
    int frame = 12, text, fill;
    bool dim = false;
    std::string label;
    if (id == kStatus) {
        text = 11; fill = 10;
        label = c.hilite == 2 ? "Score Possibilities" : status_;
    } else {
        text = 0; fill = 15;
        if (c.hilite == 2) { text = 15; fill = 0; }
        else if (c.hilite == kInactive) {
            if (c.refCon == 0) { text = fill = frame = 4; }
            else if (c.refCon == 1) dim = true;
        }
        label = id == kRoll ? rollLabel_ : "Score";
    }
    Rect r = c.rect;
    cv_.setPenSize(2, 1);
    cv_.setSolidPenPat(frame);
    cv_.frameRRect(r, 14, 14);
    cv_.setPenSize(1, 1);
    r = Rect{r.top + 1, r.left + 2, r.bottom - 1, r.right - 2};
    cv_.setSolidPenPat(fill);
    cv_.paintRRect(r, 10, 10);
    cv_.setForeColor(text);
    cv_.setTextMode(modeForeCopy);
    drawCentered((r.left + r.right) / 2, r.top + 9, label);
    if (dim) {
        const auto saved = cv_.penMask();
        cv_.setPenMask(Canvas::grayMask());
        cv_.paintRRect(r, 10, 10);
        cv_.setPenMask(saved);
    }
}

void App::setCtl(int id, int hilite, int refCon)                 // P251E
{
    ctl_[id].hilite = hilite;
    ctl_[id].refCon = refCon;
    drawControl(id);
}

void App::hiliteControl(int id, int hilite)
{
    ctl_[id].hilite = hilite;
    drawControl(id);
}

void App::setValue(int id, int v)
{
    ctl_[id].value = v;
    drawControl(id);
}

int App::hitControl(int lx, int ly) const
{
    for (int i = 0; i < kControlCount; ++i)
        if (ctl_[i].exists && ctl_[i].rect.contains(lx, ly)) return i;
    return -1;
}

// =============================================================== game flow

void App::defaultPlayers(bool names)                             // P7F24
{
    for (int i = 0; i < 4; ++i) {
        PlayerRecord& r = rec_[i];
        if (names) {
            r.name = "Player #" + std::to_string(i + 1);
            r.species = i == 1 ? 0 : 1;
        }
        r.upper = r.lower = r.grand = r.series = r.bonus = r.yahtzeeBonus = 0;
        r.open.fill(true);
        r.score.fill(0);
    }
}

void App::newOrQuitGame()                                        // P18FD
{
    if (gameInProgress_) {
        standingsShown_ = computerTurnFlag_ = gameInProgress_ = gameOver_ = false;
        seriesOver_ = true;
        gamesPlayed_ = 0;
        setItemEnabled(idSetup, true);
        setItemText(idNewGame, "New Game");
        killControls();
        drawTitle();
        return;
    }
    setItemEnabled(idSetup, false);
    setItemText(idNewGame, "Quit Game");
    gamesPlayed_ = 0;
    standingsShown_ = seriesOver_ = gameOver_ = false;
    gameInProgress_ = true;
    layoutBonus_ = opt_.yahtzeeBonus;               // fixed for the whole series
    rollNo_ = 0;
    current_ = 1;
    defaultPlayers(false);
    computerTurnFlag_ = rec_[0].species == 0;
    drawTitle();
    drawPanels();
    createControls();
    drawTotalsLabels();
    categoriesInactive(1);
    for (int p = 1; p <= nPlayers_; ++p) drawColumn(p, p == 1);
    statusRoll(1);
    diceInactive();
    banner("GAME 1", true);
    rollLabel_ = "Roll";
    setCtl(kRoll, 0, 1);
}

void App::setDieValues(const std::array<int, 5>& v) { forced_ = v; haveForced_ = true; }

int App::drawDie()
{
    return opt_.modernRoll ? osDie() : std::uniform_int_distribution<int>(1, 6)(rng_);
}

void App::rollOne(int i)                                         // P1483
{
    ctl_[kDie0 + i].refCon = 0;
    int v = drawDie();
    if (haveForced_) v = forced_[i];
    setValue(kDie0 + i, v);
}

void App::rollSelected()                                         // P14E8
{
    for (int i = 0; i < 5; ++i)
        if (ctl_[kDie0 + i].refCon == 1) setValue(kDie0 + i, 0);
    for (int i = 0; i < 5; ++i)
        if (ctl_[kDie0 + i].refCon == 1) rollOne(i);
}

void App::sortDice()
{
    for (int i = 0; i < 5; ++i) sorted_[i] = {ctl_[kDie0 + i].value, i + 1};
    bool swapped;
    do {                                   // the original's bubble sort (stable)
        swapped = false;
        for (int i = 0; i < 4; ++i)
            if (sorted_[i + 1].first < sorted_[i].first) { std::swap(sorted_[i], sorted_[i + 1]); swapped = true; }
    } while (swapped);
}

void App::rollDice()                                             // P15C0
{
    ++rollNo_;
    switch (rollNo_) {
    case 1:
        banner(" ", false);
        for (int i = 0; i < 5; ++i) { rollOne(i); ctl_[kDie0 + i].hilite = 0; }
        setCtl(kRoll, kInactive, 1);
        statusRoll(2);
        categoriesActive(current_);
        break;
    case 2:
        rollSelected();
        setCtl(kRoll, kInactive, 1);
        statusRoll(3);
        break;
    case 3:
        rollSelected();
        setCtl(kRoll, kInactive, 0);
        setStatus("Select Category");
        for (int i = 0; i < 5; ++i) ctl_[kDie0 + i].hilite = kInactive;
        rollNo_ = 0;
        break;
    }
    haveForced_ = false;
    sortDice();
    if (layoutBonus_ && yahtzee::earnsYahtzeeBonus(diceValues(), yahtzeeBox(rec_[current_ - 1])))
        setStatus("Yahtzee Bonus! +100");
}

void App::categoriesInactive(int p)                              // P261C
{
    for (int c = 0; c < 13; ++c)
        if (rec_[p - 1].open[c]) ctl_[kCat0 + c].hilite = kInactive;
    setCtl(kScore, kInactive, 0);
}

void App::categoriesActive(int p)                                // P2583
{
    for (int c = 0; c < 13; ++c)
        if (rec_[p - 1].open[c]) ctl_[kCat0 + c].hilite = 0;
    setCtl(kScore, kInactive, 1);
}

void App::diceInactive()                                         // P2761
{
    for (int i = 0; i < 5; ++i) {
        ctl_[kDie0 + i].hilite = kInactive;
        setValue(kDie0 + i, 0);
    }
}

void App::syncCategories(int prev, int cur)                      // P2CF0
{
    for (int c = 0; c < 13; ++c)
        if (rec_[prev - 1].open[c] != rec_[cur - 1].open[c])
            hiliteControl(kCat0 + c, rec_[cur - 1].open[c] ? 0 : kInactive);
}

void App::totals(PlayerRecord& r)                                // P27DB
{
    const yahtzee::Totals t = yahtzee::computeTotals(r.score, layoutBonus_ ? r.yahtzeeBonus : 0);
    r.upper = t.upper; r.lower = t.lower; r.bonus = t.bonus; r.grand = t.grand;
}

int App::yahtzeeBox(const PlayerRecord& r) const
{
    return r.open[yahtzee::Yahtzee - 1] ? yahtzee::kYahtzeeBoxOpen : r.score[yahtzee::Yahtzee - 1];
}

int App::potential(int cat) const
{
    const yahtzee::Dice d = diceValues();
    if (!layoutBonus_) return yahtzee::scoreCategory(cat, d);
    return yahtzee::scoreWithJoker(cat, d, yahtzeeBox(rec_[current_ - 1]));
}

int App::jokerChoice() const
{
    // The computer with a Yahtzee and its Yahtzee box already filled: take
    // the open box worth most; on a tie prefer the matching upper box (it
    // helps the 35 bonus), otherwise the later box, as the original does.
    const PlayerRecord& r = rec_[current_ - 1];
    const int face = ctl_[kDie0].value;
    int best = 0, bestPts = -1;
    for (int c = 1; c <= 13; ++c) {
        if (!r.open[c - 1]) continue;
        const int pts = potential(c);
        if (pts > bestPts || (pts == bestPts && best != face)) { best = c; bestPts = pts; }
    }
    return best;
}

yahtzee::Dice App::diceValues() const
{
    yahtzee::Dice d{};
    for (int i = 0; i < 5; ++i) d[i] = ctl_[kDie0 + i].value;
    return d;
}

bool App::anyOpen(int p) const
{
    const auto& o = rec_[p - 1].open;
    return std::any_of(o.begin(), o.end(), [](bool b) { return b; });
}

void App::scoreCategory()                                        // P1384
{
    const int c = selectedCat_;
    setCtl(kCat0 + c - 1, kInactive, 0);
    setCtl(kScore, kInactive, 0);
    PlayerRecord& r = rec_[current_ - 1];
    r.score[c - 1] = potential(c);                  // before the box closes
    if (layoutBonus_ && yahtzee::earnsYahtzeeBonus(diceValues(), yahtzeeBox(r)))
        r.yahtzeeBonus += yahtzee::kYahtzeeBonus;
    r.open[c - 1] = false;
    totals(r);
    drawColumn(current_, true);
    diceInactive();
    rollNo_ = 0;
    selectedCat_ = 0;
    computerTurnFlag_ = false;
    const int prev = current_;
    if (++current_ > nPlayers_) current_ = 1;
    if (anyOpen(nPlayers_)) nextTurn(prev, current_);
    else gameOverScreen();
}

void App::nextTurn(int prev, int cur)                            // P0F92
{
    if (nPlayers_ > 1) drawColumn(prev, false);
    drawColumn(cur, true);
    syncCategories(prev, cur);
    categoriesInactive(cur);
    if (rec_[cur - 1].species == 0) computerTurnFlag_ = true;
    else banner("ROLL DICE", true);
    statusRoll(1);
    setCtl(kRoll, 0, 1);
}

void App::gameOverScreen()                                       // P10BF
{
    gameOver_ = true;
    if (++gamesPlayed_ == gamesPerSeries_) seriesOver_ = true;
    setCtl(kRoll, kInactive, 0);
    int winners = 0;
    std::string msg;
    for (int p = 1; p <= nPlayers_; ++p) {
        drawColumn(p, nPlayers_ == 1);
        PlayerRecord& r = rec_[p - 1];
        r.series += r.grand;
        if (r.grand == topGrand()) {
            if (++winners == 1) msg = r.name + " Wins!";
            else msg = "Winners Tied!";
        }
    }
    if (nPlayers_ == 1) msg = "You Win!";
    setStatus(msg);
    if (showStandings_) {
        banner("STANDINGS", true);
        rollLabel_ = "View";
        setCtl(kRoll, 0, 1);
    } else if (seriesOver_) {
        banner("GAME OVER", true);
        later(120, [this] { setStatus("Click for New Game"); });
    } else {
        banner("NEXT GAME", true);
        rollLabel_ = "Play";
        setCtl(kRoll, 0, 1);
    }
}

void App::standings()                                            // P09CC
{
    standingsShown_ = true;
    windowPort();
    clearCard();
    if (seriesOver_) {
        banner("GAME OVER", true);
        setCtl(kRoll, kInactive, 0);
    } else {
        banner("NEXT GAME", true);
        rollLabel_ = "Play";
        setCtl(kRoll, 0, 1);
    }
    useFont(16);
    shadowText(true, "Standings", 129, 55, 1, 14, 10);
    shadowLine(1, 14, 128, 60, cv_.stringWidth("Standings"), true);
    std::string sub;
    if (seriesOver_) sub = "After all games played...";
    else sub = "After " + std::to_string(gamesPlayed_) + " of " + std::to_string(gamesPerSeries_) +
               " games played...";
    if (gamesPerSeries_ == 1) sub = " ";
    useFont(8);
    shadowText(true, sub, 129, 74, 13, 9, 10);

    std::vector<int> order;
    for (int p = 0; p < nPlayers_; ++p) order.push_back(p);
    bool swapped;
    do {
        swapped = false;
        for (int i = 0; i + 1 < (int)order.size(); ++i)
            if (rec_[order[i + 1]].series > rec_[order[i]].series) { std::swap(order[i], order[i + 1]); swapped = true; }
    } while (swapped);

    cv_.setForeColor(11);
    for (int i = 1; i <= nPlayers_; ++i) {
        const PlayerRecord& r = rec_[order[i - 1]];
        const std::string v = std::to_string(r.series);
        const int y = 104 - 5 * nPlayers_ + 10 * i;
        drawAt(56, y, r.name);
        drawAt(198 - cv_.stringWidth(v), y, v);
    }
    std::string msg;
    if (nPlayers_ > 1 && rec_[order[0]].series == rec_[order[1]].series) msg = "Leaders Tied!";
    else msg = rec_[order[0]].name + "'s Ahead!";
    if (nPlayers_ == 1) msg = "You're Bound To Win!";
    if (seriesOver_) msg = "Click to Play Again";
    setStatus(msg);
}

void App::playNext()                                             // P07C3
{
    windowPort();
    clearCard();
    standingsShown_ = gameOver_ = false;
    for (int p = 0; p < nPlayers_; ++p) {
        PlayerRecord& r = rec_[p];
        r.upper = r.bonus = r.lower = r.grand = r.yahtzeeBonus = 0;
        r.score.fill(0);
        r.open.fill(true);
    }
    banner("GAME " + std::to_string(gamesPlayed_ + 1), true);
    if (rec_[0].species == 0) computerTurnFlag_ = true;
    for (int c = 0; c < 13; ++c) setCtl(kCat0 + c, 0, 0);
    categoriesInactive(1);
    drawTotalsLabels();
    for (int p = 1; p <= nPlayers_; ++p) drawColumn(p, p == 1);
    statusRoll(1);
    rollLabel_ = "Roll";
    setCtl(kRoll, 0, 1);
}

void App::buttonHit(int id)                                      // P1C67
{
    if (id == kScore) { scoreCategory(); return; }
    if (rollLabel_ == "Roll") rollDice();
    else if (rollLabel_ == "View") standings();
    else if (rollLabel_ == "Play") playNext();
}

void App::dieHit(int)                                            // P1E8B
{
    bool any = false;
    for (int i = 0; i < 5; ++i) any |= ctl_[kDie0 + i].refCon == 1;
    const bool rollActive = ctl_[kRoll].hilite != kInactive;
    if (rollActive && !any) setCtl(kRoll, kInactive, 1);
    else if (!rollActive && any) setCtl(kRoll, 0, 1);
}

void App::categoryHit(int id)                                    // P1F95
{
    if (ctl_[id].refCon == 0) {
        if (selectedCat_ != 0) setCtl(kCat0 + selectedCat_ - 1, 0, 0);
        selectedCat_ = ctl_[id].value;
        setCtl(id, 0, 1);
        if (ctl_[kScore].hilite == kInactive) setCtl(kScore, 0, 1);
    } else {
        selectedCat_ = 0;
        setCtl(id, 0, 0);
        setCtl(kScore, kInactive, 1);
    }
}

void App::showPossibilities(bool on)                             // P1DF0, P1D50
{
    hiliteControl(kStatus, on ? 2 : 0);
    windowPort();
    cv_.setForeColor(on ? 7 : 10);
    cv_.setTextMode(modeForeCopy);
    const PlayerRecord& r = rec_[current_ - 1];
    for (int c = 0; c < 13; ++c)
        if (r.open[c])
            drawNumber(columnX(current_), ctl_[kCat0 + c].rect.top, potential(c + 1));
}

// ============================================================ the computer

void App::later(int ticks, std::function<void()> fn) { queue_.push_back({ticks, std::move(fn)}); }

void App::flash(int id, std::function<void()> then)              // P2FF2
{
    const int e1 = (id == kRoll || id == kScore) ? 2 : 0;
    for (int k = 1; k <= 6; ++k) {
        later(0, [this, id, e1, k] {
            if (k % 2) setCtl(id, e1, 1);
            else setCtl(id, 0, 0);
        });
        later(4, [] {});
    }
    later(0, [this, id, e1, then] {
        if (e1 == 0) setCtl(id, 0, 1);
        then();
    });
}

void App::computerTurn()                                         // P20D6
{
    flash(kRoll, [this] { computerRollStep(); });
}

void App::computerRollStep()
{
    rollDice();
    const yahtzee::Dice d = diceValues();
    const yahtzee::PlanTable plans = yahtzee::planAll(d, rollNo_);
    yahtzee::ScoreCard card;
    const PlayerRecord& r = rec_[current_ - 1];
    card.open = r.open;
    card.score = r.score;
    const int target = yahtzee::chooseCategory(d, card, plans, false);
    // Yahtzee Bonus option: a Yahtzee with the Yahtzee box already filled
    // is kept and placed by jokerChoice(); the original's planner knows
    // nothing of the joker and might roll it away.
    const bool joker = layoutBonus_ && yahtzee::isYahtzee(d) &&
                       yahtzeeBox(r) != yahtzee::kYahtzeeBoxOpen;
    bool done = true;
    if (rollNo_ != 0 && target > 0 && !joker) {
        for (int die = 1; die <= 5; ++die) {
            bool flagged = false;
            for (int p = 0; p < 5; ++p)
                if (sorted_[p].second == die && plans[target - 1].reroll[p]) flagged = true;
            if (!flagged) continue;
            done = false;
            hiliteControl(kDie0 + die - 1, 0x21);
            ctl_[kDie0 + die - 1].refCon = 1;
            hiliteControl(kDie0 + die - 1, kInactive);
            hiliteControl(kRoll, 0);
        }
    }
    if (!done) { flash(kRoll, [this] { computerRollStep(); }); return; }

    setCtl(kScore, 0, 1);
    selectedCat_ = joker ? jokerChoice() : yahtzee::chooseCategory(d, card, plans, true);
    flash(kCat0 + selectedCat_ - 1, [this] {
        flash(kScore, [this] { scoreCategory(); });
    });
}

void App::tick()
{
    if (dialog_.kind == Dialog::Setup && dialog_.editEnabled && !dialog_.editSelected && ++dialog_.blink >= 30) {
        dialog_.blink = 0;
        dialog_.caretOn = !dialog_.caretOn;
        drawEditLine();
    }
    if (waitTicks_ > 0) { --waitTicks_; return; }
    while (!queue_.empty() && waitTicks_ == 0) {
        Step s = std::move(queue_.front());
        queue_.pop_front();
        s.fn();
        waitTicks_ = s.wait;
    }
    if (queue_.empty() && waitTicks_ == 0 && computerTurnFlag_ && gameInProgress_ && menuOpen_ < 0 &&
        !dialogOpen()) {
        computerTurnFlag_ = false;     // cleared again by P1384; stops re-entry
        computerTurn();
    }
}

// ================================================================== input

void App::mouseDown(int x, int y)
{
    if (dialogOpen()) { dialogMouseDown(x, y); return; }
    if (y < kMenuBarHeight) {
        const int m = menuAt(x);
        if (m >= 0) { menuOpen_ = m; menuItem_ = -1; drawMenu(m); }
        return;
    }
    if (busy() || menuOpen_ >= 0) return;
    const int lx = x - 3, ly = y - 15;
    if (lx < 0 || ly < 0 || lx >= 314 || ly >= 183) return;

    // P2262: at the end of a series the next click starts a new one.
    if (gameInProgress_ && seriesOver_ && (!showStandings_ || standingsShown_)) {
        killControls();
        gameInProgress_ = false;
        newOrQuitGame();
        return;
    }
    const int id = hitControl(lx, ly);
    if (id < 0) return;
    if (id == kStatus) {
        if (ctl_[kScore].refCon == 1) { tracking_ = kStatus; showPossibilities(true); }
        return;
    }
    if (ctl_[id].hilite == kInactive) return;
    tracking_ = id;
    trackingInside_ = true;
    const int part = ctl_[id].kind == Control::Die ? 0x21 : ctl_[id].kind == Control::Category ? 0x22 : 2;
    hiliteControl(id, part);
}

void App::mouseMove(int x, int y)
{
    if (dialogOpen()) { dialogMouseMove(x, y); return; }
    if (menuOpen_ >= 0) {
        int item = itemAt(menuOpen_, x, y);
        const int m = y < kMenuBarHeight ? menuAt(x) : -1;
        if (m >= 0 && m != menuOpen_) { closeMenu(); menuOpen_ = m; menuItem_ = -1; drawMenu(m); return; }
        if (item != menuItem_) { menuItem_ = item; drawMenu(menuOpen_); }
        return;
    }
    if (tracking_ < 0 || tracking_ == kStatus) return;
    const bool inside = ctl_[tracking_].rect.contains(x - 3, y - 15);
    if (inside != trackingInside_) {
        trackingInside_ = inside;
        const Control& c = ctl_[tracking_];
        const int part = c.kind == Control::Die ? 0x21 : c.kind == Control::Category ? 0x22 : 2;
        hiliteControl(tracking_, inside ? part : 0);
    }
}

void App::mouseUp(int x, int y)
{
    if (dialogOpen()) { dialogMouseUp(x, y); return; }
    if (menuOpen_ >= 0) {
        const int m = menuOpen_, item = itemAt(m, x, y);
        closeMenu();
        if (item >= 0 && menus_[m].items[item].enabled) command(menus_[m].items[item].id);
        return;
    }
    const int id = tracking_;
    tracking_ = -1;
    if (id < 0) return;
    if (id == kStatus) { showPossibilities(false); return; }
    const bool inside = ctl_[id].rect.contains(x - 3, y - 15);
    Control& c = ctl_[id];
    if (c.kind == Control::Die) {
        if (inside) c.refCon = c.refCon == 0 ? 1 : 0;
        hiliteControl(id, 0);
        if (inside) dieHit(id);
    } else {
        hiliteControl(id, 0);
        if (inside) {
            if (c.kind == Control::Category) categoryHit(id);
            else buttonHit(id);
        }
    }
}

void App::key(char c, bool cmd)
{
    if (dialogOpen()) {
        if (cmd) return;
        // Return chooses the default button, as in ModalDialog.
        if (c == '\r') {
            for (auto& b : dialog_.buttons)
                if (b.item == dialog_.defaultItem && b.visible && b.enabled) { dialogItemHit(b.item); return; }
            return;
        }
        if (dialog_.kind == Dialog::Setup) setupKey(c);
        return;
    }
    if (!cmd) return;
    c = char(std::toupper((unsigned char)c));
    for (auto& m : menus_)
        for (auto& it : m.items)
            if (it.key == c && it.enabled) { command(it.id); return; }
}

void App::command(int id)
{
    if (busy() || dialogOpen()) return;
    switch (id) {
    case idQuit: quit_ = true; break;
    case idNewGame: newOrQuitGame(); break;
    case idAbout: openAbout(); break;
    case idInstructions: openInstructions(); break;
    case idSetup: openSetup(); break;
    case idOptions: openOptions(); break;
    default: break;
    }
}

// ================================================================ dialogs

void App::dialogPort()
{
    const Rect& f = dialog_.frame;
    cv_.setPort(f.left, f.top, Rect{0, 0, f.bottom - f.top, f.right - f.left});
    cv_.setBackPattern(dialog_.background);
}

void App::openDialog(Dialog::Kind kind, const Rect& frame, int background)
{
    dialog_ = Dialog{};
    dialog_.kind = kind;
    dialog_.frame = frame;
    dialog_.background = background;
    dialog_.savedScreen.resize(Canvas::W * Canvas::H);
    for (int y = 0; y < Canvas::H; ++y)
        for (int x = 0; x < Canvas::W; ++x) dialog_.savedScreen[y * Canvas::W + x] = cv_.pixel(x, y);
}

void App::closeDialog()
{
    for (int y = 0; y < Canvas::H; ++y)
        for (int x = 0; x < Canvas::W; ++x) cv_.setPixel(x, y, dialog_.savedScreen[y * Canvas::W + x]);
    dialog_ = Dialog{};
    useFont(8);
    cv_.setTextFace(0);
    dirty_ = true;
}

void App::drawDialogButton(const DialogButton& b, bool pressed)
{
    // Control Manager simple buttons: black outline, white face, black text,
    // inverted while pressed. Style 0 is rounded, style 3 square with a drop
    // shadow; the default button (Return) gets a second, bold outline.
    dialogPort();
    useFont(8);
    cv_.setTextFace(0);
    Rect r = b.rect;
    cv_.setSolidPenPat(0);
    if (b.style == 3) {
        cv_.paintRect(Rect{r.top + 1, r.left + 2, r.bottom + 1, r.right + 2});   // shadow
        cv_.setPenSize(2, 1);
        cv_.frameRect(r);
    } else {
        cv_.setPenSize(2, 1);
        cv_.frameRRect(r, 14, 14);
        if (b.item == dialog_.defaultItem && (dialog_.kind == Dialog::Setup || dialog_.kind == Dialog::Options)) {
            cv_.setPenSize(3, 2);
            cv_.frameRRect(Rect{r.top - 3, r.left - 5, r.bottom + 3, r.right + 5}, 20, 20);
        }
    }
    cv_.setPenSize(1, 1);
    r = Rect{r.top + 1, r.left + 2, r.bottom - 1, r.right - 2};
    cv_.setSolidPenPat(pressed ? 0 : 15);
    if (b.style == 3) cv_.paintRect(r);
    else cv_.paintRRect(r, 10, 10);
    cv_.setForeColor(pressed ? 15 : 0);
    cv_.setTextMode(modeForeCopy);
    drawCentered((r.left + r.right) / 2, r.top + 9, b.label);
    if (!b.enabled) {
        const auto saved = cv_.penMask();
        cv_.setPenMask(Canvas::grayMask());
        cv_.setSolidPenPat(15);
        cv_.paintRect(r);
        cv_.setPenMask(saved);
    }
}

void App::drawDialog()
{
    // Frame of a modal dialog box, then the content in its erase colour.
    screenPort();
    const Rect& f = dialog_.frame;
    cv_.setPenSize(1, 1);
    cv_.setSolidPenPat(0);
    cv_.paintRect(Rect{f.top - 3, f.left - 6, f.bottom + 3, f.right + 6});
    cv_.setSolidPenPat(15);
    cv_.paintRect(Rect{f.top - 2, f.left - 4, f.bottom + 2, f.right + 4});
    cv_.setSolidPenPat(0);
    cv_.paintRect(Rect{f.top - 1, f.left - 2, f.bottom + 1, f.right + 2});
    dialogPort();
    cv_.eraseRect(Rect{0, 0, f.bottom - f.top, f.right - f.left});
    if (dialog_.kind == Dialog::About) {
        for (auto& b : dialog_.buttons) if (b.visible) drawDialogButton(b, false);
        drawAboutText();
    } else if (dialog_.kind == Dialog::Setup) {
        drawSetupFrame();
        for (auto& b : dialog_.buttons) if (b.visible) drawDialogButton(b, false);
        for (auto& c : dialog_.checks) drawCheck(c, false);
        drawEditLine();
    } else if (dialog_.kind == Dialog::Options) {
        drawOptionsFrame();
        for (auto& b : dialog_.buttons) if (b.visible) drawDialogButton(b, false);
        for (auto& c : dialog_.checks) drawCheck(c, false);
    } else {
        panel(8, 5, 299, 156, 13, 13, 10, 12);
        drawInstructionsPage();
        for (auto& b : dialog_.buttons) if (b.visible) drawDialogButton(b, false);
    }
    dirty_ = true;
}

void App::openAbout()                                            // P71EB
{
    openDialog(Dialog::About, makeRect(72, 23, 248, 190), 1);
    dialog_.buttons.push_back({makeRect(53, 146, 123, 160), " Okay ", 1, true, 3});
    drawDialog();
}

void App::drawAboutText()
{
    dialogPort();
    cv_.setTextFace(0);
    useFont(16);
    shadowText(false, "YAHTZEE", 35, 21, 4, 12, 1);
    useFont(8);
    shadowText(true, "By", 88, 33, 4, 12, 1);
    shadowText(true, "David W. Buell", 88, 46, 4, 12, 1);
    // The original also lists the author's 1988 mailing address and phone
    // number here (baselines 58, 68 and 78); the port leaves them out.
    drawCentered(88, 93, "Written in TML Pascal");
    drawCentered(88, 103, "September 25, 1988");
    drawCentered(88, 113, "Version 1.1");
    cv_.setForeColor(9);
    drawCentered(88, 128, "Distribute freely");
    drawCentered(88, 138, "but do not sell");
}

void App::openInstructions()                                     // P5BCB, P708A
{
    openDialog(Dialog::Instructions, makeRect(6, 17, 314, 196), 4);
    dialog_.buttons.push_back({makeRect(111, 161, 195, 174), "Done", 2, true, 3});
    dialog_.buttons.push_back({makeRect(17, 161, 101, 174), "Last Page", 3, false, 3});
    dialog_.buttons.push_back({makeRect(205, 161, 289, 174), "Next Page", 4, true, 3});
    dialog_.page = 1;
    drawDialog();
}

void App::drawInstructionsPage()                                 // P5F3E, P5D97
{
    const InstructionPage& pg = instructionPages()[dialog_.page - 1];
    dialogPort();
    cv_.setSolidPenPat(10);
    cv_.paintRect(makeRect(21, 9, 289, 154));
    useFont(16);
    cv_.setTextFace(1);
    shadowText(true, pg.title, 154, 27, 8, 6, 10);
    shadowLine(8, 6, 154, 31, cv_.stringWidth(pg.title), true);
    useFont(8);
    cv_.setTextMode(modeForeXOR);
    for (const auto& line : pg.lines) {
        cv_.moveTo(21, line.y);
        for (const char* p = line.text; *p; ++p) {
            switch (*p) {
            case '^': cv_.setTextFace(1); cv_.setForeColor(0xA ^ 9); break;
            case '<': cv_.setTextFace(0); cv_.setForeColor(0xA ^ 1); break;
            case '>': cv_.setForeColor(0xA ^ 12); break;
            case '~': cv_.setForeColor(0xA ^ 15); break;
            default: cv_.drawChar((unsigned char)*p);
            }
        }
    }
    cv_.setTextFace(0);
    dirty_ = true;
}

void App::dialogItemHit(int item)
{
    if (dialog_.kind == Dialog::About) { if (item == 1) closeDialog(); return; }
    if (dialog_.kind == Dialog::Setup) { setupItemHit(item); return; }
    if (dialog_.kind == Dialog::Options) { optionsItemHit(item); return; }
    if (item == 2) { closeDialog(); return; }
    const int pages = int(instructionPages().size());
    if (item == 3 && dialog_.page > 1) --dialog_.page;
    else if (item == 4 && dialog_.page < pages) ++dialog_.page;
    else return;
    // Last Page is hidden on page 1, Next Page on the last page.
    for (auto& b : dialog_.buttons) {
        const bool want = b.item == 2 || (b.item == 3 && dialog_.page > 1) || (b.item == 4 && dialog_.page < pages);
        if (want != b.visible) {
            b.visible = want;
            dialogPort();
            const Rect& r = b.rect;
            if (want) drawDialogButton(b, false);
            else cv_.eraseRect(Rect{r.top, r.left, r.bottom + 1, r.right + 2});
        }
    }
    drawInstructionsPage();
}

bool App::dialogMouseDown(int x, int y)
{
    const int lx = x - dialog_.frame.left, ly = y - dialog_.frame.top;
    for (size_t i = 0; i < dialog_.checks.size(); ++i) {
        const auto& c = dialog_.checks[i];
        dialogPort();
        Rect hit = c.rect;
        hit.right += 4 + cv_.stringWidth(c.title);
        if (c.enabled && hit.contains(lx, ly)) {
            dialog_.pressedCheck = int(i);
            dialog_.pressedInside = true;
            drawCheck(c, true);
            dirty_ = true;
            return true;
        }
    }
    if (dialog_.kind == Dialog::Setup && dialog_.editEnabled && dialog_.editRect.contains(lx, ly)) {
        // Click places the caret at the nearest character boundary.
        dialogPort();
        useFont(8);
        cv_.setTextFace(0);
        int px = dialog_.editRect.left + 3, pos = 0;
        while (pos < int(dialog_.editText.size())) {
            const int w = cv_.charWidth((unsigned char)dialog_.editText[pos]);
            if (lx < px + w / 2) break;
            px += w; ++pos;
        }
        dialog_.caret = pos;
        dialog_.editSelected = false;
        dialog_.caretOn = true; dialog_.blink = 0;
        drawEditLine();
        return true;
    }
    for (size_t i = 0; i < dialog_.buttons.size(); ++i) {
        const auto& b = dialog_.buttons[i];
        if (b.visible && b.enabled && b.rect.contains(lx, ly)) {
            dialog_.pressed = int(i);
            dialog_.pressedInside = true;
            drawDialogButton(b, true);
            dirty_ = true;
            return true;
        }
    }
    return false;
}

bool App::dialogMouseMove(int x, int y)
{
    if (dialog_.pressedCheck >= 0) {
        const auto& c = dialog_.checks[dialog_.pressedCheck];
        Rect hit = c.rect;
        hit.right += 4 + cv_.stringWidth(c.title);
        const bool inside = hit.contains(x - dialog_.frame.left, y - dialog_.frame.top);
        if (inside != dialog_.pressedInside) { dialog_.pressedInside = inside; drawCheck(c, inside); dirty_ = true; }
        return true;
    }
    if (dialog_.pressed < 0) return false;
    const auto& b = dialog_.buttons[dialog_.pressed];
    const bool inside = b.rect.contains(x - dialog_.frame.left, y - dialog_.frame.top);
    if (inside != dialog_.pressedInside) {
        dialog_.pressedInside = inside;
        drawDialogButton(b, inside);
        dirty_ = true;
    }
    return true;
}

bool App::dialogMouseUp(int x, int y)
{
    if (dialog_.pressedCheck >= 0) {
        const DialogCheck c = dialog_.checks[dialog_.pressedCheck];
        dialog_.pressedCheck = -1;
        drawCheck(c, false);
        dirty_ = true;
        if (dialog_.pressedInside) dialogItemHit(c.item);
        return true;
    }
    if (dialog_.pressed < 0) return false;
    const DialogButton b = dialog_.buttons[dialog_.pressed];
    dialog_.pressed = -1;
    drawDialogButton(b, false);
    dirty_ = true;
    if (b.rect.contains(x - dialog_.frame.left, y - dialog_.frame.top)) dialogItemHit(b.item);
    return true;
}

// =================================================================== setup

void App::openSetup()                                            // P50ED
{
    openDialog(Dialog::Setup, makeRect(10, 19, 310, 194), 1);
    for (int i = 0; i < 4; ++i) dialog_.roster[i] = {rec_[i].name, rec_[i].species};
    auto& d = dialog_;
    d.buttons.push_back({makeRect(49, 148, 105, 161), "Enroll", 1, true, 3});
    d.buttons.push_back({makeRect(165, 148, 223, 161), "Cancel", 2, true, 0});
    d.buttons.push_back({makeRect(239, 148, 284, 161), "Play", 3, true, 0});
    d.editRect = makeRect(31, 93, 141, 106);                   // item 4
    d.editText = d.roster[0].first.substr(0, 13);
    d.caret = int(d.editText.size());
    d.checks.push_back({makeRect(31, 121, 44, 131), "Human", 5, 2, d.roster[0].second == 1});
    d.checks.push_back({makeRect(31, 132, 44, 142), "Computer", 6, 2, d.roster[0].second != 1});
    d.checks.push_back({makeRect(165, 105, 178, 114), "View Standings", 7, 0, showStandings_});
    const int gx[3] = {174, 214, 254};
    for (int n = 1; n <= 6; ++n) {
        const int x = gx[(n - 1) / 2], y = (n - 1) % 2 ? 86 : 75;
        d.checks.push_back({makeRect(x, y, x + 13, y + 10), std::to_string(n) + " ", 7 + n, 1, gamesPerSeries_ == n});
    }
    drawDialog();
}

void App::drawSetupFrame()
{
    dialogPort();
    panel(6, 12, 149, 169, 12, 14, 15, 12);
    panel(156, 55, 294, 119, 12, 14, 15, 12);
    panel(38, 6, 117, 18, 12, 14, 15, 12);
    panel(186, 49, 264, 61, 12, 14, 15, 12);
    useFont(16);
    cv_.setTextFace(1);
    shadowText(false, "Setup", 182, 27, 0, 12, 1);
    shadowLine(0, 12, 182, 31, cv_.stringWidth("Setup"), false);
    useFont(8);
    cv_.setTextFace(1);
    cv_.setForeColor(0);
    drawAt(42, 15, " Players ");
    drawAt(190, 58, " Options ");
    cv_.setTextFace(0);
    cv_.setForeColor(12);
    drawAt(14, 90, "Name:");
    drawAt(14, 118, "Species:");
    drawAt(14, 29, "Roster:");
    drawAt(165, 72, "Games per Series:");
    cv_.setForeColor(0);
    cv_.setSolidPenPat(14);
    cv_.moveTo(15, 78); cv_.line(125, 0);
    cv_.moveTo(165, 100); cv_.line(121, 0);
}

void App::drawCheck(const DialogCheck& c, bool pressed)
{
    // Control Manager radio buttons (round) and check boxes (square), with
    // the title to the right; a thicker outline while pressed.
    dialogPort();
    useFont(8);
    cv_.setTextFace(0);
    const Rect& r = c.rect;
    cv_.setPenSize(1, 1);
    cv_.setSolidPenPat(15);
    if (c.family) cv_.paintRRect(r, 13, 10); else cv_.paintRect(r);
    cv_.setSolidPenPat(0);
    cv_.setPenSize(pressed ? 2 : 1, 1);
    if (c.family) cv_.frameRRect(r, 13, 10); else cv_.frameRect(r);
    cv_.setPenSize(1, 1);
    if (c.on) {
        if (c.family) cv_.paintRRect(Rect{r.top + 3, r.left + 4, r.bottom - 3, r.right - 4}, 5, 4);
        else {
            cv_.moveTo(r.left + 2, r.top + 1); cv_.lineTo(r.right - 3, r.bottom - 2);
            cv_.moveTo(r.right - 3, r.top + 1); cv_.lineTo(r.left + 2, r.bottom - 2);
        }
    }
    cv_.setForeColor(0);
    cv_.setTextMode(modeForeCopy);
    drawAt(r.right + 4, r.top + 8, c.title);
    if (!c.enabled) {
        const auto saved = cv_.penMask();
        cv_.setPenMask(Canvas::grayMask());
        cv_.setSolidPenPat(15);
        cv_.paintRect(Rect{r.top, r.left, r.bottom, r.right + 4 + cv_.stringWidth(c.title)});
        cv_.setPenMask(saved);
    }
    dirty_ = true;
}

void App::drawEditLine()
{
    // A LineEdit field: framed white box; selected text is inverted.
    auto& d = dialog_;
    dialogPort();
    useFont(8);
    cv_.setTextFace(0);
    const Rect& r = d.editRect;
    cv_.setPenSize(1, 1);
    cv_.setSolidPenPat(15);
    cv_.paintRect(r);
    cv_.setSolidPenPat(0);
    cv_.frameRect(Rect{r.top - 1, r.left - 1, r.bottom + 1, r.right + 1});
    const int x = r.left + 3, base = r.top + 10;
    if (d.editSelected && d.editEnabled && !d.editText.empty()) {
        cv_.setSolidPenPat(0);
        cv_.paintRect(Rect{r.top + 1, x - 1, r.bottom - 1, x + cv_.stringWidth(d.editText)});
        cv_.setForeColor(15);
    } else {
        cv_.setForeColor(0);
    }
    cv_.setTextMode(modeForeCopy);
    drawAt(x, base, d.editText);
    if (!d.editSelected && d.editEnabled && d.caretOn) {
        const int cx = x + cv_.stringWidth(d.editText.substr(0, d.caret)) - 1;
        cv_.setSolidPenPat(0);
        cv_.paintRect(Rect{r.top + 2, cx, r.bottom - 2, cx + 1});
    }
    dirty_ = true;
}

void App::setupKey(char c)
{
    auto& d = dialog_;
    if (!d.editEnabled) return;
    if (d.editSelected && (c == 8 || (c >= 32 && c < 127))) {
        d.editText.clear(); d.caret = 0; d.editSelected = false;
        if (c == 8) { drawEditLine(); return; }
    }
    d.editSelected = false;
    if (c == 8) { if (d.caret > 0) d.editText.erase(--d.caret, 1); }
    else if (c == 0x1C) { if (d.caret > 0) --d.caret; }
    else if (c == 0x1D) { if (d.caret < int(d.editText.size())) ++d.caret; }
    else if (c >= 32 && c < 127 && d.editText.size() < 13) d.editText.insert(d.caret++, 1, c);
    d.caretOn = true; d.blink = 0;
    drawEditLine();
}

void App::setupItemHit(int item)
{
    auto& d = dialog_;
    auto check = [&](int it) -> DialogCheck& {
        for (auto& c : d.checks) if (c.item == it) return c;
        return d.checks[0];
    };
    if (item >= 5 && item != 7) {                       // radio: one per family
        const int fam = check(item).family;
        for (auto& c : d.checks) if (c.family == fam && c.on != (c.item == item)) { c.on = c.item == item; drawCheck(c, false); }
        return;
    }
    if (item == 7) { check(7).on = !check(7).on; drawCheck(check(7), false); return; }
    if (item == 2) { closeDialog(); return; }           // Cancel
    if (item == 3) {                                    // Play
        for (int i = 0; i < 4; ++i) { rec_[i].name = d.roster[i].first; rec_[i].species = d.roster[i].second; }
        for (int n = 1; n <= 6; ++n) if (check(7 + n).on) gamesPerSeries_ = n;
        showStandings_ = check(7).on;
        if (d.enrolled != 0) nPlayers_ = d.enrolled;
        closeDialog();
        newOrQuitGame();                                // G0006: start a game
        return;
    }
    if (item == 1) {                                    // Enroll
        const int k = ++d.enrolled;
        auto& entry = d.roster[k - 1];
        entry.first = d.editText;
        entry.second = check(5).on ? 1 : 0;
        const int y = 39 + 11 * (k - 1);
        dialogPort();
        useFont(8);
        cv_.setTextFace(0);
        cv_.setTextMode(modeForeCopy);
        cv_.setForeColor(7);
        if (entry.second == 0) drawAt(26, y, std::string(1, char(20)));   // the apple marks a computer
        cv_.setForeColor(0);
        drawAt(37, y, entry.first);
        if (k < 4) {
            d.editText = d.roster[k].first.substr(0, 13);
            d.editSelected = true;
            d.caret = int(d.editText.size());
            const bool human = d.roster[k].second == 1;
            if (check(5).on != human) setupItemHit(human ? 5 : 6);
        } else {
            d.editText = "Roster Full";
            d.editSelected = false;
            d.editEnabled = false;
            check(5).on = check(6).on = false;
            check(5).enabled = check(6).enabled = false;
            drawCheck(check(5), false);
            drawCheck(check(6), false);
            for (auto& b : d.buttons) if (b.item == 1) b.visible = false;   // RemoveDItem(Enroll)
            dialogPort();
            cv_.setBackPattern(15);
            cv_.eraseRect(makeRect(49, 148, 107, 162));
            d.defaultItem = 3;                                              // SetDefButton(Play)
            for (auto& b : d.buttons) if (b.item == 3) drawDialogButton(b, false);
        }
        drawEditLine();
    }
}

// ================================================================ options
// Game > Options... is the port's own dialog, drawn in the Setup dialog's
// style: cream box, white tabbed panel, check boxes, Cancel and a default OK.

void App::setOptions(const Options& o) { opt_ = o; }

void App::openOptions()
{
    openDialog(Dialog::Options, makeRect(40, 36, 280, 168), 1);
    auto& d = dialog_;
    d.buttons.push_back({makeRect(170, 111, 220, 124), "OK", 1, true, 0});
    d.buttons.push_back({makeRect(96, 111, 154, 124), "Cancel", 2, true, 0});
    d.checks.push_back({makeRect(16, 46, 29, 55), "Yahtzee Bonus", 3, 0, opt_.yahtzeeBonus});
    d.checks.push_back({makeRect(16, 74, 29, 83), "Modern Roll", 4, 0, opt_.modernRoll});
    d.checks[0].enabled = !gameInProgress_;          // changes the score card
    drawDialog();
}

void App::drawOptionsFrame()
{
    dialogPort();
    panel(6, 36, 234, 102, 12, 14, 15, 12);
    useFont(8);
    cv_.setTextFace(1);
    const int tabW = cv_.stringWidth(" Game Play ");
    panel(16, 30, 16 + tabW + 8, 42, 12, 14, 15, 12);
    useFont(16);
    shadowText(true, "Options", 120, 21, 0, 12, 1);
    shadowLine(0, 12, 120, 25, cv_.stringWidth("Options"), true);
    useFont(8);
    cv_.setTextFace(1);
    cv_.setForeColor(0);
    drawAt(20, 39, " Game Play ");
    cv_.setTextFace(0);
    cv_.setForeColor(12);
    drawAt(33, 66, gameInProgress_ ? "Change it between games" : "+100 for each extra Yahtzee");
    drawAt(33, 94, "Fresh dice from your system");
}

void App::optionsItemHit(int item)
{
    auto& d = dialog_;
    if (item == 3 || item == 4) {
        DialogCheck& c = d.checks[item - 3];
        c.on = !c.on;
        drawCheck(c, false);
        return;
    }
    if (item == 2) { closeDialog(); return; }
    if (item == 1) {
        Options o;
        o.yahtzeeBonus = d.checks[0].on;
        o.modernRoll = d.checks[1].on;
        closeDialog();
        const bool changed = o.yahtzeeBonus != opt_.yahtzeeBonus || o.modernRoll != opt_.modernRoll;
        setOptions(o);
        if (changed && onOptionsChanged) onOptionsChanged(opt_);
    }
}

// ================================================================== menus

void App::drawMenuBar()
{
    screenPort();
    cv_.setSolidPenPat(15);
    cv_.paintRect(makeRect(0, 0, Canvas::W, kMenuBarHeight - 1));
    cv_.setSolidPenPat(0);
    cv_.paintRect(makeRect(0, kMenuBarHeight - 1, Canvas::W, kMenuBarHeight));
    useFont(8);
    cv_.setTextMode(modeForeCopy);
    int x = 6;
    for (size_t i = 0; i < menus_.size(); ++i) {
        Menu& m = menus_[i];
        m.x = x;
        if (m.title == "@") {
            // A small rainbow apple of the port's own drawing.
            static const char* const apple[10] = {
                "......#...", ".....#....", "..##.##...", ".########.", "#########.",
                "########..", "########..", "#########.", ".########.", "..##..##.."};
            static const int stripes[10] = {10, 10, 10, 10, 9, 6, 7, 3, 4, 4};
            for (int r = 0; r < 10; ++r)
                for (int c = 0; c < 10; ++c)
                    if (apple[r][c] == '#') cv_.setPixel(x + 4 + c, 1 + r, stripes[r]);
            m.width = 18;
        } else {
            m.width = cv_.stringWidth(m.title) + 12;
            cv_.setForeColor(0);
            drawAt(x + 6, 9, m.title);
        }
        x += m.width;
    }
    dirty_ = true;
}

namespace { std::vector<uint8_t> savedBits; Rect savedRect; }

static Rect menuRect(const Menu& m, const Canvas& cv)
{
    int w = 0;
    for (auto& it : m.items) w = std::max(w, cv.stringWidth(it.text) + (it.key ? 22 : 0));
    return makeRect(m.x, 12, m.x + w + 16, 12 + int(m.items.size()) * 11 + 3);
}

void App::drawMenu(int mi)
{
    const Menu& m = menus_[mi];
    screenPort();
    useFont(8);
    Rect r = menuRect(m, cv_);
    if (savedBits.empty()) {
        savedRect = Rect{r.top, r.left, r.bottom + 1, r.right + 1};
        for (int y = savedRect.top; y < savedRect.bottom; ++y)
            for (int x = savedRect.left; x < savedRect.right; ++x) savedBits.push_back(cv_.pixel(x, y));
    }
    // title highlight
    cv_.setSolidPenPat(0);
    cv_.paintRect(makeRect(m.x, 0, m.x + m.width, kMenuBarHeight - 1));
    if (m.title != "@") { cv_.setForeColor(15); cv_.setTextMode(modeForeCopy); drawAt(m.x + 6, 9, m.title); }
    // box with shadow
    cv_.setSolidPenPat(0);
    cv_.paintRect(makeRect(r.left + 1, r.top + 1, r.right + 1, r.bottom + 1));
    cv_.setSolidPenPat(15);
    cv_.paintRect(r);
    cv_.setSolidPenPat(0);
    cv_.setPenSize(1, 1);
    cv_.frameRect(r);
    for (size_t i = 0; i < m.items.size(); ++i) {
        const MenuItem& it = m.items[i];
        const int top = r.top + 2 + int(i) * 11;
        if (it.text == "-") {
            cv_.setSolidPenPat(14);
            cv_.paintRect(makeRect(r.left + 1, top + 5, r.right - 1, top + 6));
            continue;
        }
        const bool hot = int(i) == menuItem_ && it.enabled;
        if (hot) { cv_.setSolidPenPat(0); cv_.paintRect(makeRect(r.left + 1, top, r.right - 1, top + 11)); }
        cv_.setForeColor(hot ? 15 : it.enabled ? 0 : 14);
        cv_.setTextMode(modeForeCopy);
        drawAt(r.left + 8, top + 8, it.text);
        if (it.key) {
            const std::string k = std::string("@") + it.key;   // stands in for the Apple key
            drawAt(r.right - 8 - cv_.stringWidth(k), top + 8, k);
        }
    }
    dirty_ = true;
}

void App::closeMenu()
{
    if (!savedBits.empty()) {
        size_t k = 0;
        for (int y = savedRect.top; y < savedRect.bottom; ++y)
            for (int x = savedRect.left; x < savedRect.right; ++x) cv_.setPixel(x, y, savedBits[k++]);
        savedBits.clear();
    }
    menuOpen_ = menuItem_ = -1;
    drawMenuBar();
}

int App::menuAt(int x) const
{
    for (size_t i = 0; i < menus_.size(); ++i)
        if (x >= menus_[i].x && x < menus_[i].x + menus_[i].width) return int(i);
    return -1;
}

int App::itemAt(int mi, int x, int y) const
{
    const Rect r = menuRect(menus_[mi], cv_);
    if (!r.contains(x, y)) return -1;
    const int i = (y - r.top - 2) / 11;
    if (i < 0 || i >= int(menus_[mi].items.size()) || menus_[mi].items[i].text == "-") return -1;
    return i;
}

void App::setItemEnabled(int id, bool on)
{
    for (auto& m : menus_) for (auto& it : m.items) if (it.id == id) it.enabled = on;
}

void App::setItemText(int id, const std::string& s)
{
    for (auto& m : menus_) for (auto& it : m.items) if (it.id == id) it.text = s;
}

} // namespace gs
