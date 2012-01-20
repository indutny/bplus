CSTDFLAG = --std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS = -g
CPPFLAGS += -Iinclude -Ideps/snappy
LINKFLAGS =

all: bplus.a

test: test/api
	test/api

test/api: bplus.a test/api.c
	g++ $(CFLAGS) $(CPPFLAGS) $(LINKFLAGS) test/api.c -o test/api \
		bplus.a

OBJS =
OBJS += deps/snappy/snappy-sinksource.o
OBJS += deps/snappy/snappy.o
OBJS += deps/snappy/snappy-c.o
OBJS += src/writer.o
OBJS += src/pages.o
OBJS += src/bplus.o

bplus.a: $(OBJS)
	$(AR) rcs bplus.a $(OBJS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) -Iinclude/private -c $< -o $@

deps/snappy/%.o: deps/snappy/%.cc
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) -Iinclude/private -c $< -o $@

.PHONY: all test
