.PHONY: all
all: async.o test.out

async.o: async.c async.h
	gcc -g -c -Wall -Werror -o $@ $<

test.out: test.c async.o
	gcc -g -Wall -Werror -o $@ $^

.PHONY: clean
clean:
	rm -f *.o *.out
