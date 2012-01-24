#
# Configurable options
#   MODE = release | debug (default: debug)
#   SNAPPY = 0 | 1 (default: 1)
#
CSTDFLAG = --std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -fPIC -Iinclude -Ideps/snappy
CPPFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

ifeq ($(ARCH),i386)
	CPPFLAGS += -arch i386
endif

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

OBJS =

ifneq ($(SNAPPY),0)
	OBJS += deps/snappy/snappy-sinksource.o
	OBJS += deps/snappy/snappy.o
	OBJS += deps/snappy/snappy-c.o
endif

OBJS += src/compressor.o
OBJS += src/utils.o
OBJS += src/writer.o
OBJS += src/values.o
OBJS += src/pages.o
OBJS += src/bplus.o

DEPS=
DEPS += include/bplus.h
DEPS += include/private/errors.h
DEPS += include/private/pages.h
DEPS += include/private/values.h
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

TESTS =
TESTS += test/test-api
TESTS += test/test-reopen
TESTS += test/test-range
TESTS += test/test-corruption
TESTS += test/test-bulk
TESTS += test/bench-basic
TESTS += test/bench-bulk

test: $(TESTS)
	@test/test-api
	@test/test-reopen
	@test/test-range
	@test/test-bulk
	@test/test-corruption

test/%: test/%.cc bplus.a
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LINKFLAGS) $< -o $@ bplus.a

clean:
	@rm -f $(OBJS) $(TESTS)

.PHONY: all test
