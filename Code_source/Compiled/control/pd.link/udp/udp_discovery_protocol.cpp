#include "udp_discovery_protocol.hpp"

namespace udpdiscovery {

namespace impl {
bool SerializeString(SerializeDirection direction, std::string* value,
                     int value_size, BufferView* buffer_view) {
  switch (direction) {
    case kSerialize:
      buffer_view->InsertBack(*value, value_size);
      break;

    case kParse:
      if (!buffer_view->CanRead(value_size)) {
        return false;
      }
      value->resize(value_size);
      for (int i = 0; i < (int)value_size; ++i) {
        (*value)[i] = buffer_view->Read();
      }
      break;
  }

  return true;
}

ProtocolVersion GetProtocolVersion(uint8_t version) {
  if (version == kProtocolVersion0) {
    return kProtocolVersion0;
  } else if (version == kProtocolVersion1) {
    return kProtocolVersion1;
  }
  return kProtocolVersionUnknown;
}

PacketType GetPacketType(uint8_t packet_type) {
  if (packet_type == kPacketIAmHere) {
    return kPacketIAmHere;
  } else if (packet_type == kPacketIAmOutOfHere) {
    return kPacketIAmOutOfHere;
  }
  return kPacketTypeUnknown;
}
}  // namespace impl

bool Packet::Serialize(ProtocolVersion protocol_version,
                       std::string& buffer_out) {
  if (protocol_version == kProtocolVersion0) {
    if (user_data_.size() > kMaxUserDataSizeV0) {
      return false;
    }
  } else if (protocol_version == kProtocolVersion1) {
    if (user_data_.size() > kMaxUserDataSizeV1) {
      return false;
    }
  }

  impl::BufferView buffer_view(&buffer_out);
  return Serialize(protocol_version, impl::kSerialize, &buffer_view);
}

ProtocolVersion Packet::Parse(const std::string& buffer) {
  impl::BufferView buffer_view(const_cast<std::string*>(&buffer));

  uint8_t magic[4];
  if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &magic[0],
                                               &buffer_view)) {
    return kProtocolVersionUnknown;
  }
  if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &magic[1],
                                               &buffer_view)) {
    return kProtocolVersionUnknown;
  }
  if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &magic[2],
                                               &buffer_view)) {
    return kProtocolVersionUnknown;
  }
  if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &magic[3],
                                               &buffer_view)) {
    return kProtocolVersionUnknown;
  }

  uint8_t version = kProtocolVersion0;
  if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &version,
                                               &buffer_view)) {
    return kProtocolVersionUnknown;
  }

  ProtocolVersion protocol_version = kProtocolVersionUnknown;
  if (magic[0] == 'R' && magic[1] == 'N' && magic[2] == '6' &&
      magic[3] == 'U') {
    protocol_version = kProtocolVersion0;
  } else if (magic[0] == 'S' && magic[1] == 'O' && magic[2] == '7' &&
             magic[3] == 'V') {
    protocol_version = impl::GetProtocolVersion(version);
    if (protocol_version == kProtocolVersionUnknown ||
        protocol_version == kProtocolVersion0) {
      return kProtocolVersionUnknown;
    }
  }

  if (protocol_version == kProtocolVersionUnknown) {
    return kProtocolVersionUnknown;
  }

  if (!Serialize(protocol_version, impl::kParse, &buffer_view)) {
    return kProtocolVersionUnknown;
  }

  return protocol_version;
}

bool Packet::Serialize(ProtocolVersion protocol_version,
                       impl::SerializeDirection direction,
                       impl::BufferView* buffer_view) {
  if (direction == impl::kSerialize) {
    if (protocol_version == kProtocolVersion0) {
      buffer_view->push_back('R');
      buffer_view->push_back('N');
      buffer_view->push_back('6');
      buffer_view->push_back('U');
    } else if (protocol_version == kProtocolVersion1) {
      buffer_view->push_back('S');
      buffer_view->push_back('O');
      buffer_view->push_back('7');
      buffer_view->push_back('V');
    } else {
      return false;
    }

    uint8_t version = protocol_version;
    impl::SerializeUnsignedIntegerBigEndian(impl::kSerialize, &version,
                                            buffer_view);
  }

  uint8_t reserved = 0;
  for (int i = 0; i < 3; ++i) {
    if (!impl::SerializeUnsignedIntegerBigEndian(direction, &reserved,
                                                 buffer_view)) {
      return false;
    }
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &packet_type_,
                                               buffer_view)) {
    return false;
  }

  if (direction == impl::kParse) {
    if (impl::GetPacketType(packet_type_) == kPacketTypeUnknown) {
      return false;
    }
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &application_id_,
                                               buffer_view)) {
    return false;
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &peer_id_,
                                               buffer_view)) {
    return false;
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &snapshot_index_,
                                               buffer_view)) {
    return false;
  }

  uint16_t user_data_size = (uint16_t)user_data_.size();
  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &user_data_size,
                                               buffer_view)) {
    return false;
  }

  if (direction == impl::kParse) {
    if (protocol_version == kProtocolVersion0) {
      if (user_data_size > kMaxUserDataSizeV0) {
        return false;
      }
    } else if (protocol_version == kProtocolVersion1) {
      if (user_data_size > kMaxUserDataSizeV1) {
        return false;
      }
    }
  }

  uint16_t padding_size = 0;
  if (protocol_version == kProtocolVersion0) {
    if (!impl::SerializeUnsignedIntegerBigEndian(direction, &padding_size,
                                                 buffer_view)) {
      return false;
    }

    if (padding_size > kMaxPaddingSizeV0) {
      return false;
    }
  }

  // End of serializing header.

  if (direction == impl::kParse) {
    if (buffer_view->LeftUnparsed() != user_data_size + padding_size) {
      return false;
    }
  }

  if (!impl::SerializeString(direction, &user_data_, user_data_size,
                             buffer_view)) {
    return false;
  }

  // Do not serialize padding even for protocol version 0.

  return true;
}

}  // namespace udpdiscovery
