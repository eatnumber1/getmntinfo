CC := clang

# Debugging
PREPEND_CFLAGS := -ggdb3

# Optimization
PREPEND_CFLAGS := -fast -O4

# Warning flags
PREPEND_CFLAGS += -Wall -Werror -Wextra -Wfatal-errors -Wformat=2 -Winit-self -Wswitch-enum -Wunused-parameter -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wshorten-64-to-32 -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wpacked -Wredundant-decls -Wnested-externs -Wunreachable-code -Winline -Winvalid-pch -Wvolatile-register-var -Wpointer-sign $(CFLAGS)

# Feature flags
PREPEND_CFLAGS += -fno-non-lvalue-assign -fno-writable-strings -fstrict-aliasing

# Optimization flags
PREPEND_CFLAGS += -funit-at-a-time -fmerge-all-constants -fmodulo-sched -fgcse-sm -fgcse-las -fgcse-after-reload -funsafe-loop-optimizations -fsched2-use-superblocks -fsee -fipa-pta -ftree-loop-linear -ftree-loop-im -ftree-loop-ivcanon -fivopts -ftree-vectorize -ftracer -fvariable-expansion-in-unroller -freorder-blocks-and-partition -falign-loops-max-skip -falign-jumps -fweb -fwhole-program -ffast-math -frtl-abstract-sequences -frename-registers -funswitch-loops -fbranch-target-load-optimize -fbranch-target-load-optimize2 -fsection-anchors

CFLAGS := $(PREPEND_CFLAGS) $(CFLAGS)

.PHONY: clean
.DEFAULT_GOAL := getmntinfo

clean:
	$(RM) -r getmntinfo getmntinfo.dSYM

getmntinfo: getmntinfo.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<
