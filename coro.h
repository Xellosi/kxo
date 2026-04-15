#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Adapted from cserv (src/coro/) for kxo userspace visualization.
 * Context switch via inline assembly for x86-64 and aarch64.
 */

#if defined(__i386__)
#define REGPARM(n) __attribute__((regparm((n))))
#else
#define REGPARM(n)
#endif

/* --- low-level context switch --- */

struct co_ctx {
    void **sp; /* saved stack pointer */
};

struct co_stack {
    void *ptr;
    size_t size_bytes;
};

typedef void (*co_routine)(void *args) REGPARM(1);

void co_switch(struct co_ctx *from, struct co_ctx *to)
    __attribute__((__noinline__)) REGPARM(2);

void co_stack_init(struct co_ctx *ctx,
                   struct co_stack *stack,
                   co_routine routine,
                   void *args);

int co_stack_alloc(struct co_stack *stack, size_t size_bytes);
void co_stack_free(struct co_stack *stack);

/* --- scheduler --- */

typedef void (*co_func)(int argc, void *argv[]);

void co_sched_init(void);
void co_start(co_func fn, int argc, ...);
void co_run(void);
void co_yield (void);

/* --- channel --- */

typedef struct co_chan co_chan;

co_chan *co_chan_make(size_t elem_size, size_t cap);
int co_chan_send(co_chan *ch, const void *data);
int co_chan_recv(co_chan *ch, void *data);
void co_chan_release(co_chan *ch);
