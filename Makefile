CPU	:= x86_64

CFLAGS_COMMON	:= -I. -Wall -Werror
CFLAGS_DEBUG	:= $(CFLAGS_COMMON) -g3
CFLAGS_OPTIM	:= $(CFLAGS_COMMON) -O2 -fomit-frame-pointer -DNDEBUG

ifdef DEBUG
CFLAGS	:= $(CFLAGS_DEBUG)
else
CFLAGS	:= $(CFLAGS_OPTIM)
endif

OBJ	:= dllist.o ste.o ste-$(CPU).o

all: $(OBJ)

test: $(OBJ) test.c
	gcc $(OBJ) test.c -o test

.PHONY: clean
clean:
	rm -f $(OBJ) test

