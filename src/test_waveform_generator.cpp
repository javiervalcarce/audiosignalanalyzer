// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <getopt.h>

#include "waveform_generator.h"

using namespace thd_analyzer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// tabla de opciones para getopt_long
struct option long_options[] = {
      { "device",    required_argument, 0, 'a' },
      { "frequency", required_argument, 0, 'f' },
      { "sweep",     no_argument,       0, 's' },
      { "help",      no_argument,       0, 'h' },   
      { 0,           0,                 0,  0  }
};

std::string device_;
WaveformGenerator* obj = NULL;
int frequency = 440;
bool sweep = false;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Usage();


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {

      // hw0,2
      device_ = "default";

      while (1) {

            int c;
            int option_index = 0;
            
            c = getopt_long(argc, argv, "dfh", long_options, &option_index);
            if (c == -1) {
                  break;
            }
            
            switch (c) {
            case 'h':
                  Usage();
                  exit(0);
                  break;
            case 's':
                  sweep = true;
                  break;
            case 'd':
                  device_ = std::string(optarg);
                  break;
            case 'f':
                  frequency = strtoul(optarg, NULL, 0);
                  break;
            case '?':
                  // getopt_long already printed an error message
                  exit(1);
                  break;
            }
      }

      obj = new WaveformGenerator(device_.c_str()); 
      if (obj->Init() != 0) {
            printf("Error: During ALSA device initialization.\n");
            return 1;
      } 

      printf("Playing %d Hz tone for 30 seconds...\n", frequency);
      obj->SetFrequency(frequency);

      int count = 30;
      while (count > 0) {
            sleep(1);
            count--;
            if (sweep == true) {
                  frequency += 500;
                  obj->SetFrequency(frequency);
            }
      }
      printf("\n");
      return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Usage() {
      printf("Usage: ./test_waveform_generator <alsa-playback-device-name>\n");
      printf("Defaults to L and R channels\n");
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
