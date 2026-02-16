#include "ai.h"

#include <stdlib.h>

#define BIG_DIST 100000

/*
 * ai.c:
 * یک AI سبک و قابل‌فهم برای حالت PvC.
 *
 * ایدهٔ کلی:
 * - برای move: حرکتی را برمی‌داریم که فاصله تا هدف را کمتر کند.
 * - برای wall: دیواری را ترجیح می‌دهیم که حریف را بیشتر کند کند
 *   و خودمان را زیاد عقب نیندازد.
 */

typedef struct {
    int row;
    int col;
    WallDir dir;
} WallPos;

/* کوتاه‌ترین فاصله تا ردیف هدف (با BFS ساده روی خانه‌ها) */
static int shortest_steps_to_goal(const Board *b, Coord start, int goal_row) {
    if (!board_in_range(b, start.row, start.col)) return BIG_DIST;

    /* چون BOARD_MAX ثابت است، آرایه dist اندازهٔ ثابت می‌گیرد */
    int dist[BOARD_MAX][BOARD_MAX];
    Coord queue[BOARD_MAX * BOARD_MAX];
    int head = 0;
    int tail = 0;

    for (int r = 0; r < BOARD_MAX; r++) {
        for (int c = 0; c < BOARD_MAX; c++) {
            dist[r][c] = -1;
        }
    }

    dist[start.row][start.col] = 0;
    queue[tail++] = start;

    while (head < tail) {
        Coord cur = queue[head++];
        int cur_dist = dist[cur.row][cur.col];
        if (cur.row == goal_row) return cur_dist;

        int dr[4] = {-1, 1, 0, 0};
        int dc[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = cur.row + dr[i];
            int nc = cur.col + dc[i];
            if (!board_in_range(b, nr, nc)) continue;
            if (board_is_blocked(b, cur.row, cur.col, nr, nc)) continue;
            if (dist[nr][nc] != -1) continue;
            dist[nr][nc] = cur_dist + 1;
            queue[tail++] = (Coord){nr, nc};
        }
    }

    return BIG_DIST;
}

/* بهترین حرکت را با کمترین فاصله تا هدف پیدا می‌کند */
static int pick_best_move(const GameState *g, PlayerId p, Coord *best_move) {
    Coord moves[16];
    int move_count = game_list_valid_moves(g, p, moves, 16);
    if (move_count <= 0) return 0;

    int goal = game_goal_row(p, g->board.size);
    int best_dist = BIG_DIST;
    int best_idx[16];
    int best_count = 0;

    /* اگر چند حرکت امتیاز برابر داشتند، یکی را تصادفی انتخاب می‌کنیم */
    for (int i = 0; i < move_count; i++) {
        Board tmp = g->board;
        tmp.players[p] = moves[i];
        int d = shortest_steps_to_goal(&tmp, tmp.players[p], goal);

        if (d < best_dist) {
            best_dist = d;
            best_idx[0] = i;
            best_count = 1;
        } else if (d == best_dist && best_count < 16) {
            best_idx[best_count++] = i;
        }
    }

    if (best_count <= 0) return 0;
    *best_move = moves[best_idx[rand() % best_count]];
    return 1;
}

/* بهترین دیوار را با معیار «کند کردن بیشترِ حریف نسبت به خودی» پیدا می‌کند */
static int pick_best_wall(const GameState *g, PlayerId p, WallPos *best_wall, int *best_score) {
    int walls_left = g->board.walls_left[p];
    if (walls_left <= 0) return 0;

    PlayerId opp = (PlayerId)(1 - p);
    int my_goal = game_goal_row(p, g->board.size);
    int opp_goal = game_goal_row(opp, g->board.size);
    int base_my = shortest_steps_to_goal(&g->board, g->board.players[p], my_goal);
    int base_opp = shortest_steps_to_goal(&g->board, g->board.players[opp], opp_goal);

    int best = -BIG_DIST;
    WallPos best_list[64];
    int best_count = 0;

    /*
     * امتیاز دیوار:
     * score = (بدتر شدن مسیر حریف) - (بدتر شدن مسیر خودی)
     * هرچه بزرگ‌تر بهتر.
     */
    for (int r = 0; r < g->board.size - 1; r++) {
        for (int c = 0; c < g->board.size - 1; c++) {
            WallDir dirs[2] = {DIR_H, DIR_V};
            for (int k = 0; k < 2; k++) {
                Board tmp = g->board;
                WallDir dir = dirs[k];
                if (!board_place_wall(&tmp, r, c, dir, 1)) continue;

                int my_after = shortest_steps_to_goal(&tmp, tmp.players[p], my_goal);
                int opp_after = shortest_steps_to_goal(&tmp, tmp.players[opp], opp_goal);

                int score = (opp_after - base_opp) - (my_after - base_my);

                if (score > best) {
                    best = score;
                    best_list[0].row = r;
                    best_list[0].col = c;
                    best_list[0].dir = dir;
                    best_count = 1;
                } else if (score == best && best_count < 64) {
                    best_list[best_count].row = r;
                    best_list[best_count].col = c;
                    best_list[best_count].dir = dir;
                    best_count++;
                }
            }
        }
    }

    if (best_count <= 0) return 0;
    *best_wall = best_list[rand() % best_count];
    *best_score = best;
    return 1;
}

/* هوش مصنوعی ساده و بهتر:
   - حرکت: کم‌کردن فاصله تا هدف
   - دیوار: کندکردن حریف وقتی واقعاً مفید باشد */
int ai_choose_action(const GameState *g, PlayerId p, AiAction *out) {
    if (!g || !out) return 0;

    Coord best_move;
    int has_move = pick_best_move(g, p, &best_move);

    WallPos best_wall;
    int wall_score = -BIG_DIST;
    int has_wall = pick_best_wall(g, p, &best_wall, &wall_score);

    PlayerId opp = (PlayerId)(1 - p);
    int my_goal = game_goal_row(p, g->board.size);
    int opp_goal = game_goal_row(opp, g->board.size);
    int my_dist = shortest_steps_to_goal(&g->board, g->board.players[p], my_goal);
    int opp_dist = shortest_steps_to_goal(&g->board, g->board.players[opp], opp_goal);

    /*
     * تصمیم نهایی:
     * - اگر حرکت نداریم ولی دیوار داریم -> دیوار
     * - اگر دیوار واقعا مفید بود و حریف جلوتر/برابر بود -> دیوار
     * - در بقیه حالات -> حرکت
     */
    int should_wall = 0;
    if (!has_move && has_wall) {
        should_wall = 1;
    } else if (has_wall) {
        /* دیوار فقط وقتی انتخاب شود که مفید باشد و حریف عقب نیفتاده باشد */
        if (wall_score >= 1 && opp_dist <= my_dist) {
            should_wall = 1;
        }
    }

    if (should_wall) {
        out->type = AI_ACT_WALL;
        out->row = best_wall.row;
        out->col = best_wall.col;
        out->dir = best_wall.dir;
    } else {
        if (!has_move) return 0;
        out->type = AI_ACT_MOVE;
        out->move = best_move;
    }

    return 1;
}
