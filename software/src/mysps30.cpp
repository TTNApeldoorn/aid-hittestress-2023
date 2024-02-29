/*
  SPS30 module

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

#include <sps30.h>
#include "configuration.h"
#include "mysps30.h"


// SPS30 support
#define auto_clean_days 4
struct sps30_measurement m;


bool Sps30::init() {
  sensirion_i2c_init();
  int retry = 5;
  while (sps30_probe() != 0) {   
    Serial.print("SPS sensor probing failed\r\n");
    delay(500);
    if( retry-- < 0)
      return false;
  }
  //int16_t ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);  // Used to drive the fan for pre-defined sequence every X days
  //if (ret) {
  //  Serial.print("error setting the auto-clean interval: ");
  //  Serial.println( ret);
  //}
  return true;
}
bool Sps30::read() {
  //printf("Sps30::read\n");
  pm10 = pm2_5 = pm1_0 = 0.0;

  sps30_start_measurement();  // Start of loop start fan to flow air past laser sensor
  delay(4000);                //Wait 4+1 seconds while fan is active before read data
  
  for(int i=0; i<5; i++) {
    delay( 1000);
    if( !_read()) {
      sps30_stop_measurement();
      return false;
    }    
    //printf("PM1.0=%.2f PM2.5=%.2f PM10=%.2f\n", m.mc_1p0, m.mc_2p5, m.mc_10p0);
    pm10  += m.mc_10p0;
    pm2_5 += m.mc_2p5;
    pm1_0 += m.mc_1p0;
  } 

  pm10  /= 5.0;
  pm2_5 /= 5.0; 
  pm1_0 /= 5.0;;
  //printf("PM1.0=%.2f PM2.5=%.2f PM10=%.2f\n", pm1_0, pm2_5, pm10);
  sps30_stop_measurement();  //Disables Fan 
  return true;
}


// private read
bool Sps30::_read() {
  int16_t ret;
  uint16_t data_ready;
  int16_t retry = 10;

  ret = sps30_read_data_ready(&data_ready);       // check ready
  while( ret >=0 && retry > 0 && !data_ready) {   // if ok, wait until ready
    delay( 100);
    ret = sps30_read_data_ready(&data_ready);
    retry--;
  }
  
  if( retry <= 0) {
    printf("error too many read retries\n");
    return false; // to many retries
  }
  ret = sps30_read_measurement(&m);    // read
  if( ret < 0) {
    printf("error reading measurement: %d\n", ret);
    return false;
  }
  return true;
}
