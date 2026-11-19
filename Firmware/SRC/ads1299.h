#ifndef HB_ADS1299_H
#define HB_ADS1299_H
#include "hb/hal_port.h"
#ifdef __cplusplus
extern "C" {
#endif

#define HB_ADS1299_CHANNELS 8u
#define HB_ADS1299_FRAME_BYTES 27u

typedef enum {
    HB_AFE_OK = 0,
    HB_AFE_ERR_ARGUMENT,
    HB_AFE_ERR_RAILS,
    HB_AFE_ERR_SPI,
    HB_AFE_ERR_ID,
    HB_AFE_ERR_TIMEOUT,
    HB_AFE_ERR_STATE,
    HB_AFE_ERR_REGISTER_VERIFY
} hb_afe_result_t;

typedef enum {
    HB_AFE_RATE_250_SPS = 250,
    HB_AFE_RATE_500_SPS = 500,
    HB_AFE_RATE_1000_SPS = 1000,
    HB_AFE_RATE_2000_SPS = 2000,
    HB_AFE_RATE_4000_SPS = 4000,
    HB_AFE_RATE_8000_SPS = 8000,
    HB_AFE_RATE_16000_SPS = 16000
} hb_afe_rate_t;

typedef struct {
    hb_afe_rate_t sample_rate;
    uint8_t pga_gain; /* Supported ADS1299 gains: 1,2,4,6,8,12,24. */
    bool enable_internal_reference_buffer;
    uint16_t reference_settle_ms; /* conservative startup hold; verify on hardware */
    uint16_t vcap1_timeout_ms;
} hb_afe_config_t;

typedef struct {
    uint32_t timestamp_ms;
    uint32_t status24;
    int32_t channel[HB_ADS1299_CHANNELS];
} hb_ads1299_sample_t;

typedef struct {
    hb_hal_port_t *port;
    hb_afe_config_t config;
    uint8_t device_id;
    bool initialized;
    bool continuous_read;
    bool streaming;
} hb_ads1299_t;

/* Configure a device after coordinated hardware reset of both banks. */
hb_afe_result_t hb_ads1299_init_released(hb_ads1299_t *, hb_hal_port_t *, const hb_afe_config_t *);
hb_afe_result_t hb_ads1299_read_ready_frame(hb_ads1299_t *, hb_ads1299_sample_t *);
hb_afe_config_t hb_ads1299_default_config(void);
hb_afe_result_t hb_ads1299_init(hb_ads1299_t *afe, hb_hal_port_t *port,
                                const hb_afe_config_t *config);
hb_afe_result_t hb_ads1299_start(hb_ads1299_t *afe);
hb_afe_result_t hb_ads1299_stop(hb_ads1299_t *afe);
hb_afe_result_t hb_ads1299_read_register(hb_ads1299_t *afe, uint8_t address,
                                         uint8_t *value);
hb_afe_result_t hb_ads1299_write_register(hb_ads1299_t *afe, uint8_t address,
                                          uint8_t value);
hb_afe_result_t hb_ads1299_read_sample(hb_ads1299_t *afe,
                                       hb_ads1299_sample_t *sample,
                                       uint32_t timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
