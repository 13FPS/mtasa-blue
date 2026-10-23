/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share the actual immediate native pose prerequisites and bookends
 ****************************************************************************/
#pragma once
#include "NativePedTransformReader.h"
#include "NativePedTeleportProfile.h"
#include "NativeStartupWorldAccess.h"
#include "NativeUObjectMembershipAccess.h"
#include <array>
#include <cstring>
#include <vector>
namespace mta::game_de::NativePedPoseContext
{
    using Binding = NativePedTransformReader::Binding;
    using CheckOwner = NativePedTransformReader::CheckOwner;
    // Native Teleport requires a matrix. The named orientation operation has
    // a separately proved inline-heading branch; only it opts into that mode.
    enum class MatrixMode
    {
        Required,
        OrInline
    };
    bool Read(std::uintptr_t, void*, std::size_t) noexcept;
    struct Token
    {
        std::uintptr_t               address = 0;
        std::size_t                  size = 0;
        std::array<unsigned char, 8> bytes{};
    };
    struct Ledger
    {
        std::vector<Token> tokens;
        template <class T>
        bool Observe(std::uintptr_t address, T& output)
        {
            static_assert(sizeof(T) <= 8);
            if (tokens.size() >= NativePedTeleportProfile::MaximumObservationTokens)
                return false;
            Token token;
            token.address = address;
            token.size = sizeof(T);
            if (!Read(address, token.bytes.data(), token.size))
                return false;
            std::memcpy(&output, token.bytes.data(), sizeof(T));
            tokens.push_back(token);
            return true;
        }
        bool Fence() const noexcept;
    };
    struct Context
    {
        Ledger                                                                                                ledger;
        NativePedTransformReader::Value                                                                       transform;
        std::uintptr_t                                                                                        intelligence = 0, actor = 0, component = 0;
        NativeStartupWorldAccess::Value                                                                       world;
        std::array<NativeUObjectMembershipAccess::Snapshot, NativePedTeleportProfile::MaximumObjectSnapshots> objects{};
        std::size_t                                                                                           objectCount = 0;
    };
    // These are fresh value observations, not an engine lock, native object
    // lease or a second reference registry. The existing callback/token owner
    // retains authority and must retire any entered, unacknowledged prefix.
    ClockResult Owner(Binding, CheckOwner) noexcept;
    bool        Pins(Binding) noexcept;
    ClockResult Fence(Binding, CheckOwner, const Context&) noexcept;
    ClockResult Capture(Binding, CheckOwner, std::uint32_t handle, Context&, MatrixMode = MatrixMode::Required);
    bool        SameLifetime(const Context&, const Context&) noexcept;
}
