/* STM32H7 HAL adapter for the portable W25Q128 driver. */
#include "main.h"
#include "quadspi.h"
#include "hb/w25q128.h"

extern QSPI_HandleTypeDef hqspi;

static bool qspi_transfer(void *context, uint8_t opcode, uint32_t address,
                          uint8_t address_bytes, uint8_t dummy_bytes,
                          const uint8_t *tx, uint8_t *rx, size_t length,
                          uint32_t timeout_ms) {
    (void)context;
    if (length > 65535u || (address_bytes != 0u && address_bytes != 3u))
        return false;

    QSPI_CommandTypeDef command = {0};
    command.Instruction = opcode;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.AddressMode = address_bytes == 3u ? QSPI_ADDRESS_1_LINE : QSPI_ADDRESS_NONE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.Address = address;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = length == 0u ? QSPI_DATA_NONE : QSPI_DATA_1_LINE;
    command.NbData = (uint32_t)length;
    command.DummyCycles = dummy_bytes;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(&hqspi, &command, timeout_ms) != HAL_OK) return false;
    if (length == 0u) return true;
    if (tx != NULL)
        return HAL_QSPI_Transmit(&hqspi, (uint8_t *)tx, timeout_ms) == HAL_OK;
    if (rx != NULL)
        return HAL_QSPI_Receive(&hqspi, rx, timeout_ms) == HAL_OK;
    return false;
}

static uint32_t qspi_millis(void *context) { (void)context; return HAL_GetTick(); }
static void qspi_idle(void *context) { (void)context; __NOP(); }

hb_flash_port_t hb_stm32h7_make_flash_port(void) {
    hb_flash_port_t port = {0};
    port.transfer = qspi_transfer;
    port.millis = qspi_millis;
    port.idle = qspi_idle;
    return port;
}
