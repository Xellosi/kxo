/* Minimal coroutine runtime adapted from cserv (src/coro/).
 * Context switch via inline assembly for x86-64 and aarch64.
 * Adds a simple buffered channel for inter-coroutine communication.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "coro.h"

/* Platform context switch */

#define CO_STR_(x) #x
#define CO_STR(x) CO_STR_(x)
#define CO_ASM_SYM(name) CO_STR(__USER_LABEL_PREFIX__) #name

__asm__(
    ".text\n"
    ".globl " CO_ASM_SYM(co_switch) "\n"
    CO_ASM_SYM(co_switch) ":\n"
#if defined(__x86_64__)
#define NUM_SAVED 6
    "push %rbp\n"
    "push %rbx\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"
    "mov %rsp, (%rdi)\n"
    "mov (%rsi), %rsp\n"
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %rbx\n"
    "pop %rbp\n"
    "pop %rcx\n"
    "jmp *%rcx\n"
#elif defined(__aarch64__)
#define NUM_SAVED 11
    "sub sp, sp, #96\n"
    "mov x8, sp\n"
    "stp x19, x20, [x8], #16\n"
    "stp x21, x22, [x8], #16\n"
    "stp x23, x24, [x8], #16\n"
    "stp x25, x26, [x8], #16\n"
    "stp x27, x28, [x8], #16\n"
    "stp x29, lr, [x8], #16\n"
    "mov x8, sp\n"
    "str x8, [x0]\n"
    "ldr x8, [x1]\n"
    "mov sp, x8\n"
    "ldp x19, x20, [x8], #16\n"
    "ldp x21, x22, [x8], #16\n"
    "ldp x23, x24, [x8], #16\n"
    "ldp x25, x26, [x8], #16\n"
    "ldp x27, x28, [x8], #16\n"
    "ldp x29, lr, [x8], #16\n"
    "mov sp, x8\n"
    "br lr\n"
#else
#error "unsupported architecture: need x86_64 or aarch64"
#endif
);

/* Entry trampoline: pops (args, routine) from stack set up by co_stack_init,
 * then calls routine(args).
 */
void co_entry_trampoline(void);

__asm__(
    ".text\n"
    ".globl " CO_ASM_SYM(co_entry_trampoline) "\n"
    CO_ASM_SYM(co_entry_trampoline) ":\n"
#if defined(__x86_64__)
    "pop %rdi\n"
    "pop %rcx\n"
    "call *%rcx\n"
#elif defined(__aarch64__)
    "mov x8, sp\n"
    "ldp x0, x9, [x8], #16\n"
    "mov sp, x8\n"
    "blr x9\n"
#else
#error "unsupported architecture"
#endif
);

void co_stack_init(struct co_ctx *ctx,
                   struct co_stack *stack,
                   co_routine routine,
                   void *args)
{
    ctx->sp = (void **) stack->ptr;
    *--ctx->sp = (void *) routine;
    *--ctx->sp = (void *) args;
    *--ctx->sp = (void *) co_entry_trampoline;
    ctx->sp -= NUM_SAVED;
}

static long get_page_size(void)
{
    static long pgsz;
    if (!pgsz)
        pgsz = sysconf(_SC_PAGESIZE);
    return pgsz;
}

int co_stack_alloc(struct co_stack *stack, size_t size_bytes)
{
    stack->size_bytes = size_bytes;
    size_t total = size_bytes + get_page_size(); /* guard page */

    stack->ptr = mmap(NULL, total, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack->ptr == MAP_FAILED)
        return -1;

    /* Guard page at the low end to catch stack overflow */
    mprotect(stack->ptr, get_page_size(), PROT_NONE);
    /* Stack top: ptr grows downward from the high end */
    stack->ptr = (char *) stack->ptr + total;
    return 0;
}

void co_stack_free(struct co_stack *stack)
{
    size_t total = stack->size_bytes + get_page_size();
    void *base = (char *) stack->ptr - total;
    munmap(base, total);
}

/* Scheduler: simple FIFO queue of coroutines */

#define CO_MAX 16
#define CO_MAX_ARGS 8
#define CO_STACK_BYTES (64 * 1024)

enum co_state { CO_FREE = 0, CO_READY, CO_RUNNING, CO_BLOCKED, CO_DONE };

struct coroutine {
    struct co_ctx ctx;
    struct co_stack stack;
    enum co_state state;
    co_func func;
    int argc;
    void *argv[CO_MAX_ARGS];
    struct coroutine *next; /* linked list for run queue */
};

/* Run queue (singly-linked FIFO) */
static struct {
    struct coroutine pool[CO_MAX];
    struct coroutine main_co;  /* scheduler context */
    struct coroutine *current; /* currently running coroutine */
    struct coroutine *head;    /* run queue head */
    struct coroutine *tail;    /* run queue tail */
} sched;

