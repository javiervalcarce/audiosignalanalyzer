// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#ifndef THDANALYZER_THD_ANALYZER_H_
#define THDANALYZER_THD_ANALYZER_H_

#include <stdint.h>
#include <string>
#include <alsa/asoundlib.h>

namespace thd_analyzer {


      /**
       * Análisis de un tono puro. Estima la frecuencia central, su potencia y la de sus armónicos y con todo ello
       * calcula la distorsión harmónica total (THD - Total Harmonic Distortion).
       *
       */
      class ThdAnalyzer {
      public:
     
            /**
             * Constructor.
             *
             * @param capture_device.
             */
            ThdAnalyzer(const char* capture_device);

            /**
             * Destructor.
             *
             */
            ~ThdAnalyzer();

            /**
             * Inicialización.
             */
            int Init();

            /**
             *
             *
             */
            int Start();

            /**
             *
             *
             */
            int Stop();

            /**
             * Frecuencia cuyo coeficiente en la transformada tiene módulo máximo, expresado en Hz.
             *
             * @param channel Número de canal. En un dispositivo estéreo el 0 es el izquierdo y el 1 es el derecho.
             */
            double Frequency(int channel);

            /**
             * Amplitud máxima del pedazo de señal, normalizada entre 0.0 y 1.0, para convertir esto en voltios hay que
             * saber el rango dinámico de entrada del conversor A/D.
             *
             * @param channel Número de canal. En un dispositivo estéreo el 0 es el izquierdo y el 1 es el derecho.
             */
            double Amplitude(int channel);

            int Count() { return fft_count_; }

      private:

            /**
             */
            struct AdcProperties {
                  double input_range; // rango de entrada, correspondiente a una muetra 1.0, en voltios
                  int sampling_frequency; // frecuencia de muestreo, en hercios
            };


            /**
             */
            struct SignalProperties {
                  double rms_amplitude;
                  double main_harmonic;
                  double thd;
            };


            /**
             */
            struct Buffer {
                  Buffer(int msize) {
                        size= msize;
                        data = new double[msize];    
                  }
    
                  ~Buffer() {
                        delete[] data;
                  }
    
                  int size;
                  double* data;
            };

            pthread_t thread_;
            pthread_attr_t thread_attr_;

            // Indica si el objeto se ha inicializado mediante Init()
            bool initialized_;
            bool exit_thread_;
            // Dispositivo ALSA de captura
            std::string device_;

            // Representa el dispositivo ALSA de captura de audio.
            snd_pcm_t* capture_handle_;

            // Búfer en el se reciben las muestras en el formato en que las entrega el ADC, que normalmente será int16_t
            // las muestras podrán pertenecer a un solo canal o a varios intercalados. Si por ejemplo hay dos canales
            // (estereo) entonces las muestras estarán dispuestas de la forma L R L R L R L R L R...)
            int16_t* buf_data_;
            int      buf_size_;
            int fft_count_;

            // Muestras correspondientes al canal L y R convertidas a coma flotante y normalizadas en el intevalo real
            // [-1.0, 1.0)
            double** channel_data_;

            int block_size_;
            int sample_rate_;
            int log2_block_size_;

            // Medidas realizadas sobre las señales L y R
            double channel_frequency_[2];
            double channel_amplitude_[2];

            static void* ThreadFuncHelper(void* p);
            void* ThreadFunc();


            int DumpToTextFile(const char* file_name, int file_index, double* x, double* y, int size);
            
            /**
             * This computes an in-place complex-to-complex FFT 
             * x and y are the real and imaginary arrays of 2^m points.
             * dir =  1 gives forward transform
             * dir = -1 gives reverse transform 
             */
            void FFT(short int dir, long m, double* x, double* y);

            /**
             *
             */
            int Process();
      
      };

}

#endif // THDANALYZER_THD_ANALYZER_H_



