#ifndef LOG_H
#define LOG_H

typedef enum {
  LOG_ERROR = 0,
  LOG_WARNING,
  LOG_INFO,
  LOG_DEBUG,
} LOGLEVEL;

// Set the log level. Log messages will only be printed if they are
// of the same or greater priority.
void set_log_level(LOGLEVEL level);

// Log a message to the console. The message will only be printed
// if the level is the same or greater priority than the global
// loglevel variable.
void logmsg(LOGLEVEL level, const char *fmt, ...);

#endif // LOG_H
