EXE = ppmjoy
SRCS = ppm.c must.c config.c cJSON.c
OBJS = $(SRCS:.c=.o)

CFLAGS=-Wno-unused-but-set-variable -Wall -g
LIBS=-lasound

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXE)
