#ifndef HB_REV16_CONTROL_H
#define HB_REV16_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
void hb_rev16_acknowledge(void);
bool hb_rev16_prepare(void);
void hb_rev16_bias(bool enabled);
void hb_rev16_heartbeat(void);
extern volatile uint32_t hb_last_reset_flags;
extern volatile uint32_t hb_last_afe_faults;
#endif
