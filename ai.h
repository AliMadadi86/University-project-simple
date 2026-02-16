#ifndef AI_H
#define AI_H

#include "game.h"

/*
 * ai.h:
 * قرارداد تصمیم‌گیری بازیکن کامپیوتر در حالت PvC.
 */

/* نوع کنشی که AI می‌تواند انتخاب کند */
typedef enum {
    AI_ACT_MOVE = 0,
    AI_ACT_WALL = 1
} AiActionType;

/* جزئیات کنش انتخابی AI */
typedef struct {
    AiActionType type;
    Coord move;
    int row;
    int col;
    WallDir dir;
} AiAction;

/* تولید یک کنش معتبر برای بازیکن p */
int ai_choose_action(const GameState *g, PlayerId p, AiAction *out);

#endif
