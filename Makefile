#
# Configurable options
#   MODE = release | debug (default: debug)
#   SNAPPY = 0 | 1 (default: 1)
#
CSTDFLAG = --std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -Iinclude -Ideps/snappy
CPPFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

ifeq ($(MODE),release)
	CPPFLAGS += -O3
else
	CFLAGS += -g
endif
LINKFLAGS =

# run make with SNAPPY=0 to turn it off
ifneq ($(SNAPPY),0)
	DEFINES += -DBP_USE_SNAPPY=1
else
	DEFINES += -DBP_USE_SNAPPY=0
endif

all: bplus.a

TESTS =
TESTS += test/api
TESTS += test/reopen
TESTS += test/range
TESTS += test/corruption
TESTS += test/one-thread-bench

test: $(TESTS)
	@test/api
	@test/reopen
	@test/range
	@test/corruption

test/%: test/%.cc bplus.a
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LINKFLAGS) $< -o $@ bplus.a

OBJS =

ifneq ($(SNAPPY),0)
	OBJS += deps/snappy/snappy-sinksource.o
	OBJS += deps/snappy/snappy.o
	OBJS += deps/snappy/snappy-c.o
endif

OBJS += src/compressor.o
OBJS += src/utils.o
OBJS += src/writer.o
OBJS += src/pages.o
OBJS += src/bplus.o

DEPS=
DEPS += include/bplus.h
DEPS += include/private/errors.h
DEPS += include/private/pages.h
DEPS += include/private/tree.h
DEPS += include/private/utils.h
DEPS += include/private/compressor.h
DEPS += include/private/writer.h

bplus.a: $(OBJS)
	$(AR) rcs bplus.a $(OBJS)

src/%.o: src/%.c $(DEPS)
	$(CC) $(CFLAGS) $(CSTDFLAG) $(CPPFLAGS) $(DEFINES) -c $< -o $@

deps/snappy/%.o: deps/snappy/%.cc
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: all test
