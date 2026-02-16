#include "io.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

int io_read_line(char *buf, int cap) {
    if (!fgets(buf, cap, stdin)) return 0;
    trim_newline(buf);
    return 1;
}

static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

int io_read_int(const char *prompt, int min, int max) {
    char line[LINE_MAX_LEN];
    int value;
    for (;;) {
        printf("%s", prompt);
        if (!io_read_line(line, sizeof(line))) return min;
        if (parse_int(line, &value) && value >= min && value <= max) return value;
        printf("Please enter a number between %d and %d.\n", min, max);
    }
}

void io_read_string(const char *prompt, char *out, int cap) {
    printf("%s", prompt);
    if (!io_read_line(out, cap)) out[0] = '\0';
}

static int parse_dir_char(char ch, WallDir *dir) {
    ch = (char)toupper((unsigned char)ch);
    if (ch == 'H') {
        *dir = DIR_H;
        return 1;
    }
    if (ch == 'V') {
        *dir = DIR_V;
        return 1;
    }
    return 0;
}

int io_parse_action(const char *line, Action *a) {
    int r;
    int c;
    char d;
    char filename[128];

    if (!line || !a) return 0;
    memset(a, 0, sizeof(*a));
    a->type = ACT_INVALID;

    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) {
        a->type = ACT_QUIT;
        return 1;
    }

    if (sscanf(line, "move %d %d", &r, &c) == 2) {
        a->type = ACT_MOVE;
        a->target.row = r;
        a->target.col = c;
        return 1;
    }

    if (sscanf(line, "wall %d %d %c", &r, &c, &d) == 3) {
        WallDir dir;
        if (!parse_dir_char(d, &dir)) return 0;
        a->type = ACT_WALL;
        a->row = r;
        a->col = c;
        a->dir = dir;
        return 1;
    }

    if (sscanf(line, "save %127s", filename) == 1) {
        a->type = ACT_SAVE;
        strncpy(a->filename, filename, sizeof(a->filename) - 1);
        return 1;
    }
    if (strcmp(line, "save") == 0 || strcmp(line, "s") == 0) {
        a->type = ACT_SAVE;
        strcpy(a->filename, "save.bin");
        return 1;
    }

    if (sscanf(line, "load %127s", filename) == 1) {
        a->type = ACT_LOAD;
        strncpy(a->filename, filename, sizeof(a->filename) - 1);
        return 1;
    }
    if (strcmp(line, "load") == 0 || strcmp(line, "l") == 0) {
        a->type = ACT_LOAD;
        strcpy(a->filename, "save.bin");
        return 1;
    }

    if (sscanf(line, "%d %d %c", &r, &c, &d) == 3) {
        WallDir dir;
        if (!parse_dir_char(d, &dir)) return 0;
        a->type = ACT_WALL;
        a->row = r;
        a->col = c;
        a->dir = dir;
        return 1;
    }

    if (sscanf(line, "%d %d", &r, &c) == 2) {
        a->type = ACT_MOVE;
        a->target.row = r;
        a->target.col = c;
        return 1;
    }

    return 0;
}

static char cell_char(const Game *g, int row, int col) {
    int i;
    for (i = 0; i < PLAYER_COUNT; i++) {
        if (g->players[i].row == row && g->players[i].col == col) return (char)('1' + i);
    }
    return '.';
}

void io_print_board(const Game *g) {
    int r;
    int c;
    int n = g->size;

    printf("    ");
    for (c = 0; c < n; c++) printf("%2d  ", c);
    printf("\n");

    for (r = 0; r < n; r++) {
        printf("%2d  ", r);
        for (c = 0; c < n; c++) {
            printf(" %c ", cell_char(g, r, c));
            if (c != n - 1) printf(g->block_right[r][c] ? "|" : " ");
        }
        printf("\n");

        if (r != n - 1) {
            printf("    ");
            for (c = 0; c < n; c++) {
                printf(g->block_down[r][c] ? "---" : "   ");
                if (c != n - 1) printf(" ");
            }
            printf("\n");
        }
    }
}

void io_print_status(const Game *g) {
    printf("P1 (%s): walls=%d blocked=%d\n", g->player_name[0], g->walls_left[0], g->blocked_turns[0]);
    printf("P2 (%s): walls=%d blocked=%d\n", g->player_name[1], g->walls_left[1], g->blocked_turns[1]);
    printf("Current: %s (player %d)\n", g->player_name[g->current_player], g->current_player + 1);
}
