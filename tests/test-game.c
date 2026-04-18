/* Unit tests for game logic: bit manipulation, win detection, scoring.
 * Compiled with -Itests so that <linux/slab.h> etc. resolve to
 * our stub headers in tests/linux/, providing userspace shims.
 */

#define TEST_MAIN
#include "../include/game.h"
#include "common.h"

/* Pull in the implementation directly so we can test static functions */
#include "../src/game.c"

/* Pull in scoring after xo_segment_lines is defined */
#include "../include/util.h"

/* ---- Bit manipulation ---- */

static void test_set_get_bits(void)
{
    TEST("round-trip all cells");
    unsigned int table = 0;
    for (int pos = 0; pos < N_GRIDS; pos++) {
        table = VAL_SET_CELL(table, pos, CELL_O);
        ASSERT_EQ(TABLE_GET_CELL(table, pos), CELL_O);
    }
    for (int pos = 0; pos < N_GRIDS; pos++) {
        table = VAL_SET_CELL(table, pos, CELL_X);
        ASSERT_EQ(TABLE_GET_CELL(table, pos), CELL_X);
    }
    for (int pos = 0; pos < N_GRIDS; pos++) {
        table = VAL_SET_CELL(table, pos, CELL_EMPTY);
        ASSERT_EQ(TABLE_GET_CELL(table, pos), CELL_EMPTY);
    }
    ASSERT_EQ(table, 0u);
    PASS();
}

static void test_set_bits_boundary(void)
{
    TEST("pos=15 (highest cell) no UB");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 15, CELL_X);
    ASSERT_EQ(TABLE_GET_CELL(table, 15), CELL_X);
    for (int i = 0; i < 15; i++)
        ASSERT_EQ(TABLE_GET_CELL(table, i), CELL_EMPTY);
    PASS();
}

static void test_set_bits64(void)
{
    TEST("SET/GET_RECORD_CELL round-trip");
    unsigned long moves = 0;
    for (int step = 0; step < N_GRIDS; step++) {
        moves = SET_RECORD_CELL(moves, step, step);
        ASSERT_EQ(GET_RECORD_CELL(moves, step), (unsigned int) step);
    }
    PASS();
}

static void run_bitops_tests(void)
{
    SECTION_BEGIN("bitops");
    test_set_get_bits();
    test_set_bits_boundary();
    test_set_bits64();
    SECTION_END();
}

/* ---- Win detection ---- */

static void test_check_win_empty(void)
{
    TEST("empty board");
    ASSERT_EQ(check_win(0), CELL_EMPTY);
    PASS();
}

static void test_check_win_o_row(void)
{
    TEST("O wins top row (0,1,2)");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 0, CELL_O);
    table = VAL_SET_CELL(table, 1, CELL_O);
    table = VAL_SET_CELL(table, 2, CELL_O);
    ASSERT_EQ(check_win(table), CELL_O);
    PASS();
}

static void test_check_win_x_col(void)
{
    TEST("X wins first column (0,4,8)");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 0, CELL_X);
    table = VAL_SET_CELL(table, 4, CELL_X);
    table = VAL_SET_CELL(table, 8, CELL_X);
    ASSERT_EQ(check_win(table), CELL_X);
    PASS();
}

static void test_check_win_diagonal(void)
{
    TEST("O wins diagonal (0,5,10)");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 0, CELL_O);
    table = VAL_SET_CELL(table, 5, CELL_O);
    table = VAL_SET_CELL(table, 10, CELL_O);
    ASSERT_EQ(check_win(table), CELL_O);
    PASS();
}

static void test_check_win_anti_diagonal(void)
{
    TEST("X wins anti-diagonal (2,5,8)");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 2, CELL_X);
    table = VAL_SET_CELL(table, 5, CELL_X);
    table = VAL_SET_CELL(table, 8, CELL_X);
    ASSERT_EQ(check_win(table), CELL_X);
    PASS();
}

static void test_check_win_no_winner(void)
{
    TEST("partial board, no winner");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 0, CELL_O);
    table = VAL_SET_CELL(table, 1, CELL_X);
    table = VAL_SET_CELL(table, 5, CELL_O);
    ASSERT_EQ(check_win(table), CELL_EMPTY);
    PASS();
}

static void test_check_win_draw(void)
{
    TEST("full board, draw");
    unsigned int table = 0;
    unsigned char pattern[16] = {
        CELL_O, CELL_X, CELL_O, CELL_X, CELL_O, CELL_X, CELL_O, CELL_X,
        CELL_X, CELL_O, CELL_X, CELL_O, CELL_X, CELL_O, CELL_X, CELL_O,
    };
    for (int i = 0; i < 16; i++)
        table = VAL_SET_CELL(table, i, pattern[i]);
    ASSERT_EQ(check_win(table), CELL_D);
    PASS();
}

static void test_win_patterns_count(void)
{
    TEST("24 win patterns for 4x4 goal=3");
    ASSERT_EQ(WIN_PATT_LEN(BOARD_SIZE, GOAL), 24);
    PASS();
}

static void test_win_patterns_exhaustive(void)
{
    TEST("all 24 patterns detect O win");
    for (int i = 0; i < 24; i++) {
        unsigned int table = 0;
        for (int k = 0; k < GOAL; k++)
            table = VAL_SET_CELL(table, xo_segment_lines[i][k], CELL_O);
        ASSERT_EQ(check_win(table), CELL_O);
    }
    PASS();
}

