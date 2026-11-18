#include "hb/acquisition.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t regs[0x18];
    uint8_t last_command;
    uint32_t tick;
    uint32_t delayed_ms;
    bool rails;
    bool drdy;
    bool saw_chip_select;
    bool reset_asserted;
    bool pwdn_asserted;
    bool start_inactive;
    bool start_active;
    bool cs_asserted;
    unsigned packets;
    uint8_t last_packet[HB_PACKET_BYTES];
} mock_t;

static bool mock_spi(void *ctx, const uint8_t *tx, uint8_t *rx, size_t n, uint32_t timeout) {
    mock_t *m=(mock_t*)ctx; (void)timeout;
    if (n==1u) { m->last_command=tx[0]; if (rx) rx[0]=0; return true; }
    if (n==3u && (tx[0]&0xE0u)==0x20u) {
        if (rx) { rx[0]=0; rx[1]=0; rx[2]=m->regs[tx[0]&0x1Fu]; }
        return true;
    }
    if (n==3u && (tx[0]&0xE0u)==0x40u) {
        m->regs[tx[0]&0x1Fu]=tx[2]; return true;
    }
    if (n==HB_ADS1299_FRAME_BYTES) {
        if (!rx) return false;
        memset(rx,0,n);
        rx[0]=0xC0; rx[1]=0x12; rx[2]=0x34;
        /* CH1 positive 0x123456; CH2 negative -2 (0xFFFFFE). */
        rx[3]=0x12; rx[4]=0x34; rx[5]=0x56;
        rx[6]=0xFF; rx[7]=0xFF; rx[8]=0xFE;
        return true;
    }
    return false;
}
static void mock_cs(void *ctx, bool asserted) { mock_t *m=(mock_t*)ctx; m->saw_chip_select |= asserted; m->cs_asserted=asserted; }
static void mock_reset(void *ctx, bool asserted) { ((mock_t*)ctx)->reset_asserted=asserted; }
static void mock_pwdn(void *ctx, bool asserted) { ((mock_t*)ctx)->pwdn_asserted=asserted; }
static bool mock_drdy(void *ctx) { return ((mock_t*)ctx)->drdy; }
static bool mock_rails(void *ctx) { return ((mock_t*)ctx)->rails; }
static bool mock_vcap(void *ctx) { (void)ctx; return true; }
static void mock_set_start(void *ctx, bool active) { mock_t *m=(mock_t*)ctx; m->start_active=active; m->start_inactive=!active; }
static void mock_delay(void *ctx, uint32_t ms) { mock_t *m=(mock_t*)ctx; m->delayed_ms += ms; m->tick += ms; }
static uint32_t mock_millis(void *ctx) { return ((mock_t*)ctx)->tick; }
static uint32_t mock_micros(void *ctx) { return ((mock_t*)ctx)->tick * 1000u; }
static void mock_idle(void *ctx) { ((mock_t*)ctx)->tick++; }
static void mock_fault(void *ctx, const char *message) { (void)ctx; (void)message; }
static bool mock_sink(void *ctx, const uint8_t *p, size_t n) {
    mock_t *m=(mock_t*)ctx; if (n!=HB_PACKET_BYTES) return false;
    memcpy(m->last_packet,p,n); m->packets++; return true;
}

