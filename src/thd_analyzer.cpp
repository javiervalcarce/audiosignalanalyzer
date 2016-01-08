// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include "thd_analyzer.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <limits>
#include <cassert>
#include <alsa/asoundlib.h>


using namespace thd_analyzer;

static const int kChannelCount = 2;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::ThdAnalyzer(const char* pcm_capture_device) {

      device_ = pcm_capture_device;
      initialized_ = false;
      exit_thread_ = false;
      capture_handle_ = NULL;

      sample_rate_ = 192000; //48000;
      block_size_ = 4096; // 8192 TODO: Configurable.
      block_count_ = 0;

      // calculo de log2(block_size_)
      block_size_log2_ = 0;
      int n = 1;
      while (n < block_size_) {
            block_size_log2_++;
            n = n * 2;
      }


      memset(&thread_, 0, sizeof(pthread_t));

      pthread_attr_init(&thread_attr_);
      pthread_attr_setdetachstate(&thread_attr_, PTHREAD_CREATE_JOINABLE);
      //pthread_attr_getstacksize(&thread_attr_, &stacksize);


      // Formato nativo de las muestras, tal cual vienen el propio hardware.
      // 16 bits LE = 4 bytes por frame, un frame es una muestra del canal L y otra del canal R
      buf_data_ = new int16_t[kChannelCount * block_size_];
      buf_size_ = 0;
      
      channel_ = new Channel[kChannelCount];

      int c;

      for (c = 0; c < kChannelCount; c++) {
            // canal L
            channel_[c].data = new double[block_size_];
            channel_[c].pwsd = new double[block_size_]; // es real, |X(k)|^2
            channel_[c].peakf = 0;
            channel_[c].peakv = 0.0;
            channel_[c].rms = 0.0;
      }

      Xre_ = new double[block_size_];
      Xim_ = new double[block_size_];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::~ThdAnalyzer() {
      
      //assert(initialized_ == true);

      exit_thread_ = true;
      //pthread_cancel(thread_);  
      pthread_join(thread_, NULL);
      
      if (capture_handle_ != NULL) {
            snd_pcm_close(capture_handle_);
      }

      int c;
      for (c = 0; c < kChannelCount; c++) {
            delete[] channel_[c].data;
            delete[] channel_[c].pwsd;
      }

      delete[] channel_;

      delete[] Xre_;
      delete[] Xim_;

      delete[] buf_data_;
      
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Init() {

      assert(initialized_ == false);

      // Creación del hilo que lee y procesa las muestras de audio.
      int r;
      r = pthread_create(&thread_, &thread_attr_, ThdAnalyzer::ThreadFuncHelper, this);
      assert(r == 0);

      initialized_ = true;
      return 0;
}

/*
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Start() {
      assert(initialized_ == true);
      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Stop() {
      assert(initialized_ == true);
      return 0;
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::FindPeak(int channel) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);
      return channel_[channel].peakf;
}

/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::RmsAmplitude(int channel) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);
      return channel_[channel].rms;
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensity(int channel, int frequency_index) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);
      return channel_[channel].peakv;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensityDecibels(int channel, int frequency_index) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);

      return 10.0 * log10(channel_[channel].peakv);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::AnalogFrequency(int frequency_index) {
      assert(initialized_ == true);
      return ((double) frequency_index * sample_rate_) / (double) block_size_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::BlockCount() const {       
      assert(initialized_ == true);
      return block_count_; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::BlockSize() const {
      assert(initialized_ == true);
      return block_size_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* ThdAnalyzer::ThreadFuncHelper(void* p) {
      ThdAnalyzer* o = (ThdAnalyzer*) p;
      return o->ThreadFunc();
}

void* ThdAnalyzer::ThreadFunc() {

      int err;

      // Inicialización del dispositivo de captura de audio: Parámetros HW
      // ---------------------------------------------------------------------------------------------------------------      
      snd_pcm_hw_params_t* hw_params = NULL;

	//printf("Opening %s...\n", device_.c_str());

      // Blocking MODE
      if ((err = snd_pcm_open(&capture_handle_, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            fprintf(stderr, "cannot open audio device %s (%s)\n", device_.c_str(), snd_strerror(err));
            return NULL;
      }
		   
      if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
            fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
            return NULL;
      }
				 
      if ((err = snd_pcm_hw_params_any(capture_handle_, hw_params)) < 0) {
            fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params_set_access(capture_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            fprintf(stderr, "cannot set access type (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params_set_format(capture_handle_, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            fprintf(stderr, "cannot set sample format (%s)\n", snd_strerror(err));
            return NULL;
      }

      if ((err = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params, (unsigned int*) &sample_rate_, NULL)) < 0) {
            fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params_set_channels(capture_handle_, hw_params, 2)) < 0) {
            fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params(capture_handle_, hw_params)) < 0) {
            fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      snd_pcm_hw_params_free(hw_params);
      hw_params = NULL;

      // Inicialización del dispositivo de captura de audio:
      // Parámetros SW
      // ---------------------------------------------------------------------------------------------------------------
      // tell ALSA to wake us up whenever block_size_ or more frames
      // of playback data can be delivered. Also, tell ALSA that we'll
      // start the device ourselves.
      snd_pcm_sw_params_t* sw_params = NULL;
      if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
            fprintf(stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror (err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_current(capture_handle_, sw_params)) < 0) {
            fprintf(stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_avail_min(capture_handle_, sw_params, block_size_)) < 0) {
            fprintf(stderr, "cannot set minimum available count (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_start_threshold(capture_handle_, sw_params, 0U)) < 0) {
            fprintf(stderr, "cannot set start mode (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params(capture_handle_, sw_params)) < 0) {
            fprintf(stderr, "cannot set software parameters (%s)\n", snd_strerror(err));
            return NULL;
      }

      // ???
      snd_pcm_sw_params_free(sw_params);
      sw_params = NULL;



      // ---------------------------------------------------------------------------------------------------------------
      if ((err = snd_pcm_prepare(capture_handle_)) < 0) {
            fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
            return NULL;
      }

      while (1) {

            int r;
            int frames = block_size_;
            int16_t* b = buf_data_;

            do {
                  // Bloqueante
                  r = snd_pcm_readi(capture_handle_, b, frames);
                  
                  // 2 int16_t por frame
                  b += r * 2; 
                  frames -= r;
                  
                  // DEBUG
                  //if (r == 0) printf("Error r = 0\n");
                  //if (r <  0) printf("Error r < 0\n");

            } while (r >= 1 && frames > 0);

            int i;

            // Conversión de formato: de int16_t a coma flotante 
            for (i = 0; i < block_size_; i++) {
                  channel_[0].data[i] = ((double) buf_data_[2 * i + 0]) / 32678.0;
                  channel_[1].data[i] = ((double) buf_data_[2 * i + 1]) / 32678.0;
            }


            /*
            // Señales sintéticas para depuración
            // Debug ---------------------------------------------------------------
            static int z = 0;
            for (i = 0; i < block_size_; i++) {
                  channel_[0].data[i] = 1.00 * cos(4687.5 * M_PI / (sample_rate_ / 2) * z);
                  channel_[1].data[i] = 0.1  * cos(4687.5 * M_PI / (sample_rate_ / 2) * z);
                  z++;
            }
            // Debug ---------------------------------------------------------------
            */

            Process();
            block_count_++;

            if (exit_thread_ == true) {
                  break;
            }
      }

      snd_pcm_close(capture_handle_);
      capture_handle_ = NULL;

      return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Process() {


      int c;
      int n;
      int k;

      int N;

      double* re;
      double* im;

      //
      // Proceso los canales de 2 en 2 calculando 2 FFT reales por el precio de una FFT compleja
      //
      // Dos FFT reales de N puntos por el precio de una FFT compleja de N puntos. Ver:
      // Proakis, Manolakis, "Tratamiento digital de señales", 3º ediciion, ISBN 84-8322-000-8, capítulo 6, pág 485.
      //
      for (c = 0; c < kChannelCount; c += 2) {
            
            N = block_size_;
            re = channel_[c + 0].data;
            im = channel_[c + 1].data;

            FFT(1, block_size_log2_, re, im);
            
            for (k = 0; k < N; k++) {
                  
                  // Señal c + 0
                  Xre_[k] =  ( re[k] + re[N-k] );
                  Xim_[k] =  ( im[k] - im[N-k] );
                  channel_[c + 0].pwsd[k] = Xre_[k]*Xre_[k] + Xim_[k]*Xim_[k]; // SQRT

                  // Señal c + 1
                  Xre_[k] =  ( im[k] + im[N-k] );
                  Xim_[k] =  ( re[N-k] - re[k] ); 
                  channel_[c + 1].pwsd[k] = Xre_[k]*Xre_[k] + Xim_[k]*Xim_[k]; // SQRT
            }
      }

      // Nota: channel_[c + 1].pwsd[k] es la amplitud al cuadrado de la frecuencia k-ésima de la señal. Por ejemplo: un
      // tono 0.5*cos(wn) lo detecta con amplitud 0.25, hay que aplicar la raiz cuadrada si queremos obtener la
      // amplitud, esto se hace así para reducir carga computacional (no hay que llamar a sqrt() en cada muestra).


      int max_index;
      double max_value;

      // Proceso canal por canal
      // Búque del máximo absoluto (peakv) y la posición en la que está (peakf)
      for (c = 0; c < kChannelCount; c++) {
 
            // Conversión a dB - Desactivada
            /*
            for (k = 0; k < block_size_ / 2; k++) {                  
                  channel_[c].pwsd[k] = 10.0 * log10(channel_[c].pwsd[k]);
                  assert(errno != EDOM);
                  assert(errno != ERANGE);
            }
            */
            
            // Búsqueda del máximo absoluto
            max_value = 1e-16;  // Umbral mínimo de detección 
            max_index = -1;     // -1 significa que ninguna de las frecuencias tiene una potencia que supere el umbral prefijado.


            // modulo al cuadrado, solo frecuencias positivas
            for (k = 0; k < block_size_ / 2; k++) {
                  if (channel_[c].pwsd[k] > max_value) {
                        max_value = channel_[c].pwsd[k];
                        max_index = k;
                  }
            }
            

            channel_[c].peakv = max_value; 
            channel_[c].peakf = max_index;
      }

      return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::DumpToTextFile(const char* file_name, int file_index, double* x, double* y, int size) {
      // Impresión por pantalla de la DFT^2 en dB (10log10)
      return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThdAnalyzer::FFT(short int dir, long m,double *x,double *y) {

      long n,i,i1,j,k,i2,l,l1,l2;
      double c1,c2,tx,ty,t1,t2,u1,u2,z;

      // Calculate the number of points
      n = 1;
      for (i=0;i<m;i++) 
            n *= 2;

      // bit reversal 
      i2 = n >> 1;
      j = 0;
      for (i=0;i<n-1;i++) {
            if (i < j) {
                  tx = x[i];
                  ty = y[i];
                  x[i] = x[j];
                  y[i] = y[j];
                  x[j] = tx;
                  y[j] = ty;
            }
            k = i2;
            while (k <= j) {
                  j -= k;
                  k >>= 1;
            }
            j += k;
      }

      // Compute the FFT
      c1 = -1.0; 
      c2 = 0.0;
      l2 = 1;
      for (l=0;l<m;l++) {
            l1 = l2;
            l2 <<= 1;
            u1 = 1.0; 
            u2 = 0.0;
            for (j=0;j<l1;j++) {
                  for (i=j;i<n;i+=l2) {
                        i1 = i + l1;
                        t1 = u1 * x[i1] - u2 * y[i1];
                        t2 = u1 * y[i1] + u2 * x[i1];
                        x[i1] = x[i] - t1; 
                        y[i1] = y[i] - t2;
                        x[i] += t1;
                        y[i] += t2;
                  }
                  z =  u1 * c1 - u2 * c2;
                  u2 = u1 * c2 + u2 * c1;
                  u1 = z;
            }
            c2 = sqrt((1.0 - c1) / 2.0);
            if (dir == 1) 
                  c2 = -c2;
            c1 = sqrt((1.0 + c1) / 2.0);
      }


      // Scaling for forward transform
      if (dir == 1) {
            for (i=0;i<n;i++) {
                  x[i] /= n;
                  y[i] /= n;
            }
      }
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
