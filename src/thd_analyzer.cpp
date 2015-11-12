// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-

#include "thd_analyzer.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <alsa/asoundlib.h>

using namespace thd_analyzer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::ThdAnalyzer(const char* capture_device) {
      device_ = capture_device;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThdAnalyzer::~ThdAnalyzer() {

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Init() {
      return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Start() {

      // Ejemplo
      int i;
      int err;
      short buf[128];
      snd_pcm_t *capture_handle;
      snd_pcm_hw_params_t *hw_params;
	
      if ((err = snd_pcm_open (&capture_handle, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            fprintf (stderr, "cannot open audio device %s (%s)\n", 
                     device_.c_str(),
                     snd_strerror (err));
            exit (1);
      }
		   
      if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
            fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
				 
      if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
            fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            fprintf (stderr, "cannot set access type (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            fprintf (stderr, "cannot set sample format (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      unsigned int samplerate = 44100;
      if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &samplerate, NULL)) < 0) {
            fprintf (stderr, "cannot set sample rate (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) {
            fprintf (stderr, "cannot set channel count (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
            fprintf (stderr, "cannot set parameters (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      snd_pcm_hw_params_free (hw_params);
	
      if ((err = snd_pcm_prepare (capture_handle)) < 0) {
            fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
	
      for (i = 0; i < 10; ++i) {
            if ((err = snd_pcm_readi (capture_handle, buf, 128)) != 128) {
                  fprintf (stderr, "read from audio interface failed (%s)\n",
                           snd_strerror (err));
                  exit (1);
            }
      }
	
      snd_pcm_close (capture_handle);
      exit (0);

      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::Stop() {
      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::Frequency() {
      return 0.0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double ThdAnalyzer::Amplitude() {
      return 0.0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThdAnalyzer::Process(Buffer* chunk) {
    
      assert(chunk->size == 1024);
    
      int m = 9;   // x^m == 1024
      int n = 512; // 2^(m-1)
    
      // Dos FFT reales por el precio de una FFT compleja.
      double* x = chunk->data;
      double* y = chunk->data + n;
      FFT(1, m, x, y);
    
      double z[512];
    
      // Periodograma (FFT promediada)
      int i;
      for (i = 0; i < n; i++) {
            z[i] = (x[i] + y[i]) / 2.0;
      }
    
    
    
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
