/*

  Main module

  # Modified by Kyle T. Gabriel to fix issue with incorrect GPS data for TTNMapper

  Copyright (C) 2018 by Xose Pérez <xose dot perez at gmail dot com>

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

#include "configuration.h"
#include "rom/rtc.h"
#include <Wire.h>
#include "axp20x.h"
#include "ttn.h"
#include "gps.h"
#include "mysps30.h"
#include "am2315.h"

AXP20X_Class axp;
bool pmu_irq = false;
String baChStatus = "No charging";

bool packetSent = false, packetQueued = false;
bool sensorCycle = false, gpsCycle = false;
int start;

// deep sleep support
RTC_DATA_ATTR int bootCount = 0;
esp_sleep_source_t wakeCause;  // the reason we booted this time

// payload structs
struct Msg {
  int16_t temp, hum, pm2_5, pm10, batt, pm1_0;
};
static Msg msg = {0, 0, 0, 0, 0, 0}; 

struct Stat {
  float lat, lng;
  int16_t alt, hdop, batt, version, cputemp;
};
static Stat stat = {0.0, 0.0, 0, 0, 0, 0, 0}; 

// Sensors
Gps gps;          // GPS sensor
Am2315 am2315;   // temperature hum sensor
Sps30 sps30;     // dust sensor

// -----------------------------------------------------------------------------
// Application
// -----------------------------------------------------------------------------

/**
 * sleep, turn off LORA and GPS power
 */
void sleep(int msec) {
  printf("sleep for %d\n", msec);
  // Set the user button to wake the board
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);  //sleep_interrupt(BUTTON_PIN, LOW);
  LMIC_shutdown();                                            // cleanly shutdown the radio

  // turn on after initial testing with real hardware
  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);  // LORA radio
  axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);  // GPS main power

  // FIXME - use an external 10k pulldown so we can leave the RTC peripherals powered off
  // until then we need the following lines
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  // Only GPIOs which are have RTC functionality can be used in this bit map: 0,2,4,12-15,25-27,32-39.
  uint64_t gpioMask = (1ULL << BUTTON_PIN);

  // FIXME change polarity so we can wake on ANY_HIGH instead - that would allow us to use all three buttons (instead of just the first)
  gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  esp_sleep_enable_ext1_wakeup(gpioMask, ESP_EXT1_WAKEUP_ALL_LOW);

  esp_sleep_enable_timer_wakeup(msec * 1000ULL);  // call expects usecs
  esp_deep_sleep_start();
}

void _txCallback() {
  // We only want to say 'packetSent' for our packets (not packets needed for joining)
  if (packetQueued) {
    printf("Message sent\n");
    packetQueued = false;
    packetSent = true;
  }
}

void _rxCallback(unsigned int port, uint8_t* buf, unsigned int len) {
  printf("[TTN] Response: ");
  for (int i = 0; i < len; i++) {
    printf("%02x ", buf[i]);
  }
  printf("\n");
}

/**
 * Init the power manager chip
 * 
 * axp192 power 
    DCDC1 0.7-3.5V @ 1200mA max -> OLED  // If you turn this off you'll lose comms to the axp192 because the OLED and the axp192 share the same i2c bus, instead use ssd1306 sleep mode
    DCDC2 -> unused
    DCDC3 0.7-3.5V @ 700mA max -> ESP32 (keep this on!)
    LDO1 30mA -> charges GPS backup battery  // charges the tiny J13 battery by the GPS to power the GPS ram (for a couple of days), can not be turned off
    LDO2 200mA -> LORA
    LDO3 200mA -> GPS
 */

