/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify typed controller snapshot transactions without a game backend
 *****************************************************************************/

#include "../../Client/definitive/MtaControllerSnapshot.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
    int checks = 0;

    void Check(bool condition, const std::string& message)
    {
        ++checks;
        // Partial input must remain unpublished in optimized bridge builds too.
        if (!condition)
            throw std::runtime_error(message);
    }

    using Store = deonline::ControllerSnapshot;
    static_assert(std::is_same_v<decltype(std::declval<const Store&>().Current()), const CControllerState&>, "Use the canonical MTA state");
    static_assert(sizeof(CControllerState) == 48, "Keep the canonical 24-field layout");

    // This is an independent ordering fixture for the 20 native pad fields.
    // The four MTA logical extras have no native pad evidence in this seam.
    const std::array<signed short CControllerState::*, 20> expectedFields = {
        &CControllerState::LeftStickX,    &CControllerState::LeftStickY,    &CControllerState::RightStickX,    &CControllerState::RightStickY,
        &CControllerState::LeftShoulder1, &CControllerState::LeftShoulder2, &CControllerState::RightShoulder1, &CControllerState::RightShoulder2,
        &CControllerState::DPadUp,        &CControllerState::DPadDown,      &CControllerState::DPadLeft,       &CControllerState::DPadRight,
        &CControllerState::Start,         &CControllerState::Select,        &CControllerState::ButtonSquare,   &CControllerState::ButtonTriangle,
        &CControllerState::ButtonCross,   &CControllerState::ButtonCircle,  &CControllerState::ShockButtonL,   &CControllerState::ShockButtonR};
    const std::array<std::int32_t, 20> values = {-128, 128, -127, 127, 255, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    void Fill(Store& store, std::int32_t value)
    {
        Check(store.Begin(), "begin a complete transaction");
        for (std::uint32_t index = 0; index < 20; ++index)
            Check(store.Set(index, value), "supply every native field");
        Check(store.Commit(), "publish a complete transaction");
    }

    void Verify(Store& store, std::int32_t current, std::int32_t previous, std::uint32_t previousMask)
    {
        Check(store.CurrentMask() == 0xfffffu && store.PreviousMask() == previousMask, "published coverage is preserved");
        for (std::uint32_t index = 0; index < 20; ++index)
        {
            Check(store.Read(0, index) == static_cast<std::uint32_t>(current), "current field retains its signed bits");
            Check(store.Read(1, index) == static_cast<std::uint32_t>(previous), "previous field retains its signed bits");
        }
    }

    void TestMappingAndCoverage()
    {
        Store store;
        Check(store.CurrentMask() == 0 && store.PreviousMask() == 0, "zero storage initially has no verified coverage");
        Check(store.Begin(), "begin ordered field mapping");
        for (std::uint32_t index = 0; index < 20; ++index)
            Check(store.Set(index, values[index]), "accept a valid signed native field");
        Check(store.CurrentMask() == 0, "pending values remain unpublished");
        Check(store.Commit(), "all twenty fields permit one publication");
        Check(store.CurrentMask() == 0xfffffu && store.PreviousMask() == 0, "first publication covers only native fields");
        for (std::uint32_t index = 0; index < 20; ++index)
        {
            Check(store.Current().*(expectedFields[index]) == values[index], "native index maps to the canonical named member");
            Check(store.Read(0, index) == static_cast<std::uint32_t>(values[index]), "read sign extends native signed values to 32 bits");
            Check(store.Read(1, index) == 0, "first previous snapshot is unknown");
        }
        Check(store.Current().m_bChatIndicated == 0 && store.Current().m_bPedWalk == 0 && store.Current().m_bVehicleMouseLook == 0 &&
                  store.Current().m_bRadioTrackSkip == 0,
              "unmapped extras keep zero storage without gaining coverage");
        Check((store.CurrentMask() & 0xf00000u) == 0, "MTA logical extras remain unverified");
        Check(store.Read(0, 20) == 0 && store.Read(0, 0xffffffffu) == 0 && store.Read(2, 0) == 0, "reads outside the ABI are bounded");
    }

    void TestNoPartialPublicationAndAbort()
    {
        Store store;
        Fill(store, 11);
        Fill(store, -12);
        Check(store.Begin(), "begin partial input");
        for (std::uint32_t index = 0; index < 19; ++index)
            Check(store.Set(index, 99), "accept pending partial input");
        Verify(store, -12, 11, 0xfffffu);
        Check(!store.Commit(), "incomplete transaction cannot publish");
        Check(!store.Set(19, 99) && !store.Commit(), "failed commit cannot be repaired without a fresh begin");
        Verify(store, -12, 11, 0xfffffu);

        Check(store.Begin(), "begin invalid-index transaction");
        Check(!store.Set(20, 1), "reject an unmapped logical extra");
        for (std::uint32_t index = 0; index < 20; ++index)
            Check(!store.Set(index, 2), "invalid transaction remains poisoned");
        Check(!store.Commit(), "invalid index cannot overwrite published snapshots");
        Verify(store, -12, 11, 0xfffffu);

        Check(store.Begin(), "begin a transaction to replace");
        Check(store.Set(0, 100), "supply a field in the abandoned transaction");
        Check(store.Begin(), "a new begin clears pending fields");
        for (std::uint32_t index = 1; index < 20; ++index)
            Check(store.Set(index, 100), "supply fields after replacement begin");
        Check(!store.Commit(), "abandoned transaction's field cannot complete the new one");
        Verify(store, -12, 11, 0xfffffu);
    }

    void TestExtremesAndRangeFailure()
    {
        Store store;
        Check(!store.Set(0, 1) && !store.Commit(), "set and commit require an active begin");
        Fill(store, -32768);
        Fill(store, 32767);
        Verify(store, 32767, -32768, 0xfffffu);
        const std::array<std::int32_t, 4> invalid = {-32769, 32768, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
        for (const auto value : invalid)
        {
            Check(store.Begin(), "begin range failure fixture");
            Check(store.Set(0, 5), "pending valid data precedes range failure");
            Check(!store.Set(1, value), "out-of-int16 input does not truncate into MTA state");
            Check(!store.Set(1, 6) && !store.Commit(), "range failure cannot be repaired or published");
            Verify(store, 32767, -32768, 0xfffffu);
        }
        Fill(store, -1);
        Verify(store, -1, 32767, 0xfffffu);
    }

    void TestSingleAdvancementAndReset()
    {
        Store store;
        Fill(store, 1);
        Fill(store, 2);
        Verify(store, 2, 1, 0xfffffu);
        Check(!store.Commit(), "repeated commit does not advance previous a second time");
        Verify(store, 2, 1, 0xfffffu);
        Check(store.Begin(), "begin duplicate-index fixture");
        for (std::uint32_t index = 0; index < 20; ++index)
            Check(store.Set(index, 3), "supply complete duplicate fixture");
        Check(store.Set(0, -4), "duplicate valid field replaces pending value only");
        Check(store.Commit(), "duplicate field does not prevent complete publication");
        Check(store.Read(0, 0) == 0xfffffffcu && store.Read(1, 0) == 2, "one commit publishes the last pending value and prior current");

        Check(store.Begin() && store.Set(0, 100), "reset also discards an active pending transaction");
        store.Reset();
        Check(store.CurrentMask() == 0 && store.PreviousMask() == 0, "reset removes verified coverage");
        Check(!store.Commit() && !store.Set(1, 100), "reset leaves no active transaction");
        for (std::uint32_t index = 0; index < 20; ++index)
            Check(store.Read(0, index) == 0 && store.Read(1, index) == 0, "reset removes both snapshots' stored native values");
        Fill(store, 7);
        Verify(store, 7, 0, 0);
    }
}

int main()
{
    try
    {
        TestMappingAndCoverage();
        TestNoPartialPublicationAndAbort();
        TestExtremesAndRangeFailure();
        TestSingleAdvancementAndReset();
        std::cout << "Passed " << checks << " MTA controller snapshot checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MTA controller snapshot failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
