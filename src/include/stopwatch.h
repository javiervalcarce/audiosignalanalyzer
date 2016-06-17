// Hi Emacs, this is -*- coding: utf-8; mode: c++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 6 -*-

#ifndef GSTMC_STOPWATCH_H_
#define GSTMC_STOPWATCH_H_

#include <time.h>
#include <stdint.h>
#include <string>


/**
 * Stopwatch.
 *
 * This class is a C++ implementation of the Stopwatch class included in the Dart standard library.
 * See https://api.dartlang.org/apidocs/channels/be/dartdoc-viewer/dart:core.Stopwatch
 *
 * Javier Valcarce, <javier.valcarce@sepsa.es>
 */
class Stopwatch {
public:
      
      /**
       * Creates a Stopwatch in stopped state with a zero elapsed count.
       */
      Stopwatch();

      /**
       * Returns true if stopwatch is running.
       */
      bool IsRunning() const { return is_running_; }

      /**
       * Returns the elapsed microseconds since the last Start().
       */
      uint64_t ElapsedMicroseconds();

      /**
       * Returns the elapsed milliseconds since the last Start().
       */
      uint64_t ElapsedMilliseconds();

      /**
       * Returns elapsed time in string format by components in days, minutes, hours, seconds and remaining microseconds.
       */
      std::string ElapsedDaysHoursMinutesSeconds();
      std::string ElapsedDaysHoursMinutesSecondsMicroseconds();

      /**
       * Returns elapsed time component by component in days, minutes, hours, seconds and remaining microseconds.
       */
      void ElapsedDaysHoursMinutesSecondsMicroseconds(int* days, int* hours, int* minutes, int* seconds, int* microseconds);


      /**
       * Returns current date and time in ascii form with format "sábado, 26 de diciembre de 2015, 00:27:38 CET"
       * Thread-safe.
       */
      static std::string CurrentTime();
      

      /**
       * Resets the elapsed count to zero.
       * This method does not stop or start the Stopwatch.
       */
      void Reset();
      
      /**
       * Starts the Stopwatch.
       *
       * The elapsed count is increasing monotonically. If the Stopwatch has been stopped, then calling start again
       * restarts it without res*etting the elapsed count.  If the Stopwatch is currently running, then calling start
       * does nothing.
       */
      void Start();

      /**
       * Stops the Stopwatch.
       *
       * The elapsedTicks count stops increasing after this call. If the Stopwatch is currently not running, then
       * calling this method has no effect.
       */
      void Stop();


private:

      uint64_t time0_; // initial point (us)
      uint64_t time1_; // final point (us)

      bool     is_running_;
      uint64_t elapsed_us_;
      uint64_t elapsed_ms_;
      
      uint64_t Timestamp();
};


#endif // GSTMC_STOPWATCH_H_
