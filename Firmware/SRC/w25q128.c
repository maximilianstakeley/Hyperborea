#include "hb/w25q128.h"
#include <string.h>

enum {
    CMD_WRITE_ENABLE = 0x06,
    CMD_READ_STATUS1 = 0x05,
    CMD_READ_JEDEC_ID = 0x9F,
    CMD_READ_DATA = 0x03,
    CMD_PAGE_PROGRAM = 0x02,
    CMD_SECTOR_ERASE_4K = 0x20
};
#define STATUS_BUSY 0x01u
#define STATUS_WEL  0x02u

static bool xfer(hb_w25q128_t *f, uint8_t op, uint32_t address,
                 uint8_t address_bytes, const uint8_t *tx, uint8_t *rx,
                 size_t length) {
    return f != NULL && f->port != NULL && f->port->transfer != NULL &&
           f->port->transfer(f->port->context, op, address, address_bytes, 0u,
                             tx, rx, length, 1000u);
}
static hb_flash_result_t read_status(hb_w25q128_t *f, uint8_t *status) {
    if (status == NULL) return HB_FLASH_ERR_ARGUMENT;
    return xfer(f, CMD_READ_STATUS1, 0u, 0u, NULL, status, 1u)
        ? HB_FLASH_OK : HB_FLASH_ERR_TRANSPORT;
}
static hb_flash_result_t write_enable(hb_w25q128_t *f) {
    if (!xfer(f, CMD_WRITE_ENABLE, 0u, 0u, NULL, NULL, 0u))
        return HB_FLASH_ERR_TRANSPORT;
    uint8_t status = 0u;
    hb_flash_result_t r = read_status(f, &status);
    if (r != HB_FLASH_OK) return r;
    return (status & STATUS_WEL) != 0u ? HB_FLASH_OK : HB_FLASH_ERR_TRANSPORT;
}
static bool range_valid(uint32_t address, size_t length) {
    return address < HB_W25Q128_CAPACITY_BYTES &&
           length <= (size_t)(HB_W25Q128_CAPACITY_BYTES - address);
}

hb_flash_result_t hb_w25q128_wait_ready(hb_w25q128_t *f, uint32_t timeout_ms) {
    if (f == NULL || f->port == NULL || f->port->millis == NULL ||
        f->port->idle == NULL) return HB_FLASH_ERR_ARGUMENT;
    const uint32_t started = f->port->millis(f->port->context);
    for (;;) {
        uint8_t status = 0u;
        hb_flash_result_t r = read_status(f, &status);
        if (r != HB_FLASH_OK) return r;
        if ((status & STATUS_BUSY) == 0u) return HB_FLASH_OK;
        if ((uint32_t)(f->port->millis(f->port->context) - started) >= timeout_ms)
            return HB_FLASH_ERR_TIMEOUT;
        f->port->idle(f->port->context);
    }
}

hb_flash_result_t hb_w25q128_init(hb_w25q128_t *f, hb_flash_port_t *port) {
    if (f == NULL || port == NULL || port->transfer == NULL ||
        port->millis == NULL || port->idle == NULL) return HB_FLASH_ERR_ARGUMENT;
    memset(f, 0, sizeof *f);
    f->port = port;
    uint8_t id[3] = {0u, 0u, 0u};
    if (!xfer(f, CMD_READ_JEDEC_ID, 0u, 0u, NULL, id, sizeof id))
        return HB_FLASH_ERR_TRANSPORT;
    memcpy(f->jedec_id, id, sizeof id);
    /* Winbond W25Q128JV: manufacturer EFh, memory type 40h, capacity 18h. */
    if (id[0] != 0xEFu || id[1] != 0x40u || id[2] != 0x18u)
        return HB_FLASH_ERR_ID;
    hb_flash_result_t r = hb_w25q128_wait_ready(f, 1000u);
    if (r == HB_FLASH_OK) f->initialized = true;
    return r;
}

hb_flash_result_t hb_w25q128_read(hb_w25q128_t *f, uint32_t address,
                                  uint8_t *data, size_t length) {
    if (f == NULL || !f->initialized || (data == NULL && length != 0u))
        return HB_FLASH_ERR_ARGUMENT;
    if (!range_valid(address, length)) return HB_FLASH_ERR_RANGE;
    if (length == 0u) return HB_FLASH_OK;
    hb_flash_result_t ready = hb_w25q128_wait_ready(f, 1000u);
    if (ready != HB_FLASH_OK) return ready;
    return xfer(f, CMD_READ_DATA, address, 3u, NULL, data, length)
        ? HB_FLASH_OK : HB_FLASH_ERR_TRANSPORT;
}

hb_flash_result_t hb_w25q128_program(hb_w25q128_t *f, uint32_t address,
                                     const uint8_t *data, size_t length) {
    if (f == NULL || !f->initialized || (data == NULL && length != 0u))
        return HB_FLASH_ERR_ARGUMENT;
    if (!range_valid(address, length)) return HB_FLASH_ERR_RANGE;
    size_t offset = 0u;
    while (offset < length) {
        const uint32_t current = address + (uint32_t)offset;
        const size_t page_remaining = HB_W25Q128_PAGE_BYTES -
                                      (current % HB_W25Q128_PAGE_BYTES);
        const size_t chunk = (length - offset < page_remaining)
                                 ? length - offset : page_remaining;
        hb_flash_result_t r = hb_w25q128_wait_ready(f, 1000u);
        if (r != HB_FLASH_OK) return r;
        r = write_enable(f);
        if (r != HB_FLASH_OK) return r;
        uint8_t status = 0u;
        r = read_status(f, &status);
        if (r != HB_FLASH_OK || (status & STATUS_WEL) == 0u)
            return r == HB_FLASH_OK ? HB_FLASH_ERR_TRANSPORT : r;
        if (!xfer(f, CMD_PAGE_PROGRAM, current, 3u, data + offset, NULL, chunk))
            return HB_FLASH_ERR_TRANSPORT;
        r = hb_w25q128_wait_ready(f, 5000u);
        if (r != HB_FLASH_OK) return r;
        offset += chunk;
    }
    return HB_FLASH_OK;
}

hb_flash_result_t hb_w25q128_erase_sector(hb_w25q128_t *f,
                                          uint32_t sector_address) {
    if (f == NULL || !f->initialized) return HB_FLASH_ERR_ARGUMENT;
    if (!range_valid(sector_address, HB_W25Q128_SECTOR_BYTES) ||
        (sector_address % HB_W25Q128_SECTOR_BYTES) != 0u)
        return HB_FLASH_ERR_RANGE;
    hb_flash_result_t r = hb_w25q128_wait_ready(f, 5000u);
    if (r != HB_FLASH_OK) return r;
    r = write_enable(f);
    if (r != HB_FLASH_OK) return r;
    if (!xfer(f, CMD_SECTOR_ERASE_4K, sector_address, 3u, NULL, NULL, 0u))
        return HB_FLASH_ERR_TRANSPORT;
    return hb_w25q128_wait_ready(f, 10000u);
}
