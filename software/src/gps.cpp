/*

  GPS module

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

#include <TinyGPS++.h>
#include "configuration.h"
#include "gps.h"

static TinyGPSPlus tinyGpsPlus;
static HardwareSerial serialGPS(GPS_SERIAL_NUM);

void Gps::init() {
  lat = lng = 0.0;
  hdop = alt = 0;
  serialGPS.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(100);
}

bool Gps::read() {
  printf("Gps::read\n");

  bool fix = false;
  int i = 0;
  long start = millis();

  // leave loop after timout or
  // if Vext power is switched off, exit the loop (caused by RGB_LORAWAN Led, be sure that it is deactivated)
  while (millis() - start < GPS_MAX_WAIT_FOR_LOCK) {
    if (serialGPS.available() > 0) {
      char c = serialGPS.read();
      //Serial.print(c);
      tinyGpsPlus.encode(c);
      i++;
      if (tinyGpsPlus.location.isUpdated()) {     //  deze kan ook ? if (gps.location.isValid())
        lat = tinyGpsPlus.location.lat();
        lng = tinyGpsPlus.location.lng();
        hdop = 1000 * tinyGpsPlus.hdop.hdop();
        alt = tinyGpsPlus.altitude.meters(); 
        fix = true;
        break;
      }
    }
  }
  printf("GPS chars read:%d\n", i);

  Serial.print("lat: ");
  Serial.print(lat);
  Serial.print(" lon: ");
  Serial.print(lng);
  Serial.print(" alt: ");
  Serial.print(alt);
  Serial.print(" hdop: ");
  Serial.println(hdop / 1000.0);
  return fix;
}

