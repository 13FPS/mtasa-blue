/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared immediate native model context for real request/ref operations
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include "NativeUObjectMembershipAccess.h"
namespace mta::game_de::NativeStreamingModelContext
{
    struct Value
    {
        std::uintptr_t                          root = 0, provider = 0, rows = 0, modelVtable = 0, typeMethod = 0, animationMethod = 0;
        std::int32_t                            count = 0;
        NativeUObjectMembershipAccess::Snapshot rootMembership{}, providerMembership{};
    };

    // Internal stack observations under an already admitted current native
    // callback. Capture/recheck never creates an owner, SDK service, weak
    // token, loaded state or persistent root/provider/model lifetime lease.
    bool QualifyProfile(std::uintptr_t image) noexcept;
    // This model-independent observation qualifies only the current provider
    // header/membership and row range. Metadata/vtable fields remain zero;
    // no selected model or future ped row is created, read or admitted here.
    ClockResult CaptureProvider(std::uintptr_t image, Value&) noexcept;
    ClockResult Capture(std::uintptr_t image, const NativeModelBankReader::Value&, Value&) noexcept;
    bool        Same(const Value&, const Value&) noexcept;
    ClockResult Recheck(std::uintptr_t image, const NativeModelBankReader::Value&, const Value&) noexcept;
    bool        MembershipUnchanged(std::uintptr_t image, const Value&) noexcept;
}
