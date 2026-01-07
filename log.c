#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"

static LOGLEVEL loglevel = LOG_INFO;

typedef struct {
  char *name;
  int value;
} level_map_t;

level_map_t level_names[] = {
    {"DEBUG", LOG_DEBUG}, {"INFO", LOG_INFO}, {"WARNING", LOG_WARNING},
    {"ERROR", LOG_ERROR}, {NULL, 0},
};

void set_log_level(LOGLEVEL level) { loglevel = level; }

char *levelname(int level) {
  for (int i = 0; level_names[i].name; i++) {
    if (level_names[i].value == level) {
      return level_names[i].name;
    }
  }

  return NULL;
}

void logmsg(LOGLEVEL level, const char *fmt, ...) {
  char timebuf[128];

  if (level <= loglevel) {
    va_list ap;
    time_t now;

    now = time(NULL);
    strftime(timebuf, sizeof(timebuf) - 1, "%Y-%m-%dT%H:%M:%S",
             localtime(&now));

    va_start(ap, fmt);
    fprintf(stderr, "%s %7s ", timebuf, levelname(level));
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
  }
}
