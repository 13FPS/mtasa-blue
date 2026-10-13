/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Game-independent transport for one small custom server
 *****************************************************************************/

#pragma once

#include "Protocol.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace deonline
{
    // The game adapter will call this from one owning thread. Keeping sockets outside
    // game hooks lets us test synchronization before trusting any DE memory layout.
    class TransportClient
    {
    public:
        TransportClient();
        ~TransportClient();
        TransportClient(const TransportClient&) = delete;
        TransportClient& operator=(const TransportClient&) = delete;

        bool                            Open(const char* host, std::uint16_t port, std::uint64_t nonce);
        void                            Close();
        bool                            Pump();
        bool                            Submit(const Transform& transform);
        bool                            Connected() const;
        std::uint32_t                   PlayerId() const;
        const std::vector<PlayerState>& Players() const;
        const std::string&              Error() const;

    private:
        struct State;
        std::unique_ptr<State> m_state;
    };
}  // namespace deonline
