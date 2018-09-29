TARGETS = lzma_test zlib_test

CC = gcc
CFLAGS = -Wall -Werror -g

ifeq ($(shell uname -s),Darwin)
CFLAGS += -I/opt/local/include
endif

all: $(TARGETS)

lzma_test: lzma_test.c Makefile
	$(CC) -o $@ $(CFLAGS) $< -llzma
	xz -c $< > $<.xz
	xz -c $@ > $@.xz
	./$@ $<.xz $@.xz

zlib_test: zlib_test.c Makefile
	$(CC) -o $@ $(CFLAGS) $< -lz
	gzip -c $< > $<.gz
	gzip -c $@ > $@.gz
	./$@ $<.gz $@.gz

.phony: clean
clean:
	rm -f *.o *~ *.xz *.gz $(TARGETS)
