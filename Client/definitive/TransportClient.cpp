/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include <winsock2.h>
#include <ws2tcpip.h>
#include "TransportClient.h"
#include <array>
#include <chrono>

namespace deonline
{
    using Clock = std::chrono::steady_clock;

    struct TransportClient::State
    {
        SOCKET                   socket = INVALID_SOCKET;
        bool                     winsock = false;
        std::uint64_t            nonce = 0;
        std::uint64_t            session = 0;
        std::uint32_t            id = 0;
        std::uint32_t            sendSequence = 0;
        std::uint32_t            snapshotSequence = 0;
        bool                     haveSnapshot = false;
        Clock::time_point        nextJoin{};
        Clock::time_point        lastSnapshot{};
        std::vector<PlayerState> players;
        std::string              error;

        bool Send(const std::vector<std::uint8_t>& packet)
        {
            const int sent = send(socket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
            if (sent == static_cast<int>(packet.size()))
                return true;
            error = "UDP send failed: " + std::to_string(WSAGetLastError());
            return false;
        }
    };

    TransportClient::TransportClient() : m_state(std::make_unique<State>())
    {
    }
    TransportClient::~TransportClient()
    {
        Close();
    }

    bool TransportClient::Open(const char* host, std::uint16_t port, std::uint64_t nonce)
    {
        Close();
        m_state->error.clear();
        if (!host || !*host || !port || !nonce)
        {
            m_state->error = "A host, nonzero port and nonzero connection nonce are required";
            return false;
        }
        WSADATA   data{};
        const int startup = WSAStartup(MAKEWORD(2, 2), &data);
        if (startup != 0)
        {
            m_state->error = "Winsock initialization failed: " + std::to_string(startup);
            return false;
        }
        m_state->winsock = true;
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        addrinfo*         addresses = nullptr;
        const std::string service = std::to_string(port);
        const int         resolved = getaddrinfo(host, service.c_str(), &hints, &addresses);
        if (resolved != 0)
        {
            m_state->error = "Server address resolution failed: " + std::to_string(resolved);
            Close();
            return false;
        }
        for (const addrinfo* address = addresses; address; address = address->ai_next)
        {
            SOCKET candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (candidate == INVALID_SOCKET)
                continue;
            // A connected UDP socket filters packets from endpoints other than this
            // server, so an unrelated peer cannot supply movement snapshots.
            if (connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
            {
                m_state->socket = candidate;
                break;
            }
            closesocket(candidate);
        }
        freeaddrinfo(addresses);
        if (m_state->socket == INVALID_SOCKET)
        {
            m_state->error = "Could not open a UDP connection to the server";
            Close();
            return false;
        }
        u_long nonblocking = 1;
        if (ioctlsocket(m_state->socket, FIONBIO, &nonblocking) != 0)
        {
            m_state->error = "Could not set the socket to nonblocking mode";
            Close();
            return false;
        }
        m_state->nonce = nonce;
        m_state->nextJoin = Clock::now();
        return true;
    }

    void TransportClient::Close()
    {
        if (m_state->socket != INVALID_SOCKET)
        {
            try
            {
                if (m_state->id && m_state->session)
                    m_state->Send(EncodeLeave(m_state->id, m_state->session, ++m_state->sendSequence));
            }
            catch (...)
            {
                // Leaving is best effort: allocation/send failures must not prevent
                // socket cleanup or throw from the destructor across a DLL boundary.
            }
            closesocket(m_state->socket);
            m_state->socket = INVALID_SOCKET;
        }
        if (m_state->winsock)
        {
            WSACleanup();
            m_state->winsock = false;
        }
        m_state->nonce = 0;
        m_state->session = 0;
        m_state->id = 0;
        m_state->sendSequence = 0;
        m_state->snapshotSequence = 0;
        m_state->haveSnapshot = false;
        m_state->players.clear();
    }

    bool TransportClient::Pump()
    {
        if (m_state->socket == INVALID_SOCKET)
        {
            m_state->error = "Transport is not open";
            return false;
        }
        const auto now = Clock::now();
        if (!m_state->id && now >= m_state->nextJoin)
        {
            if (!m_state->Send(EncodeJoin(m_state->nonce, ++m_state->sendSequence)))
                return false;
            m_state->nextJoin = now + std::chrono::milliseconds(250);
        }
        // Bound receive work so networking cannot monopolize a future game-thread callback.
        for (int received = 0; received < 64; ++received)
        {
            std::array<std::uint8_t, kMaxDatagramSize + 1> bytes{};
            const int length = recv(m_state->socket, reinterpret_cast<char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
            if (length == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                    break;
                if (error == WSAEMSGSIZE)
                    continue;
                m_state->error = "UDP receive failed: " + std::to_string(error);
                return false;
            }
            std::uint32_t id = 0;
            std::uint64_t session = 0;
            if (!m_state->id && DecodeWelcome(bytes.data(), static_cast<std::size_t>(length), m_state->nonce, id, session))
            {
                m_state->id = id;
                m_state->session = session;
                m_state->lastSnapshot = now;
                continue;
            }
            std::uint32_t            sequence = 0;
            std::vector<PlayerState> players;
            if (m_state->id && DecodeSnapshot(bytes.data(), static_cast<std::size_t>(length), m_state->id, m_state->session, sequence, players))
            {
                // Reordered datagrams must not rewind the remote-player state.
                if (!m_state->haveSnapshot || IsNewerSequence(sequence, m_state->snapshotSequence))
                {
                    m_state->players = std::move(players);
                    m_state->snapshotSequence = sequence;
                    m_state->haveSnapshot = true;
                    m_state->lastSnapshot = now;
                }
            }
        }
        if (m_state->id && now - m_state->lastSnapshot > std::chrono::seconds(5))
        {
            m_state->error = "Server snapshots timed out";
            Close();
            return false;
        }
        return true;
    }

    bool TransportClient::Submit(const Transform& transform)
    {
        if (!Connected() || !IsValidTransform(transform))
            return false;
        return m_state->Send(EncodeUpdate(m_state->id, m_state->session, transform, ++m_state->sendSequence));
    }

    bool TransportClient::Connected() const
    {
        return m_state->id != 0;
    }
    std::uint32_t TransportClient::PlayerId() const
    {
        return m_state->id;
    }
    const std::vector<PlayerState>& TransportClient::Players() const
    {
        return m_state->players;
    }
    const std::string& TransportClient::Error() const
    {
        return m_state->error;
    }
}  // namespace deonline
