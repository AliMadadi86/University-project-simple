#ifndef SIMPLE_GAME_H
#define SIMPLE_GAME_H

#include <stddef.h>

#define MAX_SIZE 50
#define PLAYER_COUNT 2
#define NAME_SIZE 32

typedef enum {
    DIR_H = 0,
    DIR_V = 1
} WallDir;

typedef enum {
    MODE_PVP = 1,
    MODE_PVC = 2
} GameMode;

typedef struct {
    int row;
    int col;
} Pos;

typedef struct {
    int size;
    Pos players[PLAYER_COUNT];
    int walls_left[PLAYER_COUNT];
    unsigned char block_right[MAX_SIZE][MAX_SIZE];
    unsigned char block_down[MAX_SIZE][MAX_SIZE];
    unsigned char h_wall_at[MAX_SIZE][MAX_SIZE];
    unsigned char v_wall_at[MAX_SIZE][MAX_SIZE];
    int blocked_turns[PLAYER_COUNT];
    int current_player;
    GameMode mode;
    char player_name[PLAYER_COUNT][NAME_SIZE];
} Game;

void game_seed_rng(void);
void game_clear(Game *g, int size);
void game_start(Game *g, int size, int walls_per_player, GameMode mode, const char *name1, const char *name2);
int game_set_player_pos(Game *g, int player, int row, int col);

int game_in_range(const Game *g, int row, int col);
int game_is_blocked(const Game *g, int r1, int c1, int r2, int c2);
int game_can_place_wall(const Game *g, int row, int col, WallDir dir);
int game_add_wall_from_map(Game *g, int row, int col, WallDir dir);
int game_place_wall(Game *g, int player, int row, int col, WallDir dir, char *err, size_t err_cap);

int game_can_move(const Game *g, int player, Pos target);
int game_list_moves(const Game *g, int player, Pos *out, int max_out);
int game_move_player(Game *g, int player, Pos target, char *err, size_t err_cap);

int game_check_winner(const Game *g);
int game_next_player(int current_player);

int game_try_ai_turn(Game *g, char *msg, size_t msg_cap);
void game_apply_magic(Game *g, char *msg, size_t msg_cap);

#endif
