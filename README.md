# kxo: A Tic-Tac-Toe Game Engine implemented as Linux kernel module

## Introduction

`kxo` is a Linux kernel module that runs multiple [tic-tac-toe](https://en.wikipedia.org/wiki/Tic-tac-toe)
games simultaneously, with AI agents competing on a 4x4 board (3-in-a-row wins).
Up to 9 concurrent games run in-kernel, each pairing two randomly selected AI algorithms.

This educational module demonstrates several essential Linux kernel programming concepts:
  - Packed bitboard representation (32-bit unsigned integer, 2 bits per cell)
  - Workqueue management for asynchronous AI computation
  - Tasklet scheduling for bottom-half game dispatch
  - Spinlock and mutex synchronization across concurrent games
  - Timer-driven periodic game evaluation
  - Character device and sysfs interfaces
  - `ioctl` for exporting per-game load average metrics

### AI Algorithms

Three AI algorithms are available, randomly assigned to each game:
- **Monte Carlo Tree Search (MCTS)**: Probabilistic search with UCT selection,
  100K iterations per move, xoroshiro128++ PRNG for rollout randomization.
- **Negamax**: Depth-first alpha-beta search with principal variation splitting,
  iterative deepening to depth 6, Zobrist transposition table.
- **Reinforcement Learning (RL)**: Temporal-difference learning agent with
  hashtable-based state storage (16K entries, LRU eviction).

### Board Encoding

The 4x4 board is encoded as a single `uint32_t` using 2 bits per cell:
`00` = empty, `01` = O, `10` = X.
Win detection uses precomputed bitmask patterns, and move generation operates
entirely through bitwise operations. This compact representation reduces
kernel-to-userspace transfer to a single integer per board state.

## Build and Run

```shell
make              # build kxo.ko and xo-user
sudo insmod kxo.ko
sudo ./xo-user    # TUI visualization
sudo rmmod kxo
```

Requires kernel headers at `/lib/modules/$(uname -r)/build`.

### Userspace TUI

`xo-user` renders all 9 game boards in a 3x3 grid using Unicode box-drawing
characters and ANSI terminal control. It uses a lightweight coroutine runtime
(adapted from [cserv](https://github.com/sysprog21/cserv)) for cooperative
multitasking between I/O, display, and tab rendering.

Controls:
- `Ctrl+P`: Toggle pause/resume of board display
- `Ctrl+Q`: Terminate all kernel-space games
- `q` / `w`: Switch between Records and Load Average tabs

## Architecture

```
Timer (100ms, softirq)
  -> tasklet (game dispatch by bitmask)
    -> workqueue (WQ_UNBOUND, per-game work items):
         ai_one_work:  Player O move (MCTS / Negamax / RL)
         ai_two_work:  Player X move (MCTS / Negamax / RL)
         drawboard:    produce board to kfifo
         finish_game:  final board + game reset + RL update
  -> /dev/kxo (read/poll) -> xo-user (TUI)
  -> /sys/class/kxo/kxo/kxo_state (sysfs display/pause/end)
  -> ioctl XO_IO_LDAVG (per-game load averages)
```

### Synchronization

- Per-game mutex (`game->lock`): protects board state during AI moves and
  game-over reset
- `consumer_lock` mutex: serializes kfifo writes from concurrent drawboard
  and finish-game workers
- `avg_lock` spinlock: protects load average accumulators (used from both
  workqueue and timer softirq context)
- `mcts_lock` / `negamax_lock` mutexes: serialize access to MCTS and Negamax
  global state (PRNG, transposition table, history heuristic)
- Zobrist spinlock (`zorbist_lock`): protects hash table operations
- RL spinlock (`rl_lock`): protects state value lookups and updates

## License

`kxo` is released under the MIT license. Use of this source code is governed
by a MIT-style license that can be found in the LICENSE file.
