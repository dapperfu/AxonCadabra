#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_err.h"
#include "esp_nimble_hci.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "ax_log.h"

static const char *TAG = "AxonCadabra";

static const uint8_t AXON_OUI_STR[] = "00:25:DF";

static const uint16_t AXON_SVC_UUID16 = 0xFE6C;

static const uint8_t AXON_BASE_PAYLOAD[24] = {
    0x01, 0x58, 0x38, 0x37, 0x30, 0x30, 0x32, 0x46, 0x50, 0x34, 0x01, 0x02,
    0x00, 0x00, 0x00, 0x00, 0xCE, 0x1B, 0x33, 0x00, 0x00, 0x02, 0x00, 0x00,
};

static uint8_t g_addr_type = BLE_ADDR_RANDOM;
static uint8_t g_payload[24];
static uint16_t g_fuzz_value = 0U;

static int ax_gap_event(struct ble_gap_event *event, void *arg);

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

    AX_LOG_INFO(TAG, "advertising started");
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

    AX_LOG_INFO(TAG, "scanning started (OUI %s)", (const char *)AXON_OUI_STR);
    return 0;
}

static void ax_payload_apply_fuzz(void)
{
    (void)memcpy(g_payload, AXON_BASE_PAYLOAD, sizeof(g_payload));

    g_fuzz_value = (uint16_t)((g_fuzz_value + 1U) & 0xFFFFU);

    g_payload[10] = (uint8_t)((g_fuzz_value >> 8) & 0xFFU);
    g_payload[11] = (uint8_t)(g_fuzz_value & 0xFFU);
    g_payload[20] = (uint8_t)((g_fuzz_value >> 4) & 0xFFU);
    g_payload[21] = (uint8_t)((g_fuzz_value << 4) & 0xFFU);
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

            if (strncmp(addr_str, (const char *)AXON_OUI_STR, 8) == 0)
            {
                AX_LOG_INFO(TAG, "found target %s rssi=%d", addr_str, event->disc.rssi);
            }
            return 0;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE:
            AX_LOG_WARN(TAG, "scan complete; restarting");
            (void)ax_scan_start();
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            AX_LOG_WARN(TAG, "advertise complete; restarting");
            (void)ax_adv_start();
            return 0;

        default:
            return 0;
    }
}

static void ax_on_sync(void)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &g_addr_type);
    if (rc != 0)
    {
        AX_LOG_ERROR(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    (void)memcpy(g_payload, AXON_BASE_PAYLOAD, sizeof(g_payload));

    AX_LOG_INFO(TAG, "BLE host synced; addr_type=%u", (unsigned)g_addr_type);

    (void)ax_adv_start();
    (void)ax_scan_start();
}

static void ax_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    esp_err_t err;

    ax_log_set_level(AX_LOG_LEVEL_INFO);

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            AX_LOG_ERROR(TAG, "nvs_flash_erase failed: %d", (int)err);
            return;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        AX_LOG_ERROR(TAG, "nvs_flash_init failed: %d", (int)err);
        return;
    }

    err = esp_nimble_hci_and_controller_init();
    if (err != ESP_OK)
    {
        AX_LOG_ERROR(TAG, "esp_nimble_hci_and_controller_init failed: %d", (int)err);
        return;
    }

    nimble_port_init();

    ble_hs_cfg.sync_cb = ax_on_sync;

    nimble_port_freertos_init(ax_host_task);

    /*
     * Remaining application behavior (fuzz loop, UART commands) is added in
     * subsequent commits.
     */
    vTaskDelete(NULL);
}
