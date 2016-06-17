// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#ifndef THDANALYZER_THD_ANALYZER_H_
#define THDANALYZER_THD_ANALYZER_H_

#include <stdint.h>
#include <string>
#include <alsa/asoundlib.h>
#include <pthread.h>

#include "spectrum_mask.h"


namespace thd_analyzer {

      /**
       * Analizador de espectro en tiempo real para señales de audio.
       *
       * Analiza N señales de audio provenientes de un dispositivo ADC multicanal con interfaz Linux ALSA . El ADc del
       * sistema puede ser, como digo, multicanal de N canales. Lo más típico es que sea estéreo con lo cual N = 2 (L y
       * R) pero este software está preparado para trabajar en general con N canales incluyendo por supuesto audio
       * monoaural (N = 1).
       *
       * Realiza las siguientes medidas:
       *
       * - Estimación de la densidad espectral de potencia por el método del periodograma (FFT al cuadrado).
       * - Localización de la frecuencia cuya potencia es máxima en el espectro.
       * - Relación señal a ruido más interferencias (SNRI )
       * - comprueba si un espectro se ajusta a una máscara dada o no, y contabiliza el número de frecuencias que están 
       *   rebasando la máscara.
       *
       * Con las medidas anteriores podemos usar este componente, por ejemplo, para ver el espectro de la señal o para
       * detectar la presencia o no de tonos enterrados en ruido y estimar su frecuencia y su amplitud.
       *
       */
      class ThdAnalyzer {
      public:


            /**
             * Estado interno del analizador de espectro.
             */
            enum InternalState {

                  // Aun no se ha llamado a Init(), hasta que no llame a Init() no podra ponerlo en marcha con Start().
                  kNotInitialized,

                  // Parado. Puede ponerlo en marcha con Start().
                  kStopped,

                  // En marcha. Puede pararlo con Stop().
                  kRunning,

                  // Error fatal, el analizador esta parado, el hilo ha terminado, destruya el objeto (con delete) y 
                  // vuelva a instanciarlo.
                  kCrashed
            };


            /**
             * Constructor.
             * 
             *
             * @param capture_device El dispositivo ALSA que captura de muestras de audio, por ejemplo "default" es el
             * dispositivo predeterminado del sistema y casi siempre representa la entrada de linea o micrófono, otros
             * dispositivos son por ejemplo "hw:0,0", "hw:1,0", "plughw:0,0", etc. Todo esto depende del sistema en
             * concreto, pruebe a ejecutar el programa "arecord -l" para ver una lista de dispositivos. Muy importante:
             * ajuste primero el volumen de grabación y active el capturador (unmute) con el programa alsamixer.
             *
             * @param sampling_rate Frecuencia de muestreo (Fs) del ADC, en Hz. Cuidado: No siempre funciona lo de
             * ajustar la frecuencia de muestreo, en mi PC, no sé aun el porqué, independientemente de lo que le pongas
             * aquí la Fs es siempre 192000 Hz. Valores típicos son: 44100 Hz, 48000 Hz, 96000 Hz y 192000 Hz. Por otro
             * lado, la resolución de las muestras es fijo de 16 bits con signo (Little Endian INT16).
             *
             * @param log2_block_size Logaritmo en base 2 del número de puntos que se calculan en elespectro. Si aquí se
             * pasa por ejemplo 10 significa que se van a calcular 2^10 = 1024 puntos del espectro. Cuantos más puntos
             * más resolución espectral tendremos pero también más carga computacional. Valores típicos son 10, 11 o 12.
             * 
             */
            ThdAnalyzer(const char* capture_device, int sampling_rate, int log2_block_size);


            /**
             * Destructor. 
             *             
             */
            ~ThdAnalyzer();


            /**
             * Inicialización.
             *
             * Configura el dispositivo ADC de captura y pone en marcha el hilo de procesado de señal.
             */
            int Init();


            /**
             * Estado interno del analizador de espectro.
             */
            InternalState State() const { return internal_state_; }

            /**
             * Ultimo error ocurrido.
             */
            const std::string& ErrorDescription() const { return error_description_; }

            /**
             * Da comienzo a la captura de muestras de audio y al análisis de las señales. Mientras el análisis esté en
             * marcha podremos llamar a las funciones para obtener las medidas.
             * 
             * Esta llama es asincrona.
             */
            int Start();


            /**
             * Detiene la captura de muestras de audio y el análisis. Las medidas efectuadas no cambiarán, se mantendrán 
             * en el último estado en que estaban antes de llamar a Stop().
             * 
             * Esta llama es asincrona, es decir, el analizador no se detendra inmediatamente sino en un instante 
             * posterior (en menos de ~10 ms, depende del tamaño DftSize()). TODO: Escribir un Stop() sincrono.
             */
            int Stop();


            /**
             * Frecuencia de muestreo en hercios (Hz) que está usando el analizador.
             */
            int SamplingFrequency() const;

            /**
             * Número de puntos de la DFT, que será siempre una potencia de 2. 
             */
            int DftSize() const;
            
            /**
             * Resolución en frecuencia que tiene este analizador, expresado en Hz.
             *
             * No es posible distinguir dos tonos que estén separados menos que esta cantidad, que es simplemente
             * SamplingFrequency() / DftSize()
             */
            double AnalogResolution() const;

