/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared read-only admission for immediate native streaming removal
 ****************************************************************************/
#pragma once
#include "NativeStreamingRemoveMemory.h"
namespace mta::game_de::NativeStreamingRemoveAdmission
{
    namespace R = NativeStreamingRemoveMemory;
    struct Value
    {
        NativeModelLoadedReader::Value state{};
        NativeModelBankReader::Value   model{};
        R::Value                       native{};
        R::StreamRow                   idle{};
    };
    // An immediate observation, never a removal lease. The reference release
    // transaction must qualify all removal work before accepting its zero,
    // then qualify the same state again at the actual native entry boundary.
    inline ClockResult Capture(std::uintptr_t image, std::uint32_t modelIndex, NativeModelLoadedReader::CheckOwner owner, Value& output,
                               NativeModelLoadedReader::Value* observed = nullptr)
    {
        Value                   value;
        NativeModelLoadedReader loaded({image}, owner);
        auto                    result = loaded.Read(modelIndex, value.state);
        if (result != ClockResult::Ok)
            return result;
        if (observed)
            *observed = value.state;
        if (value.state.rawLoadState != 0 && value.state.rawLoadState != 1)
            return ClockResult::UnsupportedBuild;
        if (!R::QualifyProfile(image))
            return ClockResult::UnsupportedBuild;
        if (value.state.rawLoadState == 0)
        {
            if (!NativeImageAccess::ReadImage(image, value.state.rowAddress, value.idle.data(), value.idle.size(), true))
                return ClockResult::ReadFailure;
        }
        else
        {
            if (modelIndex >= NativeModelBankProfile::ModelCount)
                return ClockResult::UnsupportedBuild;
            NativeModelBankReader bank({image}, owner);
            R::Value              confirmed;
            if ((result = bank.Read(modelIndex, value.model)) != ClockResult::Ok)
                return result;
            if (!value.model.nativeModelInfo)
                return ClockResult::NotInitialized;
            if ((result = R::Capture(image, value.model, value.native)) != ClockResult::Ok || (result = bank.Recheck(value.model)) != ClockResult::Ok ||
                (result = R::Capture(image, value.model, confirmed)) != ClockResult::Ok)
                return result;
            if (!R::Same(value.native, confirmed) || !R::Memberships(image, value.native))
                return ClockResult::BindingChanged;
        }
        if ((result = loaded.Recheck(value.state)) != ClockResult::Ok)
            return result;
        if (!R::QualifyProfile(image))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    inline ClockResult Recheck(std::uintptr_t image, NativeModelLoadedReader::CheckOwner owner, const Value& before)
    {
        Value after;
        auto  result = Capture(image, before.state.modelIndex, owner, after);
        if (result != ClockResult::Ok)
            return result;
        if (before.state.modelIndex != after.state.modelIndex || before.state.rowAddress != after.state.rowAddress ||
            before.state.rawFlags != after.state.rawFlags || before.state.rawLoadState != after.state.rawLoadState ||
            before.model.nativeModelInfo != after.model.nativeModelInfo || before.idle != after.idle ||
            (before.state.rawLoadState == 1 && !R::Same(before.native, after.native)))
            return ClockResult::BindingChanged;
        return ClockResult::Ok;
    }
}
