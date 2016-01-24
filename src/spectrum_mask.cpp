// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include "spectrum_mask.h"
#include <cmath>

using namespace thd_analyzer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SpectrumMask::SpectrumMask(int sampling_rate, int fft_size) {

      fs   = sampling_rate;
      size = fft_size;
      value = new double[size];

      Reset(0);

      error_count = 0;
      first_trespassing_frequency = nan("");
      first_trespassing_value = nan("");
      last_trespassing_frequency = nan("");
      last_trespassing_value = nan("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SpectrumMask::~SpectrumMask() {
      delete[] value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SpectrumMask::SetBandAttenuation(double f1, double f2, double attenuation) {

      // Máscara rectangular simple
      double analog_resolution = (double) fs / (double) size; 
      int d1 = (int) floor(f1 / analog_resolution);
      int d2 = (int) ceil (f2 / analog_resolution);
      int i;
      for (i = d1; i <= d2; i++) {
            value[i] = -attenuation; // dB
      }
      
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SpectrumMask::Reset(double attenuation) {
      int i;
      for (i = 0; i < size; i++) {
            value[i] = -attenuation; // dB
      }
      
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
