#ifndef __UDP_DISCOVERY_PEER_PARAMETERS_H_
#define __UDP_DISCOVERY_PEER_PARAMETERS_H_

#include <stdint.h>

#include "udp_discovery_protocol_version.hpp"

namespace udpdiscovery {
  class PeerParameters {
   public:
    enum SamePeerMode {
      kSamePeerIp,
      kSamePeerIpAndPort,
    };

   public:
    PeerParameters()
        : min_supported_protocol_version_(kProtocolVersionCurrent),
          max_supported_protocol_version_(kProtocolVersionCurrent),
          application_id_(0),
          can_use_broadcast_(true),
          can_use_multicast_(false),
          port_(0),
          multicast_group_address_(0),
          send_timeout_ms_(5000),
          discovered_peer_ttl_ms_(10000),
          can_be_discovered_(false),
          can_discover_(false),
          discover_self_(false),
          same_peer_mode_(kSamePeerIpAndPort) {
    }

    ProtocolVersion min_supported_protocol_version() {
      return min_supported_protocol_version_;
    }

    ProtocolVersion max_supported_protocol_version() {
      return max_supported_protocol_version_;
    }

    void set_supported_protocol_version(ProtocolVersion version) {
      min_supported_protocol_version_ = version;
      max_supported_protocol_version_ = version;
    }

    void set_supported_protocol_versions(ProtocolVersion min_version, ProtocolVersion max_version) {
      min_supported_protocol_version_ = min_version;
      max_supported_protocol_version_ = max_version;
    }

    uint32_t application_id() const {
      return application_id_;
    }

    void set_application_id(uint32_t application_id) {
      application_id_ = application_id;
    }

    bool can_use_broadcast() const {
      return can_use_broadcast_;
    }

    void set_can_use_broadcast(bool can_use_broadcast) {
      can_use_broadcast_ = can_use_broadcast;
    }

    bool can_use_multicast() const {
      return can_use_multicast_;
    }

    void set_can_use_multicast(bool can_use_multicast) {
      can_use_multicast_ = can_use_multicast;
    }

    int port() const {
      return port_;
    }

    void set_port(int port) {
      port_ = port;
    }

    unsigned int multicast_group_address() const {
      return multicast_group_address_;
    }

    void set_multicast_group_address(unsigned int group_address) {
      multicast_group_address_ = group_address;
    }

    long send_timeout_ms() const {
      return send_timeout_ms_;
    }

    void set_send_timeout_ms(long send_timeout_ms) {
      if (send_timeout_ms < 0)
        return;
      send_timeout_ms_ = send_timeout_ms;
    }

    long discovered_peer_ttl_ms() const {
      return discovered_peer_ttl_ms_;
    }

    void set_discovered_peer_ttl_ms(long discovered_peer_ttl_ms) {
      if (discovered_peer_ttl_ms < 0)
        return;
      discovered_peer_ttl_ms_ = discovered_peer_ttl_ms;
    }

    bool can_be_discovered() const {
      return can_be_discovered_;
    }

    void set_can_be_discovered(bool can_be_discovered) {
      can_be_discovered_ = can_be_discovered;
    }

    bool can_discover() const {
      return can_discover_;
    }

    void set_can_discover(bool can_discover) {
      can_discover_ = can_discover;
    }

    bool discover_self() const {
      return discover_self_;
    }

    void set_discover_self(bool discover_self) {
      discover_self_ = discover_self;
    }

    SamePeerMode same_peer_mode() const {
      return same_peer_mode_;
    }

    void set_same_peer_mode(SamePeerMode same_peer_mode) {
      same_peer_mode_ = same_peer_mode;
    }

   private:
    ProtocolVersion min_supported_protocol_version_;
    ProtocolVersion max_supported_protocol_version_;
    uint32_t application_id_;
    bool can_use_broadcast_;
    bool can_use_multicast_;
    int port_;
    unsigned int multicast_group_address_;
    long send_timeout_ms_;
    long discovered_peer_ttl_ms_;
    bool can_be_discovered_;
    bool can_discover_;
    bool discover_self_;
    SamePeerMode same_peer_mode_;
  };
}

#endif
