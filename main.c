#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "game.h"
#include "io.h"
#include "save.h"

static int parse_wall_dir_char(char ch, WallDir *dir) {
    if (ch == 'H' || ch == 'h') {
        *dir = DIR_H;
        return 1;
    }
    if (ch == 'V' || ch == 'v') {
        *dir = DIR_V;
        return 1;
    }
    return 0;
}

static int parse_int_token(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

static int parse_wall_triplet(const char *t1, const char *t2, const char *t3, int *row, int *col, WallDir *dir) {
    int r;
    int c;
    WallDir d;

    if (parse_int_token(t1, &r) && parse_int_token(t2, &c) && t3[0] && t3[1] == '\0' &&
        parse_wall_dir_char(t3[0], &d)) {
        *row = r;
        *col = c;
        *dir = d;
        return 1;
    }

    if (t1[0] && t1[1] == '\0' && parse_wall_dir_char(t1[0], &d) &&
        parse_int_token(t2, &r) && parse_int_token(t3, &c)) {
        *row = r;
        *col = c;
        *dir = d;
        return 1;
    }

    return 0;
}

static int load_map_from_file(Game *g, const char *filename) {
    FILE *fp;
    int n;
    int i;
    int k1;
    int k2;

    fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: cannot open map file: %s\n", filename);
        return 0;
    }

    if (fscanf(fp, "%d", &n) != 1 || n < 2 || n > MAX_SIZE) {
        printf("Error: invalid board size in map file.\n");
        fclose(fp);
        return 0;
    }

    game_start(g, n, 0, MODE_PVP, "P1", "P2");

    {
        int r;
        int c;
        if (fscanf(fp, "%d %d", &r, &c) != 2 || !game_set_player_pos(g, 0, r, c)) {
            printf("Error: invalid player1 position.\n");
        }
    }
    {
        int r;
        int c;
        if (fscanf(fp, "%d %d", &r, &c) != 2 || !game_set_player_pos(g, 1, r, c)) {
            printf("Error: invalid player2 position.\n");
        }
    }

    if (fscanf(fp, "%d", &k1) != 1 || k1 < 0) {
        printf("Error: invalid player1 wall count.\n");
        fclose(fp);
        return 0;
    }
    for (i = 0; i < k1; i++) {
        char t1[32];
        char t2[32];
        char t3[32];
        int row;
        int col;
        WallDir dir;
        if (fscanf(fp, "%31s %31s %31s", t1, t2, t3) != 3) {
            printf("Error: bad wall line for player1.\n");
            break;
        }
        if (!parse_wall_triplet(t1, t2, t3, &row, &col, &dir) || !game_add_wall_from_map(g, row, col, dir)) {
            printf("Error: invalid wall for player1 (%s %s %s)\n", t1, t2, t3);
        }
    }

    if (fscanf(fp, "%d", &k2) != 1 || k2 < 0) {
        printf("Error: invalid player2 wall count.\n");
        fclose(fp);
        return 0;
    }
    for (i = 0; i < k2; i++) {
        char t1[32];
        char t2[32];
        char t3[32];
        int row;
        int col;
        WallDir dir;
        if (fscanf(fp, "%31s %31s %31s", t1, t2, t3) != 3) {
            printf("Error: bad wall line for player2.\n");
            break;
        }
        if (!parse_wall_triplet(t1, t2, t3, &row, &col, &dir) || !game_add_wall_from_map(g, row, col, dir)) {
            printf("Error: invalid wall for player2 (%s %s %s)\n", t1, t2, t3);
        }
    }

    fclose(fp);
    return 1;
}

static void print_commands(void) {
    printf("Commands:\n");
    printf("  move r c      (or: r c)\n");
    printf("  wall r c H|V  (or: r c H|V)\n");
    printf("  save [file]\n");
    printf("  load [file]\n");
    printf("  quit\n");
}

static void print_usage(const char *exe_name) {
    printf("Usage:\n");
    printf("  %s                Start interactive game\n", exe_name);
    printf("  %s <map.txt>      Print map from file\n", exe_name);
    printf("  %s --help         Show this help\n", exe_name);
    printf("  %s --console      Accepted for compatibility (console is default)\n", exe_name);
}

typedef struct {
    const char *map_file;
    const char *bad_arg;
    int show_help;
    int has_bad_arg;
} CliOptions;

static CliOptions parse_cli(int argc, char **argv) {
    CliOptions o;
    int i;
    o.map_file = NULL;
    o.bad_arg = NULL;
    o.show_help = 0;
    o.has_bad_arg = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            o.show_help = 1;
            continue;
        }
        if (strcmp(argv[i], "--console") == 0) {
            continue;
        }
        if (argv[i][0] == '-') {
            o.has_bad_arg = 1;
            o.bad_arg = argv[i];
            return o;
        }
        if (o.map_file) {
            o.has_bad_arg = 1;
            o.bad_arg = argv[i];
            return o;
        }
        o.map_file = argv[i];
    }
    return o;
}

