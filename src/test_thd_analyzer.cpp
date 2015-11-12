// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
/**
 * Analizador de distorsión armónica total de una señal tonal de audio.
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
      { "device",   required_argument, 0, 'a' },
      { "help",     no_argument,       0, 'h' },   
      { 0,          0,                 0,  0  }
};

std::string device_;
ThdAnalyzer* driver = NULL;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MilliSleep(int milliseconds);
void Usage();


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) {

      // Lectura de las opciones en la línea de órdenes
      if (argc == 1) {
            Usage();
            return 0;
      }

      // hw0,2
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


      // Conversión de la dirección en el bus i2c (cadena) en un número entero.
      char* endptr;
      int address;

      driver = new ThdAnalyzer(device_.c_str()); 
      if (driver->Init() != 0) {
            printf("Error: During ALSA device initialization.\n");
            return 1;
      } 

      printf("\n");
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
