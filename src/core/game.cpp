#include "game.h"
#include <algorithm>
#include <numeric>

namespace yahtzee {

Game::Game(unsigned seed) : rng_(seed)
{
    Player you;   you.name = "Player #1";
    Player comp;  comp.name = "Computer"; comp.computer = true;
    setPlayers({you, comp});
}

void Game::setPlayers(const std::vector<Player>& players)
{
    players_ = players;
    if (players_.size() > kMaxPlayers) players_.resize(kMaxPlayers);
    newSeries();
}

void Game::setGamesPerSeries(int games)
{
    gamesPerSeries_ = std::clamp(games, 1, 6);
}

void Game::newSeries()
{
    gamesPlayed_ = 0;
    for (auto& p : players_) p.seriesTotal = 0;
    newGame();
}

void Game::newGame()
{
    for (auto& p : players_) {
        p.card.open.fill(true);
        p.card.score.fill(0);
        p.totals = Totals{};
    }
    current_ = 0;
    roll_ = 0;
    selected_.fill(false);
    gameOver_ = false;
}

bool Game::canRoll() const
{
    if (gameOver_ || roll_ >= kRollsPerTurn) return false;
    if (roll_ == 0) return true;
    return std::any_of(selected_.begin(), selected_.end(), [](bool b) { return b; });
}

void Game::rollDie(int die)
{
    // P1483: Random mod 6 + 1
    dice_[die] = std::uniform_int_distribution<int>(1, 6)(rng_);
}

void Game::roll()
{
    if (!canRoll()) return;
    for (int i = 0; i < kDiceCount; ++i)
        if (roll_ == 0 || selected_[i]) rollDie(i);
    selected_.fill(false);
    ++roll_;
}

void Game::toggleSelected(int die)
{
    if (canSelect() && die >= 0 && die < kDiceCount) selected_[die] = !selected_[die];
}

bool Game::canScore(int category) const
{
    return !gameOver_ && roll_ >= 1 && category >= 1 && category <= kCategoryCount
        && players_[current_].card.open[category - 1];
}

void Game::score(int category)
{
    if (!canScore(category)) return;
    Player& p = players_[current_];
    p.card.open[category - 1] = false;
    p.card.score[category - 1] = scoreCategory(category, dice_);
    p.totals = computeTotals(p.card.score);

    roll_ = 0;
    selected_.fill(false);
    current_ = (current_ + 1) % playerCount();

    // P0F1C: the game goes on while the last player still has open boxes.
    const auto& last = players_.back().card.open;
    if (std::none_of(last.begin(), last.end(), [](bool b) { return b; })) {
        gameOver_ = true;
        ++gamesPlayed_;
        for (auto& pl : players_) pl.seriesTotal += pl.totals.grand;
    }
}

std::vector<int> Game::winners() const
{
    int top = 0;
    for (const auto& p : players_) top = std::max(top, p.totals.grand);
    std::vector<int> w;
    for (int i = 0; i < playerCount(); ++i)
        if (players_[i].totals.grand == top) w.push_back(i);
    return w;
}

PlanTable Game::plans() const
{
    // After the third roll the original's roll counter is back to 0,
    // which tells the planner there is no further roll.
    return planAll(dice_, roll_ < kRollsPerTurn ? kRollsPerTurn - roll_ : 0);
}

std::array<int, kDiceCount> Game::sortedOrder() const
{
    std::array<int, kDiceCount> idx;
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return dice_[a] < dice_[b]; });
    return idx;
}

std::array<bool, kDiceCount> Game::computerRerollChoice() const
{
    std::array<bool, kDiceCount> choice{};
    if (roll_ < 1 || roll_ >= kRollsPerTurn) return choice;
    const PlanTable t = plans();
    const int target = chooseCategory(dice_, players_[current_].card, t, false);
    if (target < 1) return choice;
    const auto order = sortedOrder();
    for (int pos = 0; pos < kDiceCount; ++pos)
        if (t[target - 1].reroll[pos]) choice[order[pos]] = true;
    return choice;
}

int Game::computerScoringChoice() const
{
    return chooseCategory(dice_, players_[current_].card, plans(), true);
}

} // namespace yahtzee
