#pragma once

/* This timer class will be used by TCPSender to detact the packet loss */
class Timer
{
public:
  Timer() = default;

  void start_or_reset_timer( uint64_t timeout )
  {
    remaining_time_ = timeout;
    running_ = true;
    expired_ = false;
  }

  void tick( uint64_t time_elapsed )
  {
    if ( !running_ )
      return;
    if ( time_elapsed >= remaining_time_ ) {
      expired_ = true;
      stop();
    }
    remaining_time_ -= time_elapsed;
  }

  void stop() { running_ = false; }
  bool is_expired() const { return expired_; }
  bool is_running() const { return running_; }

private:
  uint64_t remaining_time_ {};
  bool running_ { false };
  bool expired_ { false };
};