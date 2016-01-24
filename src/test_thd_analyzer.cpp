// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
/**
 * Analizador de espectro de señales de audio.
 *
 * Copyright(c) 2015 SEPSA
 * Javier Valcarce, <javier.valcarce@sepsa.es>
 */

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <string>
#include <getopt.h>
#include "thd_analyzer.h"

using namespace thd_analyzer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// tabla de opciones para getopt_long
struct option long_options[] = {
      { "device",   required_argument, 0, 'd' },
      { "help",     no_argument,       0, 'h' },   
      { 0,          0,                 0,  0  }
};

std::string device_;
ThdAnalyzer* analyzer = NULL;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MilliSleep(int milliseconds);
void Usage();


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {


      //hw:1,0
      //hw:1,2
      device_ = "hw:0,0";//"default";

      while (1) {

            int c;
            int option_index = 0;
            
            c = getopt_long(argc, argv, "dh", long_options, &option_index);
            if (c == -1) {
                  break;
            }
            
            switch (c) {
            case 'h':
                  Usage();
                  exit(0);
                  break;
            case 'd':
                  device_ = std::string(optarg);
                  break;
            case '?':
                  // getopt_long already printed an error message
                  exit(1);
                  break;
            }
      }


      printf("Using device %s\n", device_.c_str());
      

      // Curioso: en mi PC, son sistema de audio IDT 92HD81, al usar
      // la llamada snd_pcm_hw_params_set_rate_min() si pongo la Fs
      // exacta que admite el hardware me selecciona la Fs
      // inmediatamente superior, por ejemplo, si pongo 44100 se
      // configura a 48000.  Si queremos 44100 hay que poner una
      // cantidad ligeramente inferior, por ejemplo 44099

      analyzer = new ThdAnalyzer(device_.c_str(), 44099, 12);  // 44,1 kHz, FFT-8192
      //analyzer = new ThdAnalyzer(device_.c_str(), 47999, 12);    // 48   kHz, FFT-8192
      //analyzer = new ThdAnalyzer(device_.c_str(), 191999, 12); // 192  kHz, FFT-8192

      if (analyzer->Init() != 0) {
            printf("Error: During ALSA device initialization.\n");
            return 1;
      } 

      printf("Actual sampling rate is %d\n", analyzer->SamplingFrequency());
      
      analyzer->Start();

      int fbin;
      
      const thd_analyzer::SpectrumMask* mask;
      int c;

      for (c = 0; c < 2; c++) {
            analyzer->Mask(c)->Reset(20.0);
            analyzer->Mask(c)->SetBandAttenuation(0000.0, 00100.0, 15.0);
            analyzer->Mask(c)->SetBandAttenuation(0430.0, 00450.0, -5.0);
      }

      int count = 0;
      printf("\n");

      while (1) {

            for (c = 0; c < 2; c++) {
                  fbin = analyzer->FindPeak(c);
                  mask = analyzer->Mask(c);                  

                  assert(mask != NULL);

                  printf("CH%d XR=%d Max=(%6.1f Hz, %9.5f dB), SNRI=%8.3f dB, TTE=%4d, F=(%6.1f Hz, %9.5f dB) L=(%6.1f Hz, %9.5f dB)\n",
                         c,
                         analyzer->OverrunCount(),
                         analyzer->AnalogFrequency(fbin),                
                         analyzer->PowerSpectralDensityDecibels(c, fbin),
                         analyzer->SNRI(c, 900, 1100),
                         mask->error_count,
                         mask->first_trespassing_frequency,
                         mask->first_trespassing_value,
                         mask->last_trespassing_frequency,
                         mask->last_trespassing_value
                         );
            }

            count++;            
            usleep(100000); // 0,05 segundos
            
            if ((count % 50) == 0) {
                  analyzer->Stop();
                  usleep(500000); // 0,5 segundos
                  analyzer->GnuplotFileDump("dump.plot");
                  analyzer->Start();
            }
            
            // Retrocedo dos líneas y vuelvo a imprimir
            printf("\r");
            printf("\x1b[A");
            printf("\x1b[A");
      }

      return 0;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MilliSleep(int milliseconds) {
      struct timespec t0;
      t0.tv_sec  = (milliseconds / 1000);
      t0.tv_nsec = (milliseconds % 1000) * 1000000; // ns
      nanosleep(&t0, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Usage() {
      printf("Usage: ./test_thd_analyzer <alsa-capture-device-name>\n");
      printf("Defaults to L channel\n");
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
