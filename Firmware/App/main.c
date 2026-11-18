/*
 * Rev28 STM32H743 application skeleton.
 * Compile only as part of the board's CubeMX/HAL target, not the native tests.
 * The board adapter owns physical pins, USB/host control and packet transport.
 */
#include "hb/dual_acquisition.h"
#include "hb/hal_port.h"
#include <stdbool.h>
#ifdef HB_BOARD_REV16
#include "hb_rev16_control.h"
#endif

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
    hb_afe_config_t config = hb_ads1299_default_config();
#ifdef HB_BOARD_REV16
    config.sample_rate = HB_AFE_RATE_4000_SPS;
    config.pga_gain = 12;
    if (!hb_rev16_prepare()) {
        hb_board_fault("Rev16 rail or SPI clock configuration failed");
        for (;;) hb_board_idle();
    }
    bool qualifying = false;
    uint32_t qualified_samples = 0;
    int32_t first_baseline[16] = {0};
#endif
    if (hb_dual_acquisition_init(&g_acquisition, port1, port2, &config,
                                 packet_sink_adapter, NULL) != HB_AFE_OK) {
        hb_board_fault("Both ADS1299 devices must pass startup/identity/configuration before acquisition");
        for (;;) { hb_board_service(); hb_board_idle(); }
    }

    /* No automatic acquisition on boot. Start requires an explicit control request. */
    for (;;) {
        hb_board_service();
        if (hb_board_stop_requested()) {
#ifdef HB_BOARD_REV16
            hb_rev16_bias(false);
            qualifying = false;
#endif
            (void)hb_board_start_requested();
            if (!g_acquisition.running) continue;
            if (hb_dual_acquisition_stop(&g_acquisition) != HB_AFE_OK)
                hb_board_fault("AFE stop command failed");
        }
        if (hb_board_start_requested() && !g_acquisition.running && !g_acquisition.faulted) {
#ifdef HB_BOARD_REV16
            hb_rev16_bias(true);
            qualifying = true;
            qualified_samples = 0;
#endif
            if (hb_dual_acquisition_start(&g_acquisition) != HB_AFE_OK) {
#ifdef HB_BOARD_REV16
                hb_rev16_bias(false);
#endif
                hb_board_fault("AFE start command failed");
            }
        }
        if (g_acquisition.running) {
#ifdef HB_BOARD_REV16
            if (qualifying) {
                hb_dual_ads1299_sample_t sample;
                if (hb_dual_ads1299_read(&g_acquisition.dual, &sample, 1) != HB_AFE_OK) {
                    hb_rev16_bias(false);
                    (void)hb_dual_acquisition_stop(&g_acquisition);
                    g_acquisition.faulted = true;
                    hb_board_fault("Baseline acquisition failed");
                    continue;
                }
                bool in_range = true;
                for (unsigned ch = 0; ch < 16; ++ch) {
                    if (!qualified_samples) first_baseline[ch] = sample.channel[ch];
                    int32_t drift = sample.channel[ch] - first_baseline[ch];
                    if (drift > 223696 || drift < -223696) in_range = false; /* 10 mV */
                    if (sample.channel[ch] > 4194304 || sample.channel[ch] < -4194304)
                        in_range = false;
                }
                qualified_samples = in_range ? qualified_samples + 1 : 0;
                qualifying = qualified_samples < 240000u;
                continue;
            }
#endif
            const hb_afe_result_t result = hb_dual_acquisition_poll(&g_acquisition, 3u);
            if (result != HB_AFE_OK) {
#ifdef HB_BOARD_REV16
                hb_rev16_bias(false);
#endif
                hb_board_fault("AFE or packet transport fault; acquisition stopped");
            }
        } else {
            hb_board_idle();
        }
    }
}
