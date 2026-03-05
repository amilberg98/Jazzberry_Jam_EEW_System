#ifndef BIQUAD_H
#define BIQUAD_H

#include <Arduino.h>

class Biquad {
        public:
        Biquad();

        float process(float x);

        void reset();

        void setCoefficients(
        float b0_1, float b1_1, float b2_1,
        float a1_1, float a2_1,
        float b0_2, float b1_2, float b2_2,
        float a1_2, float a2_2
        );


        private:

        float b0_1, b1_1, b2_1;
        float a1_1, a2_1;
        float x1_1, x2_1;
        float y1_1, y2_1;

        float b0_2, b1_2, b2_2;
        float a1_2, a2_2;
        float x1_2, x2_2;
        float y1_2, y2_2;

};

#endif