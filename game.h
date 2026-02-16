#ifndef GAME_H
#define GAME_H

#include <stddef.h>

/*
 * این فایل هستهٔ مدل داده و قرارداد توابع بازی است.
 * اگر بخواهی کل پروژه را سریع بفهمی، از همین‌جا شروع کن:
 * 1) ساختار Board: وضعیت صفحه، دیوارها و جای مهره‌ها
 * 2) ساختار GameState: وضعیت کامل یک مسابقه
 * 3) توابع board_* : قوانین پایهٔ صفحه (بدون UI)
 * 4) توابع game_*  : قوانین نوبت، حرکت، برد و جادو
 *
 * نکتهٔ مهم:
 * - بازی هم 2 نفره دارد هم 4 نفره.
 * - UI یا کنسول فقط مصرف‌کنندهٔ این API هستند.
 * - هر تغییری در قانون بازی باید اول اینجا و game.c انجام شود.
 */

/* بیشترین اندازهٔ صفحه (N x N) */
#define BOARD_MAX 50
/* بیشترین طول اسم بازیکن */
#define NAME_MAX 64
/* سقف بازیکن‌های فعال در موتور بازی */
#define MAX_PLAYERS 4

/* نوع دیوار: افقی یا عمودی */
typedef enum { DIR_H = 0, DIR_V = 1 } WallDir;

/* شناسهٔ ثابت بازیکن‌ها */
typedef enum {
    PLAYER1 = 0,
    PLAYER2 = 1,
    PLAYER3 = 2,
    PLAYER4 = 3
} PlayerId;

/* حالت‌های کلی بازی */
typedef enum {
    MODE_PVP = 0,
    MODE_PVC = 1,
    MODE_PVP4 = 2
} GameMode;

/* یک مختصات روی صفحه */
typedef struct {
    int row;
    int col;
} Coord;

/*
 * نمای کامل از صفحه:
 * - اندازه
 * - بازیکن‌های فعال
 * - دیوار باقی‌مانده/حداکثر
 * - آرایه‌های مسدودشدگی حرکت
 * - آرایهٔ موقعیت دیوار برای رسم دقیق UI
 */
typedef struct {
    int size;
    int player_count;
    Coord players[MAX_PLAYERS];
    int walls_left[MAX_PLAYERS];
    int walls_max[MAX_PLAYERS];

    unsigned char block_right[BOARD_MAX][BOARD_MAX];
    unsigned char block_down[BOARD_MAX][BOARD_MAX];

    unsigned char h_wall_at[BOARD_MAX][BOARD_MAX];
    unsigned char v_wall_at[BOARD_MAX][BOARD_MAX];
} Board;

/*
 * وضعیت کل مسابقه:
 * - board: وضعیت فنی صفحه
 * - player_name: اسم‌ها برای UI/چاپ
 * - mode: حالت بازی
 * - current_player: نوبت فعلی
 * - blocked_turns: تعداد نوبت بلاک برای هر بازیکن
 */
typedef struct {
    Board board;
    char player_name[MAX_PLAYERS][NAME_MAX];
    GameMode mode;
    int current_player;
    int blocked_turns[MAX_PLAYERS];
} GameState;

/* نوع اثر جعبهٔ جادویی */
typedef enum {
    MAGIC_CLEAR_WALLS = 0,
    MAGIC_DEC_WALLS,
    MAGIC_BLOCK_TURNS,
    MAGIC_INC_WALLS,
    MAGIC_STEAL_WALLS
} MagicType;

/* دستهٔ جادو: طلسم/پاداش */
typedef enum {
    MAGIC_KIND_TALISMAN = 0,
    MAGIC_KIND_REWARD = 1
} MagicKind;

/* خروجی یک رول جادویی */
typedef struct {
    MagicKind kind;
    MagicType type;
    PlayerId target;
    int amount;
} MagicEvent;

/* ===== توابع پایهٔ صفحه ===== */
void board_init(Board *b, int size);
void board_clear_walls(Board *b);
int board_in_range(const Board *b, int row, int col);
int board_can_place_wall(const Board *b, int row, int col, WallDir dir);
int board_place_wall(Board *b, int row, int col, WallDir dir, int validate_paths);
int board_is_blocked(const Board *b, int r1, int c1, int r2, int c2);
int board_has_path(const Board *b, Coord start, int goal_row);

/* ===== توابع اصلی بازی ===== */
void game_init(GameState *g, int size, int walls_per_player);
void game_set_mode(GameState *g, GameMode mode);
void game_set_names(GameState *g, const char *p1, const char *p2);
void game_set_player_name(GameState *g, int player_index, const char *name);
int game_player_count_for_mode(GameMode mode);
int game_player_count(const GameState *g);
int game_next_player(const GameState *g, int current_player);
int game_goal_row(PlayerId p, int size);
int game_is_win(const GameState *g, PlayerId p);
int game_find_winner(const GameState *g, PlayerId *winner);
int game_player_has_path(const GameState *g, PlayerId p);

/* ===== قوانین حرکت ===== */
int game_can_move_to(const GameState *g, PlayerId p, Coord target);
int game_list_valid_moves(const GameState *g, PlayerId p, Coord *out, int max_out);

/* ===== اعمال حرکت/دیوار ===== */
int game_move_player(GameState *g, PlayerId p, Coord target, char *err, size_t err_cap);
int game_place_wall(GameState *g, PlayerId p, int row, int col, WallDir dir, char *err, size_t err_cap);

/* ===== جادوی بازی ===== */
MagicEvent game_roll_magic(const GameState *g);
void game_apply_magic(GameState *g, MagicEvent ev);
const char *magic_kind_name(MagicKind kind);
const char *magic_type_name(MagicType type);
void game_seed_rng(void);

#endif
