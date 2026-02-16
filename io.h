#ifndef IO_H
#define IO_H

#include "game.h"

/*
 * io.h قرارداد لایهٔ ورودی/خروجی متنی است.
 * این لایه:
 * - متن خام کاربر را به Action تبدیل می‌کند
 * - برد و وضعیت بازی را چاپ می‌کند
 * - توابع کمکی دریافت عدد/رشته دارد
 */

/* حداکثر طول خط ورودی کنسول */
#define LINE_MAX 256

/* نوع کنشی که کاربر می‌تواند وارد کند */
typedef enum {
    ACT_INVALID = 0,
    ACT_MOVE,
    ACT_WALL,
    ACT_SAVE,
    ACT_LOAD,
    ACT_QUIT
} ActionType;

/* خروجی parse یک خط فرمان */
typedef struct {
    ActionType type;
    Coord target;
    int row;
    int col;
    WallDir dir;
    char filename[128];
} Action;

/* خواندن خط از stdin */
int io_read_line(char *buf, int cap);
/* تبدیل متن کاربر به Action */
int io_parse_action(const char *line, Action *out);

/* دریافت عدد در بازهٔ مشخص */
int io_read_int(const char *prompt, int min, int max);
/* دریافت رشته از کاربر */
void io_read_string(const char *prompt, char *out, int cap);

/* چاپ ASCII برد */
void io_print_board(const GameState *g);
/* چاپ وضعیت بازیکن‌ها و نوبت */
void io_print_status(const GameState *g);
/* چاپ رویداد جادویی */
void io_print_magic(const GameState *g, MagicEvent ev);

#endif
