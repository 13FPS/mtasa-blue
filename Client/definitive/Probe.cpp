/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Synthetic movement probe; does not read or control GTA
 *****************************************************************************/

#include "TransportClient.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

int main(int argc, char** argv)
{
    std::string   host = "127.0.0.1";
    unsigned long port = 22040;
    double        duration = 5.0;
    float         seed = 1.0f;
    bool          requirePeer = false;
    try
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--require-peer")
                requirePeer = true;
            else if (index + 1 < argc)
            {
                const std::string value = argv[++index];
                if (argument == "--host")
                    host = value;
                else if (argument == "--port")
                    port = std::stoul(value);
                else if (argument == "--seconds")
                    duration = std::stod(value);
                else if (argument == "--seed")
                    seed = std::stof(value);
                else
                    throw std::invalid_argument("Unknown option: " + argument);
            }
            else
                throw std::invalid_argument("Missing value for " + argument);
        }
        if (port == 0 || port > 65535 || !std::isfinite(duration) || duration <= 0 || duration > 3600 || !std::isfinite(seed) || std::abs(seed) > 1000)
            throw std::invalid_argument("Port, duration or movement seed is out of range");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\nUsage: de_client_probe [--host HOST] [--port PORT] [--seconds N] [--seed N] [--require-peer]\n";
        return 2;
    }

    std::random_device        random;
    const std::uint64_t       nonce = (static_cast<std::uint64_t>(random()) << 32) | random() | 1;
    deonline::TransportClient client;
    if (!client.Open(host.c_str(), static_cast<std::uint16_t>(port), nonce))
    {
        std::cerr << client.Error() << '\n';
        return 1;
    }
    using Clock = std::chrono::steady_clock;
    const auto    start = Clock::now();
    auto          nextUpdate = start;
    bool          joined = false;
    unsigned int  peerMovementChanges = 0;
    std::uint32_t trackedPeer = 0;
    CVector       previousPeer;
    std::size_t   maxPeers = 0;
    while (std::chrono::duration<double>(Clock::now() - start).count() < duration)
    {
        if (!client.Pump())
        {
            std::cerr << client.Error() << '\n';
            return 1;
        }
        const auto now = Clock::now();
        if (client.Connected())
        {
            joined = true;
            if (now >= nextUpdate)
            {
                // Synthetic movement isolates the network acceptance test from
                // unverified DE player pointers and spawn functions.
                const float               elapsed = static_cast<float>(std::chrono::duration<double>(now - start).count());
                const deonline::Transform transform{CVector(seed * 10.0f + elapsed, seed, 3.0f), 0.0f};
                if (!client.Submit(transform))
                {
                    std::cerr << "Failed to submit movement\n";
                    return 1;
                }
                nextUpdate = now + std::chrono::milliseconds(50);
            }
            std::size_t peers = 0;
            for (const auto& player : client.Players())
            {
                if (player.id == client.PlayerId())
                    continue;
                ++peers;
                if (trackedPeer == player.id && (player.transform.position - previousPeer).LengthSquared() > 0.0001f)
                    ++peerMovementChanges;
                if (trackedPeer == 0 || trackedPeer == player.id)
                {
                    trackedPeer = player.id;
                    previousPeer = player.transform.position;
                }
            }
            maxPeers = std::max(maxPeers, peers);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::cout << "{\"event\":\"probe_result\",\"player_id\":" << client.PlayerId() << ",\"joined\":" << (joined ? "true" : "false")
              << ",\"max_remote_players\":" << maxPeers << ",\"remote_motion_observed\":" << (peerMovementChanges >= 2 ? "true" : "false")
              << ",\"game_adapter_active\":false}\n";
    return joined && (!requirePeer || peerMovementChanges >= 2) ? 0 : 1;
}
