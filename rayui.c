#include "rayui.h"

#ifdef USE_RAYLIB

#include "ai.h"
#include "save.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

/*
 * rayui.c:
 * رابط گرافیکی بازی با raylib.
 *
 * ساختار کلی اجرای UI:
 * 1) PHASE_TURN_START   : شروع نوبت (جادو/بلاک/تشخیص برنده)
 * 2) PHASE_HUMAN_INPUT  : دریافت حرکت یا دیوار از کاربر
 * 3) PHASE_AI_INPUT     : اجرای نوبت AI در PvC
 * 4) PHASE_GAME_OVER    : پایان بازی + پنجرهٔ ذخیره
 *
 * نکتهٔ مهم:
 * - این فایل قانون بازی را اختراع نمی‌کند.
 * - اعتبار حرکت/دیوار همیشه از game.c گرفته می‌شود.
 */

#define SCREEN_W 1280
#define SCREEN_H 860
#define PANEL_W 360
#define MARGIN 24
#define MAX_LOG_LINES 12
#define LOG_LINE_CAP 180
#define STATUS_CAP 200
#define PANEL_BTN_W 150
#define PANEL_BTN_H 34

/* حالت ورودی انسانی: حرکت یا دیوار */
typedef enum {
    UI_INPUT_MOVE = 0,
    UI_INPUT_WALL = 1
} UiInputMode;

/* حالت‌های اصلی چرخهٔ رابط گرافیکی */
typedef enum {
    PHASE_TURN_START = 0,
    PHASE_HUMAN_INPUT,
    PHASE_AI_INPUT,
    PHASE_GAME_OVER
} UiPhase;

/* حالت پنجرهٔ ورود نام فایل برای ذخیره/بارگذاری */
typedef enum {
    PROMPT_NONE = 0,
    PROMPT_SAVE,
    PROMPT_LOAD,
    PROMPT_SAVE_ON_FINISH
} UiPromptMode;

typedef struct {
    int board_x;
    int board_y;
    int cell;
    int board_px;
    int wall_thick;
} BoardLayout;

/* وضعیت سراسری رابط کاربری */
typedef struct {
    UiInputMode input_mode;
    WallDir wall_dir;
    int magic_enabled;
    int winner_declared;
    int winner_id;
    UiPromptMode prompt_mode;
    char prompt_filename[128];
    char last_filename[128];
    char status[STATUS_CAP];
    char log_lines[MAX_LOG_LINES][LOG_LINE_CAP];
    int log_count;
} UiState;

/* کپی امن رشته برای جلوگیری از تکرار strncpy */
static void copy_text(char *dst, int cap, const char *src) {
    if (!dst || cap <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, (size_t)cap - 1);
    dst[cap - 1] = '\0';
}

/* جابه‌جایی بین حالت حرکت و دیوار */
static void toggle_input_mode(UiState *ui) {
    if (ui->input_mode == UI_INPUT_MOVE) ui->input_mode = UI_INPUT_WALL;
    else ui->input_mode = UI_INPUT_MOVE;
}

/* جابه‌جایی بین دیوار افقی و عمودی */
static void toggle_wall_dir(UiState *ui) {
    if (ui->wall_dir == DIR_H) ui->wall_dir = DIR_V;
    else ui->wall_dir = DIR_H;
}

/* تنظیم پیام وضعیت در پنل سمت راست */
static void ui_set_status(UiState *ui, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(ui->status, sizeof(ui->status), fmt, args);
    ui->status[sizeof(ui->status) - 1] = '\0';
    va_end(args);
}

/* افزودن پیام به گزارش رخدادها */
static void ui_push_log(UiState *ui, const char *fmt, ...) {
    char line[LOG_LINE_CAP];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    line[sizeof(line) - 1] = '\0';
    va_end(args);

    if (ui->log_count < MAX_LOG_LINES) {
        copy_text(ui->log_lines[ui->log_count], LOG_LINE_CAP, line);
        ui->log_count++;
        return;
    }

    for (int i = 1; i < MAX_LOG_LINES; i++) {
        copy_text(ui->log_lines[i - 1], LOG_LINE_CAP, ui->log_lines[i]);
    }

    copy_text(ui->log_lines[MAX_LOG_LINES - 1], LOG_LINE_CAP, line);
}

