#include "save.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void set_err(char *err, size_t cap, const char *msg) {
    if (err && cap) {
        snprintf(err, cap, "%s", msg);
    }
}

static int write_u32(FILE *fp, uint32_t value) {
    return fwrite(&value, sizeof(value), 1, fp) == 1;
}

static int read_u32(FILE *fp, uint32_t *value) {
    return fread(value, sizeof(*value), 1, fp) == 1;
}

static int validate_loaded_game(const Game *g) {
    if (!g) return 0;
    if (g->size < 2 || g->size > MAX_SIZE) return 0;
    if (g->current_player < 0 || g->current_player >= PLAYER_COUNT) return 0;
    if (!game_in_range(g, g->players[0].row, g->players[0].col)) return 0;
    if (!game_in_range(g, g->players[1].row, g->players[1].col)) return 0;
    if (g->players[0].row == g->players[1].row && g->players[0].col == g->players[1].col) return 0;
    if (g->walls_left[0] < 0 || g->walls_left[1] < 0) return 0;
    if (g->blocked_turns[0] < 0 || g->blocked_turns[1] < 0) return 0;
    if (!(g->mode == MODE_PVP || g->mode == MODE_PVC)) return 0;
    return 1;
}

int save_game(const char *filename, const Game *g, char *err, size_t err_cap) {
    FILE *fp;
    uint32_t version = 1;
    const char header[4] = {'S', 'Q', 'D', 'R'};

    if (!filename || !filename[0] || !g) {
        set_err(err, err_cap, "Invalid save request.");
        return 0;
    }

    fp = fopen(filename, "wb");
    if (!fp) {
        set_err(err, err_cap, "Cannot open file for save.");
        return 0;
    }

    if (fwrite(header, 1, 4, fp) != 4) goto fail;
    if (!write_u32(fp, version)) goto fail;
    if (!write_u32(fp, (uint32_t)g->size)) goto fail;
    if (!write_u32(fp, (uint32_t)g->mode)) goto fail;
    if (!write_u32(fp, (uint32_t)g->current_player)) goto fail;
    if (!write_u32(fp, (uint32_t)g->blocked_turns[0])) goto fail;
    if (!write_u32(fp, (uint32_t)g->blocked_turns[1])) goto fail;
    if (!write_u32(fp, (uint32_t)g->walls_left[0])) goto fail;
    if (!write_u32(fp, (uint32_t)g->walls_left[1])) goto fail;
    if (!write_u32(fp, (uint32_t)g->players[0].row)) goto fail;
    if (!write_u32(fp, (uint32_t)g->players[0].col)) goto fail;
    if (!write_u32(fp, (uint32_t)g->players[1].row)) goto fail;
    if (!write_u32(fp, (uint32_t)g->players[1].col)) goto fail;

    if (fwrite(g->player_name, sizeof(g->player_name), 1, fp) != 1) goto fail;
    if (fwrite(g->block_right, sizeof(g->block_right), 1, fp) != 1) goto fail;
    if (fwrite(g->block_down, sizeof(g->block_down), 1, fp) != 1) goto fail;
    if (fwrite(g->h_wall_at, sizeof(g->h_wall_at), 1, fp) != 1) goto fail;
    if (fwrite(g->v_wall_at, sizeof(g->v_wall_at), 1, fp) != 1) goto fail;

    fclose(fp);
    return 1;

fail:
    fclose(fp);
    set_err(err, err_cap, "Failed to write save file.");
    return 0;
}

int load_game(const char *filename, Game *g, char *err, size_t err_cap) {
    FILE *fp;
    char header[4];
    uint32_t version;
    uint32_t u;
    Game temp;

    if (!filename || !filename[0] || !g) {
        set_err(err, err_cap, "Invalid load request.");
        return 0;
    }

    fp = fopen(filename, "rb");
    if (!fp) {
        set_err(err, err_cap, "Cannot open save file.");
        return 0;
    }

    if (fread(header, 1, 4, fp) != 4) goto bad_file;
    if (memcmp(header, "SQDR", 4) != 0) goto bad_file;
    if (!read_u32(fp, &version)) goto bad_file;
    if (version != 1) goto bad_file;

    memset(&temp, 0, sizeof(temp));
    if (!read_u32(fp, &u)) goto bad_file; temp.size = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.mode = (GameMode)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.current_player = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.blocked_turns[0] = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.blocked_turns[1] = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.walls_left[0] = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.walls_left[1] = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.players[0].row = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.players[0].col = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.players[1].row = (int)u;
    if (!read_u32(fp, &u)) goto bad_file; temp.players[1].col = (int)u;

    if (fread(temp.player_name, sizeof(temp.player_name), 1, fp) != 1) goto bad_file;
    if (fread(temp.block_right, sizeof(temp.block_right), 1, fp) != 1) goto bad_file;
    if (fread(temp.block_down, sizeof(temp.block_down), 1, fp) != 1) goto bad_file;
    if (fread(temp.h_wall_at, sizeof(temp.h_wall_at), 1, fp) != 1) goto bad_file;
    if (fread(temp.v_wall_at, sizeof(temp.v_wall_at), 1, fp) != 1) goto bad_file;

    temp.player_name[0][NAME_SIZE - 1] = '\0';
    temp.player_name[1][NAME_SIZE - 1] = '\0';

    if (!validate_loaded_game(&temp)) {
        fclose(fp);
        set_err(err, err_cap, "Save file data is invalid.");
        return 0;
    }

    *g = temp;
    fclose(fp);
    return 1;

bad_file:
    fclose(fp);
    set_err(err, err_cap, "Save file is corrupted or unsupported.");
    return 0;
}
