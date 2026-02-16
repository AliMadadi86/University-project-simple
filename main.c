#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "game.h"
#include "io.h"
#include "save.h"
#include "ai.h"
#include "rayui.h"

/*
 * main.c مسئول orchestration کل برنامه است:
 * - ورودی خط فرمان
 * - ساخت بازی جدید یا load
 * - انتخاب بین UI متنی و UI گرافیکی
 * - حلقهٔ نوبت در حالت متنی
 *
 * منطق قوانین بازی اینجا نیست؛
 * قوانین داخل game.c قرار دارد و main.c فقط آن‌ها را صدا می‌زند.
 */

/* خروجی تجزیهٔ آرگومان‌های خط فرمان */
typedef struct {
    int force_console;
    int force_gui;
    const char *map_file;
} CliOptions;

/* کپی امن رشته با پایان صفر */
static void copy_text(char *dst, int cap, const char *src) {
    if (!dst || cap <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, (size_t)cap - 1);
    dst[cap - 1] = '\0';
}

/* خواندن یک مختصات ساده از فایل */
static int read_coord(FILE *fp, Coord *p) {
    return fscanf(fp, "%d %d", &p->row, &p->col) == 2;
}

/* تبدیل کاراکتر جهت دیوار به نوع داخلی */
static int parse_dir_char(char ch, WallDir *dir) {
    if (ch == 'H' || ch == 'h') { *dir = DIR_H; return 1; }
    if (ch == 'V' || ch == 'v') { *dir = DIR_V; return 1; }
    return 0;
}

