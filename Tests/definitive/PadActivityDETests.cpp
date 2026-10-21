/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual CPad activity access using owned native-pad storage
 *****************************************************************************/
#include "../../Client/game_de/CPadActivityDE.h"
#include "../../Client/game_de/NativePadActivityAccess.h"
#include "../../Client/game_de/NativePadStateAccess.h"
#include "../../Client/game_de/NativePadUpdateAccess.h"
#include "../../Client/game_de/NativePadReader.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    constexpr std::size_t FieldOffset = 0x164, FieldSize = 4, Prefix = 64;
    enum class Policy
    {
        Allow,
        RefuseWrite,
        RefuseRead,
        ChangeBeforeRead,
        InspectProtection
    };
    Policy         policy = Policy::Allow;
    std::uintptr_t expectedField = 0;
    unsigned       checks = 0, reads = 0, writes = 0, stateCalls = 0;
    bool           exactRange = true;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, description);
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

    void Expect(std::uintptr_t pad, Policy selected = Policy::Allow)
    {
        expectedField = pad + FieldOffset;
        policy = selected;
        reads = writes = 0;
        exactRange = true;
    }

    bool PageCheck(std::uintptr_t address, std::size_t bytes, bool writable, bool executable) noexcept
    {
        exactRange = exactRange && address == expectedField && bytes == FieldSize && !executable;
        if (writable)
            ++writes;
        else
            ++reads;
        if (!exactRange || (writable && policy == Policy::RefuseWrite) || (!writable && policy == Policy::RefuseRead))
            return false;
        if (policy == Policy::InspectProtection)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                page.Type != MEM_PRIVATE || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || (writable && (page.Protect & 0xff) != PAGE_READWRITE))
                return false;
        }
        if (!writable && policy == Policy::ChangeBeforeRead)
        {
            // Simulate a later native edit inside this owned page. A failed
            // comparison must not overwrite it with an invented rollback.
            constexpr std::uint32_t changed = 0xdecafbad;
            std::memcpy(reinterpret_cast<void*>(address), &changed, sizeof(changed));
        }
        return true;
    }

    bool NoStateAccess(std::uintptr_t, std::size_t, bool, bool) noexcept
    {
        ++stateCalls;
        return false;
    }

    class UnusedUpdateOwner final : public mta::game_de::NativePadUpdateAccess
    {
    public:
        bool IsEnabled(bool&) const noexcept override
        {
            ++calls;
            return false;
        }
        bool SetDisabled(bool) noexcept override
        {
            ++calls;
            return false;
        }
        mutable unsigned calls = 0;
    };

    // The production type stays abstract. Only genuinely unqualified SDK
    // methods are supplied by this test, and they fail if accidentally used.
    class TestPad final : public mta::game_de::CPadActivityDE
    {
    public:
        TestPad(mta::game_de::NativePadStateAccess& stateAccess, mta::game_de::NativePadUpdateAccess& updateAccess,
                mta::game_de::NativePadActivityAccess& activityAccess) noexcept
            : CPadActivityDE(stateAccess, updateAccess, activityAccess)
        {
        }
        void          Restore() override { Unqualified(); }
        void          Store() override { Unqualified(); }
        void          SetHornHistoryValue(bool) override { Unqualified(); }
        short         GetShakeDur() override { Unqualified(); }
        unsigned char GetShakeFreq() override { Unqualified(); }
        void          SetShakeDur(short) override { Unqualified(); }

    private:
        [[noreturn]] static void Unqualified() { throw std::logic_error("unqualified SDK activity fixture method"); }
    };

    struct Page
    {
        unsigned char* data = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        ~Page()
        {
            if (data)
                VirtualFree(data, 0, MEM_RELEASE);
        }
    };

    std::array<unsigned char, 4096> Copy(const Page& page)
    {
        std::array<unsigned char, 4096> output{};
        std::memcpy(output.data(), page.data, output.size());
        return output;
    }

    bool OnlyFieldChanged(const Page& page, std::array<unsigned char, 4096> before, std::size_t padOffset, std::uint32_t value)
    {
        std::memcpy(before.data() + padOffset + FieldOffset, &value, sizeof(value));
        return std::memcmp(page.data, before.data(), before.size()) == 0;
    }
}

