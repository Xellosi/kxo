UNAME_S := $(shell uname -s)

TARGET = kxo
kxo-objs = main.o game.o xoroshiro.o mcts.o negamax.o zobrist.o
kxo-objs += rl.o rl-state-ht.o
obj-m := $(TARGET).o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement
KDIR ?= /lib/modules/$(shell uname -r)/build

GIT_HOOKS := .git/hooks/applied

LDFLAGS :=
CFLAGS :=
CFLAGS += -g

OBJS :=
OBJS += xo-user.o
OBJS += tui.o
OBJS += coro.o

.PHONY: all kmod check-unit check clean

ifneq ($(UNAME_S),Linux)
define REQUIRE_LINUX
	$(error This target requires a Linux host (detected: $(UNAME_S)))
endef
endif

ifeq ($(UNAME_S),Linux)
all: $(GIT_HOOKS) kmod xo-user
else
all: $(GIT_HOOKS)
	@echo "Non-Linux host detected ($(UNAME_S)). Only git hooks were installed."
	@echo "Build and test require a Linux environment."
endif

kmod: main.c
	$(REQUIRE_LINUX)
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

xo-user: $(OBJS)
	$(REQUIRE_LINUX)
	$(CC) $(LDFLAGS) -o $@ $^

xo-user.o: xo-user.c
	$(REQUIRE_LINUX)
	$(CC) $< $(CFLAGS) -c -o $@

tui.o: tui.c
	$(REQUIRE_LINUX)
	$(CC) $< $(CFLAGS) -c -o $@

coro.o: coro.c
	$(REQUIRE_LINUX)
	$(CC) $< $(CFLAGS) -c -o $@

UNIT_TESTS := tests/test-game tests/test-coro

tests/test-game: tests/test-game.c tests/common.h game.h game.c util.h
	$(REQUIRE_LINUX)
	$(CC) -std=gnu99 -Wall -Wextra -g -Itests -I. -o $@ $<

tests/test-coro: tests/test-coro.c tests/common.h coro.c coro.h
	$(REQUIRE_LINUX)
	$(CC) -std=gnu99 -Wall -Wextra -g -I. -o $@ tests/test-coro.c coro.c

check-unit: $(UNIT_TESTS)
	$(REQUIRE_LINUX)
	@for t in $(UNIT_TESTS); do ./$$t || exit 1; done

check: check-unit all
	$(REQUIRE_LINUX)
	@sudo tests/test-integration.sh
	@echo ""
	@echo "All tests passed."

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
ifeq ($(UNAME_S),Linux)
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
endif
	$(RM) xo-user $(OBJS) $(UNIT_TESTS)
