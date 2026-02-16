#ifndef RAYUI_H
#define RAYUI_H

#include "game.h"

/*
 * rayui.h:
 * رابط گرافیکی raylib.
 *
 * اگر برنامه بدون USE_RAYLIB ساخته شود:
 * - rayui_available صفر برمی‌گرداند
 * - rayui_run کاری انجام نمی‌دهد
 */

/* آیا UI گرافیکی در بیلد فعلی فعال است؟ */
int rayui_available(void);
/* اجرای حلقهٔ UI گرافیکی */
int rayui_run(GameState *game);

#endif
