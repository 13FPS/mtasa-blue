/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual CPad SDK producer-control dependency boundary
 *****************************************************************************/
#include "../../Client/game_de/CPadControlDE.h"
#include "../../Client/game_de/NativePadStateAccess.h"
#include "../../Client/game_de/NativePadUpdateAccess.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace
{
    unsigned checks = 0, pageChecks = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    template <class Operation>
    void Refuses(Operation operation, const char* description)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, description);
            return;
        }
        catch (...)
        {
        }
        Check(false, description);
    }

    bool NoStateAccess(std::uintptr_t, std::size_t, bool, bool) noexcept
    {
        ++pageChecks;
        return false;
    }

    // Only the dependency is synthetic. The SDK methods under test are the
    // compiled production CPadControlDE implementation, with no copied logic.
    class TestUpdateOwner final : public mta::game_de::NativePadUpdateAccess
    {
    public:
        bool IsEnabled(bool& output) const noexcept override
        {
            ++reads;
            output = enabled;
            return allowRead;
        }

        bool SetDisabled(bool disabled) noexcept override
        {
            ++writes;
            lastDisableRequest = disabled;
            if (!allowWrite)
                return false;
            enabled = !disabled;
            return true;
        }

        mutable unsigned reads = 0;
        unsigned         writes = 0;
        bool             enabled = true, allowRead = true, allowWrite = true, lastDisableRequest = false;
    };

    class TestPad final : public mta::game_de::CPadControlDE
    {
    public:
        TestPad(mta::game_de::NativePadStateAccess& stateAccess, mta::game_de::NativePadUpdateAccess& updateAccess) noexcept
            : CPadControlDE(stateAccess, updateAccess)
        {
        }
        void          Restore() override { Unqualified(); }
        void          Store() override { Unqualified(); }
        void          SetHornHistoryValue(bool) override { Unqualified(); }
        void          SetLastTimeTouched(DWORD) override { Unqualified(); }
        short         GetShakeDur() override { Unqualified(); }
        unsigned char GetShakeFreq() override { Unqualified(); }
        void          SetShakeDur(short) override { Unqualified(); }

    private:
        [[noreturn]] static void Unqualified() { throw std::logic_error("Unqualified SDK method reached control fixture"); }
    };
}

int main()
{
    static_assert(std::is_abstract_v<mta::game_de::CPadControlDE> && std::is_abstract_v<mta::game_de::CPadStateDE>);
    static_assert(std::is_base_of_v<CPad, mta::game_de::CPadControlDE>);
    std::array<unsigned char, 160> untouched{};
    untouched.fill(0xa5);
    const auto                         before = untouched;
    mta::game_de::NativePadStateAccess states(reinterpret_cast<std::uintptr_t>(untouched.data() + 16), NoStateAccess);
    TestUpdateOwner                    owner;
    TestPad                            first(states, owner), second(states, owner);
    CPad&                              sdk = first;
    CPad&                              otherSdk = second;
    Check(owner.reads == 0 && owner.writes == 0, "constructing a borrowed SDK facade neither queries nor changes its native owner");
    Check(sdk.IsEnabled(), "SDK IsEnabled reads the admitted producer's actual enabled state");
    Check(owner.reads == 1 && owner.writes == 0, "IsEnabled performs one read and no implicit update");
    owner.enabled = false;
    Check(!sdk.IsEnabled(), "external owner changes are observed instead of a cached facade flag");
    owner.enabled = true;
    sdk.Disable(true);
    Check(owner.writes == 1 && owner.lastDisableRequest && !owner.enabled, "Disable(true) delegates native producer suppression");
    Check(!sdk.IsEnabled() && !otherSdk.IsEnabled(), "two borrowed facades observe the same global update owner");
    otherSdk.Disable(false);
    Check(owner.writes == 2 && !owner.lastDisableRequest && owner.enabled, "Disable(false) requests native producer restoration");
    Check(sdk.IsEnabled(), "restoration through the other facade becomes immediately visible");
    const auto reads = owner.reads;
    sdk.Disable(false);
    Check(owner.writes == 3 && owner.reads == reads, "repeated restoration remains an explicit owner operation without a guessed local flag");
    owner.allowWrite = false;
    Refuses([&] { sdk.Disable(true); }, "refused native suppression cannot masquerade as SDK success");
    Check(owner.writes == 4 && owner.enabled, "failed suppression preserves the owner's enabled state");
    owner.enabled = false;
    Refuses([&] { sdk.Disable(false); }, "refused restoration remains visible to the SDK caller");
    Check(owner.writes == 5 && !owner.enabled, "failed restoration cannot claim to reenable the global producer");
    owner.allowRead = false;
    owner.enabled = true;
    Refuses([&] { static_cast<void>(sdk.IsEnabled()); }, "a refusal must throw even if a dependency populated true output");
    owner.enabled = false;
    Refuses([&] { static_cast<void>(sdk.IsEnabled()); }, "a refusal must not fabricate a disabled result from false output");
    owner.allowRead = true;
    owner.allowWrite = true;
    sdk.Disable(false);
    Check(sdk.IsEnabled(), "a later qualified operation can recover after a refused owner request");
    Check(pageChecks == 0, "producer control does not borrow controller-state memory authority");
    Check(untouched == before, "producer operations leave current/previous, per-pad disable words and neighboring data untouched");
    std::printf("Passed %u actual CPadControlDE SDK producer checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
