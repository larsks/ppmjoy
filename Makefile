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
