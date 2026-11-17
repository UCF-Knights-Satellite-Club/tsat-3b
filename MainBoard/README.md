# Main board

The TSat-3B flight computer: ESP32-WROOM, BNO085 IMU, SAM-M8Q GPS, BMP581 barometer, RFM95 LoRa radio, and the servo bus
through a Waveshare Bus Servo Adapter A.
Designer: Pablo J.

> [!NOTE]
> **Schematic only.** `TSAT3B_Main_V10.kicad_sch` is the latest design (V10, title block Rev J, 2026-11-06). There is no
> `.kicad_pro` or `.kicad_pcb` yet.

📄 **Schematic:** [PDF](docs/schematic.pdf)

## Pin map (from the V10 schematic)

| Function | ESP32 pins |
|---|---|
| BNO085 IMU (SPI) | SCK IO18, MISO IO19, MOSI IO23, CS IO21, INT IO34, RST IO32 |
| RFM95 LoRa (SPI, shared bus) | CS IO26 (see issue #9), DIO0 IO39, RST IO33 |
| SAM-M8Q GPS (UART) | IO16, IO17 |
| BMP581 barometer (I2C) | SCL IO22, SDA IO4 |
| Servo bus (UART) | TX IO25, RX IO27 |

## Review

Design review feedback, including known problems in V10, is tracked in
[issue #9](https://github.com/UCF-Knights-Satellite-Club/tsat-3b/issues/9).

## To do

Upload the full KiCad project here (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, custom libraries), then a schematic PDF and, once
it's ready to order, Gerbers in `fabrication/`. Use the same layout as
[`../PowerDistributionBoard/1.5U-Rev1/`](../PowerDistributionBoard/1.5U-Rev1/).
