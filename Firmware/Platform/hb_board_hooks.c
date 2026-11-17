/*
 * Rev28 board glue. The CubeMX project must provide hb_board_hw_init(), which
 * configures clocks, GPIO safe states, SPI4, QSPI and USB Device CDC middleware.
 * The board must also provide rail monitoring, VCAP1 readiness and fault UI.
 */
#include "main.h"
#include "hb/acquisition.h"
#include "hb/w25q128.h"
#include "hb_stm32h7_port.h"
#include "hb_usb_cdc_bridge.h"

extern bool hb_board_hw_init(void);
extern void hb_board_fault(const char *message);

static hb_hal_port_t g_afe1_port;
static hb_hal_port_t g_afe2_port;
static hb_flash_port_t g_flash_port;
static hb_w25q128_t g_flash;
static bool g_flash_initialized;

bool hb_board_system_init(void) {
    if (!hb_board_hw_init()) return false;
    g_afe1_port = hb_stm32h7_make_afe1_port();
    g_afe2_port = hb_stm32h7_make_afe2_port();
    hb_usb_cdc_bridge_init();
    return true;
}

hb_hal_port_t *hb_board_afe1_port(void) { return &g_afe1_port; }
hb_hal_port_t *hb_board_afe2_port(void) { return &g_afe2_port; }
bool hb_board_packet_sink(void *context, const uint8_t *packet, size_t length) {
    return hb_usb_cdc_packet_sink(context, packet, length);
}
bool hb_board_start_requested(void) { return hb_usb_cdc_take_start_request(); }
bool hb_board_stop_requested(void) { return hb_usb_cdc_take_stop_request(); }
void hb_board_service(void) { hb_usb_cdc_service_tx(); }
void hb_board_idle(void) { __WFI(); }

/* Optional storage initialization. Call from a lower-priority task only after
   CubeMX QSPI setup; this does not start recording or erase user data. */
bool hb_board_flash_init(void) {
    g_flash_port = hb_stm32h7_make_flash_port();
    g_flash_initialized = hb_w25q128_init(&g_flash, &g_flash_port) == HB_FLASH_OK;
    return g_flash_initialized;
}
hb_w25q128_t *hb_board_flash(void) {
    return g_flash_initialized ? &g_flash : NULL;
}

