#ifndef SAVE_H
#define SAVE_H

#include "game.h"

/*
 * save.h:
 * API ذخیره/بارگذاری باینری وضعیت کامل بازی.
 *
 * پارامتر err:
 * - در صورت خطا پیام متنی مناسب در آن نوشته می‌شود.
 * - اگر موفق باشد محتوای err مهم نیست.
 */

int save_game(const char *filename, const GameState *g, char *err, size_t err_cap);
int load_game(const char *filename, GameState *g, char *err, size_t err_cap);

#endif
