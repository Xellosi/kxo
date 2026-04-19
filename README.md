# kxo: A Tic-Tac-Toe Game Engine as a Linux Kernel Module

## Introduction
`kxo` is a Linux kernel module that runs multiple
[tic-tac-toe](https://en.wikipedia.org/wiki/Tic-tac-toe) games
simultaneously, with AI agents competing on a 4x4 board where 3-in-a-row
wins. Up to 9 concurrent games run entirely in kernel space, each pairing
two randomly selected AI algorithms.

The module serves as a teaching vehicle for core kernel programming
techniques:
- Packed [bitboard](https://en.wikipedia.org/wiki/Bitboard) representation
  (32-bit integer, 2 bits per cell) for compact game state
- [Workqueue](https://docs.kernel.org/core-api/workqueue.html) management
  for asynchronous AI computation
- Tasklet scheduling for
  [bottom-half](https://en.wikipedia.org/wiki/Interrupt_handler#Bottom_half) game dispatch
- Spinlock and mutex synchronization across concurrent games
- Timer-driven periodic game evaluation
- Character device and sysfs interfaces for userspace communication
- `ioctl` for exporting per-game
  [load average](https://en.wikipedia.org/wiki/Load_(computing)) metrics

### AI Algorithms
Three AI algorithms are available, randomly assigned per game:
- [Monte Carlo tree search](https://en.wikipedia.org/wiki/Monte_Carlo_tree_search)
  (MCTS): Uses [UCT](https://en.wikipedia.org/wiki/Monte_Carlo_tree_search#Exploration_and_exploitation)
  for selection, 100K iterations per move, and a
  [xoroshiro128++](https://en.wikipedia.org/wiki/Xorshift#xoroshiro) PRNG for
  rollout randomization.
- [Negamax](https://en.wikipedia.org/wiki/Negamax) with
  [alpha-beta pruning](https://en.wikipedia.org/wiki/Alpha%E2%80%93beta_pruning):
  Employs [principal variation search](https://en.wikipedia.org/wiki/Principal_variation_search),
  iterative deepening to depth 6, and a
  [Zobrist hash](https://en.wikipedia.org/wiki/Zobrist_hashing)
  transposition table.
- [Reinforcement learning](https://en.wikipedia.org/wiki/Reinforcement_learning)
  (RL): [Temporal-difference](https://en.wikipedia.org/wiki/Temporal_difference_learning)
  agent with hashtable-based state storage (16K entries, LRU eviction).

### Board Encoding
The 4x4 board is packed into a single `uint32_t` using 2 bits per cell
(`00` = empty, `01` = O, `10` = X; `11` is reserved for draw status). Win detection uses
precomputed bitmask patterns, and move generation operates through bitwise
extraction. The board field is transferred to userspace as part of
`struct xo_table`.

## Build and Run
```shell
make              # build kxo.ko and xo-user
sudo insmod kxo.ko
sudo ./xo-user    # TUI visualization
sudo rmmod kxo
```

Requires kernel headers at `/lib/modules/$(uname -r)/build`.

### Testing
```shell
make check        # unit tests + integration test (requires sudo)
```

Unit tests (`tests/test-game`, `tests/test-coro`) run first, followed by
`tests/test-integration.sh` which exercises the live kernel module.

### Static Analysis
```shell
make cppcheck     # run cppcheck over src/, user/, and include/
```

`scripts/cppcheck.sh` is the single source of truth for cppcheck options
and suppressions; the pre-commit hook invokes it on staged files only.

### Userspace TUI
`xo-user` (source in `user/xo-user.c`, `user/tui.c`, `user/coro.c`)
renders all 9 boards in a 3x3 grid using Unicode box-drawing characters
and ANSI terminal control. A lightweight
[coroutine](https://en.wikipedia.org/wiki/Coroutine) runtime (adapted from
[cserv](https://github.com/sysprog21/cserv)) drives cooperative
multitasking between I/O, display, and tab rendering.

Controls:
- `Ctrl+P`: Toggle pause/resume of board display
- `Ctrl+Q`: Terminate all kernel-space games
- `q` / `w`: Switch between Records and Load Average tabs

## Architecture
```
 ┌──────────────────┐
 │  Timer (100 ms)  │  softirq context
 └────────┬─────────┘
          │ schedules
          ▼
 ┌──────────────────────────────┐
 │  Tasklet (game_tasklet_func) │  dispatch games by bitmask
 └────────┬─────────────────────┘
          │ queues per-game work
          ▼
 ┌──────────────────────────────────────────────┐
 │  Workqueue  "kxod"  (WQ_UNBOUND)             │
 │                                              │
 │  ┌─────────────┐  ┌─────────────┐            │
 │  │ ai_one_work │  │ ai_two_work │            │
 │  │ Player O    │  │ Player X    │            │
 │  │ MCTS/Neg/RL │  │ MCTS/Neg/RL │            │
 │  └──────┬──────┘  └──────┬──────┘            │
 │         └──────┬─────────┘                   │
 │                ▼                             │
 │  ┌────────────────────┐  ┌───────────────┐   │
 │  │ drawboard ──► kfifo│  │  finish_game  │   │
 │  └─────────┬──────────┘  │  reset + RL   │   │
 │            │             └───────────────┘   │
 └────────────┼─────────────────────────────────┘
              │ read / poll
              ▼
 ┌──────────────────────────────────────────────┐
 │  /dev/kxo  (char device)                     │
 │  /sys/class/kxo/kxo/kxo_state  (sysfs)       │
 │  ioctl XO_IO_LDAVG  (load averages)          │
 └─────────────────────┬────────────────────────┘
                       │
                       ▼
              ┌────────────────┐
              │  xo-user (TUI) │  userspace
              └────────────────┘
```

### Synchronization
- Per-game mutex (`game->lock`): protects board state during AI moves and
  game-over reset
- `consumer_lock` mutex: serializes
  [kfifo](https://docs.kernel.org/core-api/kernel-api.html#fifo-buffer) writes from
  concurrent drawboard and finish-game workers
- `avg_lock` spinlock: protects load average accumulators (used from both
  workqueue and timer softirq context)
- `mcts_lock` / `negamax_lock` mutexes: serialize access to MCTS and
  Negamax global state (PRNG, transposition table, history heuristic)
- Zobrist spinlock (`zorbist_lock`): protects hash table operations
- RL spinlock (`rl_lock`): protects state value lookups and updates

## License
`kxo` is released under the MIT license. Use of this source code is governed
by a MIT-style license that can be found in the LICENSE file.
