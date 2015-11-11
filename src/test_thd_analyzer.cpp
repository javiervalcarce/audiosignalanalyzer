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
      { "bus",      required_argument, 0, 'b' },
      { "address",  required_argument, 0, 'a' },
      { "help",     no_argument,       0, 'h' },   
      { 0,          0,                 0,  0  }
};

std::string device_i2c_bus;
std::string device_address;
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


      while (1) {

            int c;
            int option_index = 0;
            
            c = getopt_long(argc, argv, "b:a:h", long_options, &option_index);
            if (c == -1) {
                  break;
            }
            
            switch (c) {
            case 'h':
                  Usage();
                  exit(0);
                  break;
            case 'b':
                  device_i2c_bus = std::string(optarg);
                  break;
            case 'a':
                  device_address = std::string(optarg);
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

      if (device_address.substr(0, 2) == "0x") {
            // Número hexadecimal: base 16
            address = strtol(device_address.substr(2, std::string::npos).c_str(), &endptr, 16);       
      } else {
            // Número decimal: base 10
            address = strtol(device_address.c_str(), &endptr, 10);
      }
     
      if (*endptr != '\0') {
	    printf("Bad address format.\n");
            exit(1);
      }

      printf("\n");
      printf("Using bus %s\n", device_i2c_bus.c_str());
      printf("Using address 0x%02X\n", address);
      printf("\n");


      driver = new ThdAnalyzer("hw0,2");
      if (driver->Init() != 0) {
            printf("Error while initializing the device.\n");
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
      printf("Usage: ./test_io_expander --bus=<i2c-bus> --address=<i2c-address>\n");      
      printf("Default values for SEPSA IO board should be --device=/dev/i2c-10 and --address=0x65\n");
      printf("\n");

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
