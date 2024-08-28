#ifndef __UDP_DISCOVERY_PROTOCOL_VERSION_H_
#define __UDP_DISCOVERY_PROTOCOL_VERSION_H_

namespace udpdiscovery {
enum ProtocolVersion {
  kProtocolVersion0,
  kProtocolVersion1,
  kProtocolVersionCurrent = kProtocolVersion1,
  kProtocolVersionUnknown = 255
};
}

#endif
