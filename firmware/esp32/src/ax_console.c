#include "ax_console.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "driver/uart_vfs.h"

#include "ax_control.h"
#include "ax_log.h"

static const char *TAG = "AxonConsole";

static void ax_console_print_help(void)
{
    AX_LOG_INFO(TAG, "commands:");
    AX_LOG_INFO(TAG, "  status");
    AX_LOG_INFO(TAG, "  scan on|off");
    AX_LOG_INFO(TAG, "  tx on|off");
    AX_LOG_INFO(TAG, "  fuzz on|off");
    AX_LOG_INFO(TAG, "  fuzz interval <ms>");
    AX_LOG_INFO(TAG, "  log <0-5>  (0=info, 1=v, 2=vv, 3=vvv, 4=vvvv, 5+=debug)");
    AX_LOG_INFO(TAG, "  help");
}

static void ax_console_trim(char *s)
{
    size_t len;

    if (s == NULL)
    {
        return;
    }

    len = strlen(s);
    while (len > 0U && (s[len - 1U] == '\n' || s[len - 1U] == '\r'))
    {
        s[len - 1U] = '\0';
        len--;
    }
}

static int ax_console_parse_on_off(const char *s, bool *out_enabled)
{
    if (s == NULL || out_enabled == NULL)
    {
        return -1;
    }

    if (strcmp(s, "on") == 0)
    {
        *out_enabled = true;
        return 0;
    }
    if (strcmp(s, "off") == 0)
    {
        *out_enabled = false;
        return 0;
    }
    return -2;
}

static void ax_console_task(void *param)
{
    (void)param;

    char line[160];

    ax_console_print_help();

    while (true)
    {
        if (fgets(line, (int)sizeof(line), stdin) == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        ax_console_trim(line);
        if (line[0] == '\0')
        {
            continue;
        }

        char *saveptr = NULL;
        char *cmd = strtok_r(line, " \t", &saveptr);
        if (cmd == NULL)
        {
            continue;
        }

        if (strcmp(cmd, "help") == 0)
        {
            ax_console_print_help();
            continue;
        }

        if (strcmp(cmd, "status") == 0)
        {
            ax_control_status_t st;
            if (ax_control_get_status(&st) != 0)
            {
                AX_LOG_ERROR(TAG, "status unavailable");
                continue;
            }

            AX_LOG_INFO(
                TAG,
                "scan=%s tx=%s fuzz=%s fuzz_val=0x%04X interval_ms=%u log_level=%d",
                st.scan_enabled ? "on" : "off",
                st.adv_enabled ? "on" : "off",
                st.fuzz_enabled ? "on" : "off",
                st.fuzz_value,
                (unsigned)st.fuzz_interval_ms,
                ax_log_get_level()
            );
            continue;
        }

        if (strcmp(cmd, "log") == 0)
        {
            const char *arg = strtok_r(NULL, " \t", &saveptr);
            if (arg == NULL)
            {
                AX_LOG_ERROR(TAG, "usage: log <0-5>");
                continue;
            }

            int verbosity = atoi(arg);
            if (verbosity < 0)
            {
                verbosity = 0;
            }
            ax_log_set_level_from_verbosity(verbosity);
            AX_LOG_INFO(TAG, "log level set (verbosity=%d)", verbosity);
            continue;
        }

        if (strcmp(cmd, "scan") == 0)
        {
            const char *arg = strtok_r(NULL, " \t", &saveptr);
            bool enabled;
            if (ax_console_parse_on_off(arg, &enabled) != 0)
            {
                AX_LOG_ERROR(TAG, "usage: scan on|off");
                continue;
            }
            (void)ax_control_set_scan_enabled(enabled);
            AX_LOG_INFO(TAG, "scan %s requested", enabled ? "on" : "off");
            continue;
        }

        if (strcmp(cmd, "tx") == 0)
        {
            const char *arg = strtok_r(NULL, " \t", &saveptr);
            bool enabled;
            if (ax_console_parse_on_off(arg, &enabled) != 0)
            {
                AX_LOG_ERROR(TAG, "usage: tx on|off");
                continue;
            }
            (void)ax_control_set_adv_enabled(enabled);
            AX_LOG_INFO(TAG, "tx %s requested", enabled ? "on" : "off");
            continue;
        }

        if (strcmp(cmd, "fuzz") == 0)
        {
            const char *sub = strtok_r(NULL, " \t", &saveptr);
            if (sub == NULL)
            {
                AX_LOG_ERROR(TAG, "usage: fuzz on|off OR fuzz interval <ms>");
                continue;
            }

            if (strcmp(sub, "interval") == 0)
            {
                const char *ms_str = strtok_r(NULL, " \t", &saveptr);
                if (ms_str == NULL)
                {
                    AX_LOG_ERROR(TAG, "usage: fuzz interval <ms>");
                    continue;
                }
                uint32_t ms = (uint32_t)strtoul(ms_str, NULL, 10);
                if (ax_control_set_fuzz_interval_ms(ms) != 0)
                {
                    AX_LOG_ERROR(TAG, "invalid interval (min 50ms)");
                    continue;
                }
                AX_LOG_INFO(TAG, "fuzz interval set to %u ms", (unsigned)ms);
                continue;
            }

            bool enabled;
            if (ax_console_parse_on_off(sub, &enabled) != 0)
            {
                AX_LOG_ERROR(TAG, "usage: fuzz on|off OR fuzz interval <ms>");
                continue;
            }
            (void)ax_control_set_fuzz_enabled(enabled);
            AX_LOG_INFO(TAG, "fuzz %s requested", enabled ? "on" : "off");
            continue;
        }

        AX_LOG_WARN(TAG, "unknown command: %s", cmd);
    }
}

int ax_console_start(void)
{
    esp_err_t err;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    err = uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0);
    if (err != ESP_OK)
    {
        AX_LOG_ERROR(TAG, "uart_driver_install failed: %d", (int)err);
        return -1;
    }

    uart_vfs_dev_use_driver(UART_NUM_0);

    if (xTaskCreate(ax_console_task, "ax_console", 4096, NULL, 5, NULL) != pdPASS)
    {
        AX_LOG_ERROR(TAG, "xTaskCreate failed");
        return -2;
    }

    return 0;
}
