#include "player.h"
#include <iostream>

// BAD: copies 800 bytes per call — PG001 should fire here
void updatePlayer(Player p) {
    p.name[0] = 'X';
}

// BAD: return by value also copies — PG001 should fire
Player getDefaultPlayer() {
    Player p{};
    return p;
}

// GOOD: const reference — no copy, PG001 must NOT fire
void printPlayer(const Player& p) {
    std::cout << p.name << "\n";
}

// GOOD: pointer — no copy, PG001 must NOT fire
void resetPlayer(Player* p) {
    p->name[0] = '\0';
}

int main() {
    Player p{};
    updatePlayer(p);
    printPlayer(p);
    resetPlayer(&p);
    return 0;
}
