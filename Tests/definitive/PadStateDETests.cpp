/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual CPad SDK state operations in owned process memory
 *****************************************************************************/
#include "../../Client/game_de/CPadStateDE.h"
#include "../../Client/game_de/NativePadStateAccess.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
    unsigned       checks = 0;
    std::uintptr_t expectedAddress = 0;
    std::size_t    expectedSize = 0;
    bool           expectedWrite = false, allowAccess = true, policyMatched = true;
    unsigned       policyCalls = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    void Expect(std::uintptr_t address, std::size_t size, bool writing, bool allow = true)
    {
        expectedAddress = address;
        expectedSize = size;
        expectedWrite = writing;
        allowAccess = allow;
        policyMatched = true;
        policyCalls = 0;
    }

    bool CheckPage(std::uintptr_t address, std::size_t size, bool writing, bool executable) noexcept
    {
        ++policyCalls;
        policyMatched = policyMatched && address == expectedAddress && size == expectedSize && writing == expectedWrite && !executable;
        return allowAccess && policyMatched;
    }

    void Policy(const char* description)
    {
        Check(policyMatched && policyCalls == 1, description);
    }

    template <class Exception, class Operation>
    void Refuses(Operation operation, const char* description)
    {
        try
        {
            operation();
        }
        catch (const Exception&)
        {
            Check(true, description);
            return;
        }
        catch (...)
        {
        }
        Check(false, description);
    }

    bool Same(const CControllerState& first, const CControllerState& second)
    {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    }

    // The production slice remains abstract. These explicit test-only methods
    // fail if touched; they do not claim to implement unqualified DE behavior.
    class TestPad final : public mta::game_de::CPadStateDE
    {
    public:
        explicit TestPad(mta::game_de::NativePadStateAccess& access) noexcept : CPadStateDE(access) {}
        void          Restore() override { Unqualified(); }
        void          Store() override { Unqualified(); }
        bool          IsEnabled() override { Unqualified(); }
        void          Disable(bool) override { Unqualified(); }
        void          SetHornHistoryValue(bool) override { Unqualified(); }
        void          SetLastTimeTouched(DWORD) override { Unqualified(); }
        short         GetShakeDur() override { Unqualified(); }
        unsigned char GetShakeFreq() override { Unqualified(); }
        void          SetShakeDur(short) override { Unqualified(); }

    private:
        [[noreturn]] static void Unqualified() { throw std::logic_error("Unqualified SDK method reached test fixture"); }
    };

    struct Page
    {
        unsigned char* value = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        ~Page()
        {
            if (value)
                VirtualFree(value, 0, MEM_RELEASE);
        }
    };
}