/* پاک‌سازی نام فایل و افزودن پسوند در صورت نیاز */
static void sanitize_filename(const char *in, char *out, int cap) {
    int start = 0;
    int end;
    int len;
    int j = 0;

    if (!in || !out || cap <= 0) return;

    len = (int)strlen(in);
    while (start < len && (in[start] == ' ' || in[start] == '\t')) start++;
    end = len - 1;
    while (end >= start && (in[end] == ' ' || in[end] == '\t')) end--;

    if (start > end) {
        out[0] = '\0';
        return;
    }

    for (int i = start; i <= end && j < cap - 1; i++) {
        unsigned char ch = (unsigned char)in[i];
        if (ch < 32 || ch == '\"') continue;
        out[j++] = (char)ch;
    }
    out[j] = '\0';

    if (out[0] == '\0') return;
    if (!strchr(out, '.')) {
        int cur = (int)strlen(out);
        if (cur + 6 < cap) strcat(out, ".qsave");
    }
}

/* ساخت نام پیش‌فرض برای ذخیرهٔ پایان بازی */
static void make_winner_save_name(const GameState *game, PlayerId winner, char *out, int cap) {
    char base[96];
    if (!game || !out || cap <= 0) return;

    if (game->player_name[winner][0]) {
        snprintf(base, sizeof(base), "%s_win.qsave", game->player_name[winner]);
    } else {
        snprintf(base, sizeof(base), "winner_%d_win.qsave", (int)winner + 1);
    }

    int n = (int)strlen(base);
    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)base[i];
        if (!(isalnum(ch) || ch == '_' || ch == '.' || ch == '-')) base[i] = '_';
    }

    copy_text(out, cap, base);
}

/* شروع پنجرهٔ ورود نام فایل */
static void ui_begin_prompt(UiState *ui, UiPromptMode mode, const char *initial_filename, const char *hint) {
    if (!ui) return;
    ui->prompt_mode = mode;
    if (initial_filename && initial_filename[0]) {
        copy_text(ui->prompt_filename, (int)sizeof(ui->prompt_filename), initial_filename);
    } else {
        ui->prompt_filename[0] = '\0';
    }
    if (hint && hint[0]) ui_set_status(ui, "%s", hint);
}

/* بستن پنجرهٔ ورود نام فایل */
static void ui_end_prompt(UiState *ui) {
    if (!ui) return;
    ui->prompt_mode = PROMPT_NONE;
    ui->prompt_filename[0] = '\0';
}

/* بازنشانی وضعیت برنده در رابط کاربری */
static void ui_reset_winner(UiState *ui) {
    ui->winner_declared = 0;
    ui->winner_id = -1;
}

/* ذخیره‌سازی بازی و به‌روزرسانی پیام‌ها */
static int ui_do_save(GameState *game, UiState *ui, const char *filename) {
    char err[128];
    if (save_game(filename, game, err, sizeof(err))) {
        copy_text(ui->last_filename, (int)sizeof(ui->last_filename), filename);
        ui_push_log(ui, "Saved to %s", filename);
        ui_set_status(ui, "Saved to %s", filename);
        return 1;
    }
    ui_set_status(ui, "%s", err);
    return 0;
}

/* بارگذاری بازی و همگام‌سازی وضعیت رابط */
static int ui_do_load(GameState *game, UiState *ui, const char *filename, UiPhase *phase) {
    char err[128];
    if (load_game(filename, game, err, sizeof(err))) {
        copy_text(ui->last_filename, (int)sizeof(ui->last_filename), filename);
        ui->input_mode = UI_INPUT_MOVE;
        ui->wall_dir = DIR_H;
        ui_reset_winner(ui);
        ui_push_log(ui, "Loaded %s", filename);
        ui_set_status(ui, "Loaded %s", filename);
        *phase = PHASE_TURN_START;
        return 1;
    }
    ui_set_status(ui, "%s", err);
    return 0;
}

/* مدیریت تایپ کاربر داخل پنجرهٔ ذخیره/بارگذاری */
static void ui_update_prompt(GameState *game, UiState *ui, UiPhase *phase) {
    int len;
    int key;
    char final_name[128];
    UiPromptMode mode;

    if (ui->prompt_mode == PROMPT_NONE) return;

    /* دریافت کاراکترهای تایپ‌شده از کاربر */
    len = (int)strlen(ui->prompt_filename);
    key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126 && len < (int)sizeof(ui->prompt_filename) - 1) {
            ui->prompt_filename[len++] = (char)key;
            ui->prompt_filename[len] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && len > 0) {
        ui->prompt_filename[len - 1] = '\0';
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        mode = ui->prompt_mode;
        ui_end_prompt(ui);
        if (mode == PROMPT_SAVE_ON_FINISH) {
            ui_set_status(ui, "Game over. Save skipped.");
            ui_push_log(ui, "Save on finish skipped.");
        }
        return;
    }

    /* تا Enter زده نشود، prompt باز می‌ماند */
    if (!IsKeyPressed(KEY_ENTER)) return;

    sanitize_filename(ui->prompt_filename, final_name, sizeof(final_name));
    if (final_name[0] == '\0') {
        ui_set_status(ui, "Please enter a valid filename.");
        return;
    }

    mode = ui->prompt_mode;
    if (mode == PROMPT_SAVE || mode == PROMPT_SAVE_ON_FINISH) {
        if (ui_do_save(game, ui, final_name)) {
            ui_end_prompt(ui);
            if (mode == PROMPT_SAVE_ON_FINISH) {
                ui_set_status(ui, "Game over. Saved as %s", final_name);
            }
        }
        return;
    }

    if (mode == PROMPT_LOAD) {
        if (ui_do_load(game, ui, final_name, phase)) {
            ui_end_prompt(ui);
        }
    }
}

