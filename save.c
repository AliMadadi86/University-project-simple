#include "save.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <direct.h>

#define SAVE_MAGIC "QDR1"
#define SAVE_VERSION_1 1
#define SAVE_VERSION_2 2

/*
 * این فایل وضعیت کامل بازی را به‌صورت باینری ذخیره/بارگذاری می‌کند.
 *
 * نکته طراحی:
 * - نسخه جدید فایل (v2) از 2 و 4 بازیکن پشتیبانی می‌کند.
 * - برای سازگاری، loader هنوز فایل‌های قدیمی v1 را هم می‌خواند.
 */

static void set_err(char *err, size_t cap, const char *msg) {
    if (err && cap) snprintf(err, cap, "%s", msg);
}

static int close_and_fail(FILE *fp, char *err, size_t cap, const char *msg) {
    if (fp) fclose(fp);
    set_err(err, cap, msg);
    return 0;
}

static void build_save_path(const char *filename, char *path, size_t path_cap) {
    const char *userprofile = getenv("USERPROFILE");
    if (userprofile && userprofile[0]) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s\\Documents\\save_games", userprofile);
        _mkdir(dir);
        snprintf(path, path_cap, "%s\\%s", dir, filename);
    } else {
        snprintf(path, path_cap, "%s", filename);
    }
}

static FILE *open_save_for_write(const char *primary_path, const char *fallback_name) {
    FILE *fp = fopen(primary_path, "wb");
    if (fp) return fp;
    return fopen(fallback_name, "wb");
}

static FILE *open_save_for_read(const char *primary_path, const char *fallback_name) {
    FILE *fp = fopen(primary_path, "rb");
    if (fp) return fp;
    return fopen(fallback_name, "rb");
}

static int write_u32(FILE *fp, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1;
}

static int read_u32(FILE *fp, uint32_t *out) {
    return fread(out, sizeof(*out), 1, fp) == 1;
}

static int write_block(FILE *fp, const void *ptr, size_t len) {
    return fwrite(ptr, 1, len, fp) == len;
}

static int read_block(FILE *fp, void *ptr, size_t len) {
    return fread(ptr, 1, len, fp) == len;
}

static int write_u32_or_fail(FILE *fp, uint32_t v, char *err, size_t cap) {
    if (write_u32(fp, v)) return 1;
    return close_and_fail(fp, err, cap, "Error: failed to write save file");
}

static int read_u32_or_fail(FILE *fp, uint32_t *out, char *err, size_t cap) {
    if (read_u32(fp, out)) return 1;
    return close_and_fail(fp, err, cap, "Error: corrupted save file");
}

static int write_block_or_fail(FILE *fp, const void *ptr, size_t len, char *err, size_t cap) {
    if (write_block(fp, ptr, len)) return 1;
    return close_and_fail(fp, err, cap, "Error: failed to write save file");
}

static int read_block_or_fail(FILE *fp, void *ptr, size_t len, char *err, size_t cap) {
    if (read_block(fp, ptr, len)) return 1;
    return close_and_fail(fp, err, cap, "Error: corrupted save file");
}

static int u32_to_int(uint32_t value, int *out) {
    if (value > (uint32_t)INT_MAX) return 0;
    *out = (int)value;
    return 1;
}

static int u32_to_int_or_fail(uint32_t value, int *out, FILE *fp, char *err, size_t cap) {
    if (u32_to_int(value, out)) return 1;
    return close_and_fail(fp, err, cap, "Error: invalid numeric data in save");
}

static int players_in_valid_cells(const Board *b) {
    for (int i = 0; i < b->player_count; i++) {
        if (!board_in_range(b, b->players[i].row, b->players[i].col)) return 0;
    }
    for (int i = 0; i < b->player_count; i++) {
        for (int j = i + 1; j < b->player_count; j++) {
            if (b->players[i].row == b->players[j].row && b->players[i].col == b->players[j].col) return 0;
        }
    }
    return 1;
}

