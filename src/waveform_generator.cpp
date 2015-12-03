// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <cmath>

#include "waveform_generator.h"

using namespace thd_analyzer;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WaveformGenerator::WaveformGenerator(const char* playback_pcm_device) {

      device_ = playback_pcm_device;

      initialized_ = false;
      run_ = false;

      pthread_attr_init(&thread_attr_);

      sample_rate_ = 192000; //44100;
      block_size_  = 1024;
      buf_data_    = new int16_t[2 * block_size_];
      frequency_   = 440;
      amplitude_   = 1.0;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WaveformGenerator::~WaveformGenerator() {
      
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WaveformGenerator::Init() {

      assert(initialized_ == false);
      int r;
      r = pthread_create(&thread_, &thread_attr_, WaveformGenerator::ThreadFuncHelper, this);
      assert(r == 0);

      initialized_ = true;
      return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WaveformGenerator::Start() {
      run_ = true;
      // Ejemplo
      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WaveformGenerator::Stop() {
      run_ = false;
      return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WaveformGenerator::SetFrequency(double frequency) {
      frequency_ = frequency;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WaveformGenerator::SetAmplitude(double amplitude) {
      amplitude_ = amplitude;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* WaveformGenerator::ThreadFuncHelper(void* p) {
      WaveformGenerator* o = (WaveformGenerator*) p;
      return o->ThreadFunc();
}

void* WaveformGenerator::ThreadFunc() {

      snd_pcm_sframes_t frames_to_deliver;
      int err;


      // HW Parameters
      // ---------------------------------------------------------------------------------------------------------------
      snd_pcm_hw_params_t *hw_params;
      if ((err = snd_pcm_open (&playback_handle_, device_.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            fprintf (stderr, "cannot open audio device %s (%s)\n", device_.c_str(), snd_strerror(err));
            return NULL;
      }
         
      if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
            fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
            return NULL;
      }
       
      if ((err = snd_pcm_hw_params_any (playback_handle_, hw_params)) < 0) {
            fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
            return NULL;
      }
      
      if ((err = snd_pcm_hw_params_set_access (playback_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            fprintf (stderr, "cannot set access type (%s)\n", snd_strerror(err));
            return NULL;
      }
      
      if ((err = snd_pcm_hw_params_set_format (playback_handle_, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            fprintf (stderr, "cannot set sample format (%s)\n", snd_strerror(err));
            return NULL;
      }
      
      
      if ((err = snd_pcm_hw_params_set_rate_near (playback_handle_, hw_params, (unsigned int*) &sample_rate_, NULL)) < 0) {
            fprintf (stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
            return NULL;
      }


      printf("FS=%d\n", sample_rate_);
      
      if ((err = snd_pcm_hw_params_set_channels (playback_handle_, hw_params, 2)) < 0) {
            fprintf (stderr, "cannot set channel count (%s)\n", snd_strerror(err));
            return NULL;
      }
      
      if ((err = snd_pcm_hw_params (playback_handle_, hw_params)) < 0) {
            fprintf (stderr, "cannot set parameters (%s)\n", snd_strerror(err));
            return NULL;
      }
      
      snd_pcm_hw_params_free(hw_params);
      
      /* tell ALSA to wake us up whenever block_size_ or more frames of playback data can be delivered. Also, tell
               ALSA that we'll start the device ourselves.
      */

      // SW Parameters
      // ---------------------------------------------------------------------------------------------------------------      
      snd_pcm_sw_params_t* sw_params;

      if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
            fprintf(stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_current(playback_handle_, sw_params)) < 0) {
            fprintf(stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_avail_min(playback_handle_, sw_params, block_size_)) < 0) {
            fprintf(stderr, "cannot set minimum available count (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params_set_start_threshold(playback_handle_, sw_params, 0U)) < 0) {
            fprintf(stderr, "cannot set start mode (%s)\n", snd_strerror(err));
            return NULL;
      }
      if ((err = snd_pcm_sw_params(playback_handle_, sw_params)) < 0) {
            fprintf(stderr, "cannot set software parameters (%s)\n", snd_strerror(err));
            return NULL;
      }


      // The interface will interrupt the kernel every 4096 frames, and ALSA will wake up this program very soon after
      // that.
      if ((err = snd_pcm_prepare(playback_handle_)) < 0) {
            fprintf (stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
            return NULL;
      }

      while (1) {
            
            // wait till the interface is ready for data, or 1 second has elapsed.            
            if ((err = snd_pcm_wait (playback_handle_, 1000)) < 0) {
                  fprintf(stderr, "poll failed (%s)\n", strerror (errno));
                  break;
            }           
            
            // find out how much space is available for playback data
            if ((frames_to_deliver = snd_pcm_avail_update (playback_handle_)) < 0) {
                  if (frames_to_deliver == -EPIPE) {
                        fprintf(stderr, "an xrun occured\n");
                        break;
                  } else {
                        fprintf(stderr, "unknown ALSA avail update return value (%ld)\n", frames_to_deliver);
                        break;
                  }
            }
            
            frames_to_deliver = frames_to_deliver > block_size_ ? block_size_ : frames_to_deliver;
            if (SendSamples(frames_to_deliver) != frames_to_deliver) {
                  fprintf (stderr, "playback callback failed\n");
                  break;
            }
      }

      snd_pcm_close(playback_handle_);
      return NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WaveformGenerator::SendSamples(snd_pcm_sframes_t nframes) {

      int err;
      static int n = 0;
      int i;
      short s;

      double w = 2 * M_PI * frequency_ / sample_rate_;
      //double w = 2 * M_PI * frequency_ / sample_rate_ / 4; 
      // Esto funciona, ¿por qué? Sospecha: empaquetado de muestras con snd_pcm_writei


      for (i = 0; i < nframes; i++) {
            s = (int16_t) (32767 * amplitude_ * cos(w * n));
            n++;

            // Escribo un "frame". Un frame es un bloque de 2 muestras: una muestra del canal L y otra del R
            buf_data_[2 * i + 0] = s; // Canal L
            buf_data_[2 * i + 1] = s; // Canal R
      }
      
      // Envio las muestras (intercaladas L R L R L R...) al convertidor D/A
      if ((err = snd_pcm_writei(playback_handle_, buf_data_, nframes)) < 0) {
            fprintf(stderr, "write failed (%s)\n", snd_strerror(err));
      }
      
      return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
