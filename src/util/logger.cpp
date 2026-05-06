#include "logger.h"
#include <obs.h>
#include <cstdarg>
#include <cstdio>

static void mlog_impl(int log_level, const char *fmt, va_list args)
{
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    blog(log_level, MLOG_PREFIX "%s", buf);
}

void mlog_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    mlog_impl(LOG_INFO, fmt, args);
    va_end(args);
}

void mlog_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    mlog_impl(LOG_WARNING, fmt, args);
    va_end(args);
}

void mlog_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    mlog_impl(LOG_ERROR, fmt, args);
    va_end(args);
}

void mlog_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    mlog_impl(LOG_DEBUG, fmt, args);
    va_end(args);
}
