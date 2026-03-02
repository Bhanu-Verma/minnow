#include "reassembler.hh"
#include "debug.hh"
#include <iostream>
#include <memory>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    if ( first_index == 0 && data.empty() == 0 ) {
      output_.writer().close();
    }
    last_byte_to_be_delivered = first_index + data.length() - 1;
  }

  const uint64_t available_bytes = output_.writer().available_capacity();
  if ( available_bytes == 0 ) {
    return;
  }

  if ( !data.empty() ) {
    const uint64_t last_byte_allowed_to_be_buffered = next_byte_expected + available_bytes - 1;

    if ( first_index + data.length() - 1 < next_byte_expected || first_index > last_byte_allowed_to_be_buffered ) {
      return;
    }

    if ( first_index <= next_byte_expected ) {
      // Extract the part after `next_byte_expected`
      data = data.substr( next_byte_expected - first_index );

      // Remove the part that can't be buffered
      data = data.substr( 0, available_bytes );

      for ( std::size_t i = 0; i < data.length(); ++i ) {
        const std::size_t index = ( start_index + i ) % capacity;
        buffer[index] = data[i];
      }

      string to_write = "";
      while ( buffer[start_index].has_value() ) {
        to_write += *buffer[start_index];
        buffer[start_index] = std::nullopt;
        start_index = ( start_index + 1 ) % capacity;
      }

      next_byte_expected += to_write.length();

      output_.writer().push( to_write );
    } else {
      // Remove the part that can't be buffered
      data = data.substr( 0, last_byte_allowed_to_be_buffered - first_index + 1 );
      const uint64_t offset = first_index - next_byte_expected;
      for ( std::size_t i = 0; i < data.length(); ++i ) {
        const std::size_t index = ( start_index + offset + i ) % capacity;
        buffer[index] = data[i];
      }
    }
  }

  if ( last_byte_to_be_delivered && next_byte_expected == ( *last_byte_to_be_delivered ) + 1 ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( size_t i = 0; i < capacity; ++i ) {
    count += buffer[i].has_value();
  }
  return count;
}
