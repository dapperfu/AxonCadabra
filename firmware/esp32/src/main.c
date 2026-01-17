#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_err.h"
#include "esp_nimble_hci.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "ax_control.h"
#include "ax_log.h"

static const char *TAG = "AxonCadabra";

static uint8_t g_addr_type = BLE_ADDR_RANDOM;
static void ax_on_sync(void)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &g_addr_type);
    if (rc != 0)
    {
        AX_LOG_ERROR(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }
    (void)ax_control_on_ble_sync(g_addr_type);
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
    int rc;

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

    rc = ax_control_init();
    if (rc != 0)
    {
        AX_LOG_ERROR(TAG, "ax_control_init failed: %d", rc);
        return;
    }

    ble_hs_cfg.sync_cb = ax_on_sync;

    nimble_port_freertos_init(ax_host_task);

    vTaskDelete(NULL);
}
