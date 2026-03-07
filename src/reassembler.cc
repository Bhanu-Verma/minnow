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
      iter = buffer.emplace( first_index, move(data) ).first;
    } else if ( ( iter->second ).length() < data.length() ) {
      iter->second = move( data );
    }

    // merge the intervals to the right of current inserted interval
    while ( true ) {
      auto next_it = std::next( iter );
      if ( next_it == buffer.end() ) {
        break;
      }
      bool is_merged = merge_iterators( iter, next_it );
      if( !is_merged ) {
        break;
      }
    }

    // merge the intervals to the left of current inserted interval
    while ( iter != buffer.begin() ) {
      auto prev_it = std::prev(iter);

      bool is_merged = merge_iterators( prev_it, iter );
      if( is_merged ) {
        iter = prev_it;
      }
      else {
        break;
      }
    }

    if ( first_index == next_byte_expected ) {
      auto it = buffer.begin();       // Claim: it will always be the first entry in the map
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

bool Reassembler::merge_iterators( BufferType::iterator it, BufferType::iterator next_it ) {
  auto curr_first = it->first;
  auto next_first = next_it->first;

  auto curr_end = curr_first + (it->second).length() - 1;
  auto next_end = next_first + (next_it->second).length() - 1;

  if ( curr_end >= ( next_first - 1 ) ) {
    if ( curr_end < next_end) {
      uint64_t rem_length = next_end - curr_end;
      (it->second).append( (next_it->second).substr( (next_it->second).length() - rem_length ) );
    }
    buffer.erase( next_it );
    return true;
  }
  else {
    return false;
  }
}