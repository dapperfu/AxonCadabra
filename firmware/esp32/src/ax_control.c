#include "ax_control.h"

#include <string.h>

#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "ax_log.h"

static const char *TAG = "AxonCadabra";

static const char AXON_OUI_STR[] = "00:25:DF";

static const uint16_t AXON_SVC_UUID16 = 0xFE6C;

static const uint8_t AXON_BASE_PAYLOAD[24] = {
    0x01, 0x58, 0x38, 0x37, 0x30, 0x30, 0x32, 0x46, 0x50, 0x34, 0x01, 0x02,
    0x00, 0x00, 0x00, 0x00, 0xCE, 0x1B, 0x33, 0x00, 0x00, 0x02, 0x00, 0x00,
};

static uint8_t g_addr_type = BLE_ADDR_RANDOM;

static volatile bool g_scan_enabled = true;
static volatile bool g_adv_enabled = true;
static volatile bool g_fuzz_enabled = false;
static volatile uint32_t g_fuzz_interval_ms = 500U;

static bool g_scan_active = false;
static bool g_adv_active = false;

static uint16_t g_fuzz_value = 0U;
static uint8_t g_payload[24];

static struct ble_npl_eventq *g_eventq = NULL;
static struct ble_npl_event g_apply_event;
static struct ble_npl_callout g_fuzz_callout;

static int ax_gap_event(struct ble_gap_event *event, void *arg);

static void ax_payload_apply_fuzz(void)
{
    (void)memcpy(g_payload, AXON_BASE_PAYLOAD, sizeof(g_payload));

    g_fuzz_value = (uint16_t)((g_fuzz_value + 1U) & 0xFFFFU);

    g_payload[10] = (uint8_t)((g_fuzz_value >> 8) & 0xFFU);
    g_payload[11] = (uint8_t)(g_fuzz_value & 0xFFU);
    g_payload[20] = (uint8_t)((g_fuzz_value >> 4) & 0xFFU);
    g_payload[21] = (uint8_t)((g_fuzz_value << 4) & 0xFFU);
}

static int ax_adv_set_data(void)
{
    /*
     * Legacy ADV payload (31 bytes total):
     * - Flags (0x01): 0x06
     * - Service Data (0x16): UUID16 (LE) + 24-byte payload
     */
    uint8_t adv[31];
    size_t i = 0U;

    adv[i++] = 0x02; /* len */
    adv[i++] = 0x01; /* type: Flags */
    adv[i++] = 0x06; /* LE General Discoverable | BR/EDR Not Supported */

    adv[i++] = 0x1B; /* len: 1(type) + 2(uuid) + 24(payload) = 27 */
    adv[i++] = 0x16; /* type: Service Data - 16-bit UUID */
    adv[i++] = (uint8_t)(AXON_SVC_UUID16 & 0xFF);
    adv[i++] = (uint8_t)((AXON_SVC_UUID16 >> 8) & 0xFF);
    (void)memcpy(&adv[i], g_payload, sizeof(g_payload));
    i += sizeof(g_payload);

    if (i != sizeof(adv))
    {
        AX_LOG_ERROR(TAG, "adv size mismatch: %u", (unsigned)i);
        return -1;
    }

    if (ble_gap_adv_set_data(adv, (int)i) != 0)
    {
        AX_LOG_ERROR(TAG, "ble_gap_adv_set_data failed");
        return -2;
    }

    return 0;
}

static int ax_adv_start(void)
{
    struct ble_gap_adv_params params;
    int rc;

    rc = ax_adv_set_data();
    if (rc != 0)
    {
        return rc;
    }

    (void)memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(g_addr_type, NULL, BLE_HS_FOREVER, &params, ax_gap_event, NULL);
    if (rc != 0)
    {
        AX_LOG_ERROR(TAG, "ble_gap_adv_start failed: %d", rc);
        return -3;
    }

    g_adv_active = true;
    AX_LOG_INFO(TAG, "advertising started");
    return 0;
}

static int ax_adv_stop(void)
{
    int rc;

    rc = ble_gap_adv_stop();
    if (rc != 0)
    {
        return -1;
    }

    g_adv_active = false;
    AX_LOG_INFO(TAG, "advertising stopped");
    return 0;
}

static int ax_scan_start(void)
{
    struct ble_gap_disc_params params;
    int rc;

    (void)memset(&params, 0, sizeof(params));
    params.passive = 0;
    params.itvl = 0x0010;
    params.window = 0x0010;
    params.filter_duplicates = 0;

    rc = ble_gap_disc(g_addr_type, BLE_HS_FOREVER, &params, ax_gap_event, NULL);
    if (rc != 0)
    {
        AX_LOG_ERROR(TAG, "ble_gap_disc failed: %d", rc);
        return -1;
    }

    g_scan_active = true;
    AX_LOG_INFO(TAG, "scanning started (OUI %s)", AXON_OUI_STR);
    return 0;
}

