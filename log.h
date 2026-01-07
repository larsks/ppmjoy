#ifndef LOG_H
#define LOG_H

typedef enum {
  LOG_ERROR = 0,
  LOG_WARNING,
  LOG_INFO,
  LOG_DEBUG,
} LOGLEVEL;

void set_log_level(LOGLEVEL level);
void logmsg(LOGLEVEL level, const char *fmt, ...);

#endif // LOG_H
