/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified physical operations on an existing issued private row
 ****************************************************************************/
#pragma once
#include "NativeCustomAnimSequenceStorage.h"
namespace mta::game_de::NativeAnimationSequenceRowCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
    struct Value
    {
        std::uintptr_t row = 0, keyFrames = 0;
        std::uint16_t  boneTag = 0, flags = 0, frameCount = 0;
    };
    enum class Operation : std::uint32_t
    {
        Initialize,
        SetBoneTag
    };
    using Write = ClockResult (*)(std::uintptr_t, Operation, const Value&, std::uint32_t&) noexcept;
    struct Receipt
    {
        Value         before{}, after{};
        std::uint32_t exceptionCode = 0;
        bool          entered = false, accepted = false, verified = false;
    };
    // Values are physical observations only. In particular boneTag is not
    // classic's logical union getter, and keyFrames adds no child authority.
    ClockResult Read(std::uintptr_t image, const Storage&, std::uint64_t generation, std::size_t index, CheckOwner, Value& output) noexcept;
    ClockResult Initialize(std::uintptr_t image, Storage&, std::uint64_t generation, std::size_t index, CheckOwner, Receipt* = nullptr,
                           Write = nullptr) noexcept;
    ClockResult SetBoneTag(std::uintptr_t image, Storage&, std::uint64_t generation, std::size_t index, std::int32_t boneTag, CheckOwner, Receipt* = nullptr,
                           Write = nullptr) noexcept;
    // This separately admits a nonnull interior/base frame view through its
    // actual same-generation allocation and original heap, without reading
    // any payload or claiming a frame representation/stride.
    ClockResult CheckFramePointer(std::uintptr_t image, const Storage&, std::uint64_t generation, std::uintptr_t pointer, CheckOwner) noexcept;
    // Write is a CPU-only test seam. Ok means all specified field stores
    // completed. Accepted stores survive postflight failure; the enclosing
    // owner updates its logical state and stops without rollback/replay.
}
