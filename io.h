#ifndef SIMPLE_IO_H
#define SIMPLE_IO_H

#include "game.h"

#define LINE_MAX_LEN 256

typedef enum {
    ACT_INVALID = 0,
    ACT_MOVE,
    ACT_WALL,
    ACT_SAVE,
    ACT_LOAD,
    ACT_QUIT
} ActionType;

typedef struct {
    ActionType type;
    Pos target;
    int row;
    int col;
    WallDir dir;
    char filename[128];
} Action;

int io_read_line(char *buf, int cap);
int io_read_int(const char *prompt, int min, int max);
void io_read_string(const char *prompt, char *out, int cap);

int io_parse_action(const char *line, Action *a);
void io_print_board(const Game *g);
void io_print_status(const Game *g);

#endif
