#include "reassembler.hh"
#include "debug.hh"
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    if ( first_index == 0 && data.empty() == 0 ) {
      output_.writer().close();
    }
    last_byte_to_be_delivered = first_index + data.length() - 1;
  }

  // check if complete string has been delivered
  if ( is_end() ) {
    output_.writer().close();
    return;
  }

  // Early return if no space is left in underlying bytestream
  const uint64_t available_bytes = output_.writer().available_capacity();
  if ( available_bytes == 0 ) {
    return;
  }

  if ( !data.empty() ) {
    const uint64_t last_byte_allowed_to_be_buffered = next_byte_expected + available_bytes - 1;

    // Early return if no action is required for the string
    if ( first_index > last_byte_allowed_to_be_buffered || first_index + data.length() - 1 < next_byte_expected ) {
      return;
    }

    if ( first_index < next_byte_expected ) {
      data.erase( 0, next_byte_expected - first_index );
      first_index = next_byte_expected;
    }

    if ( first_index + data.length() - 1 > last_byte_allowed_to_be_buffered ) {
      data.erase( last_byte_allowed_to_be_buffered - first_index + 1 );
    }

    auto iter = buffer.find( first_index );
    if ( iter == buffer.end() ) {
      buffer[first_index] = move( data );
    } else if ( ( iter->second ).length() < data.length() ) {
      iter->second = move( data );
    }

    merge_overlapping_substrings();

    if ( first_index == next_byte_expected ) {
      auto it = buffer.find( first_index );
      next_byte_expected += ( it->second ).length();
      output_.writer().push( move( it->second ) );
      buffer.erase( it );
    }
  }

  if ( is_end() ) {
    output_.writer().close();
    return;
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( const auto& [x, y] : buffer ) {
    count += y.length();
  }
  return count;
}

bool Reassembler::is_end() const
{
  return ( last_byte_to_be_delivered.has_value() && next_byte_expected == *last_byte_to_be_delivered + 1 );
}

void Reassembler::merge_overlapping_substrings()
{
  if ( buffer.empty() )
    return;

  auto it = buffer.begin();
  uint64_t curr_first = it->first;
  std::string curr_str = it->second;
  it = buffer.erase( it );

  std::vector<std::pair<uint64_t, std::string>> merged_pairs;

  while ( it != buffer.end() ) {
    uint64_t a = it->first;
    std::string_view str_view = it->second;
    uint64_t b = a + str_view.length() - 1;

    uint64_t curr_end = curr_first + curr_str.length() - 1;

    if ( curr_end < a - 1 ) {
      merged_pairs.emplace_back( curr_first, curr_str );
      curr_first = a;
      curr_str = move( it->second );
    } else if ( curr_end < b ) {
      uint64_t rem_length = b - curr_end;
      string temp = ( it->second ).substr( str_view.length() - rem_length );
      curr_str += temp;
    }
    it = buffer.erase( it );
  }

  merged_pairs.emplace_back( curr_first, curr_str );

  for ( const auto& [x, y] : merged_pairs ) {
    buffer.emplace( x, y );
  }
}