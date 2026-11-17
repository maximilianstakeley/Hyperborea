#include "hb/dual_acquisition.h"
#include "hb/protocol.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t regs[2][0x18];
    bool drdy[2];
    bool cs[2];
    bool reset[2];
    bool pwdn[2];
    bool rails;
    bool start;
    unsigned start_calls;
    unsigned frame_reads[2];
    uint32_t tick_ms;
    uint32_t tick_us;
} dual_mock_t;
typedef struct { dual_mock_t *shared; unsigned device; } endpoint_t;
typedef struct { uint8_t bytes[HB_DUAL_PACKET_BYTES]; size_t length; unsigned calls; } sink_t;

static bool spi_for(dual_mock_t *m, unsigned device, const uint8_t *tx,
                    uint8_t *rx, size_t n) {
    if (n == 1u) { if (rx) rx[0] = 0u; return true; }
    if (n == 3u && (tx[0] & 0xE0u) == 0x20u) {
        if (rx) { rx[0] = 0u; rx[1] = 0u; rx[2] = m->regs[device][tx[0] & 0x1Fu]; }
        return true;
    }
    if (n == 3u && (tx[0] & 0xE0u) == 0x40u) {
        m->regs[device][tx[0] & 0x1Fu] = tx[2];
        return true;
    }
    if (n == HB_ADS1299_FRAME_BYTES && rx) {
        memset(rx, 0, n);
        rx[0] = device ? 0xABu : 0x12u;
        rx[1] = device ? 0xCDu : 0x34u;
        rx[2] = device ? 0xEFu : 0x56u;
        for (unsigned ch = 0u; ch < HB_ADS1299_CHANNELS; ++ch) {
            const uint32_t value = (device ? 100u : 0u) + ch + 1u;
            const size_t offset = 3u + ch * 3u;
            rx[offset] = (uint8_t)(value >> 16);
            rx[offset + 1u] = (uint8_t)(value >> 8);
            rx[offset + 2u] = (uint8_t)value;
        }
        ++m->frame_reads[device];
        return true;
    }
    return false;
}
#define DEVICE_FUNCS(n) \
    static bool spi##n(void *c,const uint8_t*t,uint8_t*r,size_t z,uint32_t ms){(void)ms;endpoint_t*e=c;return spi_for(e->shared,n,t,r,z);} \
    static void cs##n(void*c,bool v){endpoint_t*e=c;e->shared->cs[n]=v;} \
    static void reset##n(void*c,bool v){endpoint_t*e=c;e->shared->reset[n]=v;} \
    static void pwdn##n(void*c,bool v){endpoint_t*e=c;e->shared->pwdn[n]=v;} \
    static bool drdy##n(void*c){endpoint_t*e=c;return e->shared->drdy[n];}
DEVICE_FUNCS(0)
DEVICE_FUNCS(1)
static bool rails(void *c) { return ((endpoint_t *)c)->shared->rails; }
static bool vcap(void *c) { (void)c; return true; }
static void start(void *c, bool v) { dual_mock_t *m = c; m->start = v; ++m->start_calls; }
static void delay_ms(void *c, uint32_t v) { dual_mock_t *m = ((endpoint_t *)c)->shared; m->tick_ms += v; m->tick_us += v * 1000u; }
static uint32_t millis(void *c) { return ((endpoint_t *)c)->shared->tick_ms; }
static uint32_t micros(void *c) { return ((endpoint_t *)c)->shared->tick_us; }
static void idle(void *c) { dual_mock_t *m = ((endpoint_t *)c)->shared; ++m->tick_ms; m->tick_us += 1000u; }
static void fault(void *c, const char *s) { (void)c; (void)s; assert(!"unexpected AFE fault"); }
static bool packet_sink(void *c, const uint8_t *p, size_t n) {
    sink_t *sink = c;
    if (!sink || !p || n != HB_DUAL_PACKET_BYTES) return false;
    memcpy(sink->bytes, p, n); sink->length = n; ++sink->calls; return true;
}

int main(void) {
    dual_mock_t mock = {0};
    mock.regs[0][0] = 0x1Eu; mock.regs[1][0] = 0x1Eu;
    mock.rails = true; mock.drdy[0] = true; mock.drdy[1] = true;
    endpoint_t e0 = {&mock,0u}, e1 = {&mock,1u};
    hb_hal_port_t p0 = {&e0,spi0,cs0,reset0,pwdn0,drdy0,rails,vcap,start,
                        delay_ms,millis,idle,fault,micros,&mock};
    hb_hal_port_t p1 = {&e1,spi1,cs1,reset1,pwdn1,drdy1,rails,vcap,start,
                        delay_ms,millis,idle,fault,micros,&mock};
    hb_dual_acquisition_t acq;
    sink_t sink = {0};
    hb_afe_config_t cfg = hb_ads1299_default_config();
    assert(cfg.sample_rate == HB_AFE_RATE_1000_SPS);
    assert(hb_dual_acquisition_init(&acq, &p0, &p1, &cfg, packet_sink, &sink) == HB_AFE_OK);
    assert(acq.dual.initialized);
    assert(mock.regs[0][0x01] == 0x14u && mock.regs[1][0x01] == 0x14u);
    assert((mock.regs[0][0x01] & 0x80u) == 0u && (mock.regs[1][0x01] & 0x80u) == 0u);
    assert(mock.regs[0][0x03] == mock.regs[1][0x03]);
    mock.start_calls = 0u;
    assert(hb_dual_acquisition_start(&acq) == HB_AFE_OK);
    assert(mock.start && mock.start_calls == 2u);

    mock.tick_us = 987654u;
    assert(hb_dual_acquisition_poll(&acq, 3u) == HB_AFE_OK);
    assert(mock.frame_reads[0] == 1u && mock.frame_reads[1] == 1u);
    assert(acq.samples_emitted == 1u && acq.next_sequence == 1u);
    assert(sink.calls == 1u && sink.length == HB_DUAL_PACKET_BYTES);
    assert(sink.bytes[4] == HB_PACKET_DUAL_VERSION && sink.bytes[5] == HB_PACKET_TYPE_DUAL_SAMPLE);
    assert(sink.bytes[12] == (uint8_t)(987654u & 0xFFu));
    assert(sink.bytes[16] == 0x12u && sink.bytes[18] == 0x56u);
    assert(sink.bytes[19] == 0xABu && sink.bytes[21] == 0xEFu);
    assert(hb_dual_acquisition_stop(&acq) == HB_AFE_OK);
    assert(!mock.start);
    return 0;
}
