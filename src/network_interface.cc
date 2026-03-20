#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
  EthernetFrame e_frame;
  const uint32_t ipv4_addr = next_hop.ipv4_numeric();

  auto iter = translation_cache_.find( ipv4_addr );
  if ( iter != translation_cache_.end() ) {
    const EthernetAddress addr = iter->second;

    const EthernetHeader e_header { addr, ethernet_address_, EthernetHeader::TYPE_IPv4 };
    e_frame = { e_header, serialize( std::move( dgram ) ) };
  } else {
    // Add the datagram to the queue
    queued_dgrams_[ipv4_addr].push_back( std::move( dgram ) );

    // Check if request has already been sent
    if ( requests_sent_.find( ipv4_addr ) != requests_sent_.end() ) {
      return;
    }

    e_frame = arp_request_frame_for_ip( ipv4_addr );
    requests_sent_.insert( ipv4_addr );
    last_sent_.push( { ipv4_addr, curr_time_ } );
  };
  output().transmit( *this, e_frame );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram internet_datagram;
    if ( parse( internet_datagram, frame.payload ) ) {
      datagrams_received_.push( internet_datagram );
    }
  } else {
    ARPMessage arp_message;
    if ( parse( arp_message, frame.payload ) ) {
      translation_cache_[arp_message.sender_ip_address] = arp_message.sender_ethernet_address;
      last_updated_.push( { arp_message.sender_ip_address, curr_time_ } );
      latest_time_[arp_message.sender_ip_address] = curr_time_;

      // Transmit any datagrams waiting for this translation
      for ( const auto& dgram : queued_dgrams_[arp_message.sender_ip_address] ) {
        const EthernetHeader e_header {
          arp_message.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 };
        const EthernetFrame e_frame = { e_header, serialize( dgram ) };
        output().transmit( *this, e_frame );
      }

      queued_dgrams_[arp_message.sender_ip_address].clear();
      queued_dgrams_[arp_message.sender_ip_address].shrink_to_fit();

      if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST
           && arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage reply_message {};
        reply_message.opcode = ARPMessage::OPCODE_REPLY;
        reply_message.sender_ethernet_address = ethernet_address_;
        reply_message.sender_ip_address = ip_address_.ipv4_numeric();
        reply_message.target_ethernet_address = arp_message.sender_ethernet_address;
        reply_message.target_ip_address = arp_message.sender_ip_address;

        const EthernetHeader e_header {
          arp_message.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP };
        const EthernetFrame e_frame = { e_header, serialize( reply_message ) };
        output().transmit( *this, e_frame );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  curr_time_ += ms_since_last_tick;
  while ( !last_updated_.empty() && ( curr_time_ - last_updated_.front().second ) > EXPIRATION_TIME_ms ) {
    const uint32_t ip_addr = last_updated_.front().first;
    last_updated_.pop();
    if ( curr_time_ - latest_time_[ip_addr] > EXPIRATION_TIME_ms ) {
      translation_cache_.erase( ip_addr );
    }
  }
  while ( !last_sent_.empty() && ( curr_time_ - last_sent_.front().second ) > ARP_TIMEOUT_ms ) {
    const uint32_t ip_addr = last_sent_.front().first;
    last_sent_.pop();
    requests_sent_.erase( ip_addr );

    auto iter = queued_dgrams_.find( ip_addr );
    if ( iter != queued_dgrams_.end() ) {
      ( iter->second ).clear();
      ( iter->second ).shrink_to_fit();
    }
  }
}

EthernetFrame NetworkInterface::arp_request_frame_for_ip( uint32_t ip_addr )
{
  // Construct a valid ARP message
  ARPMessage arp_message {};
  arp_message.opcode = ARPMessage::OPCODE_REQUEST;
  arp_message.sender_ethernet_address = ethernet_address_;
  arp_message.sender_ip_address = ip_address_.ipv4_numeric();
  arp_message.target_ip_address = ip_addr;

  const EthernetHeader e_header { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP };
  const EthernetFrame e_frame { e_header, serialize( arp_message ) };

  return e_frame;
}