static void enqueue(struct coroutine *co)
{
    co->next = NULL;
    if (sched.tail)
        sched.tail->next = co;
    else
        sched.head = co;
    sched.tail = co;
}

static struct coroutine *dequeue(void)
{
    struct coroutine *co = sched.head;
    if (co) {
        sched.head = co->next;
        if (!sched.head)
            sched.tail = NULL;
        co->next = NULL;
    }
    return co;
}

/* Proxy: called on the coroutine's own stack. Invokes the user function, then
 * marks the coroutine done and returns to scheduler.
 */
static REGPARM(1) void co_proxy(void *arg)
{
    struct coroutine *co = arg;
    co->func(co->argc, co->argv);
    co->state = CO_DONE;
    co_switch(&co->ctx, &sched.main_co.ctx);
}

void co_sched_init(void)
{
    memset(&sched, 0, sizeof(sched));
}

void co_start(co_func fn, int argc, ...)
{
    /* Find a free slot */
    struct coroutine *co = NULL;
    for (int i = 0; i < CO_MAX; i++) {
        if (sched.pool[i].state == CO_FREE) {
            co = &sched.pool[i];
            break;
        }
    }
    if (!co)
        return;

    co->func = fn;
    co->argc = argc;

    va_list ap;
    va_start(ap, argc);
    for (int i = 0; i < argc && i < CO_MAX_ARGS; i++)
        co->argv[i] = va_arg(ap, void *);
    va_end(ap);

    /* Allocate stack if this slot has never been used */
    if (!co->stack.ptr) {
        if (co_stack_alloc(&co->stack, CO_STACK_BYTES))
            return;
    }

    co_stack_init(&co->ctx, &co->stack, co_proxy, co);
    co->state = CO_READY;
    enqueue(co);
}

void co_yield (void)
{
    struct coroutine *co = sched.current;
    if (co->state == CO_RUNNING)
        co->state = CO_READY;
    enqueue(co);
    co_switch(&co->ctx, &sched.main_co.ctx);
}

void co_run(void)
{
    for (;;) {
        struct coroutine *co = dequeue();
        if (!co) {
            /* Check if any coroutines are still blocked */
            int alive = 0;
            for (int i = 0; i < CO_MAX; i++)
                if (sched.pool[i].state == CO_BLOCKED)
                    alive = 1;
            if (!alive)
                break;
            /* Blocked coroutines exist but nothing is ready.
             * This shouldn't happen in our usage pattern because blocking
             * syscalls run inside coroutines, not via yield.
             */
            break;
        }

        sched.current = co;
        co->state = CO_RUNNING;
        co_switch(&sched.main_co.ctx, &co->ctx);

        /* Coroutine returned to scheduler */
        if (co->state == CO_DONE)
            co->state = CO_FREE;
    }
    sched.current = NULL;
}

/* Channel: bounded FIFO buffer for pointer-sized messages */

struct co_chan {
    char *buf;
    size_t elem_size;
    size_t cap;
    size_t head, tail, count;
    struct coroutine *recv_waiter; /* coroutine blocked on recv */
    struct coroutine *send_waiter; /* coroutine blocked on send */
};

co_chan *co_chan_make(size_t elem_size, size_t cap)
{
    co_chan *ch = calloc(1, sizeof(*ch));
    if (!ch)
        return NULL;
    ch->elem_size = elem_size;
    ch->cap = cap;
    ch->buf = calloc(cap, elem_size);
    if (!ch->buf) {
        free(ch);
        return NULL;
    }
    return ch;
}

int co_chan_send(co_chan *ch, const void *data)
{
    /* Block while channel is full */
    while (ch->count >= ch->cap) {
        ch->send_waiter = sched.current;
        sched.current->state = CO_BLOCKED;
        co_switch(&sched.current->ctx, &sched.main_co.ctx);
    }

    memcpy(ch->buf + (ch->tail * ch->elem_size), data, ch->elem_size);
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;

    /* Wake blocked receiver */
    if (ch->recv_waiter) {
        ch->recv_waiter->state = CO_READY;
        enqueue(ch->recv_waiter);
        ch->recv_waiter = NULL;
    }
    return 0;
}

int co_chan_recv(co_chan *ch, void *data)
{
    /* Block while channel is empty */
    while (ch->count == 0) {
        ch->recv_waiter = sched.current;
        sched.current->state = CO_BLOCKED;
        co_switch(&sched.current->ctx, &sched.main_co.ctx);
    }

    memcpy(data, ch->buf + (ch->head * ch->elem_size), ch->elem_size);
    ch->head = (ch->head + 1) % ch->cap;
    ch->count--;

    /* Wake blocked sender */
    if (ch->send_waiter) {
        ch->send_waiter->state = CO_READY;
        enqueue(ch->send_waiter);
        ch->send_waiter = NULL;
    }
    return 0;
}

void co_chan_release(co_chan *ch)
{
    if (ch) {
        free(ch->buf);
        free(ch);
    }
}
