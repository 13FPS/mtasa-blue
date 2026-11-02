/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Emit owned x64 call bridges without installing native hooks
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativePlayerMoveAnimHookBridge
{
    enum class Kind : std::uint32_t
    {
        ProcessCompare,
        ClothesCall
    };
    using Decision = std::uint32_t (*)(std::uintptr_t nativePed, std::uint32_t requested, void* context) noexcept;
    struct Request
    {
        Kind           kind = Kind::ProcessCompare;
        Decision       decision = nullptr;
        void*          context = nullptr;
        volatile LONG* inFlight = nullptr;
        std::uintptr_t originalConsumer = 0;
    };

    // The extra PUSHFQ precedes SUB, preserving incoming flags. RBP remains
    // the established frame anchor through flags/SIMD/register restoration
    // and the replayed CMP. A standard LEA/POP/RET-or-JMP epilogue skips the
    // saved flags word, making faults at the final CMP unwindable as well.
    inline constexpr std::uint32_t FrameAllocation = 0x2a8, FrameRegisterOffset = 0xf0, ShadowBytes = 0x20;
    inline constexpr std::uint32_t FxOffset = 0xa0, FxBytes = 512, SavedFlagsOffset = FrameAllocation;
    inline constexpr std::uint32_t SavedRbpOffset = FrameAllocation + 8, ReturnAddressOffset = FrameAllocation + 16;
    inline constexpr std::uint32_t EpilogueFrameDisplacement = SavedRbpOffset - FrameRegisterOffset;
    inline constexpr std::uint32_t MaximumBlobBytes = 640;
    struct GprSlot
    {
        std::uint32_t reg, offset;
    };
    // AMD64 register numbers; RSP is restored by the frame and RBP by POP.
    inline constexpr std::array<GprSlot, 14> GprSlots{{{0, 0x20},
                                                       {1, 0x28},
                                                       {2, 0x30},
                                                       {3, 0x38},
                                                       {6, 0x40},
                                                       {7, 0x48},
                                                       {8, 0x50},
                                                       {9, 0x58},
                                                       {10, 0x60},
                                                       {11, 0x68},
                                                       {12, 0x70},
                                                       {13, 0x78},
                                                       {14, 0x80},
                                                       {15, 0x88}}};
    struct Layout
    {
        std::uint32_t                              pushRbpEnd = 0, pushFlagsEnd = 0, allocationEnd = 0, frameSetEnd = 0;
        std::array<std::uint32_t, GprSlots.size()> saveEnds{}, restoreEnds{};
        std::uint32_t                              prologueEnd = 0, fxSave = 0, enterCounter = 0, callbackCall = 0, decisionStoreEnd = 0;
        std::uint32_t                              leaveCounter = 0, fxRestore = 0, flagsPush = 0, flagsPop = 0, compare = 0;
        std::uint32_t                              epilogue = 0, popRbp = 0, tail = 0, consumerLiteral = 0;
    };
    struct Program
    {
        std::array<unsigned char, MaximumBlobBytes> bytes{};
        std::uint32_t                               size = 0, codeSize = 0, unwindOffset = 0, unwindSize = 0;
        RUNTIME_FUNCTION                            function{};
        Layout                                      layout;
    };
    struct CallPatch
    {
        std::array<unsigned char, 6> bytes{};
        std::uint32_t                size = 0;
    };

    // Pure bounded emission, preserving output on refusal. Copy the whole
    // blob to owned storage before registering function with that storage's
    // base, then make code RX. This API never allocates, registers unwind,
    // changes protection, edits an image, publishes an override or installs.
    // inFlight alone never permits retirement: it is zero before enter and
    // after leave while this stub still executes. A future remover must park
    // peers, exclude every IP in the whole stub/site/consumer transfer, and
    // retain code, unwind and owner state whenever quiescence is uncertain.
    bool Emit(const Request&, Program&) noexcept;
    bool EncodeCallPatch(std::uintptr_t site, std::uintptr_t stub, Kind, CallPatch&) noexcept;
}