/* محاسبهٔ چیدمان صفحه نسبت به اندازهٔ پنجره */
static BoardLayout make_layout(int size) {
    int avail_w = SCREEN_W - PANEL_W - (MARGIN * 2);
    int avail_h = SCREEN_H - (MARGIN * 2);
    int cell_w = avail_w / size;
    int cell_h = avail_h / size;
    int cell = (cell_w < cell_h) ? cell_w : cell_h;
    if (cell < 8) cell = 8;

    BoardLayout layout;
    layout.cell = cell;
    layout.board_px = size * cell;
    layout.board_x = MARGIN + (avail_w - layout.board_px) / 2;
    layout.board_y = MARGIN + (avail_h - layout.board_px) / 2;
    layout.wall_thick = cell / 6;
    if (layout.wall_thick < 3) layout.wall_thick = 3;
    return layout;
}

static Rectangle wall_rect(BoardLayout layout, int row, int col, WallDir dir) {
    Rectangle rec;
    if (dir == DIR_H) {
        rec.x = (float)(layout.board_x + col * layout.cell);
        rec.y = (float)(layout.board_y + (row + 1) * layout.cell - layout.wall_thick / 2);
        rec.width = (float)(layout.cell * 2);
        rec.height = (float)layout.wall_thick;
    } else {
        rec.x = (float)(layout.board_x + (col + 1) * layout.cell - layout.wall_thick / 2);
        rec.y = (float)(layout.board_y + row * layout.cell);
        rec.width = (float)layout.wall_thick;
        rec.height = (float)(layout.cell * 2);
    }
    return rec;
}

static Rectangle expand_rect(Rectangle rec, float pad) {
    Rectangle out;
    out.x = rec.x - pad;
    out.y = rec.y - pad;
    out.width = rec.width + (2.0f * pad);
    out.height = rec.height + (2.0f * pad);
    return out;
}

static int point_to_cell(BoardLayout layout, Vector2 mouse, int size, Coord *out) {
    Rectangle board_bounds = {
        (float)layout.board_x,
        (float)layout.board_y,
        (float)layout.board_px,
        (float)layout.board_px
    };

    if (!CheckCollisionPointRec(mouse, board_bounds)) return 0;

    int col = (int)((mouse.x - board_bounds.x) / layout.cell);
    int row = (int)((mouse.y - board_bounds.y) / layout.cell);
    if (row < 0 || col < 0 || row >= size || col >= size) return 0;

    out->row = row;
    out->col = col;
    return 1;
}

/* تبدیل موقعیت نشانگر به نزدیک‌ترین جایگاه دیوار */
static int find_wall_slot(BoardLayout layout, Vector2 mouse, int size, WallDir dir, int *out_row, int *out_col) {
    Rectangle board_bounds = {
        (float)layout.board_x,
        (float)layout.board_y,
        (float)layout.board_px,
        (float)layout.board_px
    };

    if (!CheckCollisionPointRec(mouse, board_bounds)) return 0;

    float gx = (mouse.x - (float)layout.board_x) / (float)layout.cell;
    float gy = (mouse.y - (float)layout.board_y) / (float)layout.cell;

    int row = 0;
    int col = 0;

    if (dir == DIR_H) {
        row = (int)roundf(gy) - 1;
        col = (int)floorf(gx);
    } else {
        row = (int)floorf(gy);
        col = (int)roundf(gx) - 1;
    }

    if (row < 0 || col < 0 || row >= size - 1 || col >= size - 1) return 0;

    Rectangle rec = wall_rect(layout, row, col, dir);
    float pad = (float)layout.cell * 0.25f;
    if (pad < 10.0f) pad = 10.0f;
    if (!CheckCollisionPointRec(mouse, expand_rect(rec, pad))) return 0;

    *out_row = row;
    *out_col = col;
    return 1;
}

static int contains_coord(const Coord *list, int count, Coord x) {
    for (int i = 0; i < count; i++) {
        if (list[i].row == x.row && list[i].col == x.col) return 1;
    }
    return 0;
}

static int can_place_wall_now(const GameState *game, int row, int col, WallDir dir) {
    PlayerId p = (PlayerId)game->current_player;
    if (game->board.walls_left[p] <= 0) return 0;
    Board tmp = game->board;
    return board_place_wall(&tmp, row, col, dir, 1);
}

