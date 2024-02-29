/*

TTN module
Wrapper to use TTN with the LMIC library

Copyright (C) 2018 by Xose Pérez <xose dot perez at gmail dot com>

This code requires the MCCI LoRaWAN LMIC library
by IBM, Matthis Kooijman, Terry Moore, ChaeHee Won, Frank Rose
https://github.com/mcci-catena/arduino-lmic

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Arduino.h"
#include <basicmac.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Preferences.h>
#include "configuration.h"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

// LMIC GPIO configuration for SX1262
const lmic_pinmap lmic_pins = {
    .nss = LORA_CS, // 18
    // TXEN is controlled through DIO2 by the SX1262 (HPD16A) directly
    .tx = LMIC_CONTROLLED_BY_DIO2,
    .rx = LMIC_UNUSED_PIN,
    .rst = LORA_RST, // 23
    .dio = {LMIC_UNUSED_PIN, LORA_IO1  /* 33 */, LMIC_UNUSED_PIN},
    .busy = LORA_IO2, // 32
    // TCXO is controlled through DIO3 by the SX1262 directly
    .tcxo = LMIC_CONTROLLED_BY_DIO3,
};

// Message counter, stored in RTC memory, survives deep sleep.
static RTC_DATA_ATTR uint32_t count = 1;

// callbacks
static void (*rxCallback)(unsigned int, uint8_t*, unsigned int) = NULL;     
static void (*txCallback)(void) = NULL;

// forward decl.
static void parseHex( u1_t *byteBuffer, const char* str);
static void parseHexReverse( u1_t *byteBuffer, const char* str);
static void printKeys(u4_t netid, devaddr_t devaddr, u1_t* nwkKey, u1_t* artKey);

// handle TTN OTAA keys
static char deveui[32];  // generated from board id
extern void os_getDevEui (u1_t* buf) { parseHexReverse( buf, deveui);}
extern void os_getJoinEui (u1_t* buf) { parseHexReverse( buf, APPEUI);}
extern void os_getNwkKey (u1_t* buf) { parseHex( buf, APPKEY);}

u1_t os_getRegion (void) { return LMIC_regionCode(0); }

// -----------------------------------------------------------------------------
// Private methods
// -----------------------------------------------------------------------------

// LMIC library will call this method when an event is fired
void onLmicEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            //u4_t netid = 0;
            //devaddr_t devaddr = 0;
            //u1_t nwkKey[16];
            //u1_t artKey[16];
            //LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
            //printKeys(LMIC.netid, LMIC.devaddr, LMIC.lceCtx.nwkSKey, LMIC.lceCtx.appSKey);
            // save network keys to permanent storage 
            {
              Preferences p;
              if( p.begin("lora", false)) {
                p.putUInt("netId", LMIC.netid);
                p.putUInt("devAddr", LMIC.devaddr);
                p.putBytes("nwkKey", LMIC.lceCtx.nwkSKey, 16);
                p.putBytes("artKey", LMIC.lceCtx.appSKey, 16);
                p.end();
              }
            }
            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0);
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_SCAN_FOUND:
            Serial.println(F("EV_SCAN_FOUND"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXDONE:
            Serial.println(F("EV_TXDONE"));
            break;
        case EV_DATARATE:
            Serial.println(F("EV_DATARATE"));
            break;
        case EV_START_SCAN:
            Serial.println(F("EV_START_SCAN"));
            break;
        case EV_ADR_BACKOFF:
            Serial.println(F("EV_ADR_BACKOFF"));
            break;

         default:
            Serial.print(F("Unknown event: "));
            Serial.println(ev);
            break;
    }
}

// -----------------------------------------------------------------------------
// Public methods
// -----------------------------------------------------------------------------

extern void ttn_register_rxReady( void (*rxReady)(unsigned int, uint8_t*, unsigned int)) {
    rxCallback = rxReady;
}

extern void ttn_register_txReady( void (*txReady)()) {
    txCallback = txReady;
}