/* تجزیهٔ عدد با بررسی کامل رشته */
static int parse_int_token(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

/* خواندن قالب «ردیف ستون جهت» */
static int parse_wall_row_col_dir(
    const char *row_token,
    const char *col_token,
    const char *dir_token,
    int *row,
    int *col,
    WallDir *dir
) {
    int r;
    int c;
    WallDir d;

    if (!parse_int_token(row_token, &r)) return 0;
    if (!parse_int_token(col_token, &c)) return 0;
    if (dir_token[1] != '\0') return 0;
    if (!parse_dir_char(dir_token[0], &d)) return 0;

    *row = r;
    *col = c;
    *dir = d;
    return 1;
}

/* خواندن قالب «جهت ردیف ستون» */
static int parse_wall_dir_row_col(
    const char *dir_token,
    const char *row_token,
    const char *col_token,
    int *row,
    int *col,
    WallDir *dir
) {
    int r;
    int c;
    WallDir d;

    if (dir_token[1] != '\0') return 0;
    if (!parse_dir_char(dir_token[0], &d)) return 0;
    if (!parse_int_token(row_token, &r)) return 0;
    if (!parse_int_token(col_token, &c)) return 0;

    *row = r;
    *col = c;
    *dir = d;
    return 1;
}

/* تجزیهٔ قالب خط دیوار: «ردیف ستون جهت» یا «جهت ردیف ستون» */
static int parse_wall_triplet(FILE *fp, int *row, int *col, WallDir *dir) {
    char t1[32], t2[32], t3[32];

    if (fscanf(fp, "%31s %31s %31s", t1, t2, t3) != 3) {
        return 0;
    }

    if (parse_wall_row_col_dir(t1, t2, t3, row, col, dir)) {
        return 1;
    }

    if (parse_wall_dir_row_col(t1, t2, t3, row, col, dir)) {
        return 1;
    }

    /* اگر هیچ قالبی درست نبود، -1 برمی‌گردانیم تا caller پیام مناسب بدهد */
    return -1;
}

/* خطای مختصات بازیکن را هندل می‌کند و در صورت خطا مقدار را نامعتبر می‌گذارد */
static void read_map_player(Board *b, FILE *fp, int player_index, const char *label) {
    Coord p;

    if (!read_coord(fp, &p)) {
        printf("Error: missing %s position\n", label);
        b->players[player_index].row = -1;
        b->players[player_index].col = -1;
        return;
    }

    if (!board_in_range(b, p.row, p.col)) {
        printf("Error: %s out of range (%d %d)\n", label, p.row, p.col);
        b->players[player_index].row = -1;
        b->players[player_index].col = -1;
        return;
    }

    b->players[player_index] = p;
}

/* دیوارهای هر بازیکن را از فایل نقشه می‌خواند */
static void read_map_walls(Board *b, FILE *fp, const char *label) {
    int count = 0;

    if (fscanf(fp, "%d", &count) != 1) {
        printf("Error: missing wall count for %s\n", label);
        return;
    }

    for (int i = 0; i < count; i++) {
        int row;
        int col;
        WallDir dir;
        int parse_ok = parse_wall_triplet(fp, &row, &col, &dir);

        if (parse_ok == 0) {
            printf("Error: invalid wall line for %s\n", label);
            break;
        }
        if (parse_ok < 0) {
            printf("Error: invalid wall line format for %s\n", label);
            continue;
        }
        if (!board_place_wall(b, row, col, dir, 0)) {
            printf("Error: invalid wall (%d %d %c)\n", row, col, dir == DIR_H ? 'H' : 'V');
        }
    }
}

/* خواندن فایل نقشه برای حالت نمایش فایل */
static int load_map_from_file(Board *b, const char *filename) {
    FILE *fp = fopen(filename, "r");
    int size;

    if (!fp) {
        printf("Error: cannot open file (%s)\n", filename);
        return 0;
    }

    if (fscanf(fp, "%d", &size) != 1) {
        printf("Error: missing board size\n");
        fclose(fp);
        return 0;
    }

    if (size < 2 || size > BOARD_MAX) {
        printf("Error: board size must be between 2 and %d\n", BOARD_MAX);
        fclose(fp);
        return 0;
    }

    board_init(b, size);

    /*
     * map mode صرفاً برای نمایش فایل نقشه است،
     * پس اینجا وارد جریان کامل game state نمی‌شویم.
     */
    /* خواندن محل بازیکن‌ها */
    read_map_player(b, fp, 0, "player1");
    read_map_player(b, fp, 1, "player2");

    if (b->players[0].row != -1 && b->players[1].row != -1 &&
        b->players[0].row == b->players[1].row && b->players[0].col == b->players[1].col) {
        printf("Error: players are on the same cell\n");
    }

    /* خواندن دیوارهای بازیکن اول */
    read_map_walls(b, fp, "player1");

    /* خواندن دیوارهای بازیکن دوم */
    read_map_walls(b, fp, "player2");

    fclose(fp);
    return 1;
}

/* چاپ راهنمای دستورها در حالت متنی */
static void print_action_help(void) {
    printf("Commands:\n");
    printf("  move r c          (or: r c)\n");
    printf("  wall r c H|V      (or: r c H|V)\n");
    printf("  save [filename]\n");
    printf("  load [filename]\n");
    printf("  quit\n");
}

/* تجزیهٔ گزینه‌های خط فرمان مانند حالت متنی یا گرافیکی */
static CliOptions parse_cli_options(int argc, char **argv) {
    CliOptions opt;
    opt.force_console = 0;
    opt.force_gui = 0;
    opt.map_file = NULL;

    for (int i = 1; i < argc; i++) {
        /* فقط اولین آرگومان غیر-فلگ به عنوان map file در نظر گرفته می‌شود */
        if (!strcmp(argv[i], "--console")) {
            opt.force_console = 1;
        } else if (!strcmp(argv[i], "--gui")) {
            opt.force_gui = 1;
        } else if (!opt.map_file) {
            opt.map_file = argv[i];
        }
    }

    return opt;
}

/* راه‌اندازی اولیه: بارگذاری ذخیره یا ساخت بازی جدید */
static int setup_game(GameState *game) {
    char line[LINE_MAX];
    char err[128];
    line[0] = '\0';

    printf("Enter save filename to load or press Enter for new game: ");
    if (!io_read_line(line, sizeof(line))) return 0;

    if (line[0] != '\0') {
        if (!load_game(line, game, err, sizeof(err))) {
            printf("%s\n", err);
            printf("Starting a new game...\n");
            line[0] = '\0';
        } else {
            printf("Loaded game from %s\n", line);
        }
    }

    if (line[0] == '\0') {
        /*
         * اگر load انجام نشد:
         * 1) mode
         * 2) size
         * 3) walls
         * 4) nameها
         */
        int mode = io_read_int("Mode (1=PvP, 2=PvC, 3=PvP4): ", 1, 3);
        int size = io_read_int("Board size (2-50): ", 2, BOARD_MAX);
        int walls = io_read_int("Walls per player: ", 0, 999);
        char names[MAX_PLAYERS][NAME_MAX];

        game_init(game, size, walls);
        if (mode == 1) game_set_mode(game, MODE_PVP);
        else if (mode == 2) game_set_mode(game, MODE_PVC);
        else game_set_mode(game, MODE_PVP4);

        for (int i = 0; i < game_player_count(game); i++) {
            names[i][0] = '\0';
            /* در PvC بازیکن دوم همیشه COMPUTER است */
            if (mode == 2 && i == PLAYER2) {
                copy_text(names[i], (int)sizeof(names[i]), "COMPUTER");
            } else {
                char prompt[64];
                snprintf(prompt, sizeof(prompt), "Player%d name: ", i + 1);
                io_read_string(prompt, names[i], (int)sizeof(names[i]));
            }

            /* اگر کاربر اسم خالی داد، اسم پیش‌فرض می‌گذاریم */
            if (names[i][0] == '\0') {
                snprintf(names[i], sizeof(names[i]), "Player%d", i + 1);
            }
            game_set_player_name(game, i, names[i]);
        }
    }

    return 1;
}

/* اجرای نوبت رایانه در حالت متنی */
static int run_console_ai_turn(GameState *game) {
    char err[128];
    AiAction action;

    /* AI همیشه یک اکشن قانونی پیشنهاد می‌دهد (move یا wall) */
    if (!ai_choose_action(game, (PlayerId)game->current_player, &action)) {
        printf("Computer has no valid moves.\n");
        return 0;
    }

    if (action.type == AI_ACT_MOVE) {
        if (!game_move_player(game, (PlayerId)game->current_player, action.move, err, sizeof(err))) {
            printf("Computer move failed: %s\n", err);
        } else {
            printf("Computer moved to (%d, %d).\n", action.move.row, action.move.col);
        }
        return 1;
    }

    if (!game_place_wall(game, (PlayerId)game->current_player, action.row, action.col, action.dir, err, sizeof(err))) {
        printf("Computer wall failed: %s\n", err);
    } else {
        printf("Computer placed wall at (%d, %d) %c.\n", action.row, action.col, action.dir == DIR_H ? 'H' : 'V');
    }
    return 1;
}

/* اجرای ورودی انسانی در حالت متنی */
static int run_console_human_turn(GameState *game, int *loaded_game) {
    char line[LINE_MAX];
    char err[128];
    Action act;

    if (loaded_game) *loaded_game = 0;

    for (;;) {
        printf("Action> ");
        if (!io_read_line(line, sizeof(line))) return 0;

        if (!io_parse_action(line, &act)) {
            printf("Invalid command.\n");
            continue;
        }

        /* save/load/quit کنش‌های مدیریتی هستند و نوبت حرکتی حساب نمی‌شوند */
        if (act.type == ACT_SAVE) {
            if (save_game(act.filename, game, err, sizeof(err))) {
                printf("Game saved to %s\n", act.filename);
            } else {
                printf("%s\n", err);
            }
            continue;
        }

        if (act.type == ACT_LOAD) {
            if (load_game(act.filename, game, err, sizeof(err))) {
                printf("Game loaded from %s\n", act.filename);
                /* بعد از load باید حلقهٔ بیرونی دوباره state جدید را چاپ کند */
                if (loaded_game) *loaded_game = 1;
                return 1;
            }
            printf("%s\n", err);
            continue;
        }

        if (act.type == ACT_QUIT) {
            printf("Goodbye.\n");
            return 0;
        }

        if (act.type == ACT_MOVE) {
            if (!game_move_player(game, (PlayerId)game->current_player, act.target, err, sizeof(err))) {
                printf("%s\n", err);
                continue;
            }
            return 1;
        }

        if (act.type == ACT_WALL) {
            if (!game_place_wall(game, (PlayerId)game->current_player, act.row, act.col, act.dir, err, sizeof(err))) {
                printf("%s\n", err);
                continue;
            }
            return 1;
        }
    }
}

/* اعلام برنده و ذخیرهٔ خودکار */
static void console_show_winner_and_autosave(const GameState *game) {
    char err[128];
    io_print_board(game);
    printf("Winner: %s\n", game->player_name[game->current_player]);
    if (save_game("autosave.qsave", game, err, sizeof(err))) {
        printf("Auto-saved to autosave.qsave\n");
    } else {
        printf("Auto-save failed: %s\n", err);
    }
}

/* حلقهٔ کامل بازی در محیط متنی */
static int run_console_game(GameState *game) {
    print_action_help();
    int loaded_game = 0;

    while (1) {
        printf("\n");
        io_print_board(game);
        io_print_status(game);

        /*
         * جادوی ابتدای نوبت:
         * حتی ممکن است همان لحظه باعث برد شود (مثلاً با باز شدن کامل دیوارها).
         */
        MagicEvent ev = game_roll_magic(game);
        io_print_magic(game, ev);
        game_apply_magic(game, ev);
        {
            PlayerId winner;
            if (game_find_winner(game, &winner)) {
                game->current_player = winner;
                console_show_winner_and_autosave(game);
                break;
            }
        }

        /* اگر بازیکن فعلی بلاک باشد، بدون گرفتن اکشن انسانی/AI نوبت رد می‌شود */
        if (game->blocked_turns[game->current_player] > 0) {
            game->blocked_turns[game->current_player]--;
            printf("%s is blocked. Turn skipped.\n", game->player_name[game->current_player]);
            game->current_player = game_next_player(game, game->current_player);
            continue;
        }

        /* تنها حالت AI در نسخه فعلی: PvC و بازیکن شماره 2 */
        int is_computer = (game->mode == MODE_PVC && game->current_player == PLAYER2);

        if (is_computer) {
            if (!run_console_ai_turn(game)) break;
        } else {
            if (!run_console_human_turn(game, &loaded_game)) return 0;
        }

        if (loaded_game) {
            /* بعد از load، current_player و کل state از فایل می‌آید */
            loaded_game = 0;
            continue;
        }

        /* بررسی برد بعد از انجام کنش موفق */
        {
            PlayerId winner;
            if (game_find_winner(game, &winner)) {
                game->current_player = winner;
                console_show_winner_and_autosave(game);
                break;
            }
        }

        game->current_player = game_next_player(game, game->current_player);
    }

    return 0;
}

/* نقطهٔ شروع برنامه و تعیین مسیر اجرا */
int main(int argc, char **argv) {
    game_seed_rng();
    CliOptions opt = parse_cli_options(argc, argv);

    if (opt.map_file) {
        /* mode مخصوص نمایش map فایل */
        GameState g;
        if (!load_map_from_file(&g.board, opt.map_file)) return 1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            g.player_name[i][0] = '\0';
        }
        io_print_board(&g);
        return 0;
    }

    GameState game;
    if (!setup_game(&game)) return 0;

    if (opt.force_gui && !rayui_available()) {
        printf("raylib UI is not available in this build.\n");
        printf("Rebuild with USE_RAYLIB and link raylib to enable --gui.\n");
    }

    /*
     * اولویت اجرا:
     * - اگر کاربر --console نداده و UI گرافیکی available باشد -> rayui
     * - در غیر این صورت -> کنسول
     */
    if (!opt.force_console && rayui_available()) {
        return rayui_run(&game);
    }

    return run_console_game(&game);
}
