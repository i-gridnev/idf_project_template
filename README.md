# Project
MCU: ESP32 S3

## Pinout
Populated in **main/Kconfig.projbuild** along with some important settings.

## Firmware Flash
A builded '.bin' firmware can be flashed via official IDF 'flash_download_tool' (https://www.espressif.com/sites/default/files/tools/flash_download_tool_3.9.5.zip). Plug ESP via USB cable and find a new COM indentified in PC. Start the tool software and select '.bin' (set checkbox), offset @ = 0, select COM port, press START.