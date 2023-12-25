/*******************************************************************************
* file GPS.h
* TinyGPS wrapper, interface
* author Marcel Meek
*********************************************************************************/

#ifndef __GPS_H_
#define __GPS_H_

class Gps {
  public:
    Gps() {}
    void init();
    bool read();

    float lat, lng;
    int16_t alt, hdop;
};

#endif // __GPS_H_
