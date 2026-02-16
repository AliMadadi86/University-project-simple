#include "game.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*
 * راهنمای این فایل:
 * - اینجا تمام قوانین واقعی بازی پیاده‌سازی شده‌اند.
 * - UI (گرافیکی/متنی) فقط از این توابع استفاده می‌کند و قانونی اضافه نمی‌کند.
 * - ترتیب کلی:
 *   1) توابع کمکی داخلی (static)
 *   2) توابع board_* برای سطح صفحه
 *   3) توابع game_* برای منطق نوبت و برد
 *   4) منطق جعبهٔ جادویی
 */

/* ساخت یک Coord برای خوانایی بیشتر کد */
static Coord make_coord(int row, int col) {
    Coord c;
    c.row = row;
    c.col = col;
    return c;
}

static int clamp_nonnegative(int v) {
    if (v < 0) return 0;
    return v;
}

static int min_int(int a, int b) {
    if (a < b) return a;
    return b;
}

static void copy_text(char *dst, int cap, const char *src) {
    if (!dst || cap <= 0 || !src) return;
    strncpy(dst, src, (size_t)cap - 1);
    dst[cap - 1] = '\0';
}

static int clamp_player_count(int n) {
    if (n <= 2) return 2;
    return 4;
}

/*
 * بررسی رسیدن یک بازیکن به هدف:
 * - در 2 نفره: هدف فقط ردیف بالا/پایین است.
 * - در 4 نفره: بازیکن 3 و 4 هدف ستونی دارند.
 */
static int goal_reached_for_player(const Board *b, int p, Coord c) {
    if (b->player_count == 4) {
        if (p == PLAYER1) return c.row == 0;
        if (p == PLAYER2) return c.row == b->size - 1;
        if (p == PLAYER3) return c.col == b->size - 1;
        if (p == PLAYER4) return c.col == 0;
        return 0;
    }

    if (p == PLAYER1) return c.row == 0;
    if (p == PLAYER2) return c.row == b->size - 1;
    return 0;
}

/*
 * BFS عمومی برای اینکه ببینیم یک بازیکن از نقطهٔ فعلی، حداقل یک مسیر تا هدفش دارد یا نه.
 * این تابع پایهٔ «قانون مهم بازی» است: هیچ دیواری نباید همهٔ مسیرها را ببندد.
 */
static int board_has_path_to_player_goal(const Board *b, Coord start, int player_index) {
    if (!board_in_range(b, start.row, start.col)) return 0;
    if (player_index < 0 || player_index >= b->player_count) return 0;

    int visited[BOARD_MAX][BOARD_MAX] = {0};
    Coord queue[BOARD_MAX * BOARD_MAX];
    int head = 0;
    int tail = 0;

    visited[start.row][start.col] = 1;
    queue[tail++] = start;

    while (head < tail) {
        Coord cur = queue[head++];
        if (goal_reached_for_player(b, player_index, cur)) return 1;

        const int dr[4] = {-1, 1, 0, 0};
        const int dc[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = cur.row + dr[i];
            int nc = cur.col + dc[i];
            if (!board_in_range(b, nr, nc)) continue;
            if (board_is_blocked(b, cur.row, cur.col, nr, nc)) continue;
            if (visited[nr][nc]) continue;
            visited[nr][nc] = 1;
            queue[tail++] = make_coord(nr, nc);
        }
    }

    return 0;
}

static int is_cell_occupied(const Board *b, int row, int col, int ignore_player) {
    for (int i = 0; i < b->player_count; i++) {
        if (i == ignore_player) continue;
        if (b->players[i].row == row && b->players[i].col == col) return 1;
    }
    return 0;
}

/*
 * ثبت/حذف دیوار در آرایه‌های خام.
 * value=1 یعنی دیوار قرار داده شود، value=0 یعنی دیوار جمع شود.
 */
