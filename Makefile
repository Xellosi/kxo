TARGET = kxo
kxo-objs = main.o game.o xoroshiro.o mcts.o negamax.o zobrist.o
kxo-objs += rl.o rl-state-ht.o
obj-m := $(TARGET).o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

LDFLAGS :=
CFLAGS :=
CFLAGS += -g

OBJS :=
OBJS += xo-user.o
OBJS += tui.o
OBJS += coro.o

all: kmod xo-user

kmod: $(GIT_HOOKS) main.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

xo-user: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

xo-user.o: xo-user.c
	$(CC) $< $(CFLAGS) -c -o $@

tui.o: tui.c
	$(CC) $< $(CFLAGS) -c -o $@

coro.o: coro.c
	$(CC) $< $(CFLAGS) -c -o $@

UNIT_TESTS := tests/test-game tests/test-coro

tests/test-game: tests/test-game.c tests/common.h game.h game.c util.h
	$(CC) -std=gnu99 -Wall -Wextra -g -Itests -I. -o $@ $<

tests/test-coro: tests/test-coro.c tests/common.h coro.c coro.h
	$(CC) -std=gnu99 -Wall -Wextra -g -I. -o $@ tests/test-coro.c coro.c

check-unit: $(UNIT_TESTS)
	@for t in $(UNIT_TESTS); do ./$$t || exit 1; done

check: check-unit all
	@sudo tests/test-integration.sh
	@echo ""
	@echo "All tests passed."

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) xo-user $(OBJS) $(TESTS)
