# STM32 application entry point

`main.c` is the target application loop. It initializes the board adapter, initializes and verifies the ADS1299, waits for an explicit start request, services stop/start controls, polls data-ready and passes HBCI frames to a non-blocking board packet sink. It does not auto-start on boot.

The `hb_board_*` functions are intentionally supplied by the CubeMX board project. In particular, `hb_board_packet_sink` must accept/copy the complete packet into a bounded transport queue or asynchronous TX buffer; it must not retain the pointer to the stack-local packet. A queue overflow or transport refusal causes a latched acquisition fault.