static int detect_winner(const GameState *game, PlayerId *winner) {
    return game_find_winner(game, winner);
}

/* ورود به حالت پایان بازی و درخواست ذخیره */
static UiPhase enter_game_over(GameState *game, UiState *ui, PlayerId winner) {
    if (!ui->winner_declared) {
        char def_name[128];
        ui->winner_declared = 1;
        ui->winner_id = winner;
        ui_push_log(ui, "Winner: %s", game->player_name[winner]);
        make_winner_save_name(game, winner, def_name, sizeof(def_name));
        ui_begin_prompt(
            ui,
            PROMPT_SAVE_ON_FINISH,
            def_name,
            "Game finished. Type filename + Enter to save. Esc to skip."
        );
    }
    if (ui->prompt_mode == PROMPT_NONE) {
        ui_set_status(ui, "Game over. Press F5/F9 or Esc to close.");
    }
    return PHASE_GAME_OVER;
}

/* پایان نوبت: یا پایان بازی یا جابه‌جایی نوبت */
static UiPhase finish_turn(GameState *game, UiState *ui) {
    PlayerId winner;
    if (detect_winner(game, &winner)) {
        return enter_game_over(game, ui, winner);
    }

    game->current_player = game_next_player(game, game->current_player);
    ui_set_status(ui, "");
    return PHASE_TURN_START;
}

/* شروع نوبت جدید با بررسی برد، جادو، انسداد و رایانه */
static UiPhase begin_turn(GameState *game, UiState *ui) {
    PlayerId winner;
    if (detect_winner(game, &winner)) {
        return enter_game_over(game, ui, winner);
    }

    /*
     * ترتیب شروع نوبت:
     * 1) اگر قبلا برنده داشتیم -> game over
     * 2) اگر magic روشن است -> رول + اعمال
     * 3) اگر بازیکن بلاک بود -> skip
     * 4) تعیین اینکه نوبت AI است یا Human
     */
    int current = game->current_player;
    if (ui->magic_enabled) {
        MagicEvent ev = game_roll_magic(game);
        game_apply_magic(game, ev);

        if (ev.amount > 0) {
            ui_push_log(ui, "Magic -> %s | %s | %s (%d)",
                game->player_name[ev.target], magic_kind_name(ev.kind), magic_type_name(ev.type), ev.amount);
        } else {
            ui_push_log(ui, "Magic -> %s | %s | %s",
                game->player_name[ev.target], magic_kind_name(ev.kind), magic_type_name(ev.type));
        }

        if (detect_winner(game, &winner)) {
            return enter_game_over(game, ui, winner);
        }
    }

    if (game->blocked_turns[current] > 0) {
        game->blocked_turns[current]--;
        ui_push_log(ui, "%s is blocked. Turn skipped.", game->player_name[current]);
        game->current_player = game_next_player(game, current);
        return PHASE_TURN_START;
    }

    if (game->mode == MODE_PVC && game->current_player == PLAYER2) {
        ui_set_status(ui, "Computer turn...");
        return PHASE_AI_INPUT;
    }

    ui_set_status(ui, "%s turn", game->player_name[game->current_player]);
    return PHASE_HUMAN_INPUT;
}

/* اجرای نوبت رایانه */
static UiPhase handle_ai_turn(GameState *game, UiState *ui) {
    char err[128];
    AiAction action;
    PlayerId p = (PlayerId)game->current_player;

    if (!ai_choose_action(game, p, &action)) {
        ui_push_log(ui, "Computer has no valid moves.");
        ui_set_status(ui, "Game over. Press Esc to close.");
        return PHASE_GAME_OVER;
    }

    if (action.type == AI_ACT_MOVE) {
        if (game_move_player(game, p, action.move, err, sizeof(err))) {
            ui_push_log(ui, "Computer moved to (%d, %d)", action.move.row, action.move.col);
            return finish_turn(game, ui);
        }
        ui_push_log(ui, "Computer move failed: %s", err);
        ui_set_status(ui, "Computer move failed.");
        return PHASE_GAME_OVER;
    }

    if (game_place_wall(game, p, action.row, action.col, action.dir, err, sizeof(err))) {
        ui_push_log(ui, "Computer wall (%d, %d) %c",
            action.row, action.col, action.dir == DIR_H ? 'H' : 'V');
        return finish_turn(game, ui);
    }

    ui_push_log(ui, "Computer wall failed: %s", err);
    ui_set_status(ui, "Computer wall failed.");
    return PHASE_GAME_OVER;
}

