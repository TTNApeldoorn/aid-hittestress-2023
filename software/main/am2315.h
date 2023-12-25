/*******************************************************************************
* file am2315.h
* am2315 wrapper, interface
* author Marcel Meek
*********************************************************************************/

#ifndef __AM2315_H_
#define __AM2315_H_

class Am2315 {
  public:
    Am2315() { temp = 0.0; hum = 0.0;}
    bool read();
    float temp, hum;
};

#endif // __AM2315_H_
