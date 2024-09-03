#include "udp_discovery_peer.hpp"

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "udp_discovery_protocol.hpp"

// sockets
#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SocketType;
typedef int AddressLenType;
const SocketType kInvalidSocket = INVALID_SOCKET;
#else
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int SocketType;
typedef socklen_t AddressLenType;
const SocketType kInvalidSocket = -1;
#endif

// time
#if defined(__APPLE__)
#include <mach/mach_time.h>
#include <stdint.h>
#endif
#if !defined(_WIN32)
#include <sys/time.h>
#endif
#include <time.h>

// threads
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <stdlib.h>
#endif

static void InitSockets() {
#if defined(_WIN32)
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
}

static void SetSocketTimeout(SocketType sock, int param, int timeout_ms) {
#if defined(_WIN32)
  setsockopt(sock, SOL_SOCKET, param, (const char*)&timeout_ms,
             sizeof(timeout_ms));
#else
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = 1000 * (timeout_ms % 1000);
  setsockopt(sock, SOL_SOCKET, param, (const char*)&timeout, sizeof(timeout));
#endif
}

static void CloseSocket(SocketType sock) {
#if defined(_WIN32)
  closesocket(sock);
#else
  close(sock);
#endif
}

static bool IsRightTime(long last_action_time, long now_time, long timeout,
                        long& time_to_wait_out) {
  if (last_action_time == 0) {
    time_to_wait_out = timeout;
    return true;
  }

  long time_passed = now_time - last_action_time;
  if (time_passed >= timeout) {
    time_to_wait_out = timeout - (time_passed - timeout);
    return true;
  }

  time_to_wait_out = timeout - time_passed;
  return false;
}

static uint32_t MakeRandomId() {
  srand((unsigned int)time(0));
  return (uint32_t)rand();
}

namespace udpdiscovery {
namespace impl {
long NowTime() {
#if defined(_WIN32)
  LARGE_INTEGER freq;
  if (!QueryPerformanceFrequency(&freq)) {
    return 0;
  }
  LARGE_INTEGER cur;
  QueryPerformanceCounter(&cur);
  return (long)(cur.QuadPart * 1000 / freq.QuadPart);
#elif defined(__APPLE__)
  mach_timebase_info_data_t time_info;
  mach_timebase_info(&time_info);

  uint64_t cur = mach_absolute_time();
  return (long)((cur / (time_info.denom * 1000000)) * time_info.numer);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif

  return 0;
}

void SleepFor(long time_ms) {
#if defined(_WIN32)
  Sleep((DWORD)time_ms);
#else
  usleep((useconds_t)(time_ms * 1000));
#endif
}

class MinimalisticMutex {
 public:
  MinimalisticMutex() {
#if defined(_WIN32)
    InitializeCriticalSection(&critical_section_);
#else
    pthread_mutex_init(&mutex_, 0);
#endif
  }

  ~MinimalisticMutex() {
#if defined(_WIN32)
    DeleteCriticalSection(&critical_section_);
#else
    pthread_mutex_destroy(&mutex_);
#endif
  }

  void Lock() {
#if defined(_WIN32)
    EnterCriticalSection(&critical_section_);
#else
    pthread_mutex_lock(&mutex_);
#endif
  }

  void Unlock() {
#if defined(_WIN32)
    LeaveCriticalSection(&critical_section_);
#else
    pthread_mutex_unlock(&mutex_);
#endif
  }

 private:
#if defined(_WIN32)
  CRITICAL_SECTION critical_section_;
#else
  pthread_mutex_t mutex_;
#endif
};

class MinimalisticThread : public MinimalisticThreadInterface {
 public:
#if defined(_WIN32)
  MinimalisticThread(LPTHREAD_START_ROUTINE f, void* env) : detached_(false) {
    thread_ = CreateThread(NULL, 0, f, env, 0, NULL);
  }
#else
  MinimalisticThread(void* (*f)(void*), void* env) : detached_(false) {
    pthread_create(&thread_, 0, f, env);
  }
#endif

  ~MinimalisticThread() { detach(); }

