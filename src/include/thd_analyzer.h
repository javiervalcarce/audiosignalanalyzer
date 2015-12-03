// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#ifndef THDANALYZER_THD_ANALYZER_H_
#define THDANALYZER_THD_ANALYZER_H_

#include <stdint.h>
#include <string>
#include <alsa/asoundlib.h>

namespace thd_analyzer {


      /**
       * ThdAnalyzer.
       *
       * Procesa y analiza N señales de audio. El dispositivo de captura (ADC) del sistema puede ser, en general,
       * multicanal, y podrá muestrear N señales. Lo típico es que sea estéreo con lo cual N = 2 canales (L y R).
       *
       * Para la obtención de las muestras usa la API Linux ALSA.
       *
       */
      class ThdAnalyzer {
      public:
     
            /**
             * Constructor.
             *
             * @param capture_device El dispositivo ALSA de captura de muestras de audio, por ejemplo "default" es el
             * dispositivo predeterminado del sistema y casi siempre representa la entrada de linea o micrófono, otros
             * dispositivos son por ejemplo "hw:0,0", "hw:1,0", "plughw:0,0", etc... Todo esto depende del sistema en
             * concreto, pruebe a ejecutar el programa "arecord -l" para ver una lista de dispositivos. Importante:
             * ajuste el volumen de grabación y active el capturador con alsamixer primero.
             */
            ThdAnalyzer(const char* capture_device);


            /**
             * Destructor. 
             *
             * Detiene el hilo de procesado de señal y libera toda la memoria que se pidió en el constructor.
             */
            ~ThdAnalyzer();

            /**
             * Inicialización.
             *
             * Pone en marcha el hilo de procesado de señal, que comenzará detenido. Para ponerlo en marcha y hay que
             * llamar a Start().
             */
            int Init();


            /**
             * Da comienzo a la captura de muestras de audio y al análisis de las señales. Mientras el análisis esté en
             * marcha podremos llamar a las funciones Frequency() etc para obtener las medidas.
             */
            int Start();


            /**
             * Detiene la captura de muestras de audio y el análisis. Las medidas no cambiarán, se mantendrán en el
             * último estado antes de llamar a Stop().
             */
            int Stop();


            //
            //int Reset();


            /**
             * Frecuencia cuyo coeficiente en la transformada tiene módulo máximo, expresado en Hz.
             *
             * @param channel Número de canal. En un dispositivo estéreo el 0 es el izquierdo y el 1 es el derecho.
             */
            int FindPeak(int channel);

            /**
             * Amplitud máxima del pedazo de señal, normalizada entre 0.0 y 1.0, para convertir esto en voltios hay que
             * saber el rango dinámico de entrada del conversor A/D.
             *
             * @param channel Número de canal. En un dispositivo estéreo el 0 es el izquierdo y el 1 es el derecho.
             */
            double RmsAmplitude(int channel);
            
            
            /**
             * PSD.
             *
             * @param frequency_bin Índice de la frecuencia en la que se evaluará la densidad espectral de potencia.
             */
            double PowerSpectralDensity(int channel, int frequency_index);

            /**
             *
             */
            double AnalogFrequency(int frequency_index);


            /**
             */
            int BlockCount() const;

            /**
             */
            int BlockSize() const;

      private:


            // Parametros comunes a todos los canales

            // Tamaño de bloque en muestras. El procesamiento de señal
            // se hace por bloques de muestras, no muestra a muestra, que seria muy ineficiente.
            int block_size_;

            // log2(tamaño del bloque)
            int block_size_log2_;

            // Frecuencia de muestreo
            int sample_rate_;

            // Numero de bloques procesados
            int block_count_;

            /**
             * Canal de entrada (L o R)
             */
            struct Channel {

                  // |block_size_| muestras convertidas a coma flotante y normalizadas en el intevalo real [-1.0, 1.0)
                  double* data;

                  // |block_size_| muestras de la densidad espectral de potencia estimada
                  double* pwsd;
                  
                  double rms;
                  double peakv;
                  int peakf;
                 
            };

            Channel* channel_;

            // Búferes temporales
            double* im;
            double* abs2;


            // Hilo
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
             * Process
             */
            int Process();
      
      };

}

#endif // THDANALYZER_THD_ANALYZER_H_



