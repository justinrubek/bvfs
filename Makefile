# Name of executable. Matches name of file containing main
EXE=test

# Update with header files used. Recompiles when they change
DEPS = bvfs.h util.h

# Update with code files used (replace '.c' with '.o')
OBJ = $(EXE).o

CC=gcc 
LIBS=-lpthread
CFLAGS=-g --std=c11 $(LIBS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $< $(CFLAGS)

run: $(EXE)
	./$(EXE) $(args)

clean:
	rm -f $(OBJ)
	rm -f $(EXE)