  void Detach() { detach(); }

  void Join() {
    if (detached_) return;

#if defined(_WIN32)
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
#else
    pthread_join(thread_, 0);
#endif
    detached_ = true;
  }

 private:
  void detach() {
    if (detached_) return;

#if defined(_WIN32)
    CloseHandle(thread_);
#else
    pthread_detach(thread_);
#endif
    detached_ = true;
  }

  bool detached_;
#if defined(_WIN32)
  HANDLE thread_;
#else
  pthread_t thread_;
#endif
};

class PeerEnv : public PeerEnvInterface {
 public:
  PeerEnv()
      : binding_sock_(kInvalidSocket),
        sock_(kInvalidSocket),
        packet_index_(0),
        ref_count_(0),
        exit_(false) {}

  ~PeerEnv() {
    if (binding_sock_ != kInvalidSocket) {
      CloseSocket(binding_sock_);
    }

    if (sock_ != kInvalidSocket) {
      CloseSocket(sock_);
    }
  }

  bool Start(const PeerParameters& parameters, const std::string& user_data) {
    parameters_ = parameters;
    user_data_ = user_data;

    if (!parameters_.can_use_broadcast() && !parameters_.can_use_multicast()) {
      std::cerr
          << "udpdiscovery::Peer can't use broadcast and can't use multicast."
          << std::endl;
      return false;
    }

    if (!parameters_.can_discover() && !parameters_.can_be_discovered()) {
      std::cerr << "udpdiscovery::Peer can't discover and can't be discovered."
                << std::endl;
      return false;
    }

    InitSockets();

    peer_id_ = MakeRandomId();

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == kInvalidSocket) {
      std::cerr << "udpdiscovery::Peer can't create socket." << std::endl;
      return false;
    }

    {
      int value = 1;
      setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, (const char*)&value,
                 sizeof(value));
    }

    if (parameters_.can_discover()) {
      binding_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
      if (binding_sock_ == kInvalidSocket) {
        std::cerr << "udpdiscovery::Peer can't create binding socket."
                  << std::endl;

        CloseSocket(sock_);
        sock_ = kInvalidSocket;
      }

      {
        int reuse_addr = 1;
        setsockopt(binding_sock_, SOL_SOCKET, SO_REUSEADDR,
                   (const char*)&reuse_addr, sizeof(reuse_addr));
#ifdef SO_REUSEPORT
        int reuse_port = 1;
        setsockopt(binding_sock_, SOL_SOCKET, SO_REUSEPORT,
                   (const char*)&reuse_port, sizeof(reuse_port));
#endif
      }

      if (parameters_.can_use_multicast()) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr =
            htonl(parameters_.multicast_group_address());
        mreq.imr_interface.s_addr = INADDR_ANY;
        setsockopt(binding_sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   (const char*)&mreq, sizeof(mreq));
      }

      sockaddr_in addr;
      memset((char*)&addr, 0, sizeof(sockaddr_in));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(parameters_.port());
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      if (bind(binding_sock_, (struct sockaddr*)&addr, sizeof(sockaddr_in)) <
          0) {
        CloseSocket(binding_sock_);
        binding_sock_ = kInvalidSocket;

        CloseSocket(sock_);
        sock_ = kInvalidSocket;

        std::cerr << "udpdiscovery::Peer can't bind socket." << std::endl;
        return false;
      }

      // TODO: Implement the way to unblock recvfrom without timeouting.
      SetSocketTimeout(binding_sock_, SO_RCVTIMEO, 1000);
    }