static void run_win_tests(void)
{
    SECTION_BEGIN("check_win");
    test_check_win_empty();
    test_check_win_o_row();
    test_check_win_x_col();
    test_check_win_diagonal();
    test_check_win_anti_diagonal();
    test_check_win_no_winner();
    test_check_win_draw();
    test_win_patterns_count();
    test_win_patterns_exhaustive();
    SECTION_END();
}

/* ---- available_moves ---- */

static void test_moves_empty(void)
{
    TEST("empty board = 16 moves");
    int *moves = available_moves(0);
    ASSERT_NOT_NULL(moves);
    int count = 0;
    while (count < N_GRIDS && moves[count] != -1)
        count++;
    ASSERT_EQ(count, 16);
    free(moves);
    PASS();
}

static void test_moves_partial(void)
{
    TEST("3 occupied = 13 moves");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 0, CELL_O);
    table = VAL_SET_CELL(table, 7, CELL_X);
    table = VAL_SET_CELL(table, 15, CELL_O);
    int *moves = available_moves(table);
    ASSERT_NOT_NULL(moves);
    int count = 0;
    while (count < N_GRIDS && moves[count] != -1)
        count++;
    ASSERT_EQ(count, 13);
    free(moves);
    PASS();
}

static void test_moves_full(void)
{
    TEST("full board = 0 moves");
    unsigned int table = 0;
    for (int i = 0; i < N_GRIDS; i++)
        table = VAL_SET_CELL(table, i, (i & 1) ? CELL_X : CELL_O);
    int *moves = available_moves(table);
    ASSERT_NOT_NULL(moves);
    ASSERT_EQ(moves[0], -1);
    free(moves);
    PASS();
}

static void run_moves_tests(void)
{
    SECTION_BEGIN("available_moves");
    test_moves_empty();
    test_moves_partial();
    test_moves_full();
    SECTION_END();
}

/* ---- calculate_win_value ---- */

static void test_win_value(void)
{
    TEST("win=1.0, loss=0, draw=0.5");
    fixed_point_t one = 1U << FIXED_SCALE_BITS;
    fixed_point_t half = 1U << (FIXED_SCALE_BITS - 1);

    ASSERT_EQ(calculate_win_value(CELL_O, CELL_O), one);
    ASSERT_EQ(calculate_win_value(CELL_X, CELL_O), 0u);
    ASSERT_EQ(calculate_win_value(CELL_D, CELL_O), half);
    ASSERT_EQ(calculate_win_value(CELL_X, CELL_X), one);
    ASSERT_EQ(calculate_win_value(CELL_O, CELL_X), 0u);
    ASSERT_EQ(calculate_win_value(CELL_D, CELL_X), half);
    PASS();
}

static void run_win_value_tests(void)
{
    SECTION_BEGIN("calculate_win_value");
    test_win_value();
    SECTION_END();
}

/* ---- Scoring (util.h) ---- */

static void test_score_empty(void)
{
    TEST("empty board scores 0");
    ASSERT_EQ(get_score(0, CELL_O), 0);
    ASSERT_EQ(get_score(0, CELL_X), 0);
    PASS();
}

static void test_score_advantage(void)
{
    TEST("O two-in-a-row > 0");
    unsigned int table = 0;
    table = VAL_SET_CELL(table, 0, CELL_O);
    table = VAL_SET_CELL(table, 1, CELL_O);
    ASSERT_GT(get_score(table, CELL_O), 0);
    PASS();
}

static void run_score_tests(void)
{
    SECTION_BEGIN("get_score");
    test_score_empty();
    test_score_advantage();
    SECTION_END();
}

/* ---- Attr bit layout ---- */

static void test_attr_no_overlap(void)
{
    TEST("STEPS and AI_ALG fields independent");
    unsigned int attr = 0;
    attr = XO_SET_ATTR_STEPS(attr, 0x1f);
    ASSERT_EQ(XO_ATTR_STEPS(attr), 0x1fu);
    ASSERT_EQ(XO_ATTR_AI_ALG(attr), 0u);

    attr = 0;
    attr = XO_SET_ATTR_AI_ALG(attr, 7, 7);
    ASSERT_EQ(XO_ATTR_AI_ALG(attr), 0xfu);
    ASSERT_EQ(XO_ATTR_STEPS(attr), 0u);
    ASSERT_EQ(XO_ATTR_ID(attr), 0u);
    PASS();
}

static void run_attr_tests(void)
{
    SECTION_BEGIN("attr_bitfield");
    test_attr_no_overlap();
    SECTION_END();
}

/* ---- Main ---- */

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v"))
            g_verbose = 1;
    }

    fill_win_patterns();

    printf("kxo Game Logic Tests\n");

    run_bitops_tests();
    run_win_tests();
    run_moves_tests();
    run_win_value_tests();
    run_score_tests();
    run_attr_tests();

    if (g_tests_failed == 0)
        printf("  All %d tests passed\n", g_tests_run);
    else
        printf("  Results: %d tests, %d passed, %d failed\n", g_tests_run,
               g_tests_passed, g_tests_failed);

    return g_tests_failed > 0 ? 1 : 0;
}