void axp192Init() {
  if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
    Serial.println("AXP192 Begin PASS");
  } else {
    Serial.println("AXP192 Begin FAIL");
    return;
  }
  //axp.setChgLEDMode(LED_BLINK_4HZ);
  printf("DCDC1=%d, DCDC2=%d, LDO2=%d, LDO3=%d, DCDC3=%d, EXten=%d\n",
         axp.isDCDC1Enable(), axp.isDCDC2Enable(), axp.isLDO2Enable(), axp.isLDO3Enable(), axp.isDCDC3Enable(), axp.isExtenEnable());

  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);  // LORA radio
  //axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);  // GPS main power
  axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
  axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
  axp.setDCDC1Voltage(3300);  // for the OLED power

 printf("DCDC1=%d, DCDC2=%d, LDO2=%d, LDO3=%d, DCDC3=%d, EXten=%d\n",
         axp.isDCDC1Enable(), axp.isDCDC2Enable(), axp.isLDO2Enable(), axp.isLDO3Enable(), axp.isDCDC3Enable(), axp.isExtenEnable());

  int cur = axp.getChargeControlCur();
  Serial.printf("Current charge control current = %d mA \n", cur);
  //axp.setChargeControlCur( 15);
  //Serial.printf("Set charge control current 500 mA \n");
  //cur = axp.getChargeControlCur();
 // Serial.printf("Current charge control current = %d mA \n", cur);
 
  pinMode(PMU_IRQ, INPUT_PULLUP);
  attachInterrupt(
    PMU_IRQ, [] {
      pmu_irq = true;
    },
    FALLING);

  axp.adc1Enable(AXP202_BATT_CUR_ADC1, 1);
  axp.enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_BATT_REMOVED_IRQ | AXP202_BATT_CONNECT_IRQ, 1);
  axp.clearIRQ();

  if (axp.isCharging()) {
    Serial.printf("Charging\r\n");
    baChStatus = "Charging";
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
 
  if( bootCount >= 1000)   // force a new join after n cycles
    bootCount = 1;
  else
   bootCount++;

  wakeCause = esp_sleep_get_wakeup_cause();
  Serial.printf("booted, wake cause %d (boot count %d)\n", wakeCause, bootCount);

  Wire.begin(I2C_SDA, I2C_SCL);
  axp192Init();

  // Buttons & LED
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // TTN setup
  if (bootCount == 1)
    ttn_erase_prefs();

  if (!ttn_setup()) {
    printf("Radio module not found!\n");
    sleep( 60000);  //deepsleep retry afer one minute
  }
  ttn_register_rxReady(_rxCallback);
  ttn_register_txReady(_txCallback);

  // init and read sensors

  if (bootCount % 10) {      // send after n sensor messages a status message
    sensorCycle = true;
    if( sps30.init() && sps30.read() ) {
      msg.pm1_0 = sps30.pm1_0 * 100;
      msg.pm2_5 = sps30.pm2_5 * 100;
      msg.pm10 = sps30.pm10 * 100;
    }
    if( am2315.read()) {
      msg.temp = am2315.temp *100;
      msg.hum = am2315.hum * 100;
    }
    msg.batt = (int16_t)axp.getBattVoltage();
  }
  else {
    axp.setPowerOutPut(AXP192_LDO3, AXP202_ON); // set GPS POWER on
    gpsCycle = true;
    gps.init();
    gps.read();
    stat.lat = gps.lat;
    stat.lng = gps.lng;
    stat.hdop =gps.hdop;
    stat.alt = gps.alt;
    stat.version = APP_VERSION * 100;
    stat.batt = (int16_t)axp.getBattVoltage();
    stat.cputemp = (int16_t)axp.getTemp() * 100;
  }
  start = millis();
  printf("setup end\r\n");
}

void loop() {

  ttn_loop();

  if( ttn_connected() && !packetQueued && !packetSent) {
    printf("ttn_send\n");
    if( sensorCycle )
      ttn_send((uint8_t*)&msg, sizeof(msg), 15);
    else
      ttn_send((uint8_t*)&stat, sizeof(stat), 16);
    packetQueued = true;
  }

  // if packet has been sent, or wait max 10 sec for completing the ttn msg, then start deepsleep
  if( packetSent  || millis() - start > 10000) {
    packetSent = false;
    
    if( gpsCycle )
      sleep( 10000);           // minimum sleep time
    else
      sleep( SEND_INTERVAL - 10000);
  }
}
