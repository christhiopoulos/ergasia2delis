

CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -g
TARGETS = lexan splitter builder
OBJECTS = lexan.o splitter.o builder.o hash_table.o
DEPS = hash_table.h lexan.h splitter.h builder.h

all: $(TARGETS)


lexan: lexan.o hash_table.o
	$(CC) $(CFLAGS) -o lexan lexan.o hash_table.o


splitter: splitter.o
	$(CC) $(CFLAGS) -o splitter splitter.o

builder: builder.o hash_table.o
	$(CC) $(CFLAGS) -o builder builder.o hash_table.o


hash_table.o: hash_table.c hash_table.h
	$(CC) $(CFLAGS) -c hash_table.c


lexan.o: lexan.c lexan.h hash_table.h splitter.h 
	$(CC) $(CFLAGS) -c lexan.c


splitter.o: splitter.c splitter.h
	$(CC) $(CFLAGS) -c splitter.c


builder.o: builder.c  hash_table.h
	$(CC) $(CFLAGS) -c builder.c


clean:
	rm -f $(TARGETS) *.o output*.txt lexan_debug.log valgrind.log

# Run the program
run: lexan
	./lexan -i GreatExpectations_a.txt -l 1 -m 5 -t 10 -e ExclusionList1_a.txt -o output1.txt

# Run the program with Valgrind for memory checking
valgrind: all
	valgrind --leak-check=full --trace-children=yes ./lexan -i GreatExpectations_a.txt -l 1 -m 5 -t 5 -e ExclusionList1_a.txt -o output1.txt

.PHONY: all clean run valgrind
