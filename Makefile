CC := clang
CFLAGS += -Wall -Werror -Wextra

.PHONY: clean
.DEFAULT_GOAL := statfs

clean:
	$(RM) statfs

statfs: statfs.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<
