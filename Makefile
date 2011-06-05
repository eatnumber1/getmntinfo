CC := clang
CFLAGS := -ggdb -Wall -Werror -Wextra -Wfatal-errors -Wformat=2 -Winit-self -Wswitch-enum -Wunused-parameter -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wshorten-64-to-32 -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wpacked -Wredundant-decls -Wnested-externs -Wunreachable-code -Winline -Winvalid-pch -Wvolatile-register-var -Wpointer-sign $(CFLAGS)

.PHONY: clean
.DEFAULT_GOAL := getmntinfo

clean:
	$(RM) -r getmntinfo getmntinfo.dSYM

getmntinfo: getmntinfo.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<
