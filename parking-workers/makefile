CC=gcc
# CC=clang
CFLAGS=-I. -fdiagnostics-color=always
INC=sem.h
COMPLIB=*.gch
EXECUTABLE=parking
OBJECTS=$(EXECUTABLE).o sem.o

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) -o $(EXECUTABLE)

$(EXECUTABLE).o: $(EXECUTABLE).c
	$(CC) -c $(EXECUTABLE).c -o $(EXECUTABLE).o $(CFLAGS)

sem.o: sem.c $(INC)
	$(CC) -c sem.c  $(CFLAGS)

clean:
	rm -f $(OBJECTS) $(COMPLIB)
	rm $(EXECUTABLE)

