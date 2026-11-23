// Game flow, recovered from P15C0 (roll), P1384 (score and advance),
// P10BF (end of game) and P20D6 (the computer's turn).
#pragma once
#include "computer.h"
#include "scoring.h"
#include <array>
#include <random>
#include <string>
#include <vector>

namespace yahtzee {

constexpr int kMaxPlayers = 4;
constexpr int kRollsPerTurn = 3;

struct Player {
    std::string name;
    bool computer = false;
    ScoreCard card;
    Totals totals;
    int seriesTotal = 0;   // record offset $18: grand totals summed over the series
};

class Game {
public:
    explicit Game(unsigned seed = std::random_device{}());

    // --- setup ---------------------------------------------------------
    // Default from the original: you against the computer, 3 games a series.
    void setPlayers(const std::vector<Player>& players);
    void setGamesPerSeries(int games);           // 1..6
    void newSeries();
    void newGame();

    // --- turn ----------------------------------------------------------
    int currentPlayer() const { return current_; }          // 0-based
    const Player& player(int i) const { return players_[i]; }
    int playerCount() const { return int(players_.size()); }
    int rollNumber() const { return roll_; }                // 0 = not rolled yet
    const Dice& dice() const { return dice_; }
    bool selected(int die) const { return selected_[die]; }

    // In the original you select the dice to REROLL, not the ones to keep,
    // and Roll stays dimmed until at least one die is selected.
    bool canRoll() const;
    void roll();
    bool canSelect() const { return roll_ >= 1 && roll_ < kRollsPerTurn && !gameOver_; }
    void toggleSelected(int die);

    bool canScore(int category) const;
    int potentialScore(int category) const { return scoreCategory(category, dice_); }
    void score(int category);                  // scores, then passes the turn

    // --- computer player ---------------------------------------------
    // After a computer roll: which dice it wants to reroll. All false means
    // it is ready to score. Mirrors the loop in P20D6.
    std::array<bool, kDiceCount> computerRerollChoice() const;
    int computerScoringChoice() const;

    // --- game and series state ---------------------------------------
    bool gameOver() const { return gameOver_; }
    int gamesPlayed() const { return gamesPlayed_; }
    int gamesPerSeries() const { return gamesPerSeries_; }
    bool seriesOver() const { return gamesPlayed_ >= gamesPerSeries_; }
    std::vector<int> winners() const;          // players with the top grand total

private:
    void rollDie(int die);
    PlanTable plans() const;
    std::array<int, kDiceCount> sortedOrder() const;  // physical die at each sorted position

    std::mt19937 rng_;
    std::vector<Player> players_;
    int gamesPerSeries_ = 3;
    int gamesPlayed_ = 0;
    int current_ = 0;
    int roll_ = 0;
    Dice dice_{1, 1, 1, 1, 1};
    std::array<bool, kDiceCount> selected_{};
    bool gameOver_ = false;
};

} // namespace yahtzee