static void place_wall_internal(Board *b, int row, int col, WallDir dir, unsigned char value) {
    if (dir == DIR_H) {
        b->block_down[row][col] = value;
        b->block_down[row][col + 1] = value;
        b->h_wall_at[row][col] = value;
    } else {
        b->block_right[row][col] = value;
        b->block_right[row + 1][col] = value;
        b->v_wall_at[row][col] = value;
    }
}

/* چیدمان اولیهٔ مهره‌ها بر اساس حالت 2 یا 4 نفره */
static void setup_initial_positions(GameState *g) {
    int size = g->board.size;
    int center_top = (size - 1) / 2;
    int center_bottom = size / 2;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        g->board.players[i].row = -1;
        g->board.players[i].col = -1;
    }

    g->board.players[PLAYER1] = make_coord(size - 1, center_bottom);
    g->board.players[PLAYER2] = make_coord(0, center_top);

    if (g->board.player_count == 4) {
        g->board.players[PLAYER3] = make_coord(center_top, 0);
        g->board.players[PLAYER4] = make_coord(center_bottom, size - 1);
    }
}

/* پاکسازی کامل صفحه برای شروع بازی جدید */
void board_init(Board *b, int size) {
    b->size = size;
    b->player_count = 2;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        b->players[i].row = -1;
        b->players[i].col = -1;
        b->walls_left[i] = 0;
        b->walls_max[i] = 0;
    }

    memset(b->block_right, 0, sizeof(b->block_right));
    memset(b->block_down, 0, sizeof(b->block_down));
    memset(b->h_wall_at, 0, sizeof(b->h_wall_at));
    memset(b->v_wall_at, 0, sizeof(b->v_wall_at));
}

/* حذف همهٔ دیوارهای صفحه (برای جادوی remove_all_walls) */
void board_clear_walls(Board *b) {
    memset(b->block_right, 0, sizeof(b->block_right));
    memset(b->block_down, 0, sizeof(b->block_down));
    memset(b->h_wall_at, 0, sizeof(b->h_wall_at));
    memset(b->v_wall_at, 0, sizeof(b->v_wall_at));
}

/* چک محدودهٔ معتبر مختصات */
int board_in_range(const Board *b, int row, int col) {
    return row >= 0 && col >= 0 && row < b->size && col < b->size;
}

/*
 * اعتبارسنجی اولیهٔ دیوار:
 * - جای دیوار داخل محدوده باشد
 * - با دیوار موجود تداخل نداشته باشد
 * - با نوع مخالف (cross) قانون‌شکنی نکند
 */
int board_can_place_wall(const Board *b, int row, int col, WallDir dir) {
    if (row < 0 || col < 0 || row >= b->size - 1 || col >= b->size - 1) {
        return 0;
    }

    if (dir == DIR_H) {
        if (b->v_wall_at[row][col]) return 0;
        if (b->block_down[row][col] || b->block_down[row][col + 1]) return 0;
        return 1;
    }

    if (b->h_wall_at[row][col]) return 0;
    if (b->block_right[row][col] || b->block_right[row + 1][col]) return 0;
    return 1;
}

/*
 * تلاش برای قراردادن دیوار:
 * 1) چک اولیهٔ محل
 * 2) قراردادن موقت
 * 3) اگر validate_paths فعال باشد: برای تک‌تک بازیکنان چک می‌کند که هنوز مسیر دارند
 * 4) اگر غیرقانونی شد، rollback
 */
int board_place_wall(Board *b, int row, int col, WallDir dir, int validate_paths) {
    if (!board_can_place_wall(b, row, col, dir)) return 0;

    place_wall_internal(b, row, col, dir, 1);

    if (validate_paths) {
        for (int p = 0; p < b->player_count; p++) {
            Coord start = b->players[p];
            if (!board_in_range(b, start.row, start.col)) {
                place_wall_internal(b, row, col, dir, 0);
                return 0;
            }
            if (!board_has_path_to_player_goal(b, start, p)) {
                place_wall_internal(b, row, col, dir, 0);
                return 0;
            }
        }
    }

    return 1;
}

