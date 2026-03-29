#ifndef I2SMICROPHONE_H
#define I2SMICROPHONE_H

#include "driver/i2s_std.h"

class I2SMicrophone {
private:
    i2s_chan_handle_t rx_handle;
    int sample_rate;
    int bck_pin;
    int ws_pin;
    int din_pin;

public:
    I2SMicrophone(int bck_pin, int ws_pin, int din_pin, int sample_rate = 16000);
    bool init();
    int read(int16_t* buffer, int samples);
    void deinit();
};

#endif // I2SMICROPHONE_H