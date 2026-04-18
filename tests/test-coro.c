/* Tests for the coroutine runtime: context switch, scheduling, channels */

#define TEST_MAIN
#include "../include/coro.h"
#include "common.h"

/* ---- Basic coroutine ---- */

static int basic_ran;

static void basic_func(int argc, void *argv[])
{
    (void) argc;
    (void) argv;
    basic_ran = 1;
}

static void test_basic_start_run(void)
{
    TEST("start and run");
    co_sched_init();
    basic_ran = 0;
    co_start(basic_func, 0);
    co_run();
    ASSERT_EQ(basic_ran, 1);
    PASS();
}

/* ---- Arguments ---- */

static int arg_sum;

static void sum_func(int argc, void *argv[])
{
    arg_sum = 0;
    for (int i = 0; i < argc; i++)
        arg_sum += (int) (intptr_t) argv[i];
}

static void test_args(void)
{
    TEST("argument passing");
    co_sched_init();
    arg_sum = 0;
    co_start(sum_func, 3, (void *) 10, (void *) 20, (void *) 30);
    co_run();
    ASSERT_EQ(arg_sum, 60);
    PASS();
}

/* ---- Multiple coroutines ---- */

static int exec_order[4];
static int exec_idx;

static void order_func(int argc, void *argv[])
{
    (void) argc;
    exec_order[exec_idx++] = (int) (intptr_t) argv[0];
}

static void test_fifo_order(void)
{
    TEST("FIFO execution order");
    co_sched_init();
    exec_idx = 0;
    co_start(order_func, 1, (void *) 1);
    co_start(order_func, 1, (void *) 2);
    co_start(order_func, 1, (void *) 3);
    co_run();
    ASSERT_EQ(exec_idx, 3);
    ASSERT_EQ(exec_order[0], 1);
    ASSERT_EQ(exec_order[1], 2);
    ASSERT_EQ(exec_order[2], 3);
    PASS();
}

static void run_basic_tests(void)
{
    SECTION_BEGIN("coroutine_basic");
    test_basic_start_run();
    test_args();
    test_fifo_order();
    SECTION_END();
}

/* ---- Yield ---- */

static int yield_sequence[6];
static int yield_idx;

static void yield_func(int argc, void *argv[])
{
    (void) argc;
    int id = (int) (intptr_t) argv[0];
    yield_sequence[yield_idx++] = id;
    co_yield ();
    yield_sequence[yield_idx++] = id;
}

static void test_yield(void)
{
    TEST("interleave two coroutines");
    co_sched_init();
    yield_idx = 0;
    co_start(yield_func, 1, (void *) 1);
    co_start(yield_func, 1, (void *) 2);
    co_run();
    ASSERT_EQ(yield_idx, 4);
    ASSERT_EQ(yield_sequence[0], 1);
    ASSERT_EQ(yield_sequence[1], 2);
    ASSERT_EQ(yield_sequence[2], 1);
    ASSERT_EQ(yield_sequence[3], 2);
    PASS();
}

static void run_yield_tests(void)
{
    SECTION_BEGIN("coroutine_yield");
    test_yield();
    SECTION_END();
}

/* ---- Channel ---- */

static void chan_sender(int argc, void *argv[])
{
    (void) argc;
    co_chan *ch = argv[0];
    int val = 42;
    co_chan_send(ch, &val);
}

static int chan_received;

static void chan_receiver(int argc, void *argv[])
{
    (void) argc;
    co_chan *ch = argv[0];
    int val = 0;
    co_chan_recv(ch, &val);
    chan_received = val;
}

static void test_channel_send_recv(void)
{
    TEST("send then recv");
    co_sched_init();
    co_chan *ch = co_chan_make(sizeof(int), 4);
    ASSERT_NOT_NULL(ch);
    chan_received = 0;
    co_start(chan_sender, 1, ch);
    co_start(chan_receiver, 1, ch);
    co_run();
    ASSERT_EQ(chan_received, 42);
    co_chan_release(ch);
    PASS();
}

static int blocking_result;

static void blocking_receiver(int argc, void *argv[])
{
    (void) argc;
    co_chan *ch = argv[0];
    int val = 0;
    co_chan_recv(ch, &val);
    blocking_result = val;
}

static void blocking_sender(int argc, void *argv[])
{
    (void) argc;
    co_chan *ch = argv[0];
    int val = 99;
    co_chan_send(ch, &val);
}

static void test_channel_blocking_recv(void)
{
    TEST("recv blocks until send");
    co_sched_init();
    co_chan *ch = co_chan_make(sizeof(int), 4);
    ASSERT_NOT_NULL(ch);
    blocking_result = 0;
    co_start(blocking_receiver, 1, ch);
    co_start(blocking_sender, 1, ch);
    co_run();
    ASSERT_EQ(blocking_result, 99);
    co_chan_release(ch);
    PASS();
}

struct test_data {
    int x, y;
};

static struct test_data *ptr_result;

static void ptr_sender(int argc, void *argv[])
{
    (void) argc;
    co_chan *ch = argv[0];
    struct test_data *d = argv[1];
    void *ptr = d;
    co_chan_send(ch, &ptr);
}

static void ptr_receiver(int argc, void *argv[])
{
    (void) argc;
    co_chan *ch = argv[0];
    void *ptr = NULL;
    co_chan_recv(ch, &ptr);
    ptr_result = ptr;
}

static void test_channel_pointer(void)
{
    TEST("pointer-sized elements (xo-user pattern)");
    co_sched_init();
    co_chan *ch = co_chan_make(sizeof(void *), 4);
    ASSERT_NOT_NULL(ch);
    struct test_data data = {.x = 10, .y = 20};
    ptr_result = NULL;
    co_start(ptr_sender, 2, ch, &data);
    co_start(ptr_receiver, 1, ch);
    co_run();
    ASSERT_EQ(ptr_result, &data);
    ASSERT_EQ(ptr_result->x, 10);
    ASSERT_EQ(ptr_result->y, 20);
    co_chan_release(ch);
    PASS();
}

static void run_channel_tests(void)
{
    SECTION_BEGIN("channel");
    test_channel_send_recv();
    test_channel_blocking_recv();
    test_channel_pointer();
    SECTION_END();
}

/* ---- Reuse ---- */

static int reuse_count;

static void reuse_func(int argc, void *argv[])
{
    (void) argc;
    (void) argv;
    reuse_count++;
}

static void test_reuse(void)
{
    TEST("slots reused across 5 cycles");
    co_sched_init();
    reuse_count = 0;
    for (int round = 0; round < 5; round++) {
        co_start(reuse_func, 0);
        co_start(reuse_func, 0);
        co_start(reuse_func, 0);
        co_run();
    }
    ASSERT_EQ(reuse_count, 15);
    PASS();
}

static void run_reuse_tests(void)
{
    SECTION_BEGIN("coroutine_reuse");
    test_reuse();
    SECTION_END();
}

/* ---- Main ---- */

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v"))
            g_verbose = 1;
    }

    printf("kxo Coroutine Tests\n");

    run_basic_tests();
    run_yield_tests();
    run_channel_tests();
    run_reuse_tests();

    if (g_tests_failed == 0)
        printf("  All %d tests passed\n", g_tests_run);
    else
        printf("  Results: %d tests, %d passed, %d failed\n", g_tests_run,
               g_tests_passed, g_tests_failed);

    return g_tests_failed > 0 ? 1 : 0;
}
