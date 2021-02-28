CFLAGS = -std=gnu99 -g -Wall -Werror
SOURCES = \
	async.c \
	itable.c \
	test.c

.PHONY: all
all: test.out

test.out: $(SOURCES:%.c=%.o)
	gcc $(CFLAGS) -o $@ $^

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

%.d: %.c
	gcc -MM -MT $(@:%.d=%.o) -MF $@ $<

include $(SOURCES:%.c=%.d)