extern bool ttn_setup() {
  // get unique deveui from chip id
  uint64_t chipid = ESP.getEfuseMac();   //The chip ID is essentially its MAC address(length: 6 bytes).
  sprintf(deveui, "%08X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  printf("DEVEUI=%s\n", deveui);

  // SPI interface
  //SPI.begin(SCK_GPIO, MISO_GPIO, MOSI_GPIO, NSS_GPIO);

  // LMIC init
    os_init(nullptr);
    LMIC_reset();

#ifdef CLOCK_ERROR
  LMIC_setClockError(MAX_CLOCK_ERROR * CLOCK_ERROR / 100);
#endif
  // Disable link check validation  // to be checked MM 
  //LMIC_setLinkCheckMode(0);

  // Set default rate and transmit power for uplink (note: txpow seems to be ignored by the library)
  // LMIC_setDrTxpow(DR_SF9, 14);
  //LMIC_setAdrMode(0);

  Preferences p;
  p.begin("lora", true);  // we intentionally ignore failure here
  uint32_t netId = p.getUInt("netId", UINT32_MAX);
  uint32_t devAddr = p.getUInt("devAddr", UINT32_MAX);
  uint8_t nwkKey[16], artKey[16];
  bool keysgood = p.getBytes("nwkKey", nwkKey, sizeof(nwkKey)) == sizeof(nwkKey) && 
                  p.getBytes("artKey", artKey, sizeof(artKey)) == sizeof(artKey);
  p.end();  // close our prefs
  //printKeys(netId, devAddr, nwkKey, artKey);

  if (!keysgood) {
    // We have not yet joined a network, start a full join attempt
    // Make LMiC initialize the default channels, choose a channel, and
    // schedule the OTAA join
    count = 1;
    Serial.println("No session saved, joining from scratch");
    LMIC_startJoining();
  }
  else {
    Serial.println("Rejoining saved session");
    LMIC_setSession(netId, devAddr, nwkKey, artKey);
  }
  p.end();
  return true;
}

/// Blow away our prefs (i.e. to rejoin from scratch)
extern void ttn_erase_prefs() {
  Preferences p;
  if (p.begin("lora", false)) {
    p.clear();
    p.end();
  }
}

extern bool ttn_send(uint8_t* data, uint8_t data_size, uint8_t port) {
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    return false;
  }
  // Prepare upstream data transmission at the next possible time.
  // Parameters are port, data, length, confirmed
  //printf("LMIC_setTxData2 count=%d\n", count);
  LMIC.seqnoUp = count;
  //LMIC_setSeqnoUp(count);  // TODO  MM
  LMIC_setTxData2(port, data, data_size, 0);
  count++;
  return true;
}

extern bool ttn_connected() {
  return (LMIC.devaddr != 0);
}

void ttn_shutdown() {
  LMIC_shutdown();
}

extern void ttn_loop() {
  os_runstep();
}

// local helper fumctions
static void printKeys(u4_t netid, devaddr_t devaddr, u1_t* nwkKey, u1_t* artKey) {
  printf("netid=%d\n", netid);
  printf("devaddr=%04X\n", devaddr);

  printf("NwkSKey: %02X", nwkKey[0]);
  for (size_t i = 1; i < 16; ++i)
    printf("-%02X", nwkKey[i]);
  printf("\n");

  printf("ArtKey: %02X", artKey[0]);
  for (size_t i = 1; i < 16; ++i)
    printf("-%02X", artKey[i]);
  printf("\n");
}

// convert TTN key string big endian byte array
static void parseHex( u1_t *byteBuffer, const char* str){
  int len = strlen(str);
  for (int i = 0; i < len; i+=2){
    char substr[3];
    strncpy(substr, &(str[i]), 2);
    byteBuffer[i/2] = (u1_t)(strtol(substr, NULL, 16));
  }
}

// convert TTN key string to little endian byte array
static void parseHexReverse( u1_t *byteBuffer, const char* str){
  int len = strlen(str);
  for (int i = 0; i < len; i+=2){
    char substr[3];
    strncpy(substr, &(str[i]), 2);
    byteBuffer[((len-1)-i)/2] = (u1_t)(strtol(substr, NULL, 16));   // reverse the TTN KEY string !!
  }
}

