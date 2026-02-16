#include "io.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
 * io.c لایهٔ ورودی/خروجی متنی برنامه است:
 * - تبدیل متن کاربر به Action استاندارد
 * - چاپ صفحه به شکل ASCII
 * - نمایش وضعیت بازیکن‌ها و رویداد جادو
 *
 * قوانین بازی (حرکت مجاز/دیوار مجاز) در game.c بررسی می‌شود.
 */

/* کپی امن رشته با تضمین پایان صفر */
static void copy_text(char *dst, int cap, const char *src) {
    if (!dst || cap <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, (size_t)cap - 1);
    dst[cap - 1] = '\0';
}

/* ساخت خروجی حرکت */
static void set_action_move(Action *out, int row, int col) {
    out->type = ACT_MOVE;
    out->target.row = row;
    out->target.col = col;
}

/* ساخت خروجی دیوار */
static void set_action_wall(Action *out, int row, int col, WallDir dir) {
    out->type = ACT_WALL;
    out->row = row;
    out->col = col;
    out->dir = dir;
}

/* ساخت خروجی ذخیره/بارگذاری به‌همراه نام فایل */
static void set_action_file(Action *out, ActionType type, const char *filename) {
    out->type = type;
    copy_text(out->filename, (int)sizeof(out->filename), filename);
}

/* نویسهٔ خط جدید انتهای ورودی را حذف می‌کند */
static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/* خواندن یک خط از ورودی استاندارد */
int io_read_line(char *buf, int cap) {
    if (!fgets(buf, cap, stdin)) return 0;
    trim_newline(buf);
    return 1;
}

/* تجزیهٔ سخت‌گیرانهٔ عدد: کل رشته باید عدد باشد */
static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

/* تجزیهٔ کاراکتر جهت دیوار */
static int parse_dir_token(const char *s, WallDir *dir) {
    if (!s || !s[0]) return 0;
    char ch = (char)toupper((unsigned char)s[0]);
    if (ch == 'H') { *dir = DIR_H; return 1; }
    if (ch == 'V') { *dir = DIR_V; return 1; }
    return 0;
}