/* اجرای نوبت بازیکن انسانی با صفحه‌کلید و ماوس */
static UiPhase handle_human_input(GameState *game, UiState *ui, BoardLayout layout) {
    char err[128];
    PlayerId p = (PlayerId)game->current_player;
    int panel_x = SCREEN_W - PANEL_W;
    Rectangle btn_move = {(float)(panel_x + 16), 220.0f, PANEL_BTN_W, PANEL_BTN_H};
    Rectangle btn_wall = {(float)(panel_x + 186), 220.0f, PANEL_BTN_W, PANEL_BTN_H};
    Rectangle btn_h = {(float)(panel_x + 16), 262.0f, PANEL_BTN_W, PANEL_BTN_H};
    Rectangle btn_v = {(float)(panel_x + 186), 262.0f, PANEL_BTN_W, PANEL_BTN_H};

    /* وقتی prompt ذخیره/لود باز است، نوبت انسانی نباید کلیک بازی را مصرف کند */
    if (ui->prompt_mode != PROMPT_NONE) {
        return PHASE_HUMAN_INPUT;
    }

    if (IsKeyPressed(KEY_TAB)) {
        toggle_input_mode(ui);
    }
    if (IsKeyPressed(KEY_M)) ui->input_mode = UI_INPUT_MOVE;
    if (IsKeyPressed(KEY_W)) ui->input_mode = UI_INPUT_WALL;
    if (IsKeyPressed(KEY_H)) ui->wall_dir = DIR_H;
    if (IsKeyPressed(KEY_V)) ui->wall_dir = DIR_V;
    if (IsKeyPressed(KEY_F2)) {
        ui->magic_enabled = !ui->magic_enabled;
        ui_push_log(ui, "Magic: %s", ui->magic_enabled ? "ON" : "OFF");
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        toggle_wall_dir(ui);
    }

    /* اگر هنوز کلیک چپ نشده، فقط در همین فاز می‌مانیم */
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return PHASE_HUMAN_INPUT;
    }

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, btn_move)) {
        ui->input_mode = UI_INPUT_MOVE;
        return PHASE_HUMAN_INPUT;
    }
    if (CheckCollisionPointRec(mouse, btn_wall)) {
        ui->input_mode = UI_INPUT_WALL;
        return PHASE_HUMAN_INPUT;
    }
    if (CheckCollisionPointRec(mouse, btn_h)) {
        ui->wall_dir = DIR_H;
        return PHASE_HUMAN_INPUT;
    }
    if (CheckCollisionPointRec(mouse, btn_v)) {
        ui->wall_dir = DIR_V;
        return PHASE_HUMAN_INPUT;
    }

    /* مسیر حرکت: کلیک روی خانه -> اعتبارسنجی -> اعمال */
    if (ui->input_mode == UI_INPUT_MOVE) {
        Coord target;
        if (!point_to_cell(layout, mouse, game->board.size, &target)) {
            return PHASE_HUMAN_INPUT;
        }

        if (!game_move_player(game, p, target, err, sizeof(err))) {
            ui_set_status(ui, "%s", err);
            return PHASE_HUMAN_INPUT;
        }

        ui_push_log(ui, "%s moved to (%d, %d)", game->player_name[p], target.row, target.col);
        return finish_turn(game, ui);
    }

    /* مسیر دیوار: کلیک روی اسلات -> اعتبارسنجی -> اعمال */
    int row, col;
    if (game->board.walls_left[p] <= 0) {
        ui_set_status(ui, "Error: no walls left");
        return PHASE_HUMAN_INPUT;
    }
    if (!find_wall_slot(layout, mouse, game->board.size, ui->wall_dir, &row, &col)) {
        ui_set_status(ui, "Click closer to a wall slot.");
        return PHASE_HUMAN_INPUT;
    }

    if (!game_place_wall(game, p, row, col, ui->wall_dir, err, sizeof(err))) {
        ui_set_status(ui, "%s", err);
        return PHASE_HUMAN_INPUT;
    }

    ui_push_log(ui, "%s wall (%d, %d) %c",
        game->player_name[p], row, col, ui->wall_dir == DIR_H ? 'H' : 'V');
    return finish_turn(game, ui);
}

