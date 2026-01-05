#include "must.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void _must(const char *fileName, int lineNumber, const char *funcName,
           const char *calledFunction, int err, char *msg) {
  if (err < 0) {
    char buf[256];
    snprintf(buf, 256, "%s (%s:%d): %s failed (errno=%d): %s", funcName,
             fileName, lineNumber, calledFunction, errno, msg);
    perror(buf);
    exit(1);
  }
}
