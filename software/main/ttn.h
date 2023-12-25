/*******************************************************************************
* file ttn.h
* LoraWAN wrapper, interface using LMIC mcci catena library
* https://github.com/mcci-catena/arduino-lmic
* don't forget to set '#define CFG_eu868 1' in the file 'lmic_project_config.h'
* author Marcel Meek
*********************************************************************************/

#ifndef __TTN_H_
#define __TTN_H_

//#include <stdint.h> // uint8_t type

// external functions 
extern void ttn_register_rxReady( void (*rxReady)(unsigned int, uint8_t*, unsigned int));
extern void ttn_register_txReady( void (*txReady)());
extern bool ttn_setup();
extern bool ttn_send(uint8_t* data, uint8_t data_size, uint8_t port);
extern bool ttn_connected();
extern void ttn_erase_prefs();
extern void ttn_loop();

#endif // __TTN_H_
