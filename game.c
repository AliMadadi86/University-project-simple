#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void copy_text(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

void game_seed_rng(void) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
}

void game_clear(Game *g, int size) {
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->size = size;
    g->mode = MODE_PVP;
    g->current_player = 0;
    copy_text(g->player_name[0], NAME_SIZE, "Player1");
    copy_text(g->player_name[1], NAME_SIZE, "Player2");
}

void game_start(Game *g, int size, int walls_per_player, GameMode mode, const char *name1, const char *name2) {
    int center_top;
    int center_bottom;
    if (!g) return;

    game_clear(g, size);
    g->mode = mode;
    g->walls_left[0] = walls_per_player;
    g->walls_left[1] = walls_per_player;
    copy_text(g->player_name[0], NAME_SIZE, name1);
    copy_text(g->player_name[1], NAME_SIZE, name2);

    center_top = (size - 1) / 2;
    center_bottom = size / 2;
    g->players[0].row = size - 1;
    g->players[0].col = center_bottom;
    g->players[1].row = 0;
    g->players[1].col = center_top;
}

int game_set_player_pos(Game *g, int player, int row, int col) {
    if (!g) return 0;
    if (player < 0 || player >= PLAYER_COUNT) return 0;
    if (!game_in_range(g, row, col)) return 0;
    if (player == 0 && g->players[1].row == row && g->players[1].col == col) return 0;
    if (player == 1 && g->players[0].row == row && g->players[0].col == col) return 0;
    g->players[player].row = row;
    g->players[player].col = col;
    return 1;
}

int game_in_range(const Game *g, int row, int col) {
    return g && row >= 0 && col >= 0 && row < g->size && col < g->size;
}

int game_is_blocked(const Game *g, int r1, int c1, int r2, int c2) {
    if (!game_in_range(g, r1, c1) || !game_in_range(g, r2, c2)) return 1;
    if (r1 == r2) {
        if (c2 == c1 + 1) return g->block_right[r1][c1] != 0;
        if (c2 == c1 - 1) return g->block_right[r1][c2] != 0;
    }
    if (c1 == c2) {
        if (r2 == r1 + 1) return g->block_down[r1][c1] != 0;
        if (r2 == r1 - 1) return g->block_down[r2][c1] != 0;
    }
    return 1;
}

static int goal_row_for_player(const Game *g, int player) {
    if (!g) return -1;
    return (player == 0) ? 0 : (g->size - 1);
}

static int is_cell_occupied(const Game *g, int row, int col, int ignore_player) {
    int i;
    for (i = 0; i < PLAYER_COUNT; i++) {
        if (i == ignore_player) continue;
        if (g->players[i].row == row && g->players[i].col == col) return 1;
    }
    return 0;
}

static void set_wall(Game *g, int row, int col, WallDir dir, unsigned char value) {
    if (dir == DIR_H) {
        g->block_down[row][col] = value;
        g->block_down[row][col + 1] = value;
        g->h_wall_at[row][col] = value;
    } else {
        g->block_right[row][col] = value;
        g->block_right[row + 1][col] = value;
        g->v_wall_at[row][col] = value;
    }
}

int game_can_place_wall(const Game *g, int row, int col, WallDir dir) {
    if (!g) return 0;
    if (row < 0 || col < 0 || row >= g->size - 1 || col >= g->size - 1) return 0;

    if (dir == DIR_H) {
        if (g->v_wall_at[row][col]) return 0;
        if (g->block_down[row][col] || g->block_down[row][col + 1]) return 0;
        return 1;
    }

    if (g->h_wall_at[row][col]) return 0;
    if (g->block_right[row][col] || g->block_right[row + 1][col]) return 0;
    return 1;
}

int game_add_wall_from_map(Game *g, int row, int col, WallDir dir) {
    if (!game_can_place_wall(g, row, col, dir)) return 0;
    set_wall(g, row, col, dir, 1);
    return 1;
}

static int has_path_to_goal(const Game *g, int player) {
    int visited[MAX_SIZE][MAX_SIZE] = {0};
    Pos queue[MAX_SIZE * MAX_SIZE];
    int head = 0;
    int tail = 0;
    int goal = goal_row_for_player(g, player);
    Pos start = g->players[player];
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    int i;

    if (!game_in_range(g, start.row, start.col)) return 0;

    visited[start.row][start.col] = 1;
    queue[tail++] = start;

    while (head < tail) {
        Pos cur = queue[head++];
        if (cur.row == goal) return 1;

        for (i = 0; i < 4; i++) {
            int nr = cur.row + dr[i];
            int nc = cur.col + dc[i];
            if (!game_in_range(g, nr, nc)) continue;
            if (visited[nr][nc]) continue;
            if (game_is_blocked(g, cur.row, cur.col, nr, nc)) continue;
            visited[nr][nc] = 1;
            queue[tail].row = nr;
            queue[tail].col = nc;
            tail++;
        }
    }
    return 0;
}

