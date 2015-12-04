CFLAGS = -c -g -Wall -std=gnu99

SOURCES= mymall_api.c mymall_test.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mymall_test

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	gcc $(OBJECTS) -o $@

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o *~ $(EXECUTABLE)
