CFLAGS = -Wall -Wextra
LDFLAGS = -I. -lm -levdev -lxkbcommon -lxdo

ifeq ($(DEBUG),1)
	CFLAGS += -O0 -ggdb
else
	CFLAGS += -O3
endif

SRC = src/core.c
OUT = obj/core.o

all: ${OUT} test

obj/%.o : src/%.c
	mkdir -p obj
	${CC} ${CFLAGS} -c -o $@ $<

test: ${OUT} test.c
	${CC} ${CFLAGS} ${LDFLAGS} -o test ${OUT} test.c

clean:
	rm -f obj/*
	rm -f test

clear: clean

.PHONY: all clean clear install