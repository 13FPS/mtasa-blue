/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute owned animation bridges and verify native live state
 ****************************************************************************/
#include <windows.h>
#include <intrin.h>
#include "../../Client/game_de/NativePlayerMoveAnimHookBridge.h"
#include <game/CPlayerMoveAnimPolicy.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace Bridge = mta::game_de::NativePlayerMoveAnimHookBridge;
using Policy = MtaGame::CPlayerMoveAnimPolicy;

struct alignas(16) Snapshot
{
    std::array<std::uint64_t, 15> gpr{};
    std::uint64_t                 flags = 0;
    alignas(16) std::array<unsigned char, 512> fx{};
};
static_assert(offsetof(Snapshot, flags) == 120 && offsetof(Snapshot, fx) == 128 && sizeof(Snapshot) == 640);
extern "C"
{
    alignas(16) Snapshot g_MoveAnimBridgeSeed, g_MoveAnimBridgeAfter, g_MoveAnimBridgeConsumer;
    void*          g_MoveAnimBridgeTarget = nullptr;
    std::uintptr_t g_MoveAnimBridgeBeforeRsp = 0, g_MoveAnimBridgeAfterRsp = 0, g_MoveAnimBridgeConsumerRsp = 0;
    std::uint32_t  g_MoveAnimBridgeConsumerCalls = 0, g_MoveAnimBridgeClobberMxcsr = 0x1f81;
    void           ExecuteMoveAnimHookFixture();
    void           RecordMoveAnimConsumerEntry();
    void           ClobberMoveAnimHookVolatiles();
}

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }

    struct DecisionContext
    {
        Bridge::Kind   kind;
        int            custom;
        std::uintptr_t ped;
        std::uint32_t  requested;
        volatile LONG* inFlight;
        unsigned       calls = 0;
        bool           valid = true;
    };
    std::uint32_t Decide(std::uintptr_t ped, std::uint32_t requested, void* opaque) noexcept
    {
        auto& state = *static_cast<DecisionContext*>(opaque);
        ++state.calls;
        state.valid = state.valid && ped == state.ped && requested == state.requested && InterlockedCompareExchange(state.inFlight, 0, 0) == 1;
        // Keep the actual shared MTA policy in the executable fixture. The
        // assembly helper then exercises its permitted volatile destruction.
        volatile int chosen = state.kind == Bridge::Kind::ProcessCompare ? Policy::ChooseProcessAnimGroup(static_cast<std::int32_t>(requested), state.custom)
                                                                         : Policy::ChooseDefaultMotionGroup(static_cast<std::int32_t>(requested), state.custom);
        ClobberMoveAnimHookVolatiles();
        return static_cast<std::uint32_t>(chosen);
    }

    void Seed(std::uintptr_t ped, std::uint32_t requested)
    {
        g_MoveAnimBridgeSeed = {};
        _fxsave64(g_MoveAnimBridgeSeed.fx.data());
        const std::uint32_t mxcsr = 0x1f80;
        std::memcpy(g_MoveAnimBridgeSeed.fx.data() + 24, &mxcsr, sizeof(mxcsr));
        for (std::size_t i = 0; i != 16 * 16; ++i)
            g_MoveAnimBridgeSeed.fx[160 + i] = static_cast<unsigned char>(0x21 + i * 13);
        for (std::size_t i = 0; i != g_MoveAnimBridgeSeed.gpr.size(); ++i)
            g_MoveAnimBridgeSeed.gpr[i] = 0x1234567800000100ull + i * 0x101;
        g_MoveAnimBridgeSeed.gpr[0] = requested;
        g_MoveAnimBridgeSeed.gpr[1] = requested;
        g_MoveAnimBridgeSeed.gpr[3] = ped;
        g_MoveAnimBridgeSeed.gpr[6] = ped;
        g_MoveAnimBridgeSeed.flags = 0x293;  // IF plus differing arithmetic flags; DF/TF stay clear.
        g_MoveAnimBridgeAfter = {};
        g_MoveAnimBridgeConsumer = {};
        g_MoveAnimBridgeConsumerCalls = 0;
    }
    void CheckFx(const Snapshot& actual, const Snapshot& expected)
    {
        Check(std::memcmp(actual.fx.data() + 160, expected.fx.data() + 160, 16 * 16) == 0, "all sixteen XMM registers survive the bridge callback");
        Check(std::memcmp(actual.fx.data() + 24, expected.fx.data() + 24, sizeof(std::uint32_t)) == 0,
              "full incoming MXCSR including status survives the callback");
        Check(std::memcmp(actual.fx.data(), expected.fx.data(), 5) == 0, "x87 control, status and abridged tag retain their incoming values");
    }

    class OwnedCode
    {
    public:
        explicit OwnedCode(const Bridge::Request& request)
        {
            Check(Bridge::Emit(request, program), "valid bridge emission");
            memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(memory != nullptr, "owned executable fixture allocation");
            std::memset(memory, 0xcc, 8192);
            // Match the native mid-function site's aligned RSP and its caller
            // shadow space rather than calling the bridge from a leaf entry.
            static constexpr unsigned char frame[] = {0x48, 0x83, 0xec, 0x28};
            std::memcpy(memory + SiteOffset - sizeof(frame), frame, sizeof(frame));
            std::memcpy(memory + StubOffset, program.bytes.data(), program.size);
            Check(RtlAddFunctionTable(&program.function, 1, Stub()) != FALSE, "register actual generated unwind metadata");
            registered = true;
        }
        ~OwnedCode()
        {
            if (registered)
                RtlDeleteFunctionTable(&program.function);
            if (memory)
                VirtualFree(memory, 0, MEM_RELEASE);
        }
        std::uintptr_t Stub() const { return reinterpret_cast<std::uintptr_t>(memory + StubOffset); }
        void           Protect(DWORD desired)
        {
            DWORD previous = 0;
            Check(VirtualProtect(memory, 4096, desired, &previous) != FALSE, "owned fixture page protection transition");
            Check(FlushInstructionCache(GetCurrentProcess(), memory, 4096) != FALSE, "owned fixture instruction cache publication");
        }
        void Patch(Bridge::Kind kind)
        {
            Bridge::CallPatch patch;
            Check(Bridge::EncodeCallPatch(reinterpret_cast<std::uintptr_t>(memory + SiteOffset), Stub(), kind, patch), "encode near call for the owned site");
            std::memcpy(memory + SiteOffset, patch.bytes.data(), patch.size);
        }
        void PrepareCompare()
        {
            static constexpr unsigned char site[] = {0x39, 0x83, 0xe0, 0x06, 0, 0,  // Original CMP [RBX+6E0],EAX.
                                                     0x74, 0x0d,                    // Exercise the original equality branch.
                                                     0xc6, 0x05, 0,    0,    0, 0, 1, 0x48, 0x8d, 0x64, 0x24, 0x28, 0xc3,
                                                     0xc6, 0x05, 0,    0,    0, 0, 2, 0x48, 0x8d, 0x64, 0x24, 0x28, 0xc3};
            std::memcpy(memory + SiteOffset, site, sizeof(site));
            const std::int32_t first = BranchResultOffset - (SiteOffset + 15);
            const std::int32_t second = BranchResultOffset - (SiteOffset + 28);
            std::memcpy(memory + SiteOffset + 10, &first, sizeof(first));
            std::memcpy(memory + SiteOffset + 23, &second, sizeof(second));
        }
        void Run()
        {
            g_MoveAnimBridgeTarget = memory + SiteOffset - 4;
            ExecuteMoveAnimHookFixture();
            Check(g_MoveAnimBridgeBeforeRsp == g_MoveAnimBridgeAfterRsp && (g_MoveAnimBridgeBeforeRsp & 15) == 0,
                  "native fixture stack and Win64 call alignment are restored");
        }
        unsigned char*  memory = nullptr;
        Bridge::Program program;
        // The result byte is on a separate RW data page; executable code stays RX.
        static constexpr std::uint32_t SiteOffset = 0x20, BranchResultOffset = 0x1000, StubOffset = 0x200;

    private:
        bool registered = false;
    };
}

