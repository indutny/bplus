CSTDFLAG = --std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS = -g
CPPFLAGS += -Iinclude -Ideps/snappy
CPPFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LINKFLAGS =
DEFINES =

ifeq ($(SNAPPY),1)
	DEFINES += -DBP_USE_SNAPPY=1
else
	DEFINES += -DBP_USE_SNAPPY=0
endif

all: bplus.a

test: test/api
	test/api

test/api: bplus.a test/api.cc
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LINKFLAGS) test/api.cc -o test/api \
		bplus.a

OBJS =
ifeq ($(SNAPPY),1)
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
