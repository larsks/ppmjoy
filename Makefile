EXE = ppmjoy
SRCS = main.c must.c config.c vendor/cJSON/cJSON.c event.c
OBJS = $(SRCS:.c=.o)

CPPFLAGS=-Ivendor/cJSON
CFLAGS=-Wall -g
LIBS=-lasound

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXE)

# Generated w/ `gcc -MM *.c`
config.o: config.c config.h must.h vendor/cJSON/cJSON.h
event.o: event.c event.h config.h
main.o: main.c config.h event.h must.h
must.o: must.c must.h
