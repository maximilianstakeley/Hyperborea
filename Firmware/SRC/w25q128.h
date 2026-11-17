#ifndef HB_W25Q128_H
#define HB_W25Q128_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define HB_W25Q128_CAPACITY_BYTES (16u * 1024u * 1024u)
#define HB_W25Q128_PAGE_BYTES 256u
#define HB_W25Q128_SECTOR_BYTES 4096u

typedef struct {
    void *context;
    /* Execute one SPI/QSPI transaction. Address is sent MSB-first using
       address_bytes; dummy_bytes follow it before data. */
    bool (*transfer)(void *context, uint8_t opcode, uint32_t address,
                     uint8_t address_bytes, uint8_t dummy_bytes,
                     const uint8_t *tx, uint8_t *rx, size_t length,
                     uint32_t timeout_ms);
    uint32_t (*millis)(void *context);
    void (*idle)(void *context);
} hb_flash_port_t;

typedef enum {
    HB_FLASH_OK = 0,
    HB_FLASH_ERR_ARGUMENT,
    HB_FLASH_ERR_TRANSPORT,
    HB_FLASH_ERR_TIMEOUT,
    HB_FLASH_ERR_ID,
    HB_FLASH_ERR_RANGE,
    HB_FLASH_ERR_NOT_ERASED,
    HB_FLASH_ERR_VERIFY
} hb_flash_result_t;

typedef struct {
    hb_flash_port_t *port;
    uint8_t jedec_id[3];
    bool initialized;
} hb_w25q128_t;

hb_flash_result_t hb_w25q128_init(hb_w25q128_t *flash, hb_flash_port_t *port);
hb_flash_result_t hb_w25q128_read(hb_w25q128_t *flash, uint32_t address,
                                  uint8_t *data, size_t length);
hb_flash_result_t hb_w25q128_program(hb_w25q128_t *flash, uint32_t address,
                                     const uint8_t *data, size_t length);
hb_flash_result_t hb_w25q128_erase_sector(hb_w25q128_t *flash,
                                          uint32_t sector_address);
hb_flash_result_t hb_w25q128_wait_ready(hb_w25q128_t *flash,
                                        uint32_t timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
