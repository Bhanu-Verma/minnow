#include "wrapping_integers.hh"
#include "debug.hh"
#include <climits>

using namespace std;

constexpr uint64_t LIM = ( 1LL << 32 );
constexpr uint32_t MX_VAL = 0xffffffff;

uint64_t abs_diff( uint64_t a, uint64_t b )
{
  if ( a < b ) {
    return b - a;
  } else {
    return a - b;
  }
}

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  const uint64_t shifted_by_zero_point = n + zero_point.raw_value_;
  const uint32_t final_value = shifted_by_zero_point & MX_VAL; // equivalent to (mod LIM)
  return Wrap32 { final_value };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t temp = static_cast<uint64_t>( raw_value_ ) + LIM;
  const uint64_t x = ( temp - zero_point.raw_value_ ) & MX_VAL; // equivalent to (mod LIM)

  if ( checkpoint < x ) {
    return x;
  }

  const uint64_t num = ( checkpoint - x );

  const uint64_t q_lower = num / LIM;
  const uint64_t q_upper = ( num + LIM - 1 ) / LIM;

  const uint64_t first_choice = x + q_lower * LIM;
  const uint64_t second_choice = x + q_upper * LIM;

  if ( abs_diff( first_choice, checkpoint ) < abs_diff( second_choice, checkpoint ) ) {
    return first_choice;
  } else {
    return second_choice;
  }
}
