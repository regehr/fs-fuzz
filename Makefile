EXES=fd_fuzz file_fuzz
CFLAGS=-g -O2 -Wall

default: $(EXES)

INCS=rand48.h

FD_FUZZ_OBJS=fd_fuzz.o rand48.o

fd_fuzz: $(INCS) $(FD_FUZZ_OBJS)
	$(CC) $(CFLAGS) $(FD_FUZZ_OBJS) -o fd_fuzz

FILE_FUZZ_OBJS=file_fuzz.o rand48.o

file_fuzz: $(INCS) $(FILE_FUZZ_OBJS)
	$(CC) $(CFLAGS) $(FILE_FUZZ_OBJS) -o file_fuzz

clean:
	-rm -f *.o $(EXES)
