#include "biquad.h"

Biquad::Biquad() {
setCoefficients(
    0.95654323f, -1.91308645f, 0.95654323f, -1.91119707f, 0.91497583f,
    0.02008337f,  0.04016673f, 0.02008337f, -1.56101808f, 0.64135154f
);
  reset();
}

void Biquad::setCoefficients(
    float b0_1_, float b1_1_, float b2_1_,
    float a1_1_, float a2_1_,
    float b0_2_, float b1_2_, float b2_2_,
    float a1_2_, float a2_2_
  ) {
    b0_1 = b0_1_;
    b1_1 = b1_1_;
    b2_1 = b2_1_;
    a1_1 = a1_1_;
    a2_1 = a2_1_;

    b0_2 = b0_2_;
    b1_2 = b1_2_;
    b2_2 = b2_2_;
    a1_2 = a1_2_;
    a2_2 = a2_2_;
}

float Biquad::process(float x) {
  // ---- First SOS ----
    float y0_1 = b0_1*x + b1_1*x1_1 + b2_1*x2_1 - a1_1*y1_1 - a2_1*y2_1;

    x2_1 = x1_1;
    x1_1 = x;
    y2_1 = y1_1;
    y1_1 = y0_1;

    // ---- Second SOS ----
    float y0_2 = b0_2*y0_1 + b1_2*x1_2 + b2_2*x2_2 - a1_2*y1_2 - a2_2*y2_2;

    x2_2 = x1_2;
    x1_2 = y0_1;
    y2_2 = y1_2;
    y1_2 = y0_2;

    return y0_2;
}

void Biquad::reset()
{
    x1_1 = x2_1 = y1_1 = y2_1 = 0.0f;
    x1_2 = x2_2 = y1_2 = y2_2 = 0.0f;
}