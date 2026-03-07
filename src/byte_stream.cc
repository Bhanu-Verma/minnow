#include "byte_stream.hh"
#include "debug.hh"
#include <algorithm>
#include <cassert>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer_(), is_open_( true ) {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  const uint64_t available_capacity = capacity_ - buffered_bytes_;
  const uint64_t total_written = min( available_capacity, data.length() );
  if ( total_written == 0 ) {
    return;
  }

  data.erase( total_written );
  buffer_.emplace_back( std::move( data ) );

  bytes_pushed_ += total_written;
  buffered_bytes_ += total_written;
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  is_open_ = false;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return !is_open_;
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{
  return capacity_ - buffered_bytes_;
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  return buffer_.front();
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  len = min( len, buffered_bytes_ );
  bytes_popped_ += len;
  buffered_bytes_ -= len;

  while ( !buffer_.empty() && len >= buffer_.front().length() ) {
    len -= buffer_.front().length();
    buffer_.pop_front();
  }
  if ( !buffer_.empty() ) {
    buffer_.front().erase(0, len);
  }
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  return !is_open_ && buffered_bytes_ == 0; // Your code here.
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return buffered_bytes_;
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
