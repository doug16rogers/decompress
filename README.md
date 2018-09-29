Experiments in lzma and zlib
============================

Prerequisites
-------------

You have to have gcc installed, plus development packages for lzma and zlib:

On CentOS run:

```
    # yum install xv-devel zlib-devel
```

On Ubuntu run:

```
    # apt install liblzma-dev zlib1g-dev
```

Building and testing
--------------------

```
    $ make
    gcc -o lzma_test -Wall -Werror -g lzma_test.c -llzma
    xz -c lzma_test.c > lzma_test.c.xz
    xz -c lzma_test > lzma_test.xz
    ./lzma_test lzma_test.c.xz lzma_test.xz
    Is not ELF: lzma_test.c.xz
    ELF inside: lzma_test.xz
    gcc -o zlib_test -Wall -Werror -g zlib_test.c -lz
    gzip -c zlib_test.c > zlib_test.c.gz
    gzip -c zlib_test > zlib_test.gz
    ./zlib_test zlib_test.c.gz zlib_test.gz
    Is not ELF: zlib_test.c.gz
    ELF inside: zlib_test.gz

```
