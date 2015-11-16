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

      assert(initialized_ == false);

      int r;
      r = pthread_create(&thread_, &thread_attr_, WaveformGenerator::ThreadFuncHelper, this);
      assert(r == 0);

      initialized_ = true;
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* ThdAnalyzer::ThreadFuncHelper(void* p) {
      ThdAnalyzer* o = (ThdAnalyzer*) p;
      return o->ThreadFunc();
}

void* ThdAnalyzer::ThreadFunc() {

      snd_pcm_hw_params_t *hw_params;
      snd_pcm_sw_params_t *sw_params;
      snd_pcm_sframes_t frames_to_deliver;
      int nfds;
      int err;
      struct pollfd *pfds;
      
      if ((err = snd_pcm_open (&playback_handle, device_.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
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
       
      if ((err = snd_pcm_hw_params_any (playback_handle, hw_params)) < 0) {
            fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      if ((err = snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            fprintf (stderr, "cannot set access type (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      if ((err = snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            fprintf (stderr, "cannot set sample format (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      unsigned int sample_rate = 44100;
      if ((err = snd_pcm_hw_params_set_rate_near (playback_handle, hw_params, &sample_rate, NULL)) < 0) {
            fprintf (stderr, "cannot set sample rate (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      if ((err = snd_pcm_hw_params_set_channels (playback_handle, hw_params, 2)) < 0) {
            fprintf (stderr, "cannot set channel count (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
            fprintf (stderr, "cannot set parameters (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      snd_pcm_hw_params_free (hw_params);
      
      /* tell ALSA to wake us up whenever 4096 or more frames
            of playback data can be delivered. Also, tell
               ALSA that we'll start the device ourselves.
      */
      
      if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
            fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      if ((err = snd_pcm_sw_params_current (playback_handle, sw_params)) < 0) {
            fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      if ((err = snd_pcm_sw_params_set_avail_min (playback_handle, sw_params, 4096)) < 0) {
            fprintf (stderr, "cannot set minimum available count (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      if ((err = snd_pcm_sw_params_set_start_threshold (playback_handle, sw_params, 0U)) < 0) {
            fprintf (stderr, "cannot set start mode (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      if ((err = snd_pcm_sw_params (playback_handle, sw_params)) < 0) {
            fprintf (stderr, "cannot set software parameters (%s)\n",
                     snd_strerror (err));
            exit (1);
      }
      
      /* the interface will interrupt the kernel every 4096 frames, and ALSA
            will wake up this program very soon after that.
      */
      
      if ((err = snd_pcm_prepare (playback_handle)) < 0) {
            fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                     snd_strerror (err));
            exit (1);
      }


      while (1) {
            
            /* wait till the interface is ready for data, or 1 second
                  has elapsed.
            */
            
            if ((err = snd_pcm_wait (playback_handle, 1000)) < 0) {
                  fprintf (stderr, "poll failed (%s)\n", strerror (errno));
                  break;
            }           
            
            /* find out how much data is available in the capture device */
            
            if ((frames_to_deliver = snd_pcm_avail_update (playback_handle)) < 0) {
                  if (frames_to_deliver == -EPIPE) {
                        fprintf (stderr, "an xrun occured\n");
                        break;
                  } else {
                        fprintf (stderr, "unknown ALSA avail update return value (%d)\n", 
                                 frames_to_deliver);
                        break;
                  }
            }
            
            frames_to_deliver = frames_to_deliver > 4096 ? 4096 : frames_to_deliver;
            
            /* deliver the data */
            
            if (process_callback (frames_to_deliver) != frames_to_deliver) {
                  fprintf (stderr, "playback callback failed\n");
                  break;
            }
      }
      
      snd_pcm_close (playback_handle);

      return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ThdAnalyzer::process_callback (snd_pcm_sframes_t nframes) {

      int m = 9;   // x^m == 1024
      int n = 512; // 2^(m-1)
    
      // Dos FFT reales por el precio de una FFT compleja.
      double* x = chunk->data;
      double* y = chunk->data + n;
      nframes = nframes > 2048 ? 2048 : nframes;

      snd_pcm_readi(playback_handle, buf, nframes);

      FFT(1, m, x, y);
    
      double z[512];
    
      // Periodograma (FFT promediada)
      int i;
      for (i = 0; i < n; i++) {
            z[i] = (x[i] + y[i]) / 2.0;
      }

      return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
