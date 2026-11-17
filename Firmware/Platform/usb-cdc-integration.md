# STM32Cube USB CDC HS glue

This bridge assumes the Rev27 USB path is configured as a USB High-Speed device: STM32H743 OTG_HS in ULPI mode, USB3320 PHY and the ADuM3166 isolator. The ADuM3166 supports low/full/high-speed operation; speed negotiation and HS retiming/clock requirements must still be validated on this exact circuit.

1. Generate USB Device CDC High-Speed middleware using the exact STM32CubeH7 version and the approved 60 MHz ULPI configuration.
2. Compile `hb_usb_cdc_hs_glue.c` in the target after `usbd_cdc_if.h` is generated.
3. Call `hb_usb_cdc_bridge_init()` from the board initialization after USB device middleware init.
4. In the generated `CDC_Receive_HS()` callback, call `hb_usb_cdc_receive_bytes(Buf, *Len)` and then re-arm the OUT endpoint with the generated `USBD_CDC_ReceivePacket()` call. Exact callback placement must match the installed STM32CubeH7 middleware version.
5. In the generated `CDC_TransmitCplt_HS()` callback, call `hb_usb_cdc_transmit_complete()` before returning. The queue retains the packet storage until this callback, avoiding reuse of a USB DMA buffer while transmission is in flight.
6. Implement board hooks: `hb_board_packet_sink()` delegates to `hb_usb_cdc_packet_sink()`, `hb_board_start_requested()` delegates to `hb_usb_cdc_take_start_request()`, `hb_board_stop_requested()` delegates to `hb_usb_cdc_take_stop_request()`, and `hb_board_service()` calls `hb_usb_cdc_service_tx()`.
7. The host sends ASCII `START\n` and `STOP\n`; sample records are binary HBCI v1 packets in the opposite direction.

The bridge has a 15-packet usable single-producer/single-consumer queue. Queue saturation causes the acquisition sink to reject a frame and the firmware to latch an acquisition fault rather than silently overwrite samples. Tune queue depth and servicing based on measured worst-case USB stall duration.

The ADS1299 remains in RDATAC mode after initialization. Conversion start/stop is controlled by the dedicated AFE_START GPIO (PF11): high starts conversions, low stops them. This avoids issuing START/STOP opcodes while the converter is in RDATAC mode, where other command opcodes are not accepted until SDATAC.