/* رسم صفحهٔ بازی و پیش‌نمایش حرکت یا دیوار */
static void draw_board(const GameState *game, const UiState *ui, UiPhase phase, BoardLayout layout) {
    const Board *b = &game->board;
    int size = b->size;

    /* در حالت move، خانه‌های مجاز با رنگ متفاوت هایلایت می‌شوند */
    Coord valid_moves[16];
    int valid_count = 0;
    if (phase == PHASE_HUMAN_INPUT && ui->input_mode == UI_INPUT_MOVE) {
        valid_count = game_list_valid_moves(game, (PlayerId)game->current_player, valid_moves, 16);
    }

    /* رسم شمارهٔ محور ستون‌ها و ردیف‌ها */
    for (int c = 0; c < size; c++) {
        int x = layout.board_x + c * layout.cell + layout.cell / 2 - 6;
        DrawText(TextFormat("%d", c), x, layout.board_y - 20, 16, DARKGRAY);
    }
    /* رسم شبکهٔ خانه‌ها */
    for (int r = 0; r < size; r++) {
        int y = layout.board_y + r * layout.cell + layout.cell / 2 - 8;
        DrawText(TextFormat("%d", r), layout.board_x - 20, y, 16, DARKGRAY);
    }

    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            Coord cell = {r, c};
            Color base = ((r + c) % 2 == 0) ? (Color){243, 236, 210, 255} : (Color){228, 218, 186, 255};
            if (r == 0) base = ColorAlpha((Color){243, 141, 141, 255}, 0.18f);
            if (r == size - 1) base = ColorAlpha((Color){141, 174, 243, 255}, 0.18f);
            if (contains_coord(valid_moves, valid_count, cell)) {
                base = (Color){199, 237, 184, 255};
            }

            Rectangle rec = {
                (float)(layout.board_x + c * layout.cell),
                (float)(layout.board_y + r * layout.cell),
                (float)layout.cell,
                (float)layout.cell
            };

            DrawRectangleRec(rec, base);
            DrawRectangleLinesEx(rec, 1.0f, (Color){120, 106, 82, 255});
        }
    }

    /* رسم دیوارهای ثبت‌شده */
    for (int r = 0; r < size - 1; r++) {
        for (int c = 0; c < size - 1; c++) {
            if (b->h_wall_at[r][c]) {
                DrawRectangleRec(wall_rect(layout, r, c, DIR_H), (Color){128, 70, 34, 255});
            }
            if (b->v_wall_at[r][c]) {
                DrawRectangleRec(wall_rect(layout, r, c, DIR_V), (Color){128, 70, 34, 255});
            }
        }
    }

    /* رسم مهرهٔ بازیکن‌ها (۲ یا ۴ نفر) */
    int radius = layout.cell / 3;
    if (radius < 6) radius = 6;
    Color player_colors[MAX_PLAYERS] = {
        (Color){221, 74, 63, 255},
        (Color){56, 109, 219, 255},
        (Color){58, 161, 105, 255},
        (Color){230, 161, 48, 255}
    };
    for (int i = 0; i < b->player_count; i++) {
        Coord pi = b->players[i];
        DrawCircle(
            layout.board_x + pi.col * layout.cell + layout.cell / 2,
            layout.board_y + pi.row * layout.cell + layout.cell / 2,
            (float)radius,
            player_colors[i]
        );
    }

    /* پیش‌نمایش دیوار قبل از کلیک نهایی */
    if (phase == PHASE_HUMAN_INPUT && ui->input_mode == UI_INPUT_WALL) {
        int row, col;
        if (find_wall_slot(layout, GetMousePosition(), size, ui->wall_dir, &row, &col)) {
            int ok = can_place_wall_now(game, row, col, ui->wall_dir);
            Color preview = ok ? (Color){62, 164, 83, 140} : (Color){204, 70, 70, 140};
            DrawRectangleRec(wall_rect(layout, row, col, ui->wall_dir), preview);
        }
    }
}

