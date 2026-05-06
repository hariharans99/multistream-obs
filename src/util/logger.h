#pragma once

#include <obs.h>
#include <string>

// Thin wrapper around blog() with module prefix

#define MLOG_PREFIX "[MultiStream] "

void mlog_info(const char *fmt, ...);
void mlog_warn(const char *fmt, ...);
void mlog_error(const char *fmt, ...);
void mlog_debug(const char *fmt, ...);
