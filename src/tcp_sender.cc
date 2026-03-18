#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seqno_in_flight_;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // uint16_t rwnd = max<uint16_t>( rec_window_, 1 );
  uint16_t rwnd = rem_window_;
  std::vector<TCPSenderMessage> send_msgs = construct_messages( rwnd );

  for ( const auto& send_msg : send_msgs ) {
    /* Steps when transmitting a message */
    transmit( send_msg );
    if ( !timer_.is_running() ) {
      timer_.start_or_reset_timer( RTO_ms_ );
    }
    outstanding_messages_.push( send_msg );
    seqno_in_flight_ += send_msg.sequence_length();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage { Wrap32::wrap( curr_seqno_, isn_ ), false, "", false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  rec_window_ = msg.window_size;
  rem_window_ = rec_window_;
  if ( rec_window_ == 0 ) {
    rem_window_ = 1;
  }

  if ( msg.RST ) {
    input_.set_error();
  }

  if ( !msg.ackno.has_value() ) {
    return;
  }

  uint64_t absolute_ackno = ( msg.ackno )->unwrap( isn_, curr_seqno_ );

  /** Update the remaining window here.
      Window puts a limit on the `sequence numbers in flight`
  */
  if ( msg.window_size == 0 ) {
    rem_window_ = 1;
  } else {
    uint64_t range_end = absolute_ackno + msg.window_size - 1;
    if ( range_end >= curr_seqno_ ) {
      rem_window_ = range_end - curr_seqno_ + 1;
    } else {
      rem_window_ = 0;
    }
  }

  if ( last_ackno_.has_value() && last_ackno_ >= absolute_ackno ) {
    return;
  }

  /** Ignore the ack if it is beyond the feasible range */
  if ( absolute_ackno > curr_seqno_ ) {
    return;
  }

  last_ackno_ = absolute_ackno;

  // Pop some of the outstanding segments if required
  while ( !outstanding_messages_.empty() ) {
    TCPSenderMessage tcp_seg = outstanding_messages_.front();
    uint64_t seg_end = tcp_seg.seqno.unwrap( isn_, curr_seqno_ ) + tcp_seg.sequence_length() - 1;
    if ( seg_end < absolute_ackno ) {
      seqno_in_flight_ -= tcp_seg.sequence_length();
      outstanding_messages_.pop();
    } else {
      break;
    }
  }

  RTO_ms_ = initial_RTO_ms_;
  if ( !outstanding_messages_.empty() ) {
    timer_.start_or_reset_timer( RTO_ms_ );
  } else {
    timer_.stop();
  }
  consecutive_retransmissions_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Early return if timer is not at all running
  if ( !timer_.is_running() ) {
    return;
  }

  // Pass the time info to `timer`
  timer_.tick( ms_since_last_tick );
  if ( timer_.is_expired() ) {
    // assert( !outstanding_messages_.empty() && "Timer expired with no outstanding segments" );

    TCPSenderMessage tcp_seg = outstanding_messages_.front();
    transmit( tcp_seg );
    if ( rec_window_ != 0 ) {
      ++consecutive_retransmissions_;
      RTO_ms_ <<= 1;
    }

    timer_.start_or_reset_timer( RTO_ms_ );
  }
}

std::vector<TCPSenderMessage> TCPSender::construct_messages( uint16_t rwnd )
{
  std::vector<TCPSenderMessage> messages;

  while ( rwnd ) {
    if ( reader().is_finished() ) {
      if ( curr_seqno_ == 0 && rwnd == 1 ) {
        TCPSenderMessage msg { Wrap32::wrap( curr_seqno_, isn_ ), true, "", false, input_.has_error() };
        messages.push_back( msg );
        ++curr_seqno_;
        --rwnd;
      } else if ( !FIN_sent_ ) {
        TCPSenderMessage msg { Wrap32::wrap( curr_seqno_, isn_ ), curr_seqno_ == 0, "", true, input_.has_error() };
        messages.push_back( msg );
        curr_seqno_ += msg.sequence_length();
        --rwnd;
        FIN_sent_ = true;
      }
      break;
    }

    // uint64_t message_size = min<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE, rwnd );
    uint64_t message_size = rwnd;
    /* We need to set the SYN bit*/
    if ( curr_seqno_ == 0 ) {
      --message_size;
    }

    string payload = "";
    read( reader(),
          min<uint64_t>( message_size, min( TCPConfig::MAX_PAYLOAD_SIZE, reader().bytes_buffered() ) ),
          payload );

    TCPSenderMessage message {
      Wrap32::wrap( curr_seqno_, isn_ ), curr_seqno_ == 0, payload, false, input_.has_error() };

    uint64_t seq_len = message.sequence_length();
    message.FIN = reader().is_finished() && seq_len < message_size;

    if ( !message.SYN && !message.FIN && message.payload.empty() ) {
      break;
    }

    messages.push_back( message );
    rwnd -= message.sequence_length();
    curr_seqno_ += message.sequence_length();
    rem_window_ -= message.sequence_length();
    FIN_sent_ = FIN_sent_ || message.FIN;
  }

  return messages;
}