/* رسم پنل اطلاعات در سمت راست */
static void draw_panel(const GameState *game, const UiState *ui, UiPhase phase) {
    int panel_x = SCREEN_W - PANEL_W;
    Rectangle btn_move = {(float)(panel_x + 16), 220.0f, PANEL_BTN_W, PANEL_BTN_H};
    Rectangle btn_wall = {(float)(panel_x + 186), 220.0f, PANEL_BTN_W, PANEL_BTN_H};
    Rectangle btn_h = {(float)(panel_x + 16), 262.0f, PANEL_BTN_W, PANEL_BTN_H};
    Rectangle btn_v = {(float)(panel_x + 186), 262.0f, PANEL_BTN_W, PANEL_BTN_H};

    DrawRectangle(panel_x, 0, PANEL_W, SCREEN_H, (Color){30, 37, 46, 255});

    int y = 20;
    DrawText("QUORIDOR - MADAI AND ZIAEI", panel_x + 16, y, 22, (Color){232, 236, 239, 255});
    y += 40;

    DrawText(TextFormat("Current: %s", game->player_name[game->current_player]), panel_x + 16, y, 20, (Color){241, 196, 83, 255});
    y += 30;

    Color player_text_colors[MAX_PLAYERS] = {
        (Color){236, 102, 91, 255},
        (Color){113, 162, 255, 255},
        (Color){115, 196, 135, 255},
        (Color){242, 193, 115, 255}
    };
    for (int i = 0; i < game->board.player_count; i++) {
        DrawText(TextFormat("P%d (%s): walls %d | blocked %d",
            i + 1, game->player_name[i], game->board.walls_left[i], game->blocked_turns[i]),
            panel_x + 16, y, 18, player_text_colors[i]);
        y += 26;
    }

    if (game->board.player_count == 4) {
        DrawText("Goals: P1 up, P2 down, P3 right, P4 left", panel_x + 16, y, 17, (Color){170, 182, 196, 255});
    } else {
        DrawText("Goals: P1 -> row 0, P2 -> last row", panel_x + 16, y, 17, (Color){170, 182, 196, 255});
    }
    y += 28;
    DrawText(TextFormat("Magic: %s (F2)", ui->magic_enabled ? "ON" : "OFF"), panel_x + 16, y, 17, (Color){170, 182, 196, 255});
    y += 24;
    DrawText(TextFormat("Last file: %s", ui->last_filename), panel_x + 16, y, 17, (Color){170, 182, 196, 255});
    y += 24;

    DrawText("Mode / Direction:", panel_x + 16, y, 20, (Color){205, 210, 215, 255});
    y += 26;
    DrawRectangleRounded(btn_move, 0.3f, 6, ui->input_mode == UI_INPUT_MOVE ? (Color){68, 173, 102, 255} : (Color){74, 86, 100, 255});
    DrawRectangleRounded(btn_wall, 0.3f, 6, ui->input_mode == UI_INPUT_WALL ? (Color){68, 173, 102, 255} : (Color){74, 86, 100, 255});
    DrawText("MOVE", (int)btn_move.x + 48, (int)btn_move.y + 8, 18, RAYWHITE);
    DrawText("WALL", (int)btn_wall.x + 48, (int)btn_wall.y + 8, 18, RAYWHITE);
    DrawRectangleRounded(btn_h, 0.3f, 6, ui->wall_dir == DIR_H ? (Color){85, 129, 207, 255} : (Color){74, 86, 100, 255});
    DrawRectangleRounded(btn_v, 0.3f, 6, ui->wall_dir == DIR_V ? (Color){85, 129, 207, 255} : (Color){74, 86, 100, 255});
    DrawText("HORIZONTAL", (int)btn_h.x + 24, (int)btn_h.y + 8, 17, RAYWHITE);
    DrawText("VERTICAL", (int)btn_v.x + 30, (int)btn_v.y + 8, 17, RAYWHITE);
    y += 88;

    DrawText("Controls:", panel_x + 16, y, 20, (Color){205, 210, 215, 255});
    y += 26;
    DrawText("M=move mode  W=wall mode", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 22;
    DrawText("H/V=wall dir  RightClick=toggle", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 22;
    DrawText("F2=toggle magic", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 22;
    DrawText("LeftClick=select/apply action", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 22;
    DrawText("F5=Save As  F9=Load As", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 22;
    DrawText("Ctrl+S=quick save  Ctrl+L=quick load", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 22;
    DrawText("Esc=exit", panel_x + 16, y, 18, (Color){160, 171, 182, 255});
    y += 34;

    DrawText("Status:", panel_x + 16, y, 20, (Color){205, 210, 215, 255});
    y += 24;
    DrawText(ui->status[0] ? ui->status : "-", panel_x + 16, y, 18, (Color){241, 196, 83, 255});
    y += 32;

    DrawText("Recent events:", panel_x + 16, y, 20, (Color){205, 210, 215, 255});
    y += 24;
    for (int i = 0; i < ui->log_count; i++) {
        DrawText(ui->log_lines[i], panel_x + 16, y, 16, (Color){178, 188, 197, 255});
        y += 20;
        if (y > SCREEN_H - 24) break;
    }

    if (phase == PHASE_GAME_OVER) {
        DrawText("GAME OVER", panel_x + 16, SCREEN_H - 70, 28, (Color){241, 196, 83, 255});
        if (ui->winner_declared && ui->winner_id >= 0 && ui->winner_id < game->board.player_count) {
            DrawText(TextFormat("Winner: %s", game->player_name[ui->winner_id]), panel_x + 16, SCREEN_H - 40, 20, (Color){241, 196, 83, 255});
        }
    }
}

/* رسیدگی به کلیدهای میان‌بر سراسری */
static int handle_global_shortcuts(GameState *game, UiState *ui, UiPhase *phase) {
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ui->prompt_mode != PROMPT_NONE) return 0;

    if (IsKeyPressed(KEY_F5)) {
        ui_begin_prompt(ui, PROMPT_SAVE, ui->last_filename, "Save As: type filename + Enter.");
        return 1;
    }

    if (IsKeyPressed(KEY_F9)) {
        ui_begin_prompt(ui, PROMPT_LOAD, ui->last_filename, "Load As: type filename + Enter.");
        return 1;
    }

    if (ctrl && IsKeyPressed(KEY_S)) {
        ui_do_save(game, ui, ui->last_filename);
        return 1;
    }

    if (ctrl && IsKeyPressed(KEY_L)) {
        ui_do_load(game, ui, ui->last_filename, phase);
        return 1;
    }

    return 0;
}

static const char *prompt_title(UiPromptMode mode) {
    switch (mode) {
        case PROMPT_SAVE: return "SAVE GAME";
        case PROMPT_LOAD: return "LOAD GAME";
        case PROMPT_SAVE_ON_FINISH: return "SAVE FINISHED GAME";
        default: return "";
    }
}

/* رسم پنجرهٔ رویی برای ورود نام فایل */
static void draw_prompt_overlay(const UiState *ui) {
    if (ui->prompt_mode == PROMPT_NONE) return;

    Rectangle box = {(SCREEN_W - 760) / 2.0f, (SCREEN_H - 220) / 2.0f, 760.0f, 220.0f};
    int blink_on = (((int)(GetTime() * 2.0)) % 2) == 0;
    char line[180];

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, ColorAlpha(BLACK, 0.45f));
    DrawRectangleRounded(box, 0.08f, 10, (Color){24, 30, 38, 245});
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, 2.0f, (Color){90, 104, 120, 255});

    DrawText(prompt_title(ui->prompt_mode), (int)box.x + 24, (int)box.y + 18, 30, (Color){235, 239, 244, 255});
    DrawText("Filename:", (int)box.x + 24, (int)box.y + 70, 22, (Color){193, 201, 210, 255});

    Rectangle input = {(float)((int)box.x + 140), (float)((int)box.y + 66), box.width - 164.0f, 42.0f};
    DrawRectangleRounded(input, 0.08f, 8, (Color){248, 247, 241, 255});
    DrawRectangleRoundedLinesEx(input, 0.08f, 8, 2.0f, (Color){110, 122, 138, 255});

    snprintf(line, sizeof(line), "%s%s", ui->prompt_filename, blink_on ? "_" : "");
    DrawText(line, (int)input.x + 10, (int)input.y + 10, 22, (Color){34, 42, 50, 255});

    DrawText("Enter = confirm", (int)box.x + 24, (int)box.y + 132, 20, (Color){176, 187, 198, 255});
    DrawText("Esc = cancel", (int)box.x + 220, (int)box.y + 132, 20, (Color){176, 187, 198, 255});
    DrawText("Tip: extension .qsave is auto-added if omitted.", (int)box.x + 24, (int)box.y + 168, 18, (Color){154, 168, 183, 255});
}

int rayui_available(void) {
    return 1;
}

/* حلقهٔ اصلی رابط گرافیکی */
int rayui_run(GameState *game) {
    if (!game) return 1;

    InitWindow(SCREEN_W, SCREEN_H, "Quoridor");
    SetTargetFPS(60);

    UiState ui;
    memset(&ui, 0, sizeof(ui));
    ui.input_mode = UI_INPUT_MOVE;
    ui.wall_dir = DIR_H;
    ui.magic_enabled = 1;
    ui_reset_winner(&ui);
    ui.prompt_mode = PROMPT_NONE;
    strcpy(ui.last_filename, "save.qsave");
    ui_set_status(&ui, "Starting...");
    ui_push_log(&ui, "MADAI AND ZIAEI edition ready.");

    UiPhase phase =
     PHASE_TURN_START;

    /*
     * حلقهٔ فریم:
     * - update (input/state)
     * - draw
     */
    while (!WindowShouldClose()) {
        BoardLayout layout = make_layout(game->board.size);
        if (phase != PHASE_GAME_OVER) {
            PlayerId winner;
            if (detect_winner(game, &winner)) {
                phase = enter_game_over(game, &ui, winner);
            }
        }

        ui_update_prompt(game, &ui, &phase);

        if (ui.prompt_mode == PROMPT_NONE) {
            int consumed = handle_global_shortcuts(game, &ui, &phase);
            if (!consumed) {
                switch (phase) {
                    case PHASE_TURN_START:
                        phase = begin_turn(game, &ui);
                        break;
                    case PHASE_HUMAN_INPUT:
                        phase = handle_human_input(game, &ui, layout);
                        break;
                    case PHASE_AI_INPUT:
                        phase = handle_ai_turn(game, &ui);
                        break;
                    case PHASE_GAME_OVER:
                        break;
                    default:
                        break;
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){248, 247, 241, 255});
        draw_board(game, &ui, phase, layout);
        draw_panel(game, &ui, phase);
        draw_prompt_overlay(&ui);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

#else

int rayui_available(void) {
    return 0;
}

int rayui_run(GameState *game) {
    (void)game;
    return 0;
}

#endif
