// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include "thd_analyzer.h"

#include <cstdio>
#include <cstring>
#include <cmath>
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
      block_size_ = 8192; //4096;
      block_count_ = 0;

      // calculo de log2(block_size_)
      block_size_log2_ = 0;
      int n = 1;
      while (n < block_size_) {
            block_size_log2_++;
            n = n * 2;
      }

      // Formato nativo de las muestras, tal cual vienen el propio hardware.
      // 16 bits LE = 4 bytes por frame, un frame es una muestra del canal L y otra del canal R
      buf_data_ = new int16_t[kChannelCount * block_size_];
      buf_size_ = 0;
      
      channel_ = new Channel[kChannelCount];

      int c;

      for (c = 0; c < kChannelCount; c++) {
            // canal L
            channel_[c].data = new double[block_size_];
            channel_[c].pwsd = new double[block_size_];
            channel_[c].peakf = 0;
            channel_[c].peakv = 0.0;
            channel_[c].rms = 0.0;
      }

      // Buferes temporales
      im   = new double[block_size_];
      abs2 = new double[block_size_];


}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::~ThdAnalyzer() {
      exit_thread_ = true;
      //pthread_join() TODO

      int c;

      for (c = 0; c < kChannelCount; c++) {
            delete[] channel_[c].data;
            delete[] channel_[c].pwsd;
      }

      delete[] channel_;

      delete[] abs2;
      delete[] im;

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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::FindPeak(int channel) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);
      return channel_[channel].peakf;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::RmsAmplitude(int channel) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);
      return channel_[channel].rms;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::PowerSpectralDensity(int channel, int frequency_index) {
      assert(initialized_ == true);
      assert(channel < kChannelCount);
      return channel_[channel].peakv;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::AnalogFrequency(int frequency_index) {
      assert(initialized_ == true);
      return ((double) frequency_index * sample_rate_) / block_size_;      
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
      snd_pcm_hw_params_t* hw_params;

	//printf("Opening %s...\n", device_.c_str());

      // Blocking MODE
      if ((err = snd_pcm_open(&capture_handle_, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            fprintf (stderr, "cannot open audio device %s (%s)\n", device_.c_str(), snd_strerror(err));
            return NULL;
      }
		   
      if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
            fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
            return NULL;
      }
				 
      if ((err = snd_pcm_hw_params_any(capture_handle_, hw_params)) < 0) {
            fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params_set_access(capture_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            fprintf (stderr, "cannot set access type (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params_set_format(capture_handle_, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            fprintf (stderr, "cannot set sample format (%s)\n", snd_strerror(err));
            return NULL;
      }

      if ((err = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params, (unsigned int*) &sample_rate_, NULL)) < 0) {
            fprintf (stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params_set_channels(capture_handle_, hw_params, 2)) < 0) {
            fprintf (stderr, "cannot set channel count (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      if ((err = snd_pcm_hw_params(capture_handle_, hw_params)) < 0) {
            fprintf (stderr, "cannot set parameters (%s)\n", snd_strerror(err));
            return NULL;
      }
	
      snd_pcm_hw_params_free (hw_params);

      // Inicialización del dispositivo de captura de audio:
      // Parámetros SW
      // ---------------------------------------------------------------------------------------------------------------
      // tell ALSA to wake us up whenever block_size_ or more frames
      // of playback data can be delivered. Also, tell ALSA that we'll
      // start the device ourselves.
      snd_pcm_sw_params_t *sw_params;

      if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
            fprintf (stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror (err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_current(capture_handle_, sw_params)) < 0) {
            fprintf (stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_avail_min(capture_handle_, sw_params, block_size_)) < 0) {
            fprintf (stderr, "cannot set minimum available count (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_start_threshold(capture_handle_, sw_params, 0U)) < 0) {
            fprintf (stderr, "cannot set start mode (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params(capture_handle_, sw_params)) < 0) {
            fprintf (stderr, "cannot set software parameters (%s)\n", snd_strerror(err));
            return NULL;
      }

      // ---------------------------------------------------------------------------------------------------------------
      if ((err = snd_pcm_prepare(capture_handle_)) < 0) {
            fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
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
                  if (r == 0) printf("Error r = 0\n");
                  if (r <  0) printf("Error r < 0\n");

            } while (r >= 1 && frames > 0);

            int i;
            // Conversión de formato: de int16_t a coma flotante 
            for (i = 0; i < block_size_; i++) {
                  channel_[0].data[i] = ((double) buf_data_[2 * i + 0]) / 32678.0;
                  channel_[1].data[i] = ((double) buf_data_[2 * i + 1]) / 32678.0;
            }

            // Debug
            /*
            static int z = 0;
            for (i = 0; i < block_size_; i++) {
                  channel_[0].data[i] = cos(30000.0 * M_PI / (sample_rate_ / 2) * z);
                  channel_[1].data[i] = cos(30000.0 * M_PI / (sample_rate_ / 2) * z);
                  z++;
            }
            */

            Process();
            block_count_++;

            if (exit_thread_ == true) {
                  break;
            }
      }

      snd_pcm_close(capture_handle_);
      return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Process() {

      int i;
      int c;
      int max_index;
      double max_value;

      // TODO: Dos FFT reales por el precio de una FFT compleja.
      for (c = 0; c < kChannelCount; c++) {
            // Canal c-esimo
            for (i = 0; i < block_size_; i++) im[i] = 0.0; 
            FFT(1, block_size_log2_, channel_[c].data, im);
            
            max_index = 0;
            max_value = 0.0;

            // abs2 no hace falta.
            // solo frecuencias positivasgfv 
            for (i = 0; i < block_size_/2; i++) {
                  abs2[i] = channel_[c].data[i] * channel_[c].data[i] + im[i]*im[i];
                  if (abs2[i] >= max_value) {
                        max_value = abs2[i];
                        max_index = i;
                  }
            }
            
            // channel_frequency_[0] = (max_index - block_size_ / 2) * sample_rate_ / block_size_;
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

      /* Calculate the number of points */
      n = 1;
      for (i=0;i<m;i++) 
            n *= 2;

      /* Do the bit reversal */
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

      /* Compute the FFT */
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

      /* Scaling for forward transform */
      if (dir == 1) {
            for (i=0;i<n;i++) {
                  x[i] /= n;
                  y[i] /= n;
            }
      }
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
