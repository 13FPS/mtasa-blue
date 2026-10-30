/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine native vehicle predicates under the current model callback owner
 ****************************************************************************/
#pragma once
#include "NativeModelClassificationProfile.h"
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    struct NativeModelTypeObservation
    {
        bool          hasModel = false;
        unsigned char nativeType = 0;
    };
}
namespace mta::game_de::NativeModelClassificationCalls
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using NativeFunction = unsigned char (*)(std::int32_t model);
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t model, unsigned char& result, std::uint32_t& exception) noexcept;
    struct Receipt
    {
        std::uintptr_t              entryRva = 0, root = 0, modelInfo = 0;
        std::uint32_t               modelIndex = 0, exceptionCode = 0;
        NativeModelVehiclePredicate predicate = NativeModelVehiclePredicate::Boat;
        unsigned char               nativeResult = 0;
        // A named producer policy performs no native classifier invocation.
        bool entered = false, returned = false, policyPort = false;
    };
    // Observe only a complete pure constant-return built-in getter body.
    // The shared leaf reader invokes nothing; all bank/context/code facts are
    // bookended inside this same callback. Refusal preserves the old byte.
    ClockResult ReadType(std::uintptr_t image, std::uint32_t model, CheckOwner, NativeModelTypeObservation& output) noexcept;

    // Existing four complete classifier bodies establish AL6 followed by a
    // DWORD receiver+84 observation. Preserve that raw DWORD independently of
    // SDK enum meanings, with no native invocation or loaded-state query.
    ClockResult ReadVehicleSubtype(std::uintptr_t image, std::uint32_t model, CheckOwner, std::uint32_t& output) noexcept;

    // A bank association is borrowed only for this synchronous callback. The
    // genuine classifier owns its native virtual dispatch; no arbitrary getter
    // address or model/RwObject pointer is returned. All failures preserve output.
    ClockResult Read(std::uintptr_t image, std::uint32_t model, NativeModelVehiclePredicate, CheckOwner, bool& output, Receipt* = nullptr,
                     Invoke = nullptr) noexcept;
}
