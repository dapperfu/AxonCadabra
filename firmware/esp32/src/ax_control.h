#ifndef AX_CONTROL_H
#define AX_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ax_control_status
{
    bool scan_enabled;
    bool adv_enabled;
    bool fuzz_enabled;
    uint16_t fuzz_value;
    uint32_t fuzz_interval_ms;
} ax_control_status_t;

int ax_control_init(void);
int ax_control_on_ble_sync(uint8_t addr_type);

int ax_control_set_scan_enabled(bool enabled);
int ax_control_set_adv_enabled(bool enabled);
int ax_control_set_fuzz_enabled(bool enabled);
int ax_control_set_fuzz_interval_ms(uint32_t interval_ms);

int ax_control_get_status(ax_control_status_t *out_status);

#ifdef __cplusplus
}
#endif

#endif /* AX_CONTROL_H */