#include "PlayerMoveAnimHookBridgeUnwindTests.inl"

namespace
{
    void EncodingCases()
    {
        for (const auto kind : {Bridge::Kind::ProcessCompare, Bridge::Kind::ClothesCall})
        {
            const std::uintptr_t site = 0x200000000ull;
            for (const std::int64_t displacement :
                 {std::int64_t{-2147483648ll}, std::int64_t{-1}, std::int64_t{0}, std::int64_t{1}, std::int64_t{2147483647ll}})
            {
                const auto        stub = static_cast<std::uintptr_t>(static_cast<std::int64_t>(site + 5) + displacement);
                Bridge::CallPatch patch;
                Check(Bridge::EncodeCallPatch(site, stub, kind, patch), "rel32 includes both signed limits");
                std::int32_t actual = 0;
                std::memcpy(&actual, patch.bytes.data() + 1, sizeof(actual));
                Check(patch.bytes[0] == 0xe8 && actual == displacement && patch.size == (kind == Bridge::Kind::ProcessCompare ? 6u : 5u),
                      "patch preserves the exact call form and continuation length");
                if (kind == Bridge::Kind::ProcessCompare)
                    Check(patch.bytes[5] == 0x90, "the six-byte comparison keeps a flags-neutral NOP after CALL");
            }
            for (const auto stub : {site + 5 + 0x80000000ull, site + 5 - 0x80000001ull})
            {
                Bridge::CallPatch patch;
                patch.bytes.fill(0x5a);
                patch.size = 99;
                const auto before = patch;
                Check(!Bridge::EncodeCallPatch(site, stub, kind, patch) && patch.bytes == before.bytes && patch.size == before.size,
                      "out-of-range patch refuses without publishing partial bytes");
            }
        }
        Bridge::CallPatch patch;
        patch.bytes.fill(0x5a);
        patch.size = 99;
        Check(!Bridge::EncodeCallPatch((std::numeric_limits<std::uintptr_t>::max)() - 3, 0x1000, Bridge::Kind::ProcessCompare, patch) && patch.size == 99 &&
                  patch.bytes[0] == 0x5a,
              "origin arithmetic overflow refuses without changing output");
        volatile LONG         counter = 0;
        DecisionContext       context{Bridge::Kind::ProcessCompare, 0, 0, 0, &counter};
        const Bridge::Request valid{Bridge::Kind::ProcessCompare, &Decide, &context, &counter, 0};
        for (unsigned failure = 0; failure != 4; ++failure)
        {
            auto request = valid;
            if (failure == 0)
                request.decision = nullptr;
            else if (failure == 1)
                request.inFlight = nullptr;
            else if (failure == 2)
                request.kind = static_cast<Bridge::Kind>(999);
            else
                request.kind = Bridge::Kind::ClothesCall;
            Bridge::Program program;
            program.bytes.fill(0xa7);
            program.size = 123;
            const auto before = program;
            Check(!Bridge::Emit(request, program) && program.bytes == before.bytes && program.size == before.size,
                  "missing callback/counter/consumer or invalid bridge kind refuses before any output");
        }
    }
    void CompareCases()
    {
        bool                                         checkedUnwind = false;
        const std::array<std::pair<int, int>, 7>     requests{{{54, 0}, {54, 55}, {60, 55}, {70, 56}, {69, 138}, {54, 62}, {68, 56}}};
        alignas(16) std::array<unsigned char, 0x720> ped{};
        for (const auto& pair : requests)
        {
            const auto requested = static_cast<std::uint32_t>(pair.first);
            const auto chosen = static_cast<std::uint32_t>(Policy::ChooseProcessAnimGroup(pair.first, pair.second));
            for (const int difference : {0, -1, 1})
            {
                volatile LONG   inFlight = 0;
                DecisionContext state{Bridge::Kind::ProcessCompare, pair.second, reinterpret_cast<std::uintptr_t>(ped.data()), requested, &inFlight};
                Bridge::Request request{state.kind, &Decide, &state, &inFlight, 0};
                OwnedCode       code(request);
                const auto      current = chosen + difference;
                std::memcpy(ped.data() + 0x6e0, &current, sizeof(current));
                code.PrepareCompare();
                code.Protect(PAGE_EXECUTE_READ);
                // Only EAX is the filtered output. Seed every other native
                // register from the original request in both executions.
                Seed(state.ped, requested);
                g_MoveAnimBridgeSeed.gpr[0] = chosen;
                code.Run();
                const auto expected = g_MoveAnimBridgeAfter;
                const auto expectedBranch = code.memory[OwnedCode::BranchResultOffset];
                Check(state.calls == 0, "native comparison baseline never invokes the callback");
                code.Protect(PAGE_READWRITE);
                code.Patch(state.kind);
                code.Protect(PAGE_EXECUTE_READ);
                Seed(state.ped, requested);
                code.Run();
                Check(state.calls == 1 && state.valid && inFlight == 0, "callback receives exact ped/request with balanced in-flight ownership");
                if (g_MoveAnimBridgeAfter.gpr != expected.gpr)
                    for (std::size_t reg = 0; reg != expected.gpr.size(); ++reg)
                        if (g_MoveAnimBridgeAfter.gpr[reg] != expected.gpr[reg])
                            std::fprintf(stderr, "GPR[%zu] expected=%llx actual=%llx request=%u custom=%d\n", reg,
                                         static_cast<unsigned long long>(expected.gpr[reg]), static_cast<unsigned long long>(g_MoveAnimBridgeAfter.gpr[reg]),
                                         requested, pair.second);
                Check(g_MoveAnimBridgeAfter.gpr == expected.gpr, "all fifteen GPRs match the native comparison except its chosen EAX");
                Check((g_MoveAnimBridgeAfter.flags & 0xed5) == (expected.flags & 0xed5), "native CMP arithmetic/DF/IF flags reach the original branch");
                Check(code.memory[OwnedCode::BranchResultOffset] == expectedBranch && expectedBranch == (difference == 0 ? 2 : 1),
                      "original equality branch observes the filtered group");
                CheckFx(g_MoveAnimBridgeAfter, expected);
                if (!checkedUnwind)
                {
                    CheckBridgeUnwind(code.program, code.Stub());
                    checkedUnwind = true;
                }
            }
        }
    }
    void ClothesCases()
    {
        bool                                         checkedUnwind = false;
        alignas(16) std::array<unsigned char, 0x720> ped{};
        for (const int requested : {54, 55, 56})
        {
            for (const int custom : {0, 61, 68, 138})
            {
                volatile LONG   inFlight = 0;
                DecisionContext state{Bridge::Kind::ClothesCall, custom, reinterpret_cast<std::uintptr_t>(ped.data()), static_cast<std::uint32_t>(requested),
                                      &inFlight};
                Bridge::Request request{state.kind, &Decide, &state, &inFlight, reinterpret_cast<std::uintptr_t>(&RecordMoveAnimConsumerEntry)};
                OwnedCode       code(request);
                static constexpr unsigned char leaveFrame[] = {0x48, 0x8d, 0x64, 0x24, 0x28, 0xc3};
                std::memcpy(code.memory + OwnedCode::SiteOffset + 5, leaveFrame, sizeof(leaveFrame));
                code.Patch(state.kind);
                code.Protect(PAGE_EXECUTE_READ);
                Seed(state.ped, state.requested);
                const auto expectedSeed = g_MoveAnimBridgeSeed;
                code.Run();
                auto expected = expectedSeed.gpr;
                expected[1] = static_cast<std::uint32_t>(Policy::ChooseDefaultMotionGroup(requested, custom));
                Check(g_MoveAnimBridgeConsumerCalls == 1 && state.calls == 1 && state.valid && inFlight == 0,
                      "clothes bridge filters once and tail-transfers to the original consumer once");
                Check(g_MoveAnimBridgeConsumer.gpr == expected, "consumer sees its filtered ECX and every other incoming register");
                Check(g_MoveAnimBridgeConsumerRsp + 0x38 == g_MoveAnimBridgeBeforeRsp,
                      "tail transfer preserves the original consumer return address and shadow space");
                Check(g_MoveAnimBridgeAfter.gpr[0] == 0x12345678, "the original consumer return remains authoritative");
                CheckFx(g_MoveAnimBridgeConsumer, expectedSeed);
                if (!checkedUnwind)
                {
                    CheckBridgeUnwind(code.program, code.Stub());
                    checkedUnwind = true;
                }
            }
        }
    }
}

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    EncodingCases();
    CompareCases();
    ClothesCases();
    std::printf("Passed %u owned MTA animation hook bridge checks (64-bit)\n", checks);
    return EXIT_SUCCESS;
}
