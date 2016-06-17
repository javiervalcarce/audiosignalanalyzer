// Hi Emacs, this is -*- mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-

#include "stopwatch.h"

#include <time.h>
#include <cstdio>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Stopwatch::Stopwatch() {
      is_running_ = false;;   
      Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t Stopwatch::ElapsedMicroseconds() { 

      if (is_running_ == false) {
            return elapsed_us_; 
      }

      time1_ = Timestamp();
      elapsed_us_ = time1_ - time0_;
//      elapsed_ms_ = elapsed_us_ / 1000;

      return elapsed_us_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t Stopwatch::ElapsedMilliseconds() { 
      if (is_running_ == false) {
            return elapsed_ms_; 
      }
      
      time1_ = Timestamp();

      elapsed_us_ = time1_ - time0_;
      elapsed_ms_ = elapsed_us_ / 1000;

      return elapsed_ms_;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Stopwatch::ElapsedDaysHoursMinutesSeconds() {
      int d;
      int h;
      int m;
      int s;
      int u;
      ElapsedDaysHoursMinutesSecondsMicroseconds(&d, &h, &m, &s, &u);
      
      char b[80];
      snprintf(b, sizeof(b), "%d days, %d hours, %d minutes, %d seconds", d, h, m, s);
            
      return b;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Stopwatch::ElapsedDaysHoursMinutesSecondsMicroseconds() {
      int d;
      int h;
      int m;
      int s;
      int u;
      ElapsedDaysHoursMinutesSecondsMicroseconds(&d, &h, &m, &s, &u);
      
      char b[80];
      snprintf(b, sizeof(b), "%d days, %d hours, %d minutes, %d seconds, %d us", d, h, m, s, u);
            
      return b;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Stopwatch::ElapsedDaysHoursMinutesSecondsMicroseconds(int* days, int* hours, int* minutes, int* seconds, 
                                                           int* microseconds) {

      // Cálculo de horas, minutos y segundos y microsegundos
      uint64_t timeus = ElapsedMicroseconds();

      uint64_t d_quot = timeus / (24 * 3600000000ULL);     // Days
      uint64_t d_rest = timeus % (24 * 3600000000ULL);     // 
      uint64_t h_quot = d_rest / (     3600000000ULL);     // Hours
      uint64_t h_rest = d_rest % (     3600000000ULL);     // 
      uint64_t m_quot = h_rest / (     60000000ULL  );     // Minutes
      uint64_t m_rest = h_rest % (     60000000ULL  );     //
      uint64_t s_quot = m_rest / (     1000000ULL   );     // Seconds
      uint64_t s_rest = m_rest % (     1000000ULL   );     // Remaining microseconds

      *days = static_cast<int>(d_quot);
      *hours = static_cast<int>(h_quot);
      *minutes = static_cast<int>(m_quot);
      *seconds = static_cast<int>(s_quot);
      *microseconds = static_cast<int>(s_rest);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Stopwatch::CurrentTime() {

      char s[26];
      
      time_t v = time(NULL);
      ctime_r(&v, s);
      
      s[24] = '\0';
      
      return std::string(s);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Stopwatch::Start() {
      if (is_running_ == true) {
            return;
      }

      time0_ = Timestamp();

      elapsed_us_ = 0;
      elapsed_ms_ = 0;

      is_running_ = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Stopwatch::Stop() {
      if (is_running_ == false) {
            return; 
      }

      time1_ = Timestamp();

      elapsed_us_ = time1_ - time0_;
      elapsed_ms_ = elapsed_us_ / 1000;

      is_running_ = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Stopwatch::Reset() {

      time0_ = Timestamp();
      
      elapsed_ms_ = 0;
      elapsed_us_ = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t Stopwatch::Timestamp() {

      struct timespec ts;

#ifdef __MACH__
      // Esto es para MAC OS X
      clock_serv_t cclock;
      mach_timespec_t mts;

      host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);

      clock_get_time(cclock, &mts);
      mach_port_deallocate(mach_task_self(), cclock);
      ts.tv_sec = mts.tv_sec;
      ts.tv_nsec = mts.tv_nsec;
#else
      // Linux
      clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#endif

      return (ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000); // en us


}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