static int ax_scan_stop(void)
{
    int rc;

    rc = ble_gap_disc_cancel();
    if (rc != 0)
    {
        return -1;
    }

    g_scan_active = false;
    AX_LOG_INFO(TAG, "scanning stopped");
    return 0;
}

static void ax_fuzz_cb(struct ble_npl_event *ev)
{
    (void)ev;

    if (!g_fuzz_enabled || !g_adv_enabled)
    {
        (void)ble_npl_callout_stop(&g_fuzz_callout);
        return;
    }

    ax_payload_apply_fuzz();

    if (g_adv_active)
    {
        (void)ax_adv_stop();
    }
    (void)ax_adv_start();

    (void)ble_npl_callout_reset(&g_fuzz_callout, ble_npl_time_ms_to_ticks32(g_fuzz_interval_ms));
}

static void ax_apply_state(struct ble_npl_event *ev)
{
    (void)ev;

    if (g_adv_enabled)
    {
        if (g_adv_active)
        {
            (void)ax_adv_stop();
        }
        (void)ax_adv_start();
    }
    else if (g_adv_active)
    {
        (void)ax_adv_stop();
    }

    if (g_scan_enabled)
    {
        if (!g_scan_active)
        {
            (void)ax_scan_start();
        }
    }
    else if (g_scan_active)
    {
        (void)ax_scan_stop();
    }

    if (g_fuzz_enabled && g_adv_enabled)
    {
        (void)ble_npl_callout_reset(&g_fuzz_callout, ble_npl_time_ms_to_ticks32(g_fuzz_interval_ms));
    }
    else
    {
        (void)ble_npl_callout_stop(&g_fuzz_callout);
    }
}

static int ax_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0)
            {
                AX_LOG_INFO(TAG, "central connected; terminating (no GATT)");
                (void)ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
            else
            {
                AX_LOG_WARN(TAG, "connect failed: %d", event->connect.status);
            }
            return 0;

        case BLE_GAP_EVENT_DISC:
        {
            char addr_str[BLE_ADDR_STR_LEN];
            (void)ble_addr_to_str(&event->disc.addr, addr_str);

            if (strncmp(addr_str, AXON_OUI_STR, 8) == 0)
            {
                AX_LOG_INFO(TAG, "found target %s rssi=%d", addr_str, event->disc.rssi);
            }
            return 0;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE:
            g_scan_active = false;
            if (g_scan_enabled)
            {
                (void)ax_scan_start();
            }
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            g_adv_active = false;
            if (g_adv_enabled)
            {
                (void)ax_adv_start();
            }
            return 0;

        default:
            return 0;
    }
}

int ax_control_init(void)
{
    (void)memcpy(g_payload, AXON_BASE_PAYLOAD, sizeof(g_payload));

    g_eventq = nimble_port_get_dflt_eventq();
    if (g_eventq == NULL)
    {
        return -1;
    }

    ble_npl_event_init(&g_apply_event, ax_apply_state, NULL);
    ble_npl_callout_init(&g_fuzz_callout, g_eventq, ax_fuzz_cb, NULL);

    return 0;
}

int ax_control_on_ble_sync(uint8_t addr_type)
{
    g_addr_type = addr_type;

    AX_LOG_INFO(TAG, "BLE host synced; addr_type=%u", (unsigned)g_addr_type);

    /*
     * Ensure the apply event runs in the NimBLE event queue context.
     */
    ble_npl_eventq_put(g_eventq, &g_apply_event);
    return 0;
}

int ax_control_set_scan_enabled(bool enabled)
{
    g_scan_enabled = enabled;
    ble_npl_eventq_put(g_eventq, &g_apply_event);
    return 0;
}

int ax_control_set_adv_enabled(bool enabled)
{
    g_adv_enabled = enabled;
    ble_npl_eventq_put(g_eventq, &g_apply_event);
    return 0;
}

int ax_control_set_fuzz_enabled(bool enabled)
{
    g_fuzz_enabled = enabled;
    ble_npl_eventq_put(g_eventq, &g_apply_event);
    return 0;
}

int ax_control_set_fuzz_interval_ms(uint32_t interval_ms)
{
    if (interval_ms < 50U)
    {
        return -1;
    }
    g_fuzz_interval_ms = interval_ms;
    ble_npl_eventq_put(g_eventq, &g_apply_event);
    return 0;
}

int ax_control_get_status(ax_control_status_t *out_status)
{
    if (out_status == NULL)
    {
        return -1;
    }

    out_status->scan_enabled = g_scan_enabled;
    out_status->adv_enabled = g_adv_enabled;
    out_status->fuzz_enabled = g_fuzz_enabled;
    out_status->fuzz_value = g_fuzz_value;
    out_status->fuzz_interval_ms = g_fuzz_interval_ms;
    return 0;
}
