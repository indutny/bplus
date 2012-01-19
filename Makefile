CC = g++
CSTDFLAG = --std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS = -g
CPPFLAGS += -Iinclude
LINKFLAGS =

all: bplus.a

test: test/api
	test/api

test/api: bplus.a test/api.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LINKFLAGS) test/api.c -o test/api \
		bplus.a

OBJS =
OBJS += src/writer.o
OBJS += src/pages.o
OBJS += src/bplus.o


bplus.a: $(OBJS)
	$(AR) rcs bplus.a $(OBJS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) -Iinclude/private -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) -Iinclude/private -c $< -o $@

.PHONY: all test
