#include <wchar.h>
#include <iostream>
#include <array>
#include <algorithm>
#ifdef _WIN32
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#endif
#define WIN32_LEAN_AND_MEAN
#undef TEXT
#include <winsock2.h>
#include <ws2tcpip.h>
#undef min
typedef SSIZE_T ssize_t;
// Unix
#else
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
static const int INVALID_SOCKET = -1;
static const int SOCKET_ERROR   = -1;
using SOCKET = int;
#endif

const int socket_bind_err = 3;

class _socket_base {
public:
    enum class SocketType {
        TYPE_STREAM = SOCK_STREAM,
        TYPE_DGRAM = SOCK_DGRAM
    };

    void set_blocking(bool blocking) {
        // Get the current socket flags
    #ifdef _WIN32
        u_long mode = blocking ? 0 : 1;
        if (ioctlsocket(static_cast<SOCKET>(m_socket), FIONBIO, &mode) != 0) {
            // Failed to set socket mode
            return;
        }
    #else
        int flags = fcntl(static_cast<SOCKET>(m_socket), F_GETFL, 0);
        if (flags == -1) {
            // Failed to get socket flags
            return;
        }

        if (blocking) {
            flags &= ~O_NONBLOCK;  // Clear non-blocking flag
        } else {
            flags |= O_NONBLOCK;  // Set non-blocking flag
        }

        if (fcntl(static_cast<SOCKET>(m_socket), F_SETFL, flags) == -1) {
            // Failed to set socket flags
            return;
        }
    #endif

        return;
    }

     void set_timeout(int timeout_ms) {
#ifdef _WIN32
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms,
                    sizeof(timeout_ms));
#else
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = 1000 * (timeout_ms % 1000);
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#endif
     }

    void set_port(u_short port)
    {
        m_addr.sin_port = htons(port);
    }

    int set_address(const std::string& ip_address)
    {
        return inet_pton(AF_INET, ip_address.c_str(), &m_addr.sin_addr);
    }



protected:
    explicit _socket_base(const SocketType socket_type) : m_socket(), m_addr()
    {
    #ifdef WIN32
      // Initialize the WSDATA if no socket instance exists
      if(!s_count)
      {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
          throw std::runtime_error("Error initializing Winsock " + WSAGetLastError());
        }
      }
    #endif

        // Create the socket handle
        m_socket = socket(AF_INET, static_cast<int>(socket_type), 0);
        if (m_socket == INVALID_SOCKET)
        {
            throw std::runtime_error("Could not create socket");
        }

        m_addr.sin_family = AF_INET;

#ifdef WIN32
        ++s_count;
#endif
    }

#ifdef WIN32
    ~_socket_base()
    {
      if(! --s_count)
      {
        WSACleanup();
      }
    }
#else
    ~_socket_base() = default;
#endif

    SOCKET m_socket;
    sockaddr_in m_addr;
private:
#ifdef WIN32
    // Number of sockets is tracked to call WSACleanup on Windows
    static inline int s_count = 0;
#endif
};

class udp_client : public _socket_base
{
public:
    udp_client(u_short port, const std::string& ip_address) : _socket_base(SocketType::TYPE_DGRAM)
    {
        set_address(ip_address);
        set_port(port);
    };

    ssize_t send_data(const std::string& message)
    {
        size_t message_length = message.length();
        size_t offset = 0;
        size_t chunk_size = 512;

        while (offset < message_length)
        {
            size_t bytes_to_send = std::min(chunk_size, message_length - offset);
            ssize_t sent = sendto(m_socket, message.c_str() + offset, bytes_to_send, 0, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr));
            if (sent == SOCKET_ERROR)
            {
                return SOCKET_ERROR;
            }
            offset += sent;
        }
        // If the message is exactly 512 bytes long, we cannot distinguish between a complete and incomplete message,
        // so we send an empty message after it to solve that
        if((message_length & 511) == 0)
        {
            ssize_t sent = sendto(m_socket, "", 0, 0, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr));
            if (sent == SOCKET_ERROR)
            {
                return SOCKET_ERROR;
            }
        }

        return offset;
    }

};

class udp_server : public _socket_base
{
public:
    udp_server(u_short port, const std::string& ip_address = "0.0.0.0") : _socket_base(SocketType::TYPE_DGRAM)
    {
        set_port(port);
        set_address(ip_address);
    }

    int socket_bind()
    {
        if (bind(m_socket, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr)) == SOCKET_ERROR)
        {
            std::cout << "UDP Bind error." << std::endl;
    #ifdef WIN32
            std::cout << WSAGetLastError() << std::endl;
    #endif
            return socket_bind_err;
        }

        return 0;
    }

    bool receive_data(std::string& full_message)
    {
        std::array<char, 512> buffer;
        sockaddr_in client;
        socklen_t slen = sizeof(client);

        full_message.clear();
        ssize_t recv_len;

        while (true)
        {
            recv_len = recvfrom(m_socket, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&client), &slen);
            if (recv_len == SOCKET_ERROR)
            {
                return false;
            }

            full_message.append(buffer.data(), recv_len);

            // Break the loop if the received chunk is smaller than buffer size (indicating the end of the message)
            if ((size_t)recv_len < buffer.size())
            {
                break;
            }
        }

        return true;
    }
};