    return true;
  }

  void SetUserData(const std::string& user_data) {
    lock_.Lock();
    user_data_ = user_data;
    lock_.Unlock();
  }

  std::list<DiscoveredPeer> ListDiscovered() {
    std::list<DiscoveredPeer> result;

    lock_.Lock();
    result = discovered_peers_;
    lock_.Unlock();

    return result;
  }

  void Exit() {
    lock_.Lock();
    exit_ = true;
    lock_.Unlock();
  }

  void SendingThreadFunc() {
    lock_.Lock();
    ++ref_count_;
    lock_.Unlock();

    long last_send_time_ms = 0;
    long last_delete_idle_ms = 0;

    while (true) {
      lock_.Lock();
      if (exit_) {
        for (int protocol_version =
                 parameters_.min_supported_protocol_version();
             protocol_version <= parameters_.max_supported_protocol_version();
             ++protocol_version) {
          send(/* under_lock= */ true, (ProtocolVersion)protocol_version,
               kPacketIAmOutOfHere);
        }

        decreaseRefCountAndMaybeDestroySelfAndUnlock();
        return;
      }
      lock_.Unlock();

      long cur_time_ms = NowTime();
      long to_sleep_ms = 0;

      if (parameters_.can_be_discovered()) {
        if (IsRightTime(last_send_time_ms, cur_time_ms,
                        parameters_.send_timeout_ms(), to_sleep_ms)) {
          for (int protocol_version =
                   parameters_.min_supported_protocol_version();
               protocol_version <= parameters_.max_supported_protocol_version();
               ++protocol_version) {
            send(/* under_lock= */ false, (ProtocolVersion)protocol_version,
                 kPacketIAmHere);
          }
          last_send_time_ms = cur_time_ms;
        }
      }

      if (parameters_.can_discover()) {
        long to_sleep_until_next_delete_idle = 0;
        if (IsRightTime(last_delete_idle_ms, cur_time_ms,
                        parameters_.discovered_peer_ttl_ms(),
                        to_sleep_until_next_delete_idle)) {
          deleteIdle(cur_time_ms);
          last_delete_idle_ms = cur_time_ms;
        }

        if (to_sleep_ms > to_sleep_until_next_delete_idle) {
          to_sleep_ms = to_sleep_until_next_delete_idle;
        }
      }

      SleepFor(to_sleep_ms);
    }
  }

  void ReceivingThreadFunc() {
    lock_.Lock();
    ++ref_count_;
    lock_.Unlock();

    while (true) {
      sockaddr_in from_addr;
      AddressLenType addr_length = sizeof(sockaddr_in);

      std::string buffer;
      buffer.resize(kMaxPacketSize);

      int length = (int)recvfrom(binding_sock_, &buffer[0], buffer.size(), 0,
                                 (struct sockaddr*)&from_addr, &addr_length);

      lock_.Lock();
      if (exit_) {
        decreaseRefCountAndMaybeDestroySelfAndUnlock();
        return;
      }
      lock_.Unlock();

      if (length <= 0) {
        continue;
      }

      IpPort from;
      from.set_port(ntohs(from_addr.sin_port));
      from.set_ip(ntohl(from_addr.sin_addr.s_addr));

      buffer.resize(length);
      processReceivedBuffer(NowTime(), from, buffer);
    }
  }

 private:
  void decreaseRefCountAndMaybeDestroySelfAndUnlock() {
    --ref_count_;
    int cur_ref_count = ref_count_;

    // This method is performed when the mutex is locked.
    lock_.Unlock();

    if (cur_ref_count <= 0) {
      if (cur_ref_count < 0) {
        // Shouldn't be there.
        std::cerr << "Strangly ref count is less than 0." << std::endl;
      }

      delete this;
    }
  }

  void processReceivedBuffer(long cur_time_ms, const IpPort& from,
                             const std::string& buffer) {
    Packet packet;

    ProtocolVersion packet_version = packet.Parse(buffer);
    bool is_supported_packet_version =
        (packet_version >= parameters_.min_supported_protocol_version() &&
         packet_version <= parameters_.max_supported_protocol_version());

    if (packet_version != kProtocolVersionUnknown &&
        is_supported_packet_version) {
      bool accept_packet = false;
      if (parameters_.application_id() == packet.application_id()) {
        if (!parameters_.discover_self()) {
          if (packet.peer_id() != peer_id_) {
            accept_packet = true;
          }
        } else {
          accept_packet = true;
        }
      }

      if (accept_packet) {
        lock_.Lock();

        std::list<DiscoveredPeer>::iterator find_it = discovered_peers_.end();
        for (std::list<DiscoveredPeer>::iterator it = discovered_peers_.begin();
             it != discovered_peers_.end(); ++it) {
          if (Same(parameters_.same_peer_mode(), (*it).ip_port(), from)) {
            find_it = it;
            break;
          }
        }

        if (packet.packet_type() == kPacketIAmHere) {
          if (find_it == discovered_peers_.end()) {
            discovered_peers_.push_back(DiscoveredPeer());
            discovered_peers_.back().set_ip_port(from);
            discovered_peers_.back().SetUserData(packet.user_data(),
                                                 packet.snapshot_index());
            discovered_peers_.back().set_last_updated(cur_time_ms);
          } else {
            bool update_user_data =
                ((*find_it).last_received_packet() < packet.snapshot_index());
            if (update_user_data) {
              (*find_it).SetUserData(packet.user_data(),
                                     packet.snapshot_index());
            }
            (*find_it).set_last_updated(cur_time_ms);
          }
        } else if (packet.packet_type() == kPacketIAmOutOfHere) {
          if (find_it != discovered_peers_.end()) {
            discovered_peers_.erase(find_it);
          }
        }

        lock_.Unlock();
      }
    }
  }

  void deleteIdle(long cur_time_ms) {
    lock_.Lock();

    std::vector<std::list<DiscoveredPeer>::iterator> to_delete;
    for (std::list<DiscoveredPeer>::iterator it = discovered_peers_.begin();
         it != discovered_peers_.end(); ++it) {
      if (cur_time_ms - (*it).last_updated() >
          parameters_.discovered_peer_ttl_ms())
        to_delete.push_back(it);
    }

    for (size_t i = 0; i < to_delete.size(); ++i)
      discovered_peers_.erase(to_delete[i]);

    lock_.Unlock();
  }

  void send(bool under_lock, ProtocolVersion protocol_version,
            PacketType packet_type) {
    if (!under_lock) {
      lock_.Lock();
    }
    std::string user_data = user_data_;
    if (!under_lock) {
      lock_.Unlock();
    }

    Packet packet;
    packet.set_packet_type(packet_type);
    packet.set_application_id(parameters_.application_id());
    packet.set_peer_id(peer_id_);
    packet.set_snapshot_index(packet_index_);
    packet.SwapUserData(user_data);

    ++packet_index_;

    std::string packet_data;
    if (!packet.Serialize(protocol_version, packet_data)) {
      return;
    }

    sockaddr_in addr;
    memset((char*)&addr, 0, sizeof(sockaddr_in));

    if (parameters_.can_use_broadcast()) {
      addr.sin_family = AF_INET;
      addr.sin_port = htons(parameters_.port());
      addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    }

    if (parameters_.can_use_multicast()) {
      addr.sin_family = AF_INET;
      addr.sin_port = htons(parameters_.port());
      addr.sin_addr.s_addr = htonl(parameters_.multicast_group_address());
    }

    sendto(sock_, packet_data.data(), packet_data.size(), 0,
           (struct sockaddr*)&addr, sizeof(sockaddr_in));
  }

 private:
  PeerParameters parameters_;
  uint32_t peer_id_;
  SocketType binding_sock_;
  SocketType sock_;
  uint64_t packet_index_;

  MinimalisticMutex lock_;
  int ref_count_;
  bool exit_;
  std::string user_data_;
  std::list<DiscoveredPeer> discovered_peers_;
};

