#ifndef __DISCOVERY_DISCOVERED_PEER_H_
#define __DISCOVERY_DISCOVERED_PEER_H_

#include <stdint.h>
#include "udp_discovery_ip_port.hpp"

namespace udpdiscovery {
  class DiscoveredPeer {
   public:
    IpPort ip_port() const {
      return ip_port_;
    }

    void set_ip_port(const IpPort& ip_port) {
      ip_port_ = ip_port;
    }

    const std::string& user_data() const {
      return user_data_;
    }

    uint64_t last_received_packet() const {
      return last_received_packet_;
    }

    void SetUserData(const std::string& user_data, uint64_t last_received_packet) {
      user_data_ = user_data;
      last_received_packet_ = last_received_packet;
    }

    void set_last_updated(long last_updated) {
      last_updated_ = last_updated;
    }

    long last_updated() const {
      return last_updated_;
    }

   private:
    IpPort ip_port_;
    std::string user_data_;
    uint64_t last_received_packet_;
    long last_updated_;
  };
}

#endif
