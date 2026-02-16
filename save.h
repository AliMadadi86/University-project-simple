#ifndef SIMPLE_SAVE_H
#define SIMPLE_SAVE_H

#include "game.h"

int save_game(const char *filename, const Game *g, char *err, size_t err_cap);
int load_game(const char *filename, Game *g, char *err, size_t err_cap);

#endif
