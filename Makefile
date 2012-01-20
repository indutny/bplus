CSTDFLAG = --std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS = -g
CPPFLAGS += -Iinclude -Ideps/snappy
CPPFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LINKFLAGS =

all: bplus.a

test: test/api
	test/api

test/api: bplus.a test/api.cc
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LINKFLAGS) test/api.cc -o test/api \
		bplus.a

OBJS =
OBJS += deps/snappy/snappy-sinksource.o
OBJS += deps/snappy/snappy.o
OBJS += deps/snappy/snappy-c.o
OBJS += src/writer.o
OBJS += src/pages.o
OBJS += src/bplus.o

DEPS=
DEPS += include/bplus.h
DEPS += include/private/errors.h
DEPS += include/private/pages.h
DEPS += include/private/tree.h
DEPS += include/private/utils.h
DEPS += include/private/writer.h

bplus.a: $(OBJS)
	$(AR) rcs bplus.a $(OBJS)

src/%.o: src/%.c $(DEPS)
	$(CC) $(CFLAGS) $(CSTDFLAG) $(CPPFLAGS) -c $< -o $@

deps/snappy/%.o: deps/snappy/%.cc
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: all test