            /**
             * Devuelve la frecuencia analógica en hercios (Hz) correpondiente al índice |frequency_index|. 
             * 
             * La frecuencia analógica es |frequency_index| * (Fs / N) donde Fs es la frecuencia de muestreo y N es el
             * número de puntos de la FFT, que se obtiene con DftSize()
             *
             * @return La frecuencia analógica en hertzios (Hz) correpondiente al índice suministrado.
             */
            double AnalogFrequency(int frequency_index);

            /**
             * Escribe en disco un fichero de texto con los puntos del espectro listo para visualizar con gnuplot,
             * octave, matlab, etc. Tiene DftSize() filas y N+1 columnas, siendo N el número de canales. La primera
             * columna es la frecuencia analógica y las demás los valores del espectro.
             */
            int GnuplotFileDump(std::string file_name);


            // ----- MEDIDAS -------------------------------------------------------------------------------------------

            /**
             * Devuelve la máscara espectral que se está usando y los contadores de errores asociados.
             */
            SpectrumMask* Mask(int channel) const { return channel_[channel].mask; }

            int OverrunCount() const { return overrun_count_; }
            /**
             * Devuelve el índice de frecuencia, es decir, el punto de la FFT cuyo módulo es el máximo absoluto. Los
             * índices empiezan en 0, van desde 0 hasta (BlockSize() - 1). Para obtener la frecuencia analógica que
             * corresponde a este punto llame a AnalogFrequency().
             *
             * @param channel Número de canal. En un dispositivo de captura estéreo el 0 es el canal izquierdo y el 1 es
             * el canal derecho.
             */
            int FindPeak(int channel);
                      

            /**
             * Estimación de la relación señal a ruido más interferente (SNRI).
             *
             * Esta estimación es bastante simplista, considera que la banda de frecuencia en la que está la señal (y
             * solo está ella, ahí no hay ruido ni inteferencia) es (f1, f2) Hz, lo cual no tiene porqué ser cierto.
             */
            double SNRI(int channel, double f1, double f2);
 

            /**
             * Densidad espectral de potencia correspondiente a la frecuencia |frequency_index|. 
             * La unidad es vatios / (radian / muestra)
             *
             * @param channel El número de canal, es un dispositivo estéreo 0 es el izquierdo y 1 el derecho.
             * @param frequency_index Índice de la frecuencia en la que se evaluará la densidad espectral de potencia.
             *
             */
            double PowerSpectralDensity(int channel, int frequency_index);

            /**
             * Lo mismo que PowerSpectralDensity() pero expresado en decibelios, es decir, aplicando
             * 10*log10(x). Expresado en dB la cantidad siempre será negativa, el máximo valor posible es 0 dB.
             *
             * @param frequency_index Índice de la frecuencia en la que se evaluará la densidad espectral de potencia.
             */
            double PowerSpectralDensityDecibels(int channel, int frequency_index);


            /**
             * Número de bloques de DftSize() muestras procesados en cada canal desde que el hilo interno de procesado
             * se puso en marcha mediante Init(). Sirve por ejemplo para comprobar que el hilo interno de procesado 
             * no se ha parado y está vivo.
             */
            int BlockCount() const;



      private:

            /**
             * Datos brutos y calculados de uno de los canales de entrada del ADC (normalmente será un ADC estéreo y
             * tendrá por tanto dos canales, L y R)
             */
            struct Channel {

                  //Channel(int dft_size);
                  //~Channel();
                  
                  // Longitud de los vectores data y time
                  int size;

                  // señal en el dominio del tiempo, muestras convertidas a coma flotante y normalizadas 
                  // en el intevalo real [-1.0, 1.0)
                  double* xval;

                  // tmp
                  double* data;

                  // coeficientes de la DFT, modulo al cuadrado, |X(k)|^2
                  double* pwsd;


                  // Valor del máximo del espectro
                  double peakv;

                  // Frecuencia en la que se produce ese máximo espectral.
                  int peakf;

                  // Valor RSM (Root Mean Square) del bloque de muestras capturado
                  SpectrumMask* mask;
            };

            // Número de canales del dispositivo ADC, lo normal es que sea estéreo: 2 canales.
            int channel_count_;

            pthread_mutex_t channel_lock_;

            // Tamaño de bloque en muestras. El procesamiento de señal
            // se hace por bloques de muestras, no muestra a muestra, que seria muy ineficiente.
            int block_size_;

            // log2(tamaño del bloque)
            int block_size_log2_;

            // Frecuencia de muestreo
            int sample_rate_;

            // Numero de bloques procesados
            int block_count_;

            Channel* channel_;
            
            pthread_mutex_t lock_;
            pthread_cond_t  can_continue_;  

            // Hilo
            pthread_t thread_;
            pthread_attr_t thread_attr_;

            InternalState internal_state_;
            std::string error_description_;
            bool exit_thread_;

            // Dispositivo ALSA de captura
            std::string device_;

            // Representa el dispositivo ALSA de captura de audio.
            snd_pcm_t* capture_handle_;

            // Búfer en el se reciben las muestras en el formato en que las entrega el ADC, que normalmente será int16_t
            // las muestras podrán pertenecer a un solo canal o a varios intercalados. Si por ejemplo hay dos canales
            // (estereo) entonces las muestras estarán dispuestas de la forma L R L R L R L R L R...)
            //int16_t* buf_data_;
            int32_t* buf_data_;
            
            int overrun_count_;

            static void* ThreadFuncHelper(void* p);
            void* ThreadFunc();
            int AdcSetup();
            int Process();
      };
}

#endif  // THDANALYZER_THD_ANALYZER_H_



