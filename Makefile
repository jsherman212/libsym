CC=clang
CFLAGS=-fno-pie -g -pedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-case-range
LDFLAGS=-ldwarf -lelf -lz

driver : driver.o sym.o linkedlist.o compunit.o die.o dexpr.o symerr.o str.o
	$(CC) $(CFLAGS) $(LDFLAGS) driver.o sym.o linkedlist.o compunit.o die.o dexpr.o symerr.o str.o -o driver

driver.o : driver.c
	$(CC) $(CFLAGS) driver.c -c

sym.o : sym.c sym.h
	$(CC) $(CFLAGS) sym.c -c

linkedlist.o : linkedlist.c linkedlist.h
	$(CC) $(CFLAGS) linkedlist.c -c

compunit.o : compunit.c compunit.h
	$(CC) $(CFLAGS) compunit.c -c

die.o : die.c die.h
	$(CC) $(CFLAGS) die.c -c

dexpr.o : dexpr.c dexpr.h
	$(CC) $(CFLAGS) dexpr.c -c

symerr.o : symerr.c symerr.h
	$(CC) $(CFLAGS) symerr.c -c

str.o : str.c str.h
	$(CC) $(CFLAGS) str.c -c
