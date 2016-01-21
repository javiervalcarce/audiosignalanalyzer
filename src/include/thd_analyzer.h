// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#ifndef THDANALYZER_THD_ANALYZER_H_
#define THDANALYZER_THD_ANALYZER_H_

#include <stdint.h>
#include <string>
#include <alsa/asoundlib.h>
#include <pthread.h>


namespace thd_analyzer {


      class SpectrumMask {
      public:
            SpectrumMask(int size);
            ~SpectrumMask();

            int size;
            double* value;
            int error_count;

            double last_trespassing_frequency;
            double last_trespassing_value;

      };


      /**
       * ThdAnalyzer.
       *
       * Captura y analiza N señales de audio provenientes de un dispositivo de captura de audio con interfaz ALSA . El
       * dispositivo de captura (ADC) del sistema puede ser, en general, multicanal. Lo más típico es que sea estéreo
       * con lo cual N = 2 canales (L y R) pero este software está preparado para trabajar con N canales incluyendo por
       * supuesto N = 1.
       *
       * Realiza las siguientes medidas:
       *
       * - Estimación de la densidad espectral de potencia por el método del periodograma (FFT al cuadrado).
       * - Localización de la frecuencia cuya potencia es máxima en el espectro.
       *
       * Con las medidas anteriores podemos usar este componente, por ejemplo, para ver el espectro de la señal o para
       * detectar la presencia o no de tonos enterrados en ruido y estimar su frecuencia y su amplitud.
       *
       *
       * TODO: Añadir más medidas como la distorsión armónica total (THD) incluyendo o no el ruido de fondo en toda la
       * banda, la relación señal a ruido (SNR), etc...
       *
       */
      class ThdAnalyzer {
      public:

            /**
             * Constructor.
             *
             * @param capture_device El dispositivo ALSA de captura de muestras de audio, por ejemplo "default" es el
             * dispositivo predeterminado del sistema y casi siempre representa la entrada de linea o micrófono, otros
             * dispositivos son por ejemplo "hw:0,0", "hw:1,0", "plughw:0,0", etc. Todo esto depende del sistema en
             * concreto, pruebe a ejecutar el programa "arecord -l" para ver una lista de dispositivos. Importante:
             * ajuste el volumen de grabación y active el capturador (unmute) con el programa alsamixer primero.
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
             * Configura el dispositivo ADC de captura y pone en marcha el hilo de procesado de señal.
             */
            int Init();



            /**
             * TODO: Da comienzo a la captura de muestras de audio y al análisis de las señales. Mientras el análisis esté en
             * marcha podremos llamar a las funciones Frequency() etc para obtener las medidas.
             */
            //int Start();

            /**
             * TODO: Detiene la captura de muestras de audio y el análisis. Las medidas no cambiarán, se mantendrán en el
             * último estado antes de llamar a Stop().
             */
            //int Stop();

            /**
             * TODO: Vacía los búferes de muestras, contadores, etc en genral todo el estado interno.
             */
            //int Reset();


            /**
             * Número de puntos de la FFT, que será siempre una potencia de 2. 
             *
             * TODO: Hacerlo configurable, actualmente está fijado en 4096 puntos.
             */
            int DftSize() const;

            /**
             * Frecuencia de muestreo en hercios (Hz).
             *
             * TODO: Hacerlo configurable, actualmente está fijado en 192000 Hz.
             */
            int SamplingFrequency() const;

            /**
             * Resolución analógica que tiene este analizador, expresado en Hz.
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

            // 
            const SpectrumMask* Mask(int channel) const { return channel_[channel].mask; }

            /**
             * Escribe en disco un fichero de texto listo para visualizar el spectro con gnuplot/octave/matlab.
             * Tiene DftSize() filas y N+1 columnas, siendo N el número de canales. La primera columna es la frecuencia 
             * analógica.
             */
            int GnuplotFileDump(std::string file_name);

            // MEDIDAS

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
             * Amplitud máxima del pedazo de señal, normalizada entre 0.0 y 1.0, para convertir esto en voltios hay que
             * saber el rango dinámico de entrada del conversor A/D.
             *
             * @param channel Número de canal. En un dispositivo estéreo el 0 es el izquierdo y el 1 es el derecho.
             */
            //double RmsAmplitude(int channel);
                       

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
             * Número de bloques procesados desde que el hilo interno de procesado se puso en marcha mediante Init()
             */
            int BlockCount() const;


      private:

            /**
             * Datos brutos y calculados de uno de los canales de entrada del ADC (normalmente será un ADC estéreo y
             * tendrá por tanto dos canales, L y R)
             */
            struct Channel {

                  // TODO: pthread_mutex_lock/unlock 
                  pthread_mutex_t lock;

                  // |block_size_| muestras convertidas a coma flotante y normalizadas en el intevalo real [-1.0, 1.0)
                  double* data;

                  // |block_size_| muestras de la densidad espectral de potencia estimada |X(k)|^2
                  double* pwsd;

                  // Valor del máximo del espectro
                  double peakv;

                  // Frecuencia en la que se produce ese máximo espectral.
                  int peakf;

                  // Valor RSM (Root Mean Square) del bloque de muestras capturado
                  //double rms;
                  SpectrumMask* mask;
            };

            // Número de canales del dispositivo ADC, lo normal es que sea estéreo: 2 canales.
            int channel_count_;

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

            Channel* channel_;
            

            double* Xre_;
            double* Xim_;

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



