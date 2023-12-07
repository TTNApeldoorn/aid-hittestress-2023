# Homerus Klimaat sensor 2023
Notes on software for Homerus Klimaatsensor. 

## Software 

### Prerequisites:
 1. Buildtool Arduino IDE 2.2.1
 2. ESP32 Espressif board should be installed
 3. Select board T-Beam

### Libraries:
The following libraries shall be installed.
- AXP202X_Library-master
- MCCI_LoRaWAN_LMIC_library
- TinyGPSPlus-master
- sensirion-sps
- Adafuit_AM2315
- Adafruit_BusIO

### Instructions
Edit file `lmic_project_config.h` in the library MCCI_LoRaWAN_LMIC_library:

 - Select the correct Radioband, for Europe: `#define CFG_eu868 1`
 - Uncomment or add the line: `#define hal_init LMICHAL_init`