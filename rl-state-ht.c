#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "ai-game.h"
#include "rl-private.h"
#include "rl.h"
#include "util.h"

#define HT_BITS 14
#define MAX_STATES (1 << HT_BITS)
#define CLEAN_N_STATES (MAX_STATES * 3 / 5)

struct xo_state {
    u32 table;
    rl_fxp scores[2];
    struct hlist_node link;
    struct list_head list;
};

static DECLARE_HASHTABLE(states, HT_BITS);
static LIST_HEAD(orders);
static unsigned long st_map[BITS_TO_LONGS(MAX_STATES)];
static struct xo_state *st_buff;

static void clean_state(void)
{
    struct xo_state *st, *safe;
    u32 pos, i = 0;
    list_for_each_entry_safe(st, safe, &orders, list) {
        if (i >= CLEAN_N_STATES)
            break;

        pos = ((uintptr_t) st - (uintptr_t) st_buff) / sizeof(struct xo_state);
        hash_del(&st->link);
        list_del(&st->list);
        st->table = st->scores[AGENT_O] = st->scores[AGENT_X] = 0;
        bitmap_clear(st_map, pos, 1);
        i++;
    }
}

rl_fxp *find_rl_state(const u32 table)
{
    const u32 key = hash_32(table, HT_BITS);
    struct xo_state *st = NULL;
    /* search state */
    hash_for_each_possible(states, st, link, key)
    {
        if (st->table == table) {
            list_move_tail(&st->list, &orders);
            return st->scores;
        }
    }
    /* evict if full */
    if (bitmap_weight(st_map, MAX_STATES) >= MAX_STATES - 1)
        clean_state();

    /* allocate and insert new state */
    u32 pos = find_first_zero_bit(st_map, MAX_STATES);
    st = st_buff + pos;
    bitmap_set(st_map, pos, 1);

    st->table = table;
    st->scores[AGENT_O] =
        fixed_mul_s32(INITIAL_MUTIPLIER, get_score(table, CELL_O));
    st->scores[AGENT_X] =
        fixed_mul_s32(INITIAL_MUTIPLIER, get_score(table, CELL_X));

    INIT_LIST_HEAD(&st->list);
    list_add(&st->list, &orders);
    hash_add(states, &st->link, key);
    return st->scores;
}

void free_rl_agent(void)
{
    vfree(st_buff);
}

void init_rl_agent(void)
{
    size_t node_sz = MAX_STATES * sizeof(struct xo_state);
    st_buff = vzalloc(ALIGN(node_sz, PAGE_SIZE));
}
