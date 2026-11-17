#ifndef HB_STM32H7_PORT_H
#define HB_STM32H7_PORT_H
#include "hb/hal_port.h"
#include "hb/w25q128.h"
hb_hal_port_t hb_stm32h7_make_afe_port(void);
hb_hal_port_t hb_stm32h7_make_afe1_port(void);
hb_hal_port_t hb_stm32h7_make_afe2_port(void);
hb_flash_port_t hb_stm32h7_make_flash_port(void);
#endif
