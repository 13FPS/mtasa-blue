/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check native pad copies and fail-closed binding without a game
 *****************************************************************************/

#include "../../Client/game_de/NativePadReader.h"
#include "../../Client/sdk/game/CGameAdapter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace
{
    int checks = 0;

    void Check(bool condition, const char* reason)
    {
        ++checks;
        // Optimized test builds must still detect wrong pad/old-state offsets.
        if (!condition)
            throw std::runtime_error(reason);
    }

    // Independent exact-build evidence: caller 0x1155044/0x1155059 selects
    // objects 0x188 apart; updater 0x1155387..0x11553D0 saves +0x38 at +0x68.
    // Poisoning the prefix catches the rejected public SDK's enclosing layout.
    constexpr std::size_t stride = 0x188;
    constexpr std::size_t currentOffset = 0x38;
    constexpr std::size_t previousOffset = 0x68;
    constexpr std::size_t stateBytes = 0x30;
    using NativeState = std::array<std::int16_t, 24>;
    struct PadFixture
    {
        std::array<std::byte, currentOffset>                           prefix;
        NativeState                                                    current;
        NativeState                                                    previous;
        std::array<std::byte, stride - currentOffset - 2 * stateBytes> unrelated;
    };
    using Fixture = std::array<PadFixture, 2>;
    static_assert(sizeof(NativeState) == stateBytes && sizeof(PadFixture) == stride);
    static_assert(offsetof(PadFixture, current) == currentOffset && offsetof(PadFixture, previous) == previousOffset);
    static_assert(std::is_trivially_copyable_v<Fixture> && sizeof(Fixture) == 0x310);
    static_assert(sizeof(CControllerState) == 48);
    static_assert(noexcept(mta::game_de::CopyNativePadStates(nullptr, std::size_t{}, std::uint32_t{}, std::declval<CControllerState&>(),
                                                             std::declval<CControllerState&>())));

    // This ordering is independent of the production copy implementation. The
    // fixture uses actual int16 arrays, never a controller struct as an array.
    const std::array<signed short CControllerState::*, 24> fields = {&CControllerState::LeftStickX,
                                                                     &CControllerState::LeftStickY,
                                                                     &CControllerState::RightStickX,
                                                                     &CControllerState::RightStickY,
                                                                     &CControllerState::LeftShoulder1,
                                                                     &CControllerState::LeftShoulder2,
                                                                     &CControllerState::RightShoulder1,
                                                                     &CControllerState::RightShoulder2,
                                                                     &CControllerState::DPadUp,
                                                                     &CControllerState::DPadDown,
                                                                     &CControllerState::DPadLeft,
                                                                     &CControllerState::DPadRight,
                                                                     &CControllerState::Start,
                                                                     &CControllerState::Select,
                                                                     &CControllerState::ButtonSquare,
                                                                     &CControllerState::ButtonTriangle,
                                                                     &CControllerState::ButtonCross,
                                                                     &CControllerState::ButtonCircle,
                                                                     &CControllerState::ShockButtonL,
                                                                     &CControllerState::ShockButtonR,
                                                                     &CControllerState::m_bChatIndicated,
                                                                     &CControllerState::m_bPedWalk,
                                                                     &CControllerState::m_bVehicleMouseLook,
                                                                     &CControllerState::m_bRadioTrackSkip};

    Fixture MakeFixture()
    {
        Fixture result{};
        for (std::size_t pad = 0; pad < result.size(); ++pad)
        {
            result[pad].prefix.fill(std::byte{0x5a});
            for (std::size_t field = 0; field < 24; ++field)
            {
                const auto magnitude = static_cast<std::int16_t>(100 + pad * 1000 + field * 7);
                result[pad].current[field] = field % 2 ? magnitude : static_cast<std::int16_t>(-magnitude);
                result[pad].previous[field] = field % 2 ? static_cast<std::int16_t>(-magnitude - 200) : static_cast<std::int16_t>(magnitude + 200);
            }
            result[pad].unrelated.fill(std::byte{0xa5});
        }
        result[0].current[0] = std::numeric_limits<std::int16_t>::min();
        result[0].previous[0] = std::numeric_limits<std::int16_t>::max();
        result[1].current[1] = std::numeric_limits<std::int16_t>::max();
        result[1].previous[1] = std::numeric_limits<std::int16_t>::min();
        return result;
    }

    CControllerState Sentinel(signed short seed)
    {
        CControllerState value;
        for (std::size_t field = 0; field < fields.size(); ++field)
            value.*fields[field] = static_cast<signed short>(seed + static_cast<signed short>(field));
        return value;
    }

    void Same(const CControllerState& actual, const CControllerState& expected)
    {
        for (const auto field : fields)
            Check(actual.*field == expected.*field, "failed operation must preserve every caller output field");
    }

    void Matches(const CControllerState& actual, const NativeState& expected)
    {
        for (std::size_t field = 0; field < fields.size(); ++field)
            Check(actual.*fields[field] == expected[field], "copy must retain each signed field at the requested pad/state offset");
    }

    void TestCopies()
    {
        auto fixture = MakeFixture();
        for (std::uint32_t pad = 0; pad < 2; ++pad)
        {
            CControllerState current, previous;
            const auto       bytes = (static_cast<std::size_t>(pad) + 1) * stride;
            Check(mta::game_de::CopyNativePadStates(&fixture, bytes, pad, current, previous), "complete requested stride is sufficient");
            Matches(current, fixture[pad].current);
            Matches(previous, fixture[pad].previous);
        }

        // Byte buffers can be unaligned. memcpy-based reading must not depend
        // on a packed native struct pointer or violate C++ alignment rules.
        std::array<std::byte, sizeof(Fixture) + 1> unaligned{};
        std::memcpy(unaligned.data() + 1, &fixture, sizeof(fixture));
        CControllerState current, previous;
        Check(mta::game_de::CopyNativePadStates(unaligned.data() + 1, sizeof(fixture), 1, current, previous), "unaligned byte storage remains readable");
        Matches(current, fixture[1].current);
        Matches(previous, fixture[1].previous);

        for (const auto lowFirst : {false, true})
        {
            for (auto& pad : fixture)
            {
                pad.current.fill(lowFirst ? std::numeric_limits<std::int16_t>::min() : std::numeric_limits<std::int16_t>::max());
                pad.previous.fill(lowFirst ? std::numeric_limits<std::int16_t>::max() : std::numeric_limits<std::int16_t>::min());
            }
            for (std::uint32_t pad = 0; pad < 2; ++pad)
            {
                Check(mta::game_de::CopyNativePadStates(&fixture, sizeof(fixture), pad, current, previous), "all signed16 boundary fields are valid data");
                Matches(current, fixture[pad].current);
                Matches(previous, fixture[pad].previous);
            }
        }
    }

    void TestInvalidCopies()
    {
        const auto fixture = MakeFixture();
        const auto seedCurrent = Sentinel(-17000);
        const auto seedPrevious = Sentinel(17000);
        const auto reject = [&](const void* bytes, std::size_t count, std::uint32_t pad)
        {
            auto current = seedCurrent;
            auto previous = seedPrevious;
            Check(!mta::game_de::CopyNativePadStates(bytes, count, pad, current, previous), "invalid source/index/length must reject");
            Same(current, seedCurrent);
            Same(previous, seedPrevious);
        };
        reject(nullptr, sizeof(fixture), 0);
        reject(nullptr, sizeof(fixture), 1);
        reject(&fixture, sizeof(fixture), 2);
        reject(&fixture, sizeof(fixture), std::numeric_limits<std::uint32_t>::max());
        for (std::uint32_t pad = 0; pad < 2; ++pad)
        {
            const auto required = (static_cast<std::size_t>(pad) + 1) * stride;
            for (const auto count : {std::size_t{0}, std::size_t{47}, std::size_t{48}, std::size_t{95}, std::size_t{96}, previousOffset + stateBytes,
                                     std::size_t{0x158}, required - 1})
                reject(&fixture, count, pad);
        }
    }

    void TestUnboundReader()
    {
        mta::game_de::NativePadReader   reader;
        mta::game_de::NativeDeviceState device;
        Check(!reader.ReadDeviceState(device), "device sources require explicit native binding");
        const auto seedCurrent = Sentinel(-9000);
        const auto seedPrevious = Sentinel(9000);
        auto       current = seedCurrent;
        auto       previous = seedPrevious;
        Check(!reader.IsBound(), "reader starts without engine identity or memory authority");
        Check(!reader.Read(0, current, previous), "unbound reader cannot publish input");
        Same(current, seedCurrent);
        Same(previous, seedPrevious);
        Check(!reader.Bind() && !reader.IsBound(), "ordinary test executable must fail pinned native game binding");
        Check(!reader.Read(1, current, previous), "rejected host binding cannot grant native reads");
        Same(current, seedCurrent);
        Same(previous, seedPrevious);
        bool        foreignRead = true;
        std::thread foreign([&] { foreignRead = reader.Read(0, current, previous); });
        foreign.join();
        Check(!foreignRead, "foreign thread cannot acquire an unbound engine reader");
        Same(current, seedCurrent);
        Same(previous, seedPrevious);
        reader.Reset();
        reader.Reset();
        Check(!reader.IsBound() && !reader.Read(0, current, previous), "reset is idempotent and does not bind a game");
        Same(current, seedCurrent);
        Same(previous, seedPrevious);
    }

    void TestDeviceReadRejection()
    {
        using namespace mta::game_de;
        static_assert(NativeDeviceStateCount == 127 && sizeof(NativeDeviceState) == 508);
        static_assert(std::is_same_v<decltype(NativeDeviceState::sources), std::array<std::int32_t, 127>>);
        NativePadReader   reader;
        NativeDeviceState state;
        for (std::size_t index = 0; index < state.sources.size(); ++index)
            state.sources[index] = index % 2 ? static_cast<std::int32_t>(index) : -static_cast<std::int32_t>(index);
        state.sources.front() = (std::numeric_limits<std::int32_t>::min)();
        state.sources.back() = (std::numeric_limits<std::int32_t>::max)();
        const auto original = state.sources;
        Check(!reader.ReadDeviceState(state) && state.sources == original, "unbound device read preserves all 127 signed32 caller values");
        Check(!reader.Bind(), "device source binding still rejects an ordinary test image");
        Check(!reader.ReadDeviceState(state) && state.sources == original, "failed exact-build binding cannot publish table values");
        bool        foreignRead = true;
        std::thread foreign([&] { foreignRead = reader.ReadDeviceState(state); });
        foreign.join();
        Check(!foreignRead && state.sources == original, "foreign unbound device read preserves the complete caller output");
        reader.Reset();
        Check(!reader.ReadDeviceState(state) && state.sources == original, "reset leaves device read unavailable without changing output");
    }
}

int main()
{
    try
    {
        TestCopies();
        TestInvalidCopies();
        TestUnboundReader();
        TestDeviceReadRejection();
        std::cout << "Passed " << checks << " native pad reader checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Native pad reader checks failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