#if defined(_WIN32)
DWORD WINAPI SendingThreadFunc(void* env_typeless) {
  PeerEnv* env = (PeerEnv*)env_typeless;
  env->SendingThreadFunc();

  return 0;
}
#else
void* SendingThreadFunc(void* env_typeless) {
  PeerEnv* env = (PeerEnv*)env_typeless;
  env->SendingThreadFunc();

  return 0;
}
#endif

#if defined(_WIN32)
DWORD WINAPI ReceivingThreadFunc(void* env_typeless) {
  PeerEnv* env = (PeerEnv*)env_typeless;
  env->ReceivingThreadFunc();

  return 0;
}
#else
void* ReceivingThreadFunc(void* env_typeless) {
  PeerEnv* env = (PeerEnv*)env_typeless;
  env->ReceivingThreadFunc();

  return 0;
}
#endif
};  // namespace impl

Peer::Peer() : env_(0), sending_thread_(0), receiving_thread_(0) {}

Peer::~Peer() { Stop(false); }

bool Peer::Start(const PeerParameters& parameters,
                 const std::string& user_data) {
  Stop(false);

  impl::PeerEnv* env = new impl::PeerEnv();
  if (!env->Start(parameters, user_data)) {
    delete env;
    env = 0;

    return false;
  }

  env_ = env;

  sending_thread_ = new impl::MinimalisticThread(impl::SendingThreadFunc, env_);

  if (parameters.can_discover()) {
    receiving_thread_ =
        new impl::MinimalisticThread(impl::ReceivingThreadFunc, env_);
  }

  return true;
}

