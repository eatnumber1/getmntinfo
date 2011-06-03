CC := clang
CFLAGS := -Wall -Werror -Wextra $(CFLAGS)

.PHONY: clean
.DEFAULT_GOAL := getmntinfo

clean:
	$(RM) getmntinfo

getmntinfo: getmntinfo.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<
