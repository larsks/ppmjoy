#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "log.h"
#include "termcolor-c.h"

static LOGLEVEL loglevel = LOG_INFO;

typedef struct {
  char *name;
  int value;
} level_map_t;

level_map_t level_names[] = {
    // clang-format off
  {"DEBUG", LOG_DEBUG},
  {"INFO", LOG_INFO},
  {"WARNING", LOG_WARNING},
  {"ERROR", LOG_ERROR},
  {NULL, 0},
    // clang-format on
};

void set_log_level(LOGLEVEL level) { loglevel = level; }

// Return the symbolic name of the given log level.
static char *levelname(int level) {
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

    // print timestamp
    fprintf(stderr, "%s ", timebuf);

    // print (possibly colorized) log level
    switch (level) {
    case LOG_ERROR:
      text_red(stderr);
      break;
    case LOG_INFO:
      text_green(stderr);
      break;
    case LOG_WARNING:
      text_yellow(stderr);
      break;
    default:
      break;
    }
    fprintf(stderr, "%7s ", levelname(level));
    reset_colors(stderr);

    // print log message
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
  }
}