/*
 * آیا حرکت از خانه A به B با دیوار بسته شده؟
 * فقط برای خانه‌های مجاور معنی دارد.
 */
int board_is_blocked(const Board *b, int r1, int c1, int r2, int c2) {
    if (!board_in_range(b, r1, c1) || !board_in_range(b, r2, c2)) return 1;

    if (r1 == r2) {
        if (c2 == c1 + 1) return b->block_right[r1][c1];
        if (c2 == c1 - 1) return b->block_right[r1][c2];
    }
    if (c1 == c2) {
        if (r2 == r1 + 1) return b->block_down[r1][c1];
        if (r2 == r1 - 1) return b->block_down[r2][c1];
    }
    return 1;
}

/*
 * BFS کلاسیک برای «رسیدن به یک ردیف هدف».
 * این نسخه بیشتر در مسیرهای 2 نفره/سازگاری استفاده می‌شود.
 */
int board_has_path(const Board *b, Coord start, int goal_row) {
    if (!board_in_range(b, start.row, start.col)) return 0;

    int visited[BOARD_MAX][BOARD_MAX] = {0};
    Coord queue[BOARD_MAX * BOARD_MAX];
    int head = 0;
    int tail = 0;

    visited[start.row][start.col] = 1;
    queue[tail++] = start;

    while (head < tail) {
        Coord cur = queue[head++];
        if (cur.row == goal_row) return 1;

        const int dr[4] = {-1, 1, 0, 0};
        const int dc[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = cur.row + dr[i];
            int nc = cur.col + dc[i];
            if (!board_in_range(b, nr, nc)) continue;
            if (board_is_blocked(b, cur.row, cur.col, nr, nc)) continue;
            if (visited[nr][nc]) continue;
            visited[nr][nc] = 1;
            queue[tail++] = make_coord(nr, nc);
        }
    }

    return 0;
}

/* تبدیل mode به تعداد بازیکن */
int game_player_count_for_mode(GameMode mode) {
    if (mode == MODE_PVP4) return 4;
    return 2;
}

/* تعداد بازیکن معتبر (fail-safe) */
int game_player_count(const GameState *g) {
    if (!g) return 2;
    return clamp_player_count(g->board.player_count);
}

/*
 * اعمال mode:
 * - تعداد بازیکن‌ها را تنظیم می‌کند
 * - نوبت را reset می‌کند
 * - دیوارها/بلاک‌ها را برای بازیکن‌های غیرفعال صفر می‌کند
 * - جای مهره‌ها را دوباره می‌چیند
 */
void game_set_mode(GameState *g, GameMode mode) {
    if (!g) return;

    int base_walls = g->board.walls_max[0];
    if (base_walls < 0) base_walls = 0;

    g->mode = mode;
    g->board.player_count = game_player_count_for_mode(mode);
    g->current_player = PLAYER1;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        g->blocked_turns[i] = 0;
        if (i < g->board.player_count) {
            g->board.walls_max[i] = base_walls;
            g->board.walls_left[i] = base_walls;
        } else {
            g->board.walls_left[i] = 0;
            g->board.walls_max[i] = 0;
            g->board.players[i].row = -1;
            g->board.players[i].col = -1;
            g->player_name[i][0] = '\0';
        }
    }

    setup_initial_positions(g);
}

/* شروع یک game state از صفر */
void game_init(GameState *g, int size, int walls_per_player) {
    board_init(&g->board, size);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        g->player_name[i][0] = '\0';
        g->blocked_turns[i] = 0;
        g->board.walls_max[i] = walls_per_player;
        g->board.walls_left[i] = walls_per_player;
    }

    game_set_mode(g, MODE_PVP);
}

/* ست‌کردن نام یک بازیکن خاص */
void game_set_player_name(GameState *g, int player_index, const char *name) {
    if (!g) return;
    if (player_index < 0 || player_index >= MAX_PLAYERS) return;
    if (!name) return;
    copy_text(g->player_name[player_index], NAME_MAX, name);
}