/* تجزیه‌گر اصلی دستورهای محیط متنی */
int io_parse_action(const char *line, Action *out) {
    if (!line || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->type = ACT_INVALID;

    /*
     * parser ساده و انسانی:
     * - move r c
     * - wall r c H|V
     * - save [name]
     * - load [name]
     * - quit
     * و میان‌بر:
     * - r c
     * - r c H|V
     */
    char buf[LINE_MAX];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tokens[4] = {0};
    int count = 0;
    char *tok = strtok(buf, " \t");
    while (tok && count < 4) {
        tokens[count++] = tok;
        tok = strtok(NULL, " \t");
    }

    if (count == 0) return 0;

    char cmd = (char)tolower((unsigned char)tokens[0][0]);

    if (!strcmp(tokens[0], "move") || cmd == 'm') {
        if (count < 3) return 0;
        int r, c;
        if (!parse_int(tokens[1], &r) || !parse_int(tokens[2], &c)) return 0;
        set_action_move(out, r, c);
        return 1;
    }

    if (!strcmp(tokens[0], "wall") || cmd == 'w') {
        if (count < 4) return 0;
        int r, c;
        WallDir dir;
        if (!parse_int(tokens[1], &r) || !parse_int(tokens[2], &c)) return 0;
        if (!parse_dir_token(tokens[3], &dir)) return 0;
        set_action_wall(out, r, c, dir);
        return 1;
    }

    if (!strcmp(tokens[0], "save") || cmd == 's') {
        if (count >= 2) {
            set_action_file(out, ACT_SAVE, tokens[1]);
        } else {
            set_action_file(out, ACT_SAVE, "save.qsave");
        }
        return 1;
    }

    if (!strcmp(tokens[0], "load") || cmd == 'l') {
        if (count >= 2) {
            set_action_file(out, ACT_LOAD, tokens[1]);
        } else {
            set_action_file(out, ACT_LOAD, "save.qsave");
        }
        return 1;
    }

    if (!strcmp(tokens[0], "quit") || !strcmp(tokens[0], "exit") || cmd == 'q') {
        out->type = ACT_QUIT;
        return 1;
    }

    /* میان‌بر دستور حرکت به شکل «ردیف ستون» */
    if (count == 2) {
        int r, c;
        if (parse_int(tokens[0], &r) && parse_int(tokens[1], &c)) {
            set_action_move(out, r, c);
            return 1;
        }
    }

    /* میان‌بر دستور دیوار به شکل «ردیف ستون جهت» */
    if (count == 3) {
        int r, c;
        WallDir dir;
        if (parse_int(tokens[0], &r) && parse_int(tokens[1], &c) && parse_dir_token(tokens[2], &dir)) {
            set_action_wall(out, r, c, dir);
            return 1;
        }
    }

    return 0;
}

/* دریافت عدد با تکرار تا زمانی که معتبر شود */
int io_read_int(const char *prompt, int min, int max) {
    char line[LINE_MAX];
    int value;

    /* تا وقتی ورودی معتبر نیاید، تکرار می‌کنیم */
    for (;;) {
        if (prompt && prompt[0]) printf("%s", prompt);
        fflush(stdout);
        if (!io_read_line(line, sizeof(line))) return min;
        if (line[0] == '\0') continue;
        if (parse_int(line, &value) && value >= min && value <= max) return value;
        printf("Invalid input. Please enter a number between %d and %d.\n", min, max);
    }
}

/* دریافت رشتهٔ خام از کاربر */
void io_read_string(const char *prompt, char *out, int cap) {
    if (!out || cap <= 0) return;
    if (prompt && prompt[0]) printf("%s", prompt);
    if (!io_read_line(out, cap)) {
        out[0] = '\0';
        return;
    }
}

/* تعیین نویسهٔ نمایشی هر خانه */
static char cell_char(const GameState *g, int row, int col) {
    const Board *b = &g->board;
    for (int i = 0; i < b->player_count; i++) {
        if (b->players[i].row == row && b->players[i].col == col) {
            return (char)('1' + i);
        }
    }
    return '.';
}

/* چاپ صفحهٔ بازی به شکل نویسه‌ای */
void io_print_board(const GameState *g) {
    const Board *b = &g->board;
    int n = b->size;

    /* سرتیتر ستون‌ها */
    printf("    ");
    for (int c = 0; c < n; c++) printf("%2d  ", c);
    printf("\n");

    for (int r = 0; r < n; r++) {
        /* خط خانه‌ها به همراه دیوار عمودی بین آن‌ها */
        printf("%2d  ", r);
        for (int c = 0; c < n; c++) {
            printf(" %c ", cell_char(g, r, c));
            if (c != n - 1) {
                if (b->block_right[r][c]) printf("|");
                else printf(" ");
            }
        }
        printf("\n");

        if (r != n - 1) {
            /* خط دیوارهای افقی بین دو ردیف */
            printf("    ");
            for (int c = 0; c < n; c++) {
                if (b->block_down[r][c]) printf("---");
                else printf("   ");
                if (c != n - 1) printf(" ");
            }
            printf("\n");
        }
    }
}

/* چاپ وضعیت هر دو بازیکن و نوبت فعلی */
void io_print_status(const GameState *g) {
    const Board *b = &g->board;
    /* خروجی عمومی برای 2 یا 4 بازیکن */
    for (int i = 0; i < b->player_count; i++) {
        printf("Player%d: %s | walls: %d | blocked: %d\n",
            i + 1, g->player_name[i], b->walls_left[i], g->blocked_turns[i]);
    }
    printf("Current: %s (player %d)\n", g->player_name[g->current_player], g->current_player + 1);
}

/* چاپ رویداد مربوط به جعبهٔ جادویی */
void io_print_magic(const GameState *g, MagicEvent ev) {
    const char *target_name = g->player_name[ev.target];
    printf("Magic box -> %s | %s | %s",
        target_name, magic_kind_name(ev.kind), magic_type_name(ev.type));
    if (ev.amount > 0) printf(" (%d)", ev.amount);
    printf("\n");
}
