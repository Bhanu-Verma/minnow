#include "router.hh"
#include "debug.hh"

#include <cstdint>
#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  routing_table_.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto& i : interfaces_ ) {
    auto& buffer = i->datagrams_received();
    while ( !buffer.empty() ) {
      auto dgrm = buffer.front();
      buffer.pop();

      uint8_t longest_match = 0;
      std::optional<Address> nh {};
      size_t in;
      bool found = false;

      for ( const auto& route : routing_table_ ) {
        if ( is_match( route, dgrm.header.dst ) && route.prefix_length >= longest_match ) {
          nh = route.next_hop;
          in = route.interface_num;
          longest_match = route.prefix_length;
          found = true;
        }
      }

      if ( !found || dgrm.header.ttl == 0 ) {
        continue;
      }

      --dgrm.header.ttl;
      if ( dgrm.header.ttl == 0 ) {
        continue;
      }

      dgrm.header.compute_checksum();
      if ( nh ) {
        interface( in )->send_datagram( dgrm, *nh );
      } else {
        interface( in )->send_datagram( dgrm, Address::from_ipv4_numeric( dgrm.header.dst ) );
      }
    }
  }
}

bool Router::is_match( const Route& route, uint32_t addr ) const
{
  uint32_t mask = UINT32_MAX ^ ( ( 1LL << ( 32 - route.prefix_length ) ) - 1 );
  return ( ( route.route_prefix & mask ) == ( addr & mask ) );
}
