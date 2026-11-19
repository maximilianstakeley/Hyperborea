#ifndef HB_DUAL_ADS1299_H
#define HB_DUAL_ADS1299_H
#include "hb/ads1299.h"
#ifdef __cplusplus
extern "C" {
#endif
#define HB_DUAL_AFE_CHANNELS 16u
typedef struct {
    uint32_t timestamp_us;
    uint32_t status24[2];
    int32_t channel[HB_DUAL_AFE_CHANNELS];
} hb_dual_ads1299_sample_t;
typedef struct {
    hb_ads1299_t afe[2];
    bool initialized;
    bool streaming;
} hb_dual_ads1299_t;
/* The two ports must use distinct CS and DRDY signals. RESET and PWDN may be shared. Both
 * START callbacks must address the same physical START net and GPIO context.
 * Both devices must receive the same external 2.048 MHz clock. */
hb_afe_result_t hb_dual_ads1299_init(hb_dual_ads1299_t *dual,
    hb_hal_port_t *afe1_port, hb_hal_port_t *afe2_port,
    const hb_afe_config_t *config);
hb_afe_result_t hb_dual_ads1299_start(hb_dual_ads1299_t *dual);
hb_afe_result_t hb_dual_ads1299_stop(hb_dual_ads1299_t *dual);
hb_afe_result_t hb_dual_ads1299_read(hb_dual_ads1299_t *dual,
    hb_dual_ads1299_sample_t *sample, uint32_t timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