/* سازگاری با API قبلی: فقط اسم بازیکن 1 و 2 */
void game_set_names(GameState *g, const char *p1, const char *p2) {
    if (p1) copy_text(g->player_name[0], NAME_MAX, p1);
    if (p2) copy_text(g->player_name[1], NAME_MAX, p2);
}

/* انتخاب نوبت بعدی با گردش حلقه‌ای */
int game_next_player(const GameState *g, int current_player) {
    int count = game_player_count(g);
    if (count <= 0) return 0;
    if (current_player < 0 || current_player >= count) return 0;
    return (current_player + 1) % count;
}

/* هدف ردیفی بازیکن (فقط برای 2 نفره) */
int game_goal_row(PlayerId p, int size) {
    if (p == PLAYER1) return 0;
    if (p == PLAYER2) return size - 1;
    return -1;
}

/* آیا بازیکن p همین الان برنده شده؟ */
int game_is_win(const GameState *g, PlayerId p) {
    int player = (int)p;
    if (!g) return 0;
    if (player < 0 || player >= g->board.player_count) return 0;
    return goal_reached_for_player(&g->board, player, g->board.players[player]);
}

/* پیدا کردن اولین برندهٔ موجود */
int game_find_winner(const GameState *g, PlayerId *winner) {
    int count = game_player_count(g);
    for (int p = 0; p < count; p++) {
        if (game_is_win(g, (PlayerId)p)) {
            if (winner) *winner = (PlayerId)p;
            return 1;
        }
    }
    return 0;
}

/* چک می‌کند بازیکن p حداقل یک مسیر قانونی تا هدف خود دارد */
int game_player_has_path(const GameState *g, PlayerId p) {
    int player = (int)p;
    if (!g) return 0;
    if (player < 0 || player >= g->board.player_count) return 0;
    return board_has_path_to_player_goal(&g->board, g->board.players[player], player);
}

/*
 * مهم‌ترین تابع قوانین حرکت:
 * - در 4 نفره: عمداً ساده (فقط حرکت یک خانه مستقیم)
 * - در 2 نفره: حرکت عادی + پرش + حرکت قطری کنار حریف
 */
int game_can_move_to(const GameState *g, PlayerId p, Coord target) {
    const Board *b = &g->board;
    int player = (int)p;
    if (player < 0 || player >= b->player_count) return 0;

    Coord cur = b->players[player];
    if (!board_in_range(b, target.row, target.col)) return 0;
    if (is_cell_occupied(b, target.row, target.col, player)) return 0;

    int dr = target.row - cur.row;
    int dc = target.col - cur.col;
    int man = abs(dr) + abs(dc);

    if (b->player_count != 2) {
        if (man != 1) return 0;
        if (board_is_blocked(b, cur.row, cur.col, target.row, target.col)) return 0;
        return 1;
    }

    int opp_idx = 1 - player;
    Coord opp = b->players[opp_idx];

    if (target.row == opp.row && target.col == opp.col) return 0;

    int odr = opp.row - cur.row;
    int odc = opp.col - cur.col;
    int opp_adjacent = 0;
    if (abs(odr) + abs(odc) == 1) {
        if (!board_is_blocked(b, cur.row, cur.col, opp.row, opp.col)) {
            opp_adjacent = 1;
        }
    }

    Coord jump = make_coord(opp.row + odr, opp.col + odc);
    int jump_possible = 0;
    if (opp_adjacent) {
        if (board_in_range(b, jump.row, jump.col)) {
            if (!board_is_blocked(b, opp.row, opp.col, jump.row, jump.col)) {
                jump_possible = 1;
            }
        }
    }

    if (man == 1) {
        if (board_is_blocked(b, cur.row, cur.col, target.row, target.col)) return 0;
        return 1;
    }

    if (man == 2 && (dr == 0 || dc == 0)) {
        Coord mid = make_coord(cur.row + dr / 2, cur.col + dc / 2);
        if (mid.row != opp.row || mid.col != opp.col) return 0;
        if (board_is_blocked(b, cur.row, cur.col, mid.row, mid.col)) return 0;
        if (board_is_blocked(b, mid.row, mid.col, target.row, target.col)) return 0;
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

        if (board_is_blocked(b, opp.row, opp.col, target.row, target.col)) return 0;
        return 1;
    }

    return 0;
}

