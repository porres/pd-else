#ifndef __UDP_DISCOVERY_PEER_H_
#define __UDP_DISCOVERY_PEER_H_

#include <list>

#include "udp_discovery_discovered_peer.hpp"
#include "udp_discovery_peer_parameters.hpp"

namespace udpdiscovery {
namespace impl {
long NowTime();

void SleepFor(long time_ms);

class PeerEnvInterface {
 public:
  virtual ~PeerEnvInterface() {}

  virtual void SetUserData(const std::string& user_data) = 0;

  virtual std::list<DiscoveredPeer> ListDiscovered() = 0;

  virtual void Exit() = 0;
};

class MinimalisticThreadInterface {
 public:
  virtual ~MinimalisticThreadInterface() {}

  virtual void Detach() = 0;

  virtual void Join() = 0;
};
}  // namespace impl

class Peer {
 public:
  Peer();
  ~Peer();

  /**
   * \brief Starts discovery peeer.
   */
  bool Start(const PeerParameters& parameters, const std::string& user_data);

  /**
   * \brief Sets user data of the started discovery peer.
   */
  void SetUserData(const std::string& user_data);

  /**
   * \brief Lists all discovered peers.
   */
  std::list<DiscoveredPeer> ListDiscovered() const;

  /**
   * \brief Stops discovery peer immediately. Working threads will finish
   * execution lately.
   */
  void Stop();

  /**
   * \brief Stops discovery peer and wait for all working threads to finish
   * execution.
   */
  void StopAndWaitForThreads();

  /**
   * \brief Stops discovery peer and potentially waits for all working threads
   * to finish execution.
   *
   * \deprecated This function is deprecated. Use Stop()
   * and StopAndWaitForThreads() instead.
   */
  void Stop(bool wait_for_threads);

 private:
  Peer(const Peer&);
  Peer& operator=(const Peer&);

 private:
  impl::PeerEnvInterface* env_;
  impl::MinimalisticThreadInterface* sending_thread_;
  impl::MinimalisticThreadInterface* receiving_thread_;
};

bool Same(PeerParameters::SamePeerMode mode, const IpPort& lhv,
          const IpPort& rhv);
bool Same(PeerParameters::SamePeerMode mode,
          const std::list<DiscoveredPeer>& lhv,
          const std::list<DiscoveredPeer>& rhv);
}  // namespace udpdiscovery

#endif