static void setup_new_game(Game *g) {
    int mode;
    int size;
    int walls;
    char p1[NAME_SIZE];
    char p2[NAME_SIZE];

    mode = io_read_int("Mode (1=PvP, 2=PvC): ", 1, 2);
    size = io_read_int("Board size (2-50): ", 2, MAX_SIZE);
    walls = io_read_int("Walls per player: ", 0, 1000);

    io_read_string("Player1 name: ", p1, sizeof(p1));
    if (p1[0] == '\0') strcpy(p1, "Player1");

    if (mode == MODE_PVC) {
        strcpy(p2, "COMPUTER");
    } else {
        io_read_string("Player2 name: ", p2, sizeof(p2));
        if (p2[0] == '\0') strcpy(p2, "Player2");
    }

    game_start(g, size, walls, (GameMode)mode, p1, p2);
}

static int setup_game(Game *g) {
    char line[LINE_MAX_LEN];
    char err[128];

    printf("Load filename (Enter for new game): ");
    if (!io_read_line(line, sizeof(line))) return 0;

    if (line[0] != '\0') {
        if (load_game(line, g, err, sizeof(err))) {
            printf("Loaded from %s\n", line);
            return 1;
        }
        printf("%s\n", err);
        printf("Starting new game.\n");
    }

    setup_new_game(g);
    return 1;
}

static int run_human_turn(Game *g, int *loaded_game) {
    char line[LINE_MAX_LEN];
    Action act;
    char err[128];

    if (loaded_game) *loaded_game = 0;

    for (;;) {
        printf("Action> ");
        if (!io_read_line(line, sizeof(line))) return 0;

        if (!io_parse_action(line, &act)) {
            printf("Invalid command.\n");
            continue;
        }

        if (act.type == ACT_QUIT) return 0;

        if (act.type == ACT_SAVE) {
            if (save_game(act.filename, g, err, sizeof(err))) {
                printf("Saved to %s\n", act.filename);
            } else {
                printf("%s\n", err);
            }
            continue;
        }

        if (act.type == ACT_LOAD) {
            if (load_game(act.filename, g, err, sizeof(err))) {
                printf("Loaded from %s\n", act.filename);
                if (loaded_game) *loaded_game = 1;
                return 1;
            }
            printf("%s\n", err);
            continue;
        }

        if (act.type == ACT_MOVE) {
            if (game_move_player(g, g->current_player, act.target, err, sizeof(err))) return 1;
            printf("%s\n", err);
            continue;
        }

        if (act.type == ACT_WALL) {
            if (game_place_wall(g, g->current_player, act.row, act.col, act.dir, err, sizeof(err))) return 1;
            printf("%s\n", err);
            continue;
        }
    }
}

static int run_game_loop(Game *g) {
    for (;;) {
        int winner;
        char magic_msg[160];

        printf("\n");
        io_print_board(g);
        io_print_status(g);

        game_apply_magic(g, magic_msg, sizeof(magic_msg));
        printf("%s\n", magic_msg);

        winner = game_check_winner(g);
        if (winner >= 0) {
            io_print_board(g);
            printf("Winner: %s\n", g->player_name[winner]);
            return 0;
        }

        if (g->blocked_turns[g->current_player] > 0) {
            g->blocked_turns[g->current_player]--;
            printf("%s is blocked. Turn skipped.\n", g->player_name[g->current_player]);
            g->current_player = game_next_player(g->current_player);
            continue;
        }

        if (g->mode == MODE_PVC && g->current_player == 1) {
            char ai_msg[128];
            game_try_ai_turn(g, ai_msg, sizeof(ai_msg));
            printf("%s\n", ai_msg);
        } else {
            int loaded = 0;
            if (!run_human_turn(g, &loaded)) return 0;
            if (loaded) continue;
        }

        winner = game_check_winner(g);
        if (winner >= 0) {
            io_print_board(g);
            printf("Winner: %s\n", g->player_name[winner]);
            return 0;
        }

        g->current_player = game_next_player(g->current_player);
    }
}

int main(int argc, char **argv) {
    Game game;
    CliOptions cli = parse_cli(argc, argv);

    game_seed_rng();

    if (cli.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (cli.has_bad_arg) {
        printf("Error: unknown argument: %s\n", cli.bad_arg ? cli.bad_arg : "(null)");
        print_usage(argv[0]);
        return 1;
    }

    if (cli.map_file) {
        if (!load_map_from_file(&game, cli.map_file)) return 1;
        io_print_board(&game);
        return 0;
    }

    if (!setup_game(&game)) return 0;
    print_commands();
    return run_game_loop(&game);
}
