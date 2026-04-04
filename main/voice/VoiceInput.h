#ifndef VOICE_INPUT_H
#define VOICE_INPUT_H

#include <stdint.h>

class VoiceInput {
public:
    void setup();
    float calculateRMS(int16_t* samples, int count);
    int16_t calculateMax(int16_t* samples, int count);
};

#endif