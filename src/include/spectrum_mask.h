// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#ifndef THDANALYZER_SPECTRUM_MASK_H_
#define THDANALYZER_SPECTRUM_MASK_H_

#include <stdint.h>
#include <string>

namespace thd_analyzer {

      class ThdAnalyzer;   

      /**
       * Máscara espectral.  
       *
       * Es una simple grafica m(f) con la que el espectro X(f) de una señal se compara. Se detectan aquelas frecuencias f que rebasan la
       * linea, se cuenta su numero y mas o menos por donde estan. Es decir, aquellas valores de f tal que X(f) >= m(f)
       */
      class SpectrumMask {
      public:
            
            /**
             * Constructor.
             *
             * @param sampling_rate Frecuencia de muestreo (Fs), la maxima frecuencia analogica fmax es Fs/2
             * @param dft_size Numero de puntos de que consta la grafica m(x).
             */
            SpectrumMask(int sampling_rate, int fft_size);

            /**
             * Destructor.
             *
             */
            ~SpectrumMask();

            /**
             * Establece una banda de paso desde f1 a f2. Dentro de esta banda la atenuacion minima exigida sera 0 dB
             */
            void SetBandAttenuation(double f1, double f2, double attenuation);

            /**
             *
             */
            //void SetMinAttenuation(double decibels);

            /**
             * Establece todos los puntos de la mascara a [attenuation] dB
             * m(f) = [attenuation] para todo f.
             */
            void Reset(double attenuation);


            // Número de frecuencias f en el espectro X(f) actual que TRASPASAN la máscara m(x).
            int    error_count;

            // Primera frecuencia f analogica que traspasa la máscara.
            double first_trespassing_frequency;

            // Valor de dicha frecuencia.
            double first_trespassing_value;

            // Última frecuencia f analógica (la de valor más alto) que traspasa la máscara.
            double last_trespassing_frequency;
            
            // Valor de dicha frecuencia.
            double last_trespassing_value;

      private:
            
            friend class ThdAnalyzer;   

            // La máscar no es totalmente rígida, se puede desplazar arriba y abajo en el eje de amplitud.
            double vertical_offset;

            // Valores de la máscara, es una array de [size] elementos
            double* value;
            int size;
            int fs;
      };

}

#endif // THDANALYZER_SPECTRUM_MASK_H_




