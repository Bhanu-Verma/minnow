#include "tcp_receiver.hh"
#include "debug.hh"
#include "wrapping_integers.hh"
#include <climits>
#include <iostream>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.set_error();
  }

  if ( message.SYN ) {
    zero_point_ = message.seqno;
  }

  if ( !zero_point_.has_value() ) {
    return;
  }

  const uint64_t absolute_seqno = message.seqno.unwrap( *zero_point_, last_byte_ );
  last_byte_ = absolute_seqno;

  const uint64_t stream_index = ( message.SYN ) ? 0 : absolute_seqno - 1;
  reassembler_.insert( stream_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  std::optional<Wrap32> ack = std::nullopt;
  if ( zero_point_.has_value() ) {
    uint64_t absolute_ackno = reassembler_.get_ackno() + 1;

    const std::optional<uint64_t> last_byte_to_be_delivered = reassembler_.get_last_byte_to_be_delivered();
    if ( last_byte_to_be_delivered.has_value()
         && ( ( *last_byte_to_be_delivered ) + 1 ) == reassembler_.get_ackno() ) {
      ++absolute_ackno;
    }

    Wrap32 ackno = Wrap32::wrap( absolute_ackno, *zero_point_ );
    ack = ackno;
  }

  uint16_t rwnd = UINT16_MAX;
  const uint64_t available_capacity = reassembler_.get_available_capacity();
  if ( available_capacity < UINT16_MAX ) {
    rwnd = available_capacity;
  }

  return TCPReceiverMessage { ack, rwnd, reassembler_.reader().has_error() };
}
