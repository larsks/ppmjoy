EXE = ppmjoy
SRCS = main.c must.c config.c cJSON.c event.c
OBJS = $(SRCS:.c=.o)

CFLAGS=-Wall -g
LIBS=-lasound

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXE)

# Generated w/ `gcc -MM *.c`
cJSON.o: cJSON.c cJSON.h
config.o: config.c config.h cJSON.h must.h
event.o: event.c event.h config.h
main.o: main.c config.h event.h must.h
must.o: must.c must.h