/*
 * خروجی دادن لیست حرکت‌های مجاز برای بازیکن:
 * - در 4 نفره فقط 4 جهت پایه بررسی می‌شود
 * - در 2 نفره کاندیدهای پرش/قطری هم به لیست کاندید اضافه می‌شود
 */
int game_list_valid_moves(const GameState *g, PlayerId p, Coord *out, int max_out) {
    int player = (int)p;
    int count = 0;
    Coord cur = g->board.players[player];

    if (g->board.player_count != 2) {
        Coord candidates[4];
        candidates[0] = make_coord(cur.row - 1, cur.col);
        candidates[1] = make_coord(cur.row + 1, cur.col);
        candidates[2] = make_coord(cur.row, cur.col - 1);
        candidates[3] = make_coord(cur.row, cur.col + 1);
        for (int i = 0; i < 4 && count < max_out; i++) {
            if (game_can_move_to(g, p, candidates[i])) out[count++] = candidates[i];
        }
        return count;
    }

    Coord opp = g->board.players[1 - player];
    Coord candidates[8];
    int cand_count = 0;

    candidates[cand_count++] = make_coord(cur.row - 1, cur.col);
    candidates[cand_count++] = make_coord(cur.row + 1, cur.col);
    candidates[cand_count++] = make_coord(cur.row, cur.col - 1);
    candidates[cand_count++] = make_coord(cur.row, cur.col + 1);

    int odr = opp.row - cur.row;
    int odc = opp.col - cur.col;
    if (abs(odr) + abs(odc) == 1) {
        candidates[cand_count++] = make_coord(cur.row + 2 * odr, cur.col + 2 * odc);
        if (odr != 0) {
            candidates[cand_count++] = make_coord(cur.row + odr, cur.col - 1);
            candidates[cand_count++] = make_coord(cur.row + odr, cur.col + 1);
        } else {
            candidates[cand_count++] = make_coord(cur.row - 1, cur.col + odc);
            candidates[cand_count++] = make_coord(cur.row + 1, cur.col + odc);
        }
    }

    for (int i = 0; i < cand_count && count < max_out; i++) {
        if (game_can_move_to(g, p, candidates[i])) out[count++] = candidates[i];
    }

    return count;
}

/* اعمال حرکت اگر قانونی باشد */
int game_move_player(GameState *g, PlayerId p, Coord target, char *err, size_t err_cap) {
    if (!game_can_move_to(g, p, target)) {
        if (err && err_cap) snprintf(err, err_cap, "Error: invalid move");
        return 0;
    }

    g->board.players[p] = target;
    return 1;
}

/* اعمال دیوار اگر قانونی باشد و سهمیهٔ دیوار باقی مانده باشد */
int game_place_wall(GameState *g, PlayerId p, int row, int col, WallDir dir, char *err, size_t err_cap) {
    int player = (int)p;
    if (player < 0 || player >= g->board.player_count) {
        if (err && err_cap) snprintf(err, err_cap, "Error: invalid player");
        return 0;
    }

    if (g->board.walls_left[player] <= 0) {
        if (err && err_cap) snprintf(err, err_cap, "Error: no walls left");
        return 0;
    }

    if (!board_can_place_wall(&g->board, row, col, dir)) {
        if (err && err_cap) snprintf(err, err_cap, "Error: invalid wall placement");
        return 0;
    }

    if (!board_place_wall(&g->board, row, col, dir, 1)) {
        if (err && err_cap) snprintf(err, err_cap, "Error: wall blocks all paths");
        return 0;
    }

    g->board.walls_left[player]--;
    return 1;
}

