/*
 * Rev28 STM32H743 application skeleton.
 * Compile only as part of the board's CubeMX/HAL target, not the native tests.
 * The board adapter owns physical pins, USB/host control and packet transport.
 */
#include "hb/dual_acquisition.h"
#include "hb/hal_port.h"
#include <stdbool.h>

extern bool hb_board_system_init(void); /* HAL, clocks, GPIO, SPI, watchdog, etc. */
extern hb_hal_port_t *hb_board_afe1_port(void);
extern hb_hal_port_t *hb_board_afe2_port(void);
extern bool hb_board_packet_sink(void *context, const uint8_t *packet, size_t length);
extern bool hb_board_start_requested(void); /* host command or approved local control */
extern bool hb_board_stop_requested(void);
extern void hb_board_service(void);          /* USB stack, watchdog and command service */
extern void hb_board_fault(const char *message);
extern void hb_board_idle(void);

static hb_dual_acquisition_t g_acquisition;

static bool packet_sink_adapter(void *context, const uint8_t *packet, size_t length) {
    return hb_board_packet_sink(context, packet, length);
}

int main(void) {
    if (!hb_board_system_init()) {
        hb_board_fault("Board initialization failed; AFE acquisition inhibited");
        for (;;) hb_board_idle();
    }
    hb_hal_port_t *port1 = hb_board_afe1_port();
    hb_hal_port_t *port2 = hb_board_afe2_port();
    const hb_afe_config_t config = hb_ads1299_default_config();
    if (hb_dual_acquisition_init(&g_acquisition, port1, port2, &config,
                                 packet_sink_adapter, NULL) != HB_AFE_OK) {
        hb_board_fault("Both ADS1299 devices must pass startup/identity/configuration before acquisition");
        for (;;) { hb_board_service(); hb_board_idle(); }
    }

    /* No automatic acquisition on boot. Start requires an explicit control request. */
    for (;;) {
        hb_board_service();
        if (hb_board_stop_requested() && g_acquisition.running) {
            if (hb_dual_acquisition_stop(&g_acquisition) != HB_AFE_OK)
                hb_board_fault("AFE stop command failed");
        }
        if (hb_board_start_requested() && !g_acquisition.running && !g_acquisition.faulted) {
            if (hb_dual_acquisition_start(&g_acquisition) != HB_AFE_OK)
                hb_board_fault("AFE start command failed");
        }
        if (g_acquisition.running) {
            const hb_afe_result_t result = hb_dual_acquisition_poll(&g_acquisition, 3u);
            if (result != HB_AFE_OK)
                hb_board_fault("AFE or packet transport fault; acquisition stopped");
        } else {
            hb_board_idle();
        }
    }
}
