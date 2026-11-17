#include "hb/w25q128.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t g_memory[HB_W25Q128_CAPACITY_BYTES];
typedef struct { uint8_t status; uint32_t tick; unsigned programs; unsigned erases; } mock_flash_t;
static bool mock_transfer(void *context, uint8_t opcode, uint32_t address,
                          uint8_t address_bytes, uint8_t dummy_bytes,
                          const uint8_t *tx, uint8_t *rx, size_t length,
                          uint32_t timeout_ms) {
    mock_flash_t *m=(mock_flash_t*)context; (void)address_bytes; (void)dummy_bytes; (void)timeout_ms;
    switch (opcode) {
        case 0x9F:
            if (!rx || length!=3u) return false;
            rx[0]=0xEF; rx[1]=0x40; rx[2]=0x18; return true;
        case 0x05:
            if (!rx || length!=1u) return false;
            rx[0]=m->status; return true;
        case 0x06: m->status|=0x02u; return true;
        case 0x03:
            if (!rx || address+length>HB_W25Q128_CAPACITY_BYTES) return false;
            memcpy(rx,&g_memory[address],length); return true;
        case 0x02:
            if (!tx || (m->status&0x02u)==0u || length==0u ||
                address+length>HB_W25Q128_CAPACITY_BYTES ||
                (address%HB_W25Q128_PAGE_BYTES)+length>HB_W25Q128_PAGE_BYTES) return false;
            for (size_t i=0;i<length;i++) g_memory[address+i]&=tx[i];
            m->status&=(uint8_t)~0x02u; m->programs++; return true;
        case 0x20:
            if ((m->status&0x02u)==0u || address%HB_W25Q128_SECTOR_BYTES!=0u ||
                address+HB_W25Q128_SECTOR_BYTES>HB_W25Q128_CAPACITY_BYTES) return false;
            memset(&g_memory[address],0xFF,HB_W25Q128_SECTOR_BYTES);
            m->status&=(uint8_t)~0x02u; m->erases++; return true;
        default: return false;
    }
}
static uint32_t mock_millis(void *ctx) { return ((mock_flash_t*)ctx)->tick; }
static void mock_idle(void *ctx) { ((mock_flash_t*)ctx)->tick++; }
int main(void) {
    memset(g_memory,0xFF,sizeof g_memory);
    mock_flash_t mock={0};
    hb_flash_port_t port={&mock,mock_transfer,mock_millis,mock_idle};
    hb_w25q128_t flash;
    assert(hb_w25q128_init(&flash,&port)==HB_FLASH_OK);
    assert(flash.initialized && flash.jedec_id[2]==0x18u);

    uint8_t source[40], actual[40];
    for (unsigned i=0;i<sizeof source;i++) source[i]=(uint8_t)(0xA0u+i);
    /* Crosses a 256-byte page boundary: driver must split the operation. */
    assert(hb_w25q128_program(&flash,0xF0u,source,sizeof source)==HB_FLASH_OK);
    assert(mock.programs==2u);
    memset(actual,0,sizeof actual);
    assert(hb_w25q128_read(&flash,0xF0u,actual,sizeof actual)==HB_FLASH_OK);
    assert(memcmp(source,actual,sizeof source)==0);

    assert(hb_w25q128_erase_sector(&flash,0u)==HB_FLASH_OK);
    assert(mock.erases==1u);
    memset(actual,0,sizeof actual);
    assert(hb_w25q128_read(&flash,0xF0u,actual,sizeof actual)==HB_FLASH_OK);
    for (unsigned i=0;i<sizeof actual;i++) assert(actual[i]==0xFFu);
    assert(hb_w25q128_erase_sector(&flash,1u)==HB_FLASH_ERR_RANGE);
    assert(hb_w25q128_read(&flash,HB_W25Q128_CAPACITY_BYTES-1u,actual,sizeof actual)==HB_FLASH_ERR_RANGE);
    puts("W25Q128 driver mock tests passed");
    return 0;
}
