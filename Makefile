EXE = ppmjoy
SRCS = main.c must.c config.c cJSON.c
OBJS = $(SRCS:.c=.o)

CFLAGS=-Wno-unused-but-set-variable -Wall -g
LIBS=-lasound

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXE)

# Generated w/ `gcc -MM *.c`
cJSON.o: cJSON.c cJSON.h
config.o: config.c config.h cJSON.h must.h
main.o: main.c config.h must.h
must.o: must.c must.h
