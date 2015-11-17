// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include "thd_analyzer.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <alsa/asoundlib.h>


using namespace thd_analyzer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::ThdAnalyzer(const char* pcm_capture_device) {

      device_ = pcm_capture_device;
      initialized_ = false;
      exit_thread_ = false;
      capture_handle_ = NULL;
      buf_size_ = 0;
      fft_count_ = 0;
      channel_size_[0] = 0; 
      channel_size_[1] = 0;
      channel_frequency_[0] = 0.0;
      channel_frequency_[1] = 0.0;
      channel_amplitude_[0] = 0.0;
      channel_amplitude_[1] = 0.0;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::~ThdAnalyzer() {
      exit_thread_ = true;
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
double ThdAnalyzer::Frequency(int channel) {
      assert(initialized_ == true);
      return channel_frequency_[channel];
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::Amplitude(int channel) {
      assert(initialized_ == true);
      return channel_amplitude_[channel];
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* ThdAnalyzer::ThreadFuncHelper(void* p) {
      ThdAnalyzer* o = (ThdAnalyzer*) p;
      return o->ThreadFunc();
}

void* ThdAnalyzer::ThreadFunc() {

      int err;
      int frames_to_deliver;

      // Inicialización del dispositivo de captura de audio: Parámetros HW
      // ---------------------------------------------------------------------------------------------------------------      
      snd_pcm_hw_params_t* hw_params;

	printf("Opening %s...\n", device_.c_str());

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
	
      unsigned int samplerate = 44100;
      if ((err = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params, &samplerate, NULL)) < 0) {
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
	

      // Inicialización del dispositivo de captura de audio: Parámetros SW
      // ---------------------------------------------------------------------------------------------------------------
      // tell ALSA to wake us up whenever 4096 or more frames of playback data can be delivered. Also, tell ALSA that
      // we'll start the device ourselves.
      snd_pcm_sw_params_t *sw_params;

      if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
            fprintf (stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror (err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_current(capture_handle_, sw_params)) < 0) {
            fprintf (stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_avail_min(capture_handle_, sw_params, 4096)) < 0) {
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
            
            //usleep(100000); // DEBUG

            /*
            // Wait till the interface is ready for data, or 1 second has elapsed
            if ((err = snd_pcm_wait(capture_handle_, 1000)) < 0) {
                  fprintf (stderr, "poll failed (%s)\n", strerror(errno));
                  break;
            }           
            
            // Find out how much data is available in the capture device
            if ((frames_to_deliver = snd_pcm_avail_update(capture_handle_)) < 0) {
                  if (frames_to_deliver == -EPIPE) {
                        fprintf (stderr, "an xrun occured\n");
                        break;
                  } else {
                        fprintf (stderr, "unknown ALSA avail update return value (%d)\n", frames_to_deliver);
                        break;
                  }
            }
            */

            //printf("debug: frames_to_deliver %d\n", frames_to_deliver);

            // Limito el número de muestras que voy a leer al tamaño del bufer en el que las voy a depositar.
            frames_to_deliver = frames_to_deliver > 2048 ? 2048 : frames_to_deliver;
            
            // Lectura de las muestras de audio con signo de 16 bis
            frames_to_deliver = 2048;

            int r;
            int frames = frames_to_deliver;
            
            do {
                  r = snd_pcm_readi(capture_handle_, (void*) buf_, frames_to_deliver);
                  
                  // 2 int16_t por frame
                  buf_ += r * 2; 
                  frames -= r;
                  
                  if (r == 0) printf("ZERO!\n");
                  if (r <  0) printf("ERROR!\n");

            } while (r >= 1 && frames > 0);

            int i;
            //printf("COMPLETE\n");

            // Conversión de formato: de int16_t a coma flotante 
            for (i = 0; i < frames_to_deliver; i++) {
                  channel_data_[0][i] = ((double) buf_[2 * i + 0]) / 32678.0;
                  channel_data_[1][i] = ((double) buf_[2 * i + 1]) / 32678.0;
            }

            // proc
            Process();

            if (exit_thread_ == true) {
                  break;
            }
      }

      snd_pcm_close(capture_handle_);
      return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Process() {

      // En channel_data[x] tenemos las muestras en coma flotante correspondientes al canal x
      // En channel_size[x] tenemos el número de muestras correspondientes al canal x

      int m = 11;
      int n = 512;  // 2^(m-1)
    
      double tmp[2048];
      double abs2[2048];
      int i;

      for (i = 0; i < 2048; i++) tmp[i] = 0.0; 

      // TODO: Dos FFT reales por el precio de una FFT compleja.
      FFT(1, m, &(channel_data_[0][0]), tmp);
            
      int max_index = 0;
      double max_value = 0.0;

      int i;
      for (i = 0; i < channel_size_[0]; i++) {
            abs2[i] = channel_data_[0][i]*channel_data_[0][i] + tmp[i]*tmp[i];
            if (abs2[i] >= max_value) {
                  max_value = abs2[i];
                  max_index = i;
            }
      }

      
      channel_frequency_[0] = i * 44100.0 / 2048.0;
      fft_count_++;

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