void Peer::SetUserData(const std::string& user_data) {
  if (env_) {
    env_->SetUserData(user_data);
  }
}

std::list<DiscoveredPeer> Peer::ListDiscovered() const {
  std::list<DiscoveredPeer> result;
  if (env_) {
    result = env_->ListDiscovered();
  }
  return result;
}

void Peer::Stop() { Stop(/* wait_for_threads= */ false); }

void Peer::StopAndWaitForThreads() { Stop(/* wait_for_threads= */ true); }

void Peer::Stop(bool wait_for_threads) {
  if (!env_) {
    return;
  }

  env_->Exit();

  // Threads live longer than the object itself. So env will be deleted in one
  // of the threads.
  env_ = 0;

  if (wait_for_threads) {
    if (sending_thread_) {
      sending_thread_->Join();
    }

    if (receiving_thread_) {
      receiving_thread_->Join();
    }
  } else {
    if (sending_thread_) {
      sending_thread_->Detach();
    }

    if (receiving_thread_) {
      receiving_thread_->Detach();
    }
  }

  delete sending_thread_;
  sending_thread_ = 0;
  delete receiving_thread_;
  receiving_thread_ = 0;
}

bool Same(PeerParameters::SamePeerMode mode, const IpPort& lhv,
          const IpPort& rhv) {
  switch (mode) {
    case PeerParameters::kSamePeerIp:
      return lhv.ip() == rhv.ip();

    case PeerParameters::kSamePeerIpAndPort:
      return (lhv.ip() == rhv.ip()) && (lhv.port() == rhv.port());
  }

  return false;
}

bool Same(PeerParameters::SamePeerMode mode,
          const std::list<DiscoveredPeer>& lhv,
          const std::list<DiscoveredPeer>& rhv) {
  for (std::list<DiscoveredPeer>::const_iterator lhv_it = lhv.begin();
       lhv_it != lhv.end(); ++lhv_it) {
    std::list<DiscoveredPeer>::const_iterator in_rhv = rhv.end();
    for (std::list<DiscoveredPeer>::const_iterator rhv_it = rhv.begin();
         rhv_it != rhv.end(); ++rhv_it) {
      if (Same(mode, (*lhv_it).ip_port(), (*rhv_it).ip_port())) {
        in_rhv = rhv_it;
        break;
      }
    }

    if (in_rhv == rhv.end()) {
      return false;
    }
  }

  for (std::list<DiscoveredPeer>::const_iterator rhv_it = rhv.begin();
       rhv_it != rhv.end(); ++rhv_it) {
    std::list<DiscoveredPeer>::const_iterator in_lhv = lhv.end();
    for (std::list<DiscoveredPeer>::const_iterator lhv_it = lhv.begin();
         lhv_it != lhv.end(); ++lhv_it) {
      if (Same(mode, (*rhv_it).ip_port(), (*lhv_it).ip_port())) {
        in_lhv = lhv_it;
        break;
      }
    }

    if (in_lhv == lhv.end()) {
      return false;
    }
  }

  return true;
}
};  // namespace udpdiscovery
