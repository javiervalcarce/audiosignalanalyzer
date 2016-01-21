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
      device_ = "default";

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

      analyzer = new ThdAnalyzer(device_.c_str()); 
      if (analyzer->Init() != 0) {
            printf("Error: During ALSA device initialization.\n");
            return 1;
      } 

      //analyzer->Start();

      int fbin;
      
      const thd_analyzer::SpectrumMask* mask;
      int count = 0;

      printf("Estimating maximum power frequency:\n");
      while (1) {

            for (int c = 0; c < 2; c++) {

                  fbin = analyzer->FindPeak(c);
                  mask = analyzer->Mask(c);

                  printf("CH%d B%06d - Peak (%6.0f Hz, %9.5f dB), SNRI(900,1100) %9.5f dB, TTE: %6d, LTF: %6.0f Hz LTV: %9.5f\n",
                         c,
                         analyzer->BlockCount(), 
                         analyzer->AnalogFrequency(fbin),                          
                         analyzer->PowerSpectralDensityDecibels(c, fbin),
                         analyzer->SNRI(c, 900, 1100),
                         mask->error_count,
                         mask->last_trespassing_frequency,
                         mask->last_trespassing_value
                        );
            }

            count++;            
            usleep(100000); // 0,1 segundos

            // Retrocedo dos líneas y vuelvo a imprimir
            printf("\r");
            printf("\x1b[A");
            printf("\x1b[A");

            if ((count % 8 * 10) == 0) {
                  printf("PLOT\n");
                  analyzer->GnuplotFileDump("dump.plot");
                  usleep(250000); // 0,25 segundos
            }


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