int main(void) {
    mock_t mock={0}; mock.regs[0]=0x1E; mock.rails=true; mock.drdy=true;
    hb_hal_port_t port={&mock,mock_spi,mock_cs,mock_reset,mock_pwdn,mock_drdy,
                        mock_rails,mock_vcap,mock_set_start,mock_delay,
                        mock_millis,mock_idle,mock_fault,mock_micros,NULL};
    hb_acquisition_t acq;
    hb_afe_config_t cfg=hb_ads1299_default_config();
    assert(hb_acquisition_init(&acq,&port,&cfg,mock_sink,&mock)==HB_AFE_OK);
    assert(acq.afe.initialized);
    assert(mock.delayed_ms >= 280u); /* POR + conservative internal-reference settle */
    assert(mock.regs[0x01]==0x94); /* HR=1, DAISY_EN=0, 1 kSPS */
    assert((mock.regs[0x01] & 0x40u)==0u); /* independent-CS standard topology */
    assert(mock.regs[0x02]==0xC0);
    assert(mock.regs[0x03]==0xE0); /* reference buffer on; bias remains off */
    for (unsigned i=0;i<8;i++) assert(mock.regs[0x05+i]==0x60);
    /* Verify the three requested rates at the register level. */
    const hb_afe_rate_t rates[] = {HB_AFE_RATE_250_SPS, HB_AFE_RATE_500_SPS, HB_AFE_RATE_1000_SPS};
    const uint8_t expected_config1[] = {0x96u, 0x95u, 0x94u};
    for (unsigned i = 0; i < 3u; ++i) {
        mock_t rate_mock = {0}; rate_mock.regs[0] = 0x1Eu; rate_mock.rails = true; rate_mock.drdy = true;
        hb_hal_port_t rate_port = {&rate_mock,mock_spi,mock_cs,mock_reset,mock_pwdn,mock_drdy,
            mock_rails,mock_vcap,mock_set_start,mock_delay,mock_millis,mock_idle,mock_fault,mock_micros,NULL};
        hb_afe_config_t rate_cfg = hb_ads1299_default_config(); rate_cfg.sample_rate = rates[i];
        hb_ads1299_t rate_afe;
        assert(hb_ads1299_init(&rate_afe, &rate_port, &rate_cfg) == HB_AFE_OK);
        assert(rate_mock.regs[0x01] == expected_config1[i]);
        assert((rate_mock.regs[0x01] & 0x40u) == 0u);
    }
    assert(mock.last_command==0x10); /* RDATAC enabled while conversions stopped */
    assert(mock.start_inactive && !mock.start_active);
    uint8_t pre_stream_register=0;
    assert(hb_ads1299_read_register(&acq.afe,1u,&pre_stream_register)==HB_AFE_ERR_STATE);
    assert(hb_ads1299_write_register(&acq.afe,1u,0u)==HB_AFE_ERR_STATE);
    assert(hb_acquisition_start(&acq)==HB_AFE_OK);
    assert(mock.start_active && mock.last_command==0x10); /* no SPI opcode while RDATAC */
    uint8_t forbidden_register_value=0;
    assert(hb_ads1299_read_register(&acq.afe,1u,&forbidden_register_value)==HB_AFE_ERR_STATE);
    assert(hb_ads1299_write_register(&acq.afe,1u,0u)==HB_AFE_ERR_STATE);
    assert(hb_acquisition_poll(&acq,5)==HB_AFE_OK);
    assert(mock.packets==1 && mock.last_packet[0]=='H');
    assert(acq.samples_emitted==1);
    assert(hb_acquisition_stop(&acq)==HB_AFE_OK);
    assert(mock.start_inactive && !mock.start_active && mock.last_command==0x10);

    /* DRDY timeout must latch a fault and stop acquisition. */
    assert(hb_acquisition_start(&acq)==HB_AFE_OK);
    mock.drdy=false;
    assert(hb_acquisition_poll(&acq,2)==HB_AFE_ERR_TIMEOUT);
    assert(acq.faulted && !acq.running);
    assert(mock.reset_asserted && mock.pwdn_asserted && mock.start_inactive);
    assert(!mock.cs_asserted);

    /* Rails not good means no device configuration/streaming. */
    mock_t bad={0}; bad.regs[0]=0x1E; bad.rails=false;
    hb_hal_port_t bad_port={&bad,mock_spi,mock_cs,mock_reset,mock_pwdn,mock_drdy,
                            mock_rails,mock_vcap,mock_set_start,mock_delay,
                            mock_millis,mock_idle,mock_fault,mock_micros,NULL};
    hb_ads1299_t afe;
    assert(hb_ads1299_init(&afe,&bad_port,&cfg)==HB_AFE_ERR_RAILS);
    puts("ADS1299 mock driver tests passed");
    return 0;
}
