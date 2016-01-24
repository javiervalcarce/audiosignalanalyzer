// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <limits>
#include <cassert>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "thd_analyzer.h"
#include "fft.h"

using namespace thd_analyzer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::ThdAnalyzer(const char* pcm_capture_device, int sampling_rate, int log2_block_size) {

      pthread_mutex_init(&lock_, NULL);
      pthread_cond_init(&can_continue_, NULL);  

      device_ = pcm_capture_device;
      channel_count_ = 2;
      internal_state_ = kNotInitialized;

      exit_thread_ = false;
      capture_handle_ = NULL;
     
      // TODO: Hacer esto configurable. De momento no ha conseguido configurar este parámetro
      sample_rate_ = sampling_rate; //192000;
      block_size_log2_ = log2_block_size;
      block_count_ = 0;

      
      block_size_  = 1;
      for (int i = 0; i < block_size_log2_; i++) {
            block_size_ *= 2;
      }

      memset(&thread_, 0, sizeof(pthread_t));

      pthread_attr_init(&thread_attr_);
      pthread_attr_setdetachstate(&thread_attr_, PTHREAD_CREATE_JOINABLE);
      //pthread_attr_getstacksize(&thread_attr_, &stacksize);

      // Formato nativo de las muestras, tal cual vienen el propio hardware.
      // 16 bits LE = 4 bytes por frame, un frame es una muestra del canal L y otra del canal R
      buf_data_ = new int16_t[channel_count_ * block_size_];
      buf_size_ = 0;
      overrun_count_ = 0;

      channel_ = new Channel[channel_count_]; // TODO, en el constructor.
      for (int c = 0; c < channel_count_; c++) {

            pthread_mutex_init(&(channel_[c].lock), NULL);
            channel_[c].size = block_size_;
            channel_[c].data = new double[block_size_];
            channel_[c].pwsd = new double[block_size_]; // es real, |X(k)|^2
            channel_[c].peakf = 0;
            channel_[c].peakv = 0.0;
            //channel_[c].rms = 0.0;
            channel_[c].mask = new SpectrumMask(sample_rate_, block_size_);
            //channel_[c].mask = NULL;
      }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::~ThdAnalyzer() {
      
      exit_thread_ = true;
      //pthread_cancel(thread_);  
      pthread_join(thread_, NULL);
      
      if (capture_handle_ != NULL) {
            snd_pcm_close(capture_handle_);
      }

      int c;
      for (c = 0; c < channel_count_; c++) {
            delete[] channel_[c].data;
            delete[] channel_[c].pwsd;

            if (channel_[c].mask != NULL) {
                  delete channel_[c].mask;
            }
      }

      delete[] channel_;
      delete[] buf_data_;
      
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Init() {

      assert(internal_state_ == kNotInitialized);

      if (AdcSetup() != 0) {
            return 1;
      };

      // Creación del hilo que lee y procesa las muestras de audio.
      int r;
      r = pthread_create(&thread_, &thread_attr_, ThdAnalyzer::ThreadFuncHelper, this);
      if (r != 0) {
            return 1;
      }

      internal_state_ = kStopped;
      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Start() {
      assert(internal_state_ != kNotInitialized);

      pthread_mutex_lock(&lock_);
      internal_state_ = kRunning;
      pthread_cond_signal(&can_continue_);
      pthread_mutex_unlock(&lock_);

      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Stop() {
      assert(internal_state_ != kNotInitialized);

      pthread_mutex_lock(&lock_);
      internal_state_ = kStopped;
      pthread_cond_signal(&can_continue_);
      pthread_mutex_unlock(&lock_);

      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::AdcSetup() {

      int err;

      // Inicialización del dispositivo de captura de audio: Parámetros HW
      // ---------------------------------------------------------------------------------------------------------------      
      snd_pcm_hw_params_t* hw_params = NULL;

      if ((err = snd_pcm_open(&capture_handle_, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            fprintf(stderr, "cannot open audio device %s (%s)\n", device_.c_str(), snd_strerror(err));
            return 1;
      }
		   
      if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
            fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
            return 1;
      }
				 
      if ((err = snd_pcm_hw_params_any(capture_handle_, hw_params)) < 0) {
            fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
            return 1;
      }
	
      if ((err = snd_pcm_hw_params_set_access(capture_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            fprintf(stderr, "cannot set access type (%s)\n", snd_strerror(err));
            return 1;
      }
	
      if ((err = snd_pcm_hw_params_set_format(capture_handle_, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            fprintf(stderr, "cannot set sample format (%s)\n", snd_strerror(err));
            return 1;
      }


      // Restrict a configuration space to contain only real hardware rates
      if ((err = snd_pcm_hw_params_set_rate_resample(capture_handle_, hw_params, 0)) < 0) {
            fprintf(stderr, "cannot disable software resampler (%s)\n", snd_strerror(err));
            return 1;
      }
      
      /*
        int dir;
        if ((err = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params, (unsigned int*) &sample_rate_, &dir)) < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
        return 1;
        }
      */

      if ((err = snd_pcm_hw_params_set_rate_min(capture_handle_, hw_params, (unsigned int*) &sample_rate_, NULL)) < 0) {
            fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
            return 1;
      }
      
      if ((err = snd_pcm_hw_params_set_channels(capture_handle_, hw_params, 2)) < 0) {
            fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
            return 1;
      }
	
      if ((err = snd_pcm_hw_params(capture_handle_, hw_params)) < 0) {
            fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
            return 1;
      }
	
      snd_pcm_hw_params_free(hw_params);
      hw_params = NULL;

      // Inicialización del dispositivo de captura de audio: Parámetros SW
      // ---------------------------------------------------------------------------------------------------------------
      snd_pcm_sw_params_t* sw_params = NULL;

      if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
            fprintf(stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror (err));
            return 1;
      }
      if ((err = snd_pcm_sw_params_current(capture_handle_, sw_params)) < 0) {
            fprintf(stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
            return 1;
      }
      if ((err = snd_pcm_sw_params_set_avail_min(capture_handle_, sw_params, block_size_)) < 0) {
            fprintf(stderr, "cannot set minimum available count (%s)\n", snd_strerror(err));
            return 1;
      }
      if ((err = snd_pcm_sw_params_set_start_threshold(capture_handle_, sw_params, 0U)) < 0) {
            fprintf(stderr, "cannot set start mode (%s)\n", snd_strerror(err));
            return 1;
      }
      if ((err = snd_pcm_sw_params(capture_handle_, sw_params)) < 0) {
            fprintf(stderr, "cannot set software parameters (%s)\n", snd_strerror(err));
            return 1;
      }

      snd_pcm_sw_params_free(sw_params);
      sw_params = NULL;


      

      return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::FindPeak(int channel) {
      assert(internal_state_ != kNotInitialized);
      assert(channel < channel_count_);
      return channel_[channel].peakf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::SNRI(int channel, double f1, double f2) {

      // Estimación de la SNRI considerando la banda (af1, af2) = (900, 1100) Hz
      double analog_resolution = (double) sample_rate_ / (double) block_size_; 
      int d1 = (int) floor(f1 / analog_resolution);
      int d2 = (int) ceil (f2 / analog_resolution);

      double* psd = channel_[channel].pwsd;

      double snri;
      double acc;
      double sig;
      
      sig = 0.0;
      acc = 0.0;

      int i;
      for (i = 0; i < block_size_; i++) {
            acc += psd[i]; // !! CONCURRENCIA !! TODO: pthread_mutex_lock/unlock
      }
      
      // d1 y d2 ambos inclusive
      for (i = d1; i < d2 + 1; i++) {
            sig += psd[i]; // !! CONCURRENCIA !! TODO: pthread_mutex_lock/unlock
      }

      acc -= sig;
      snri = sig / acc;
      return 10 * log10(snri);
      //return snri;
}

/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::RmsAmplitude(int channel) {
      assert(internal_state_ != kNotInitialized);
      assert(channel < channel_count_);
      return channel_[channel].rms;
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensity(int channel, int frequency_index) {
      assert(internal_state_ != kNotInitialized);
      assert(channel < channel_count_);
      assert(frequency_index < block_size_);
      
      return channel_[channel].pwsd[frequency_index];
      //return channel_[channel].peakv;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensityDecibels(int channel, int frequency_index) {
      assert(internal_state_ != kNotInitialized);
      assert(channel < channel_count_);
      assert(frequency_index < block_size_);

      return 10.0 * log10(channel_[channel].pwsd[frequency_index]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::AnalogFrequency(int frequency_index) {
      assert(internal_state_ != kNotInitialized);
      return ((double) frequency_index * sample_rate_) / (double) block_size_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::BlockCount() const {       
      assert(internal_state_ != kNotInitialized);
      return block_count_; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::DftSize() const {
      assert(internal_state_ != kNotInitialized);
      return block_size_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::SamplingFrequency() const {
      assert(internal_state_ != kNotInitialized);
      return sample_rate_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::AnalogResolution() const {
      assert(internal_state_ != kNotInitialized);
      return (double) sample_rate_ / (double) block_size_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* ThdAnalyzer::ThreadFuncHelper(void* p) {
      ThdAnalyzer* o = (ThdAnalyzer*) p;
      return o->ThreadFunc();
}

void* ThdAnalyzer::ThreadFunc() {
      
      /*
      if (AdcSetup() != 0) {
            return NULL;
      };
      */
      int err;
      if ((err = snd_pcm_prepare(capture_handle_)) < 0) {
            fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
            goto fatal_error;
      }

      while (1) {

            // Quedo a la espera de que la aplicación principal me permita continuar
            pthread_mutex_lock(&lock_);                  
            while (internal_state_ != kRunning) {
                  pthread_cond_wait(&can_continue_, &lock_);
            }
            pthread_mutex_unlock(&lock_);

            int r;
            int frames = block_size_;
            int16_t* b = buf_data_;

            do {
                  // Bloqueante
                  r = snd_pcm_readi(capture_handle_, b, frames);
                  if (r == -EPIPE) {
                        // OVERRUN
                        overrun_count_++;
                        
                        r = snd_pcm_recover(capture_handle_, r, 0);
                        if (r < 0) {
                              goto fatal_error;
                        }
                        continue;
                  }

                  // 2 int16_t por frame
                  b += r * 2; 
                  frames -= r;

            } while (r >= 1 && frames > 0);

            int i;

            // Conversión de formato: de int16_t a coma flotante 
            pthread_mutex_lock(&(channel_[0].lock));
            pthread_mutex_lock(&(channel_[1].lock));

            for (i = 0; i < block_size_; i++) {
                  // Normalización de las muestras en el intervalo semiabierto [-1.0, 1.0)
                  channel_[0].data[i] = ((double) buf_data_[2 * i + 0]) / 32768.0; 
                  channel_[1].data[i] = ((double) buf_data_[2 * i + 1]) / 32768.0;
/*
                  assert(channel_[0].data[i] <   1.0);
                  assert(channel_[0].data[i] >= -1.0);
                  assert(channel_[1].data[i] <   1.0);
                  assert(channel_[1].data[i] >= -1.0);
*/
                  
            }
            
            // Señales sintéticas para depuración
            /*
            static int z = 0;
            for (i = 0; i < block_size_; i++) {
                  // Frecuencia en Hz
                  channel_[0].data[i] = 0.01 * cos(1000 * M_PI / (sample_rate_ / 2) * z);
                  channel_[1].data[i] = 0.95 * sin(2000 * M_PI / (sample_rate_ / 2) * z);
                  z++;                  
            }
            */
            pthread_mutex_unlock(&(channel_[0].lock));
            pthread_mutex_unlock(&(channel_[1].lock));

            Process();
            block_count_++;

            if (exit_thread_ == true) {
                  break;
            }
      }

      internal_state_ = kStopped;
      snd_pcm_close(capture_handle_);
      capture_handle_ = NULL;
      return NULL;

 fatal_error:
      internal_state_ = kCrashed;
      snd_pcm_close(capture_handle_);
      capture_handle_ = NULL;
      return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Process() {

      int c;
      int k;
      int N;
      double* re;
      double* im;
      double fftnorm;
      double Xre;
      double Xim;

      N = block_size_;
      
      // La rutina de cálculo de la FFT que utilizo no normaliza los coeficientes de la DFT de la forma estándar.
      //fftnorm = 1.20;
      fftnorm = 2.0 / (1 + sqrt(2));

      //
      // Proceso los canales de 2 en 2 calculando 2 FFT reales por el precio de una FFT compleja
      //
      // Dos FFT reales de N puntos por el precio de una FFT compleja de N puntos. Ver:
      // Proakis, Manolakis, "Tratamiento digital de señales", 3º ediciion, ISBN 84-8322-000-8, capítulo 6, pág 485.
      //
      for (c = 0; c < channel_count_; c += 2) {
      
            pthread_mutex_lock(&(channel_[c + 0].lock));
            pthread_mutex_lock(&(channel_[c + 1].lock));

            re = channel_[c + 0].data;
            im = channel_[c + 1].data;

            // Esta función calcula los coeficientes "in-place", sobreescribiendo los valores previos de señal.
            thd_analyzer::FFT(1, block_size_log2_, re, im);

            for (k = 0; k < N; k++) {
                  
                  // Señal c + 0
                  Xre =  ( re[k] + re[N-k] ) / fftnorm;
                  Xim =  ( im[k] - im[N-k] ) / fftnorm;
                  channel_[c + 0].pwsd[k] = Xre*Xre + Xim*Xim; // NO SQRT
                  //channel_[c + 0].pwsd[k] = sqrt(Xre*Xre + Xim*Xim); // NO SQRT
                  //assert(channel_[c + 0].pwsd[k] <= 1.000001);
 
                  // Señal c + 1
                  Xre =  ( im[k] + im[N-k] ) / fftnorm;
                  Xim =  ( re[N-k] - re[k] ) / fftnorm;
                  channel_[c + 1].pwsd[k] = Xre*Xre + Xim*Xim; // NO SQRT
                  //channel_[c + 1].pwsd[k] = sqrt(Xre*Xre + Xim*Xim); // NO SQRT

                  //assert(channel_[c + 0].pwsd[k] <= 1.000001);
            }

            pthread_mutex_unlock(&(channel_[c + 0].lock));
            pthread_mutex_unlock(&(channel_[c + 1].lock));
      }

      // pwsd[k] es la amplitud al cuadrado de la frecuencia k-ésima de la señal. Por ejemplo: un tono 0.5*cos(wn) lo
      // detecta con amplitud 0.25. Hay que aplicar la raiz cuadrada si queremos obtener la amplitud, esto se hace así
      // para reducir carga computacional (no hay que llamar a sqrt() en cada muestra).

      int max_index;
      double max_value;

      // Proceso canal por canal
      // Búque del máximo absoluto (peakv) y la posición en la que está (peakf)
      for (c = 0; c < channel_count_; c++) {
           
            // *** PROCESADO: Búsqueda del máximo absoluto
            max_value = 1e-16;  // Umbral mínimo de detección 
            max_index = -1;     // -1 significa que ninguna de las frecuencias tiene una potencia que supere el umbral prefijado.

            // block_size_ / 2 = solo frecuencias positivas
            for (k = 0; k < block_size_ / 2; k++) {
                  if (channel_[c].pwsd[k] > max_value) {
                        max_value = channel_[c].pwsd[k];
                        max_index = k;
                  }
            }
            
            channel_[c].peakv = max_value; 
            channel_[c].peakf = max_index;
            
            // *** PROCESADO: Comprobación de la máscara
            if (channel_[c].mask != NULL) {

                  //pthread_mutex_lock(&(channel_[c + 0].lock));

                  bool first = true;
                  SpectrumMask* m = channel_[c].mask;
                  
                  m->error_count = 0;
                  m->last_trespassing_frequency = nan("");
                  m->last_trespassing_value = nan("");
                  m->first_trespassing_frequency = nan("");
                  m->first_trespassing_value = nan("");

                  // block_size_ / 2 = solo frecuencias positivas
                  for (k = 0; k < block_size_ / 2; k++) {
                        
                        double db = 10.0 * log10(channel_[c].pwsd[k]);
                        
                        if (db > m->value[k]) {
                              m->error_count++;
                              if (first == true) {
                                    first = false;
                                    m->first_trespassing_frequency = AnalogFrequency(k);
                                    m->first_trespassing_value = db;
                                    m->last_trespassing_frequency = AnalogFrequency(k);
                                    m->last_trespassing_value = db;
                              } else {
                                    m->last_trespassing_frequency = AnalogFrequency(k);
                                    m->last_trespassing_value = db;
                              }
                        }
                  }

                  //pthread_mutex_unlock(&(channel_[c + 0].lock));
            }

      }

      return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::GnuplotFileDump(std::string file_name) {
      FILE* fd;

      fd = fopen(file_name.c_str(), "w");
      if (fd == NULL) {
            return 1;
      }
      
      int i;
      int j;

      // LOCK
      for (j = 0; j < channel_count_; j++) {
            pthread_mutex_lock(&(channel_[j].lock));
      }

      for (i = 0; i < block_size_ / 2; i++) {

            // Columna 1 - Frecuencia analógica
            fprintf(fd, "%09.2f ", AnalogFrequency(i));

            for (j = 0; j < channel_count_; j++) {
                  fprintf(fd, "%010.8f ", channel_[j].pwsd[i]);
            }

            fprintf(fd, "\n");
      }

      // UNLOCK
      for (j = 0; j < channel_count_; j++) {
            pthread_mutex_unlock(&(channel_[j].lock));
      }
      
      fclose(fd);
      return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