int game_place_wall(Game *g, int player, int row, int col, WallDir dir, char *err, size_t err_cap) {
    if (!g) return 0;
    if (player < 0 || player >= PLAYER_COUNT) return 0;

    if (g->walls_left[player] <= 0) {
        if (err) snprintf(err, err_cap, "No walls left.");
        return 0;
    }
    if (!game_can_place_wall(g, row, col, dir)) {
        if (err) snprintf(err, err_cap, "Invalid wall position.");
        return 0;
    }

    set_wall(g, row, col, dir, 1);
    if (!has_path_to_goal(g, 0) || !has_path_to_goal(g, 1)) {
        set_wall(g, row, col, dir, 0);
        if (err) snprintf(err, err_cap, "Wall blocks all paths.");
        return 0;
    }

    g->walls_left[player]--;
    return 1;
}

int game_can_move(const Game *g, int player, Pos target) {
    Pos cur;
    Pos opp;
    int dr;
    int dc;
    int manhattan;
    int odr;
    int odc;
    int opp_adjacent = 0;
    Pos jump;
    int jump_possible = 0;

    if (!g) return 0;
    if (player < 0 || player >= PLAYER_COUNT) return 0;
    if (!game_in_range(g, target.row, target.col)) return 0;
    if (is_cell_occupied(g, target.row, target.col, player)) return 0;

    cur = g->players[player];
    opp = g->players[1 - player];
    dr = target.row - cur.row;
    dc = target.col - cur.col;
    manhattan = abs(dr) + abs(dc);

    odr = opp.row - cur.row;
    odc = opp.col - cur.col;
    if (abs(odr) + abs(odc) == 1 && !game_is_blocked(g, cur.row, cur.col, opp.row, opp.col)) {
        opp_adjacent = 1;
    }

    jump.row = opp.row + odr;
    jump.col = opp.col + odc;
    if (opp_adjacent && game_in_range(g, jump.row, jump.col) &&
        !game_is_blocked(g, opp.row, opp.col, jump.row, jump.col)) {
        jump_possible = 1;
    }

    if (manhattan == 1) {
        if (game_is_blocked(g, cur.row, cur.col, target.row, target.col)) return 0;
        return 1;
    }

    if (manhattan == 2 && (dr == 0 || dc == 0)) {
        Pos mid;
        mid.row = cur.row + dr / 2;
        mid.col = cur.col + dc / 2;
        if (mid.row != opp.row || mid.col != opp.col) return 0;
        if (game_is_blocked(g, cur.row, cur.col, mid.row, mid.col)) return 0;
        if (game_is_blocked(g, mid.row, mid.col, target.row, target.col)) return 0;
        return 1;
    }

    if (abs(dr) == 1 && abs(dc) == 1) {
        if (!opp_adjacent) return 0;
        if (jump_possible) return 0;

        if (odr != 0) {
            if (target.row != cur.row + odr) return 0;
            if (abs(target.col - cur.col) != 1) return 0;
        } else {
            if (target.col != cur.col + odc) return 0;
            if (abs(target.row - cur.row) != 1) return 0;
        }

        if (game_is_blocked(g, opp.row, opp.col, target.row, target.col)) return 0;
        return 1;
    }

    return 0;
}

int game_list_moves(const Game *g, int player, Pos *out, int max_out) {
    Pos cur;
    Pos opp;
    Pos candidates[8];
    int count = 0;
    int candidate_count = 0;
    int odr;
    int odc;
    int i;

    if (!g || !out || max_out <= 0) return 0;
    if (player < 0 || player >= PLAYER_COUNT) return 0;

    cur = g->players[player];
    opp = g->players[1 - player];

    candidates[candidate_count++] = (Pos){cur.row - 1, cur.col};
    candidates[candidate_count++] = (Pos){cur.row + 1, cur.col};
    candidates[candidate_count++] = (Pos){cur.row, cur.col - 1};
    candidates[candidate_count++] = (Pos){cur.row, cur.col + 1};

    odr = opp.row - cur.row;
    odc = opp.col - cur.col;
    if (abs(odr) + abs(odc) == 1) {
        candidates[candidate_count++] = (Pos){cur.row + 2 * odr, cur.col + 2 * odc};
        if (odr != 0) {
            candidates[candidate_count++] = (Pos){cur.row + odr, cur.col - 1};
            candidates[candidate_count++] = (Pos){cur.row + odr, cur.col + 1};
        } else {
            candidates[candidate_count++] = (Pos){cur.row - 1, cur.col + odc};
            candidates[candidate_count++] = (Pos){cur.row + 1, cur.col + odc};
        }
    }

    for (i = 0; i < candidate_count && count < max_out; i++) {
        if (game_can_move(g, player, candidates[i])) {
            out[count++] = candidates[i];
        }
    }
    return count;
}