static int is_valid_mode(int mode) {
    return mode == MODE_PVP || mode == MODE_PVC || mode == MODE_PVP4;
}

static int is_valid_player_count_for_mode(int mode, int player_count) {
    return player_count == game_player_count_for_mode((GameMode)mode);
}

static int is_valid_state_for_save(const GameState *g) {
    if (g->board.size < 2 || g->board.size > BOARD_MAX) return 0;
    if (!is_valid_mode(g->mode)) return 0;
    if (!is_valid_player_count_for_mode(g->mode, g->board.player_count)) return 0;
    if (g->current_player < 0 || g->current_player >= g->board.player_count) return 0;
    if (!players_in_valid_cells(&g->board)) return 0;

    for (int i = 0; i < g->board.player_count; i++) {
        if (g->blocked_turns[i] < 0) return 0;
        if (g->board.walls_left[i] < 0) return 0;
        if (g->board.walls_max[i] < 0) return 0;
    }

    return 1;
}

static int validate_loaded_state(const GameState *g) {
    if (!is_valid_state_for_save(g)) return 0;
    for (int i = 0; i < g->board.player_count; i++) {
        if (!game_player_has_path(g, (PlayerId)i)) return 0;
    }
    return 1;
}

/* فایل قدیمی v1 فقط 2 بازیکن را ذخیره می‌کرد */
static int load_version_1(FILE *fp, GameState *g, char *err, size_t err_cap) {
    uint32_t size = 0, mode = 0, cur = 0;
    uint32_t blk1 = 0, blk2 = 0;
    uint32_t wl1 = 0, wl2 = 0, wm1 = 0, wm2 = 0;
    uint32_t p1r = 0, p1c = 0, p2r = 0, p2c = 0;

    int isize = 0, imode = 0, icur = 0;
    int iblk1 = 0, iblk2 = 0;
    int iwl1 = 0, iwl2 = 0, iwm1 = 0, iwm2 = 0;
    int ip1r = 0, ip1c = 0, ip2r = 0, ip2c = 0;

    if (!read_u32_or_fail(fp, &size, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &mode, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &cur, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &blk1, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &blk2, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &wl1, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &wl2, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &wm1, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &wm2, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &p1r, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &p1c, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &p2r, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &p2c, err, err_cap)) return 0;

    if (!u32_to_int_or_fail(size, &isize, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(mode, &imode, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(cur, &icur, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(blk1, &iblk1, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(blk2, &iblk2, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(wl1, &iwl1, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(wl2, &iwl2, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(wm1, &iwm1, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(wm2, &iwm2, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(p1r, &ip1r, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(p1c, &ip1c, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(p2r, &ip2r, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(p2c, &ip2c, fp, err, err_cap)) return 0;

    if (!is_valid_mode(imode)) return close_and_fail(fp, err, err_cap, "Error: invalid metadata in save");
    if (isize < 2 || isize > BOARD_MAX) return close_and_fail(fp, err, err_cap, "Error: invalid metadata in save");
    if (game_player_count_for_mode((GameMode)imode) != 2) {
        return close_and_fail(fp, err, err_cap, "Error: invalid metadata in save");
    }

    board_init(&g->board, isize);
    game_set_mode(g, (GameMode)imode);
    g->current_player = icur;
    g->blocked_turns[0] = iblk1;
    g->blocked_turns[1] = iblk2;
    g->board.walls_left[0] = iwl1;
    g->board.walls_left[1] = iwl2;
    g->board.walls_max[0] = iwm1;
    g->board.walls_max[1] = iwm2;
    g->board.players[0].row = ip1r;
    g->board.players[0].col = ip1c;
    g->board.players[1].row = ip2r;
    g->board.players[1].col = ip2c;

    if (!read_block_or_fail(fp, g->player_name[0], NAME_MAX, err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->player_name[1], NAME_MAX, err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.block_right, sizeof(g->board.block_right), err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.block_down, sizeof(g->board.block_down), err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.h_wall_at, sizeof(g->board.h_wall_at), err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.v_wall_at, sizeof(g->board.v_wall_at), err, err_cap)) return 0;

    g->player_name[0][NAME_MAX - 1] = '\0';
    g->player_name[1][NAME_MAX - 1] = '\0';

    if (!validate_loaded_state(g)) {
        return close_and_fail(fp, err, err_cap, "Error: invalid save state");
    }

    fclose(fp);
    return 1;
}

static int load_version_2(FILE *fp, GameState *g, char *err, size_t err_cap) {
    uint32_t size = 0, mode = 0, cur = 0, player_count = 0;
    int isize = 0, imode = 0, icur = 0, icount = 0;

    if (!read_u32_or_fail(fp, &size, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &mode, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &cur, err, err_cap)) return 0;
    if (!read_u32_or_fail(fp, &player_count, err, err_cap)) return 0;

    if (!u32_to_int_or_fail(size, &isize, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(mode, &imode, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(cur, &icur, fp, err, err_cap)) return 0;
    if (!u32_to_int_or_fail(player_count, &icount, fp, err, err_cap)) return 0;

    if (!is_valid_mode(imode)) return close_and_fail(fp, err, err_cap, "Error: invalid metadata in save");
    if (isize < 2 || isize > BOARD_MAX) return close_and_fail(fp, err, err_cap, "Error: invalid metadata in save");
    if (!is_valid_player_count_for_mode(imode, icount)) {
        return close_and_fail(fp, err, err_cap, "Error: invalid metadata in save");
    }

    board_init(&g->board, isize);
    game_set_mode(g, (GameMode)imode);
    g->current_player = icur;

    if (g->current_player < 0 || g->current_player >= g->board.player_count) {
        return close_and_fail(fp, err, err_cap, "Error: invalid current player in save");
    }

    for (int i = 0; i < g->board.player_count; i++) {
        uint32_t value = 0;
        if (!read_u32_or_fail(fp, &value, err, err_cap)) return 0;
        if (!u32_to_int_or_fail(value, &g->blocked_turns[i], fp, err, err_cap)) return 0;
    }

    for (int i = 0; i < g->board.player_count; i++) {
        uint32_t value = 0;
        if (!read_u32_or_fail(fp, &value, err, err_cap)) return 0;
        if (!u32_to_int_or_fail(value, &g->board.walls_left[i], fp, err, err_cap)) return 0;
    }

    for (int i = 0; i < g->board.player_count; i++) {
        uint32_t value = 0;
        if (!read_u32_or_fail(fp, &value, err, err_cap)) return 0;
        if (!u32_to_int_or_fail(value, &g->board.walls_max[i], fp, err, err_cap)) return 0;
    }

    for (int i = 0; i < g->board.player_count; i++) {
        uint32_t r = 0, c = 0;
        if (!read_u32_or_fail(fp, &r, err, err_cap)) return 0;
        if (!read_u32_or_fail(fp, &c, err, err_cap)) return 0;
        if (!u32_to_int_or_fail(r, &g->board.players[i].row, fp, err, err_cap)) return 0;
        if (!u32_to_int_or_fail(c, &g->board.players[i].col, fp, err, err_cap)) return 0;
    }

    for (int i = 0; i < g->board.player_count; i++) {
        if (!read_block_or_fail(fp, g->player_name[i], NAME_MAX, err, err_cap)) return 0;
        g->player_name[i][NAME_MAX - 1] = '\0';
    }

    if (!read_block_or_fail(fp, g->board.block_right, sizeof(g->board.block_right), err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.block_down, sizeof(g->board.block_down), err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.h_wall_at, sizeof(g->board.h_wall_at), err, err_cap)) return 0;
    if (!read_block_or_fail(fp, g->board.v_wall_at, sizeof(g->board.v_wall_at), err, err_cap)) return 0;

    if (!validate_loaded_state(g)) {
        return close_and_fail(fp, err, err_cap, "Error: invalid save state");
    }

    fclose(fp);
    return 1;
}

int save_game(const char *filename, const GameState *g, char *err, size_t err_cap) {
    if (!filename || !filename[0] || !g) {
        set_err(err, err_cap, "Error: invalid save request");
        return 0;
    }

    if (!is_valid_state_for_save(g)) {
        set_err(err, err_cap, "Error: invalid game state for save");
        return 0;
    }

    char path[512];
    build_save_path(filename, path, sizeof(path));

    FILE *fp = open_save_for_write(path, filename);
    if (!fp) {
        set_err(err, err_cap, "Error: cannot open save file");
        return 0;
    }

    if (!write_block_or_fail(fp, SAVE_MAGIC, 4, err, err_cap)) return 0;
    if (!write_u32_or_fail(fp, SAVE_VERSION_2, err, err_cap)) return 0;
    if (!write_u32_or_fail(fp, (uint32_t)g->board.size, err, err_cap)) return 0;
    if (!write_u32_or_fail(fp, (uint32_t)g->mode, err, err_cap)) return 0;
    if (!write_u32_or_fail(fp, (uint32_t)g->current_player, err, err_cap)) return 0;
    if (!write_u32_or_fail(fp, (uint32_t)g->board.player_count, err, err_cap)) return 0;

    for (int i = 0; i < g->board.player_count; i++) {
        if (!write_u32_or_fail(fp, (uint32_t)g->blocked_turns[i], err, err_cap)) return 0;
    }
    for (int i = 0; i < g->board.player_count; i++) {
        if (!write_u32_or_fail(fp, (uint32_t)g->board.walls_left[i], err, err_cap)) return 0;
    }
    for (int i = 0; i < g->board.player_count; i++) {
        if (!write_u32_or_fail(fp, (uint32_t)g->board.walls_max[i], err, err_cap)) return 0;
    }
    for (int i = 0; i < g->board.player_count; i++) {
        if (!write_u32_or_fail(fp, (uint32_t)g->board.players[i].row, err, err_cap)) return 0;
        if (!write_u32_or_fail(fp, (uint32_t)g->board.players[i].col, err, err_cap)) return 0;
    }
    for (int i = 0; i < g->board.player_count; i++) {
        if (!write_block_or_fail(fp, g->player_name[i], NAME_MAX, err, err_cap)) return 0;
    }

    if (!write_block_or_fail(fp, g->board.block_right, sizeof(g->board.block_right), err, err_cap)) return 0;
    if (!write_block_or_fail(fp, g->board.block_down, sizeof(g->board.block_down), err, err_cap)) return 0;
    if (!write_block_or_fail(fp, g->board.h_wall_at, sizeof(g->board.h_wall_at), err, err_cap)) return 0;
    if (!write_block_or_fail(fp, g->board.v_wall_at, sizeof(g->board.v_wall_at), err, err_cap)) return 0;

    fclose(fp);
    return 1;
}

int load_game(const char *filename, GameState *g, char *err, size_t err_cap) {
    if (!filename || !filename[0] || !g) {
        set_err(err, err_cap, "Error: invalid load request");
        return 0;
    }

    char path[512];
    build_save_path(filename, path, sizeof(path));

    FILE *fp = open_save_for_read(path, filename);
    if (!fp) {
        set_err(err, err_cap, "Error: cannot open save file");
        return 0;
    }

    char magic[4];
    if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, SAVE_MAGIC, 4) != 0) {
        return close_and_fail(fp, err, err_cap, "Error: invalid save file");
    }

    uint32_t version = 0;
    if (!read_u32(fp, &version)) {
        return close_and_fail(fp, err, err_cap, "Error: corrupted save file");
    }

    if (version == SAVE_VERSION_1) {
        return load_version_1(fp, g, err, err_cap);
    }
    if (version == SAVE_VERSION_2) {
        return load_version_2(fp, g, err, err_cap);
    }

    return close_and_fail(fp, err, err_cap, "Error: unsupported save version");
}
