#ifndef __UDP_DISCOVERY_PROTOCOL_H_
#define __UDP_DISCOVERY_PROTOCOL_H_

#include <stdint.h>

#include <string>

#include "udp_discovery_protocol_version.hpp"

namespace udpdiscovery {
namespace impl {
class BufferView {
 public:
  BufferView() : buffer_(0), parsed_(0) {}

  BufferView(std::string* buffer) : buffer_(buffer), parsed_(0) {}

  std::string* buffer() { return buffer_; }

  void push_back(char c) { buffer_->push_back(c); }

  void InsertBack(const std::string& s, int size) {
    buffer_->insert(buffer_->end(), s.begin(), s.begin() + size);
  }

  int parsed() const { return parsed_; }

  int LeftUnparsed() const { return (int)buffer_->size() - parsed_; }

  bool CanRead(int num_bytes) {
    return (parsed_ + num_bytes <= (int)buffer_->size());
  }

  char Read() {
    char c = buffer_->at(parsed_);
    ++parsed_;
    return c;
  }

 private:
  std::string* buffer_;
  int parsed_;
};

enum SerializeDirection {
  kSerialize,
  kParse,
};

template <typename ValueType>
bool SerializeUnsignedIntegerBigEndian(SerializeDirection direction,
                                       ValueType* value,
                                       BufferView* buffer_view) {
  switch (direction) {
    case kSerialize: {
      int n = sizeof(ValueType);
      for (int i = 0; i < n; ++i) {
        uint8_t c = (uint8_t)((*value) >> ((n - i - 1) * 8)) & 0xff;
        buffer_view->push_back(c);
      }
    } break;

    case kParse:
      *value = 0;
      if (buffer_view->CanRead(sizeof(ValueType))) {
        int n = sizeof(ValueType);
        for (int i = 0; i < n; ++i) {
          ValueType v = (uint8_t)buffer_view->Read();
          *value |= (v << ((n - i - 1) * 8));
        }
      } else {
        return false;
      }
      break;
  }

  return true;
}

bool SerializeString(SerializeDirection direction, std::string* value,
                     int value_size, BufferView* buffer_view);
}  // namespace impl

const size_t kMaxUserDataSizeV0 = 32768;
const size_t kMaxPaddingSizeV0 = 32768;
const size_t kMaxUserDataSizeV1 = 4096;
// Used for receiving buffer.
const size_t kMaxPacketSize = 65536;

namespace impl {
ProtocolVersion GetProtocolVersion(uint8_t version);
}  // namespace impl

enum PacketType {
  kPacketIAmHere,
  kPacketIAmOutOfHere,
  kPacketTypeUnknown = 255
};

namespace impl {
PacketType GetPacketType(uint8_t packet_type);
}  // namespace impl

class Packet {
 public:
  PacketType packet_type() { return (PacketType)packet_type_; }

  void set_packet_type(PacketType packet_type) { packet_type_ = packet_type; }

  uint32_t application_id() const { return application_id_; }

  void set_application_id(uint32_t application_id) {
    application_id_ = application_id;
  }

  uint32_t peer_id() const { return peer_id_; }

  void set_peer_id(uint32_t peer_id) { peer_id_ = peer_id; }

  uint64_t snapshot_index() const { return snapshot_index_; }

  void set_snapshot_index(uint64_t snapshot_index) {
    snapshot_index_ = snapshot_index;
  }

  const std::string& user_data() const { return user_data_; }

  void set_user_data(const std::string& user_data) { user_data_ = user_data; }

  void SwapUserData(std::string& user_data) {
    std::swap(user_data_, user_data);
  }

  // Writes the packet to the buffer for sending. Uses provided protocol_version
  // to construct data on wire. This function should return false in the case
  // when it is not possible to convert the current packet representation to
  // the wire representation of the given version. The caller can reserve memory
  // in the buffer_out and no memory will be allocated in this function.
  bool Serialize(ProtocolVersion protocol_version, std::string& buffer_out);

  // Parses the provided buffer and returns the detected protocol version. If
  // parsing fails then kProtocolVersionUnknown is returned.
  ProtocolVersion Parse(const std::string& buffer);

 private:
  bool Serialize(ProtocolVersion protocol_version,
                 impl::SerializeDirection direction,
                 impl::BufferView* buffer_view);

 private:
  uint8_t packet_type_;
  uint32_t application_id_;
  uint32_t peer_id_;
  uint64_t snapshot_index_;
  std::string user_data_;
};

};  // namespace udpdiscovery

#endif