int main()
{
    using mta::game_de::NativePadStateAccess;
    static_assert(sizeof(CControllerState) == 48 && NativePadStateAccess::Bytes{}.size() == 96);
    static_assert(std::is_abstract_v<mta::game_de::CPadStateDE> && std::is_base_of_v<CPad, mta::game_de::CPadStateDE>);
    Page page;
    Check(page.value != nullptr, "allocate a fixture-owned page without opening a game");
    std::memset(page.value, 0xa5, 4096);
    const auto           address = reinterpret_cast<std::uintptr_t>(page.value + 64);
    NativePadStateAccess access(address, CheckPage);
    TestPad              pad(access);
    CControllerState     current, previous;
    current.LeftStickX = -81;
    current.LeftStickY = -128;
    current.ButtonCross = 128;
    current.m_bPedWalk = 128;
    previous.LeftStickY = 43;
    previous.ButtonCircle = 128;
    std::memcpy(page.value + 64, &current, sizeof(current));
    std::memcpy(page.value + 112, &previous, sizeof(previous));
    const auto nativeCurrent = [&]
    {
        CControllerState value;
        std::memcpy(&value, page.value + 64, sizeof(value));
        return value;
    };
    const auto nativePrevious = [&]
    {
        CControllerState value;
        std::memcpy(&value, page.value + 112, sizeof(value));
        return value;
    };
    const auto guards = [&]
    {
        for (std::size_t index = 0; index < 4096; ++index)
            if ((index < 64 || index >= 160) && page.value[index] != 0xa5)
                return false;
        return true;
    };
    CControllerState output;
    Expect(address, 48, false);
    Check(pad.GetCurrentControllerState(&output) == &output && Same(output, current), "current getter preserves actual SDK pointer and signed field bits");
    Policy("current getter requests only its verified48-byte block");
    Expect(address + 48, 48, false);
    Check(pad.GetLastControllerState(&output) == &output && Same(output, previous), "previous getter reads independent native history");
    Policy("previous getter requests only its own48-byte block");
    CControllerState replacement;
    replacement.ButtonCross = 128;
    replacement.ButtonCircle = 17;
    Expect(address, 48, true);
    pad.SetCurrentControllerState(&replacement);
    Check(Same(nativeCurrent(), replacement) && Same(nativePrevious(), previous), "current setter cannot shift or replay the previous physical state");
    Policy("current setter writes exactly one block without reading or writing the other");
    Expect(address + 48, 48, true);
    pad.SetLastControllerState(&replacement);
    Check(Same(nativeCurrent(), replacement) && Same(nativePrevious(), replacement), "last-state setter applies MTA's separately supplied pre-frame history");
    Policy("last-state setter writes only its independently supplied block");
    CControllerState released;
    Expect(address, 48, true);
    pad.SetCurrentControllerState(&released);
    Check(nativeCurrent().ButtonCross == 0 && nativePrevious().ButtonCross == 128, "independent setters preserve a real pressed-to-released edge");
    Expect(address + 48, 48, true);
    pad.SetLastControllerState(&released);
    Expect(address, 48, true);
    pad.SetCurrentControllerState(&replacement);
    Check(nativeCurrent().ButtonCross == 128 && nativePrevious().ButtonCross == 0, "independent setters preserve the next unpressed-to-pressed edge");
    Check(guards(), "all state setters leave the enclosing pad fields and neighboring memory untouched");

    Expect(address, 48, false);
    Refuses<std::invalid_argument>([&] { pad.GetCurrentControllerState(nullptr); }, "null current output refuses before native access");
    Refuses<std::invalid_argument>([&] { pad.GetLastControllerState(nullptr); }, "null last output refuses before native access");
    Refuses<std::invalid_argument>([&] { pad.SetCurrentControllerState(nullptr); }, "null current input refuses before native access");
    Refuses<std::invalid_argument>([&] { pad.SetLastControllerState(nullptr); }, "null last input refuses before native access");
    Check(policyCalls == 0, "malformed SDK calls never request memory authority");
    const auto sentinel = output;
    for (const auto invalidAddress : std::array<std::uintptr_t, 2>{0, (std::numeric_limits<std::uintptr_t>::max)() - 32})
    {
        NativePadStateAccess invalidAccess(invalidAddress, CheckPage);
        TestPad              invalidPad(invalidAccess);
        Expect(invalidAddress, 48, false);
        Refuses<std::runtime_error>([&] { invalidPad.GetCurrentControllerState(&output); }, "malformed borrowed current address refuses before page policy");
        Refuses<std::runtime_error>([&] { invalidPad.GetLastControllerState(&output); },
                                    "malformed borrowed previous address refuses before offset arithmetic");
        Check(Same(output, sentinel), "null or wrapping borrowed addresses preserve all getter output fields");
        Refuses<std::runtime_error>([&] { invalidPad.SetCurrentControllerState(&released); }, "malformed borrowed current write cannot reach page policy");
        Refuses<std::runtime_error>([&] { invalidPad.SetLastControllerState(&replacement); }, "malformed borrowed previous write cannot reach page policy");
        Refuses<std::runtime_error>([&] { invalidPad.Clear(); }, "malformed borrowed clear cannot address memory outside its complete state pair");
        Check(policyCalls == 0, "null and overflowing borrowed addresses confer no native page authority");
    }
    Expect(address, 48, false, false);
    Refuses<std::runtime_error>([&] { pad.GetCurrentControllerState(&output); }, "current native read refusal is an explicit SDK failure");
    Check(Same(output, sentinel), "refused current getter preserves every caller output field");
    Policy("current read refusal still identifies only the requested verified block");
    Expect(address + 48, 48, false, false);
    Refuses<std::runtime_error>([&] { pad.GetLastControllerState(&output); }, "previous native read refusal is an explicit SDK failure");
    Check(Same(output, sentinel), "refused previous getter preserves every caller output field");
    const auto beforeCurrent = nativeCurrent(), beforePrevious = nativePrevious();
    Expect(address, 48, true, false);
    Refuses<std::runtime_error>([&] { pad.SetCurrentControllerState(&released); }, "current native write refusal propagates to the SDK caller");
    Expect(address + 48, 48, true, false);
    Refuses<std::runtime_error>([&] { pad.SetLastControllerState(&replacement); }, "previous native write refusal propagates to the SDK caller");
    Check(Same(nativeCurrent(), beforeCurrent) && Same(nativePrevious(), beforePrevious), "page-policy refusal changes neither controller block");
    DWORD oldProtection = 0;
    Check(VirtualProtect(page.value, 4096, PAGE_NOACCESS, &oldProtection) != FALSE, "deny OS reads only on the fixture-owned page");
    Expect(address, 48, false);
    Refuses<std::runtime_error>([&] { pad.GetCurrentControllerState(&output); }, "OS read failure cannot fabricate a successful current state");
    Check(Same(output, sentinel), "failed OS copy preserves all caller storage despite a permissive earlier page check");
    DWORD ignoredProtection = 0;
    Check(VirtualProtect(page.value, 4096, oldProtection, &ignoredProtection) != FALSE, "restore the fixture-owned page protection");
    Check(VirtualProtect(page.value, 4096, PAGE_READONLY, &oldProtection) != FALSE, "deny OS writes only on the fixture-owned private page");
    Expect(address, 48, true);
    Refuses<std::runtime_error>([&] { pad.SetCurrentControllerState(&released); }, "OS write protection refuses despite an earlier permissive page policy");
    Policy("OS write failure retains the exact48-byte SDK write boundary");
    Check(Same(nativeCurrent(), beforeCurrent) && Same(nativePrevious(), beforePrevious), "refused OS write preserves current and historical native state");
    Check(VirtualProtect(page.value, 4096, oldProtection, &ignoredProtection) != FALSE, "restore fixture-owned write protection after the refusal");
    Expect(address, 96, true, false);
    Refuses<std::runtime_error>([&] { pad.Clear(); }, "refused clear is visible to its SDK caller");
    Check(Same(nativeCurrent(), beforeCurrent) && Same(nativePrevious(), beforePrevious), "refused clear preserves both actual states");
    Expect(address, 96, true);
    pad.Clear();
    CControllerState neutral;
    Check(Same(nativeCurrent(), neutral) && Same(nativePrevious(), neutral), "SDK Clear uses the real shared controller zero initialization for both states");
    Policy("Clear owns exactly the adjacent96-byte state pair");
    Check(guards(), "Clear cannot mutate physical source queues, disable words, history or native rumble fields");
    std::printf("Passed %u actual CPadStateDE SDK state checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
