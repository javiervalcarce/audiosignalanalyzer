// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#ifndef THDANALYZER_ADC_INTERFACE_H_
#define THDANALYZER_ADC_INTERFACE_H_

#include <string>

namespace thd_analyzer {

      // Blocking I/O interface
      class AdcInterface {
      public:
            
            AdcInterface(std::string device);
            ~AdcInterface();

            int Init();
            //int Start();
            //int Stop();
            int WaitForData();
            int GetChannelData(int channel, double data);
            
      private:
            
      };

      // Blocking I/O interface
      class DacInterface {
      public:
            DacInterface(std::string device);
            ~DacInterface();

      private:
      };
}

#endif // THDANALYZER_ADC_INTERFACE_H_