/*
 * رول جعبهٔ جادویی:
 * - هدف: یک بازیکن تصادفی از بازیکن‌های فعال
 * - kind: پاداش یا طلسم
 * - type/amount: طبق kind انتخاب می‌شود
 */
MagicEvent game_roll_magic(const GameState *g) {
    MagicEvent ev;
    int count = game_player_count(g);

    ev.target = (PlayerId)(rand() % count);
    ev.kind = (MagicKind)(rand() % 2);
    ev.amount = 0;

    if (ev.kind == MAGIC_KIND_TALISMAN) {
        int pick = rand() % 3;
        if (pick == 0) {
            ev.type = MAGIC_CLEAR_WALLS;
            ev.amount = 0;
        } else if (pick == 1) {
            int opts[3] = {2, 3, 5};
            ev.type = MAGIC_DEC_WALLS;
            ev.amount = opts[rand() % 3];
        } else {
            ev.type = MAGIC_BLOCK_TURNS;
            ev.amount = (rand() % 2) + 1;
        }
    } else {
        int pick = rand() % 2;
        if (pick == 0) {
            int opts[3] = {2, 3, 5};
            ev.type = MAGIC_INC_WALLS;
            ev.amount = opts[rand() % 3];
        } else {
            ev.type = MAGIC_STEAL_WALLS;
            ev.amount = (rand() % 2) + 1;
        }
    }

    return ev;
}

/*
 * اعمال اثر جادو روی game state.
 * نکتهٔ مهم steal:
 * - در 2 نفره از حریف می‌دزدد
 * - در 4 نفره از بازیکنی که بیشترین دیوار را دارد می‌گیرد
 */
void game_apply_magic(GameState *g, MagicEvent ev) {
    int count = game_player_count(g);
    int t = (int)ev.target;
    if (t < 0 || t >= count) return;

    switch (ev.type) {
        case MAGIC_CLEAR_WALLS:
            board_clear_walls(&g->board);
            break;
        case MAGIC_DEC_WALLS:
            g->board.walls_left[t] = clamp_nonnegative(g->board.walls_left[t] - ev.amount);
            break;
        case MAGIC_BLOCK_TURNS:
            g->blocked_turns[t] += ev.amount;
            break;
        case MAGIC_INC_WALLS:
            g->board.walls_left[t] += ev.amount;
            break;
        case MAGIC_STEAL_WALLS: {
            int donor = -1;
            int best = -1;
            for (int i = 0; i < count; i++) {
                if (i == t) continue;
                if (g->board.walls_left[i] > best) {
                    best = g->board.walls_left[i];
                    donor = i;
                }
            }

            if (donor >= 0 && best > 0) {
                int stolen = min_int(ev.amount, g->board.walls_left[donor]);
                g->board.walls_left[donor] = clamp_nonnegative(g->board.walls_left[donor] - stolen);
                g->board.walls_left[t] += stolen;
            }
            break;
        }
        default:
            break;
    }
}

/* تبدیل دستهٔ جادو به متن قابل چاپ */
const char *magic_kind_name(MagicKind kind) {
    switch (kind) {
        case MAGIC_KIND_TALISMAN: return "talisman";
        case MAGIC_KIND_REWARD: return "reward";
        default: return "unknown";
    }
}

/* تبدیل نوع جادو به متن قابل چاپ */
const char *magic_type_name(MagicType type) {
    switch (type) {
        case MAGIC_CLEAR_WALLS: return "remove_all_walls";
        case MAGIC_DEC_WALLS: return "decrease_walls";
        case MAGIC_BLOCK_TURNS: return "block_turns";
        case MAGIC_INC_WALLS: return "increase_walls";
        case MAGIC_STEAL_WALLS: return "steal_walls";
        default: return "unknown";
    }
}

static int rng_seeded = 0;

/* seed کردن مولد تصادفی فقط یک‌بار در طول اجرا */
void game_seed_rng(void) {
    if (!rng_seeded) {
        srand((unsigned int)time(NULL));
        rng_seeded = 1;
    }
}
