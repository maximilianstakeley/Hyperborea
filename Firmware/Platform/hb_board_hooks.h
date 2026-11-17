#ifndef HB_BOARD_HOOKS_H
#define HB_BOARD_HOOKS_H
#include "hb/w25q128.h"
#include <stdbool.h>
bool hb_board_system_init(void);
bool hb_board_flash_init(void);
hb_w25q128_t *hb_board_flash(void);
#endif
