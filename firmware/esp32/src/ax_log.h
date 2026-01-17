#ifndef AX_LOG_H
#define AX_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Custom logging levels (required by .cursor/rules/c/logging-levels.mdc)
 *
 * Higher number means less verbose (more important).
 * g_ax_log_level is the minimum level that will be emitted.
 */
#define AX_LOG_LEVEL_ERROR   30
#define AX_LOG_LEVEL_WARN    25
#define AX_LOG_LEVEL_INFO    20
#define AX_LOG_LEVEL_VERBOSE 18
#define AX_LOG_LEVEL_DETAIL  16
#define AX_LOG_LEVEL_TRACE   14
#define AX_LOG_LEVEL_FINE    12
#define AX_LOG_LEVEL_DEBUG   10

extern int g_ax_log_level;

void ax_log_set_level(int level);
int ax_log_get_level(void);
void ax_log_set_level_from_verbosity(int verbosity);

void ax_log_message(int level, const char *tag, const char *file, int line, const char *fmt, ...);

#define AX_LOG_ERROR(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_WARN(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_INFO(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_VERBOSE(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_VERBOSE, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_DETAIL(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_DETAIL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_TRACE(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_TRACE, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_FINE(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_FINE, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AX_LOG_DEBUG(tag, fmt, ...) \
    ax_log_message(AX_LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AX_LOG_H */
