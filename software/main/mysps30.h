/*******************************************************************************
* file mysps30.h
* sds30 wrapper, interface
* author Marcel Meek
*********************************************************************************/

#ifndef __MYSPS30_H_
#define __MYSPS30_H_

class Sps30 {
  public:
    Sps30() {}
    bool init();
    bool read();
    float pm10, pm2_5, pm1_0;

  private:
    bool _read();

};

#endif // __MYSPS30_H_
