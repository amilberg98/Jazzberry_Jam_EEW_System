struct Biquad {
  float b0, b1, b2;   // input coeffs
  float a1, a2;       // outut coeffs

  // state
  float x1 = 0.0f, x2 = 0.0f;
  float y1 = 0.0f, y2 = 0.0f;

	inline float process(float x0) {
    // Direct Form I
    float y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;

    // shift 
    x2 = x1; x1 = x0;
    y2 = y1; y1 = y0;

    return y0;
  }

	inline void reset() {
    x1 = x2 = y1 = y2 = 0.0f;
  }
};

struct BandPass2SOS {
  Biquad s1; //  first row of coeff from SciPy sos
  Biquad s2; // second row of coeff from SciPy sos

  inline float process(float x) {
    x = s1.process(x);
    x = s2.process(x);
    return x;
  }

  inline void reset() { s1.reset(); s2.reset(); }
};


void setupFilters(BandPass2SOS fx, BandPass2SOS fy, 
								  BandPass2SOSfz;) {
  // SOS row 1
  fx.s1 = Biquad{
    0.99778102f, -1.99556205f, 0.99778102f,
    -1.99555712f, 0.99556697f
  };

  // SOS row 2
  fx.s2 = Biquad{
    0.06745527f, 0.13491055f, 0.06745527f,
    -1.14298050f, 0.41280160f
  };

  // copy for other axes (same filter shape)
  fy = fx;
  fz = fx;

	
}