int main()
{
    namespace Profile = mta::game_de::NativePadActivityProfile;
    using mta::game_de::NativePadActivityAccess;
    static_assert(Profile::PadStride == 0x188 && Profile::PadCount == 2 && Profile::FieldOffset == FieldOffset && Profile::FieldSize == FieldSize);
    static_assert(Profile::PadStride == mta::game_de::NativePadStride && sizeof(DWORD) == FieldSize);
    static_assert(std::is_abstract_v<mta::game_de::CPadActivityDE> && std::is_base_of_v<CPad, mta::game_de::CPadActivityDE>);
    Page page;
    Check(page.data != nullptr, "allocate one owned page holding two native pad spans and canaries");
    std::memset(page.data, 0xa5, 4096);
    const auto                         pad0 = reinterpret_cast<std::uintptr_t>(page.data + Prefix), pad1 = pad0 + Profile::PadStride;
    NativePadActivityAccess            first(pad0, PageCheck), second(pad1, PageCheck);
    mta::game_de::NativePadStateAccess states(pad0 + mta::game_de::NativePadCurrentOffset, NoStateAccess);
    UnusedUpdateOwner                  updates;
    TestPad                            fixture(states, updates, first);
    CPad&                              sdk = fixture;
    for (const std::uint32_t time : {0x01020304u, 0x80000000u, 0xffffffffu, 0u})
    {
        const auto before = Copy(page);
        Expect(pad0);
        sdk.SetLastTimeTouched(time);
        Check(exactRange && writes == 1 && reads == 1, "SDK writes and verifies exactly the qualified four-byte activity field");
        Check(OnlyFieldChanged(page, before, Prefix, time), "timestamp preserves every current/previous state, other pad, native field and outer canary");
        std::uint32_t observed = 0x12345678;
        Expect(pad0);
        Check(first.ReadLastTimeTouched(observed) && observed == time && exactRange && reads == 1 && writes == 0,
              "actual native read preserves the supplied game-time DWORD including sign bit and wrap values");
    }
    const auto beforeSecond = Copy(page);
    Expect(pad1);
    Check(second.SetLastTimeTouched(0x76543210) && OnlyFieldChanged(page, beforeSecond, Prefix + Profile::PadStride, 0x76543210),
          "the second selected native pad uses its own stride without touching pad zero");

    auto before = Copy(page);
    Expect(pad0, Policy::RefuseWrite);
    Refuses([&] { sdk.SetLastTimeTouched(42); }, "denied timestamp writes cannot become successful void SDK calls");
    Check(exactRange && writes == 1 && reads == 0 && Copy(page) == before, "write-policy refusal preserves both complete pad spans");
    std::uint32_t output = 0xcafef00d;
    Expect(pad0, Policy::RefuseRead);
    Check(!first.ReadLastTimeTouched(output) && output == 0xcafef00d, "a denied native read preserves caller output");
    before = Copy(page);
    Expect(pad0, Policy::RefuseRead);
    Refuses([&] { sdk.SetLastTimeTouched(0x12344321); }, "readback-policy refusal remains an explicit SDK failure after the native write");
    Check(writes == 1 && reads == 1 && OnlyFieldChanged(page, before, Prefix, 0x12344321),
          "failed readback does not pretend the completed field write was rolled back");
    before = Copy(page);
    Expect(pad0, Policy::ChangeBeforeRead);
    Refuses([&] { sdk.SetLastTimeTouched(0x11111111); }, "mismatching native readback cannot fabricate SDK success");
    Check(writes == 1 && reads == 1 && OnlyFieldChanged(page, before, Prefix, 0xdecafbad),
          "a later native timestamp edit is retained rather than overwritten by a hidden rollback");
    for (const auto address : {std::uintptr_t{0}, (std::numeric_limits<std::uintptr_t>::max)() - Profile::PadStride + 1})
    {
        NativePadActivityAccess invalid(address, PageCheck);
        TestPad                 invalidPad(states, updates, invalid);
        Expect(pad0);
        Refuses([&] { invalidPad.SetLastTimeTouched(1); }, "null or overflowing borrowed native pad address refuses at the SDK boundary");
        output = 0x23456789;
        Check(!invalid.ReadLastTimeTouched(output) && output == 0x23456789 && reads == 0 && writes == 0,
              "malformed native span refuses before address arithmetic or the supplied page policy");
    }
    NativePadActivityAccess missingPolicy(pad0, nullptr);
    TestPad                 missingPolicyPad(states, updates, missingPolicy);
    before = Copy(page);
    Refuses([&] { missingPolicyPad.SetLastTimeTouched(1); }, "missing native access policy cannot authorize a timestamp write");
    Check(Copy(page) == before, "missing policy leaves every owned native field unchanged");
    DWORD previous = 0;
    Check(VirtualProtect(page.data, 4096, PAGE_READONLY, &previous) != FALSE, "make only the fixture-owned native pad storage read-only");
    Expect(pad0, Policy::InspectProtection);
    Refuses([&] { sdk.SetLastTimeTouched(1); }, "a real read-only page refuses timestamp writes through the supplied page policy");
    output = 0;
    Check(first.ReadLastTimeTouched(output), "read-only owned native storage remains readable");
    DWORD ignored = 0;
    Check(VirtualProtect(page.data, 4096, previous, &ignored) != FALSE && Copy(page) == before,
          "restore the read-only page without any timestamp or canary mutation");
    before = Copy(page);
    Check(VirtualProtect(page.data, 4096, PAGE_NOACCESS, &previous) != FALSE, "deny OS access only to the fixture-owned native storage");
    Expect(pad0);
    output = 0xabcdef01;
    Check(!first.ReadLastTimeTouched(output) && output == 0xabcdef01, "OS read refusal preserves output despite permissive earlier policy");
    Refuses([&] { sdk.SetLastTimeTouched(1); }, "OS write refusal propagates through the actual SDK method");
    Check(VirtualProtect(page.data, 4096, previous, &ignored) != FALSE, "restore the fixture-owned page protection");
    Check(Copy(page) == before, "failed OS access changed no timestamp or surrounding field");
    Expect(pad0);
    sdk.SetLastTimeTouched(0x13572468);
    Check(first.ReadLastTimeTouched(output) && output == 0x13572468, "the same SDK slice can recover after a refused field operation");
    Check(stateCalls == 0 && updates.calls == 0, "activity access never borrows controller-state or global producer control authority");
    std::printf("Passed %u actual CPadActivityDE SDK timestamp checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
