#pragma once

#include <linux/list.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include "rl.h"

typedef int (*ai_alg)(unsigned int table, char player);

enum ai_game_state {
    GAME_BUSY,
    GAME_READY,
    GAME_DONE,
};

struct ai_avg {
    s64 nsecs_o;
    s64 nsecs_x;
    u64 load_avg_o;
    u64 load_avg_x;
};

struct ai_game {
    struct xo_table xo_tlb;
    char turn;
    enum ai_game_state state;
    struct mutex lock;
    struct work_struct ai_one_work;
    struct work_struct ai_two_work;
    struct work_struct drawboard_work;
};

struct ai_agent {
    ai_alg play;
    char *name;
};

static inline rl_fxp fixed_mul(rl_fxp a, rl_fxp b)
{
    return ((s64) a * b) >> RL_FIXED_SCALE_BITS;
}

static inline rl_fxp fixed_mul_s32(rl_fxp a, s32 b)
{
    b = ((s64) b * RL_FIXED_1);
    return fixed_mul(a, b);
}