int game_move_player(Game *g, int player, Pos target, char *err, size_t err_cap) {
    if (!game_can_move(g, player, target)) {
        if (err) snprintf(err, err_cap, "Invalid move.");
        return 0;
    }
    g->players[player] = target;
    return 1;
}

int game_check_winner(const Game *g) {
    if (!g) return -1;
    if (g->players[0].row == 0) return 0;
    if (g->players[1].row == g->size - 1) return 1;
    return -1;
}

int game_next_player(int current_player) {
    return (current_player + 1) % PLAYER_COUNT;
}

static void clear_all_walls(Game *g) {
    memset(g->block_right, 0, sizeof(g->block_right));
    memset(g->block_down, 0, sizeof(g->block_down));
    memset(g->h_wall_at, 0, sizeof(g->h_wall_at));
    memset(g->v_wall_at, 0, sizeof(g->v_wall_at));
}

void game_apply_magic(Game *g, char *msg, size_t msg_cap) {
    int target;
    int effect;
    int amount;
    if (!g) return;

    target = rand() % PLAYER_COUNT;
    effect = rand() % 4;

    if (effect == 0) {
        clear_all_walls(g);
        snprintf(msg, msg_cap, "Magic: all walls removed.");
        return;
    }
    if (effect == 1) {
        amount = (rand() % 2) ? 2 : 3;
        g->walls_left[target] -= amount;
        if (g->walls_left[target] < 0) g->walls_left[target] = 0;
        snprintf(msg, msg_cap, "Magic: %s lost %d walls.", g->player_name[target], amount);
        return;
    }
    if (effect == 2) {
        amount = (rand() % 2) + 1;
        g->blocked_turns[target] += amount;
        snprintf(msg, msg_cap, "Magic: %s blocked for %d turn(s).", g->player_name[target], amount);
        return;
    }
    if (effect == 3) {
        amount = (rand() % 2) ? 2 : 3;
        g->walls_left[target] += amount;
        snprintf(msg, msg_cap, "Magic: %s gained %d walls.", g->player_name[target], amount);
        return;
    }
}

int game_try_ai_turn(Game *g, char *msg, size_t msg_cap) {
    Pos moves[16];
    int move_count;
    int player = g->current_player;
    int i;
    char err[64];

    if (!g) return 0;

    move_count = game_list_moves(g, player, moves, 16);
    if (move_count <= 0 && g->walls_left[player] <= 0) {
        snprintf(msg, msg_cap, "Computer has no valid action.");
        return 0;
    }

    if (g->walls_left[player] > 0 && (rand() % 100) < 35) {
        for (i = 0; i < 250; i++) {
            int row = rand() % (g->size - 1);
            int col = rand() % (g->size - 1);
            WallDir dir = (rand() % 2) ? DIR_H : DIR_V;
            if (game_place_wall(g, player, row, col, dir, err, sizeof(err))) {
                snprintf(msg, msg_cap, "Computer placed wall at (%d, %d) %c.", row, col, dir == DIR_H ? 'H' : 'V');
                return 1;
            }
        }
    }

    if (move_count > 0) {
        Pos choice = moves[rand() % move_count];
        game_move_player(g, player, choice, err, sizeof(err));
        snprintf(msg, msg_cap, "Computer moved to (%d, %d).", choice.row, choice.col);
        return 1;
    }

    for (i = 0; i < 250; i++) {
        int row = rand() % (g->size - 1);
        int col = rand() % (g->size - 1);
        WallDir dir = (rand() % 2) ? DIR_H : DIR_V;
        if (game_place_wall(g, player, row, col, dir, err, sizeof(err))) {
            snprintf(msg, msg_cap, "Computer placed wall at (%d, %d) %c.", row, col, dir == DIR_H ? 'H' : 'V');
            return 1;
        }
    }

    snprintf(msg, msg_cap, "Computer has no valid action.");
    return 0;
}
