/*
  AM2315 module

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

#include <Adafruit_AM2315.h>
#include "configuration.h"
#include "am2315.h"

static Adafruit_AM2315 ada_am2315;

bool Am2315::read() {
  printf("Am2315::read\n");
  int retry = 5;
  while (!ada_am2315.begin() ) {  
    Serial.println("am2315 not found, check wiring & pullups!");
    if( --retry < 0)
      return false;
    delay(500);
  }
  delay(2000);  // need to wait

  retry = 5;
  while (!ada_am2315.readTemperatureAndHumidity(&temp, &hum)) {
    Serial.println("Failed to read data from AM2315");
    if( --retry < 0)
      return false;
    delay(500);
  }
  printf("Temp %.2f Hum %.2f\n", temp, hum);
  return true;
}