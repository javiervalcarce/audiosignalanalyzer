// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include "thd_analyzer.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <limits>
#include <cassert>

// Esta es la única dependecia interesante
#include <alsa/asoundlib.h>
#include <pthread.h>


using namespace thd_analyzer;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SpectrumMask::SpectrumMask(int msize) {
      size = msize;
      value = new double[size];
      int i;

      // TODO: Máscara configurable.
      for (i = 0; i < size; i++) {
            value[i] = -7.0; // dB
      }

      for (i = 18; i < 25; i++) { // banda correspondiente a un 1 kHz (900, 1100)
            value[i] = 0.0; // dB
      }

      error_count = 0;
      last_trespassing_frequency = nan("");
      last_trespassing_value = nan("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SpectrumMask::~SpectrumMask() {
      delete[] value;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::ThdAnalyzer(const char* pcm_capture_device) {

      device_ = pcm_capture_device;
      channel_count_ = 2;

      initialized_ = false;
      exit_thread_ = false;
      capture_handle_ = NULL;
     

      // TODO: Hacer esto configurable.
      sample_rate_ = 192000; //48000;
      block_size_ = 4096; // 8192 
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
      buf_data_ = new int16_t[channel_count_ * block_size_];
      buf_size_ = 0;
      
      channel_ = new Channel[channel_count_];

      int c;

      for (c = 0; c < channel_count_; c++) {

            pthread_mutex_init(&(channel_[c].lock), NULL);

            channel_[c].data = new double[block_size_];
            channel_[c].pwsd = new double[block_size_]; // es real, |X(k)|^2
            channel_[c].peakf = 0;
            channel_[c].peakv = 0.0;
            //channel_[c].rms = 0.0;
            channel_[c].mask = new SpectrumMask(block_size_);
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
      for (c = 0; c < channel_count_; c++) {
            delete[] channel_[c].data;
            delete[] channel_[c].pwsd;
            delete channel_[c].mask;
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
      assert(initialized_ == true);
      assert(channel < channel_count_);
      return channel_[channel].rms;
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensity(int channel, int frequency_index) {
      assert(initialized_ == true);
      assert(channel < channel_count_);
      assert(frequency_index < block_size_);
      
      return channel_[channel].pwsd[frequency_index];
      //return channel_[channel].peakv;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensityDecibels(int channel, int frequency_index) {
      assert(initialized_ == true);
      assert(channel < channel_count_);
      assert(frequency_index < block_size_);

      return 10.0 * log10(channel_[channel].pwsd[frequency_index]);
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
int ThdAnalyzer::DftSize() const {
      assert(initialized_ == true);
      return block_size_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::SamplingFrequency() const {
      assert(initialized_ == true);
      return sample_rate_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::AnalogResolution() const {
      assert(initialized_ == true);
      return (double) sample_rate_ / (double) block_size_;
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

/*
      // Restrict a configuration space to contain only real hardware rates
      if ((err = snd_pcm_hw_params_set_rate_resample(capture_handle_, hw_params, 0)) < 0) {
            fprintf(stderr, "cannot disable software resampler (%s)\n", snd_strerror(err));
            return NULL;
      }
*/

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
            pthread_mutex_lock(&(channel_[0].lock));
            pthread_mutex_lock(&(channel_[1].lock));

            for (i = 0; i < block_size_; i++) {
                  channel_[0].data[i] = ((double) buf_data_[2 * i + 0]) / 32768.0; // 20160121 JVG: Estaba mal, es 32768
                  channel_[1].data[i] = ((double) buf_data_[2 * i + 1]) / 32768.0;

                  //assert(channel_[0].data[i] <= 1.0);
                  //assert(channel_[1].data[i] <= 1.0);
            }
            
            // Señales sintéticas para depuración
            /*
            static int z = 0;
            for (i = 0; i < block_size_; i++) {
                  // Frecuencia en Hz
                  channel_[0].data[i] = 1.00 * cos(20000 * M_PI / (sample_rate_ / 2) * z);
                  channel_[1].data[i] = 1.00 * cos(80000 * M_PI / (sample_rate_ / 2) * z);
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
      for (c = 0; c < channel_count_; c += 2) {
            
            re = channel_[c + 0].data;
            im = channel_[c + 1].data;

            pthread_mutex_lock(&(channel_[c + 0].lock));
            pthread_mutex_lock(&(channel_[c + 1].lock));

            // Esta función calcula los coeficientes "in-place", sobreescribiendo los valores previous de señal.
            FFT(1, block_size_log2_, re, im);

            N = block_size_;
 
            for (k = 0; k < N; k++) {
                  
                  // Señal c + 0
                  Xre_[k] =  ( re[k] + re[N-k] );
                  Xim_[k] =  ( im[k] - im[N-k] );
                  channel_[c + 0].pwsd[k] = Xre_[k]*Xre_[k] + Xim_[k]*Xim_[k]; // SQRT

                  //channel_[c + 0].pwsd[k] = channel_[c + 0].pwsd[k] / block_size_ / block_size_;
 
                  // Señal c + 1
                  Xre_[k] =  ( im[k] + im[N-k] );
                  Xim_[k] =  ( re[N-k] - re[k] ); 
                  channel_[c + 1].pwsd[k] = Xre_[k]*Xre_[k] + Xim_[k]*Xim_[k]; // SQRT

                  //channel_[c + 1].pwsd[k] = channel_[c + 1].pwsd[k] / block_size_ / block_size_;
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
            channel_[c].mask->error_count = 0;
            channel_[c].mask->last_trespassing_frequency = nan("");
            channel_[c].mask->last_trespassing_value = nan("");

            // block_size_ / 2 = solo frecuencias positivas
            for (k = 0; k < block_size_ / 2; k++) {

                  double db = 10.0 * log10(channel_[c].pwsd[k]);

                  if (db > channel_[c].mask->value[k]) {
                        channel_[c].mask->error_count++;
                        channel_[c].mask->last_trespassing_frequency = AnalogFrequency(k);
                        channel_[c].mask->last_trespassing_value = db;
                  }
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
