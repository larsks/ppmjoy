#ifndef _MUST_H
#define _MUST_H

#include <stdio.h>
#include <stdlib.h>

// Wraps a system call (or other function) that returns a value < 0 on failure.
// If the system call fails, log a message that includes the file and line
// number and exit.
#define MUST(x, msg) _must(__FILE__, __LINE__, __func__, #x, (x), msg)

void _must(const char *fileName, int lineNumber, const char *funcName,
           const char *calledFunction, int err, char *msg);

#endif // _MUST_H
