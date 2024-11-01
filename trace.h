// trace.h
// Simple debugging diagnosics.

#ifndef TRACE_H
#define TRACE_H

#include "winapi-util.h"               // WIDE_STRINGIZE

#include <iostream>                    // std::{wcerr, endl}


// Level of diagnostics to print.
//
//   1: API call failures.
//
//   2: Information about messages, etc., of low volume.
//
//   3: Higher-volume messages, e.g., relating to mouse movement.
//
// `wWinMain` sets the default value to 1.
//
extern int g_tracingLevel;


// Write a diagnostic message.
#define TRACE(level, msg)           \
  if (g_tracingLevel >= (level)) {  \
    std::wcerr << msg << std::endl; \
  }

#define TRACE1(msg) TRACE(1, msg)
#define TRACE2(msg) TRACE(2, msg)
#define TRACE3(msg) TRACE(3, msg)

// Add the value of `expr` to a chain of outputs using `<<`.
#define TRVAL(expr) L" " WIDE_STRINGIZE(expr) L"=" << (expr)


#endif // TRACE_H
