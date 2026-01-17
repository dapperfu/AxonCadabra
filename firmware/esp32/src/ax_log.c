#include "ax_log.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

int g_ax_log_level = AX_LOG_LEVEL_INFO;

static esp_log_level_t ax_map_level_to_esp(int level)
{
    if (level >= AX_LOG_LEVEL_ERROR)
    {
        return ESP_LOG_ERROR;
    }
    if (level >= AX_LOG_LEVEL_WARN)
    {
        return ESP_LOG_WARN;
    }
    if (level >= AX_LOG_LEVEL_INFO)
    {
        return ESP_LOG_INFO;
    }
    /*
     * Custom verbosity levels (VERBOSE/DETAIL/TRACE/FINE/DEBUG) are mapped to
     * ESP_LOG_DEBUG; filtering is handled by g_ax_log_level.
     */
    return ESP_LOG_DEBUG;
}

void ax_log_set_level(int level)
{
    g_ax_log_level = level;
}

int ax_log_get_level(void)
{
    return g_ax_log_level;
}

void ax_log_set_level_from_verbosity(int verbosity)
{
    switch (verbosity)
    {
        case 0:
            g_ax_log_level = AX_LOG_LEVEL_INFO;
            break;
        case 1:
            g_ax_log_level = AX_LOG_LEVEL_VERBOSE;
            break;
        case 2:
            g_ax_log_level = AX_LOG_LEVEL_DETAIL;
            break;
        case 3:
            g_ax_log_level = AX_LOG_LEVEL_TRACE;
            break;
        case 4:
            g_ax_log_level = AX_LOG_LEVEL_FINE;
            break;
        default:
            g_ax_log_level = AX_LOG_LEVEL_DEBUG;
            break;
    }
}

void ax_log_message(int level, const char *tag, const char *file, int line, const char *fmt, ...)
{
    if (level < g_ax_log_level)
    {
        return;
    }

    /* Trim file path to basename for readability */
    const char *basename = file;
    const char *slash = strrchr(file, '/');
    if (slash != NULL && *(slash + 1) != '\0')
    {
        basename = slash + 1;
    }

    char prefix[96];
    (void)snprintf(prefix, sizeof(prefix), "%s:%d: ", basename, line);

    char msg[256];
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char out[352];
    (void)snprintf(out, sizeof(out), "%s%s", prefix, msg);

    esp_log_write(ax_map_level_to_esp(level), tag, "%s", out);
}
