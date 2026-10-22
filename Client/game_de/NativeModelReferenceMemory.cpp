/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reacquire an occupied native key before a single counter mutation
 ****************************************************************************/
#include "NativeModelReferenceMemory.h"
#include "NativeModelReferenceProfile.h"
#include "NativeModelReferenceMap.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelBankProfile.h"
#include "NativeStreamingModelContext.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace mta::game_de::NativeModelReferenceMemory
{
    namespace
    {
        namespace P = NativeModelReferenceProfile;
        namespace C = NativeStreamingModelContext;
        namespace M = NativeModelReferenceMap;
        using Snapshot = M::Snapshot;
        using M::Capture;
        using M::Same;
#if !defined(MTA_DE_MODEL_REFERENCE_EXCHANGE)
        ClockResult NativeExchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(std::uint32_t));
            LONG e = 0, t = 0;
            std::memcpy(&e, &expected, sizeof(e));
            std::memcpy(&t, &target, sizeof(t));
            __try
            {
                const auto result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), t, e);
                std::memcpy(&observed, &result, sizeof(observed));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Adjust(std::uintptr_t image, std::uint32_t modelIndex, int delta, CheckOwner owner, Receipt* output, Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.modelIndex = modelIndex;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount || (delta != 1 && delta != -1))
            return finish(ClockResult::InvalidArgument);
        // Native references and MTA's interface reference concern this key
        // alone. The paired-upgrade rule belongs to streaming requests/queries.
        if (!owner)
            return finish(ClockResult::NotInitialized);
        try
        {
            NativeModelBankReader          bank({image}, owner);
            NativeModelLoadedReader        loaded({image}, owner);
            NativeModelBankReader::Value   model;
            NativeModelLoadedReader::Value state;
            auto                           result = bank.Read(modelIndex, model);
            if (result != ClockResult::Ok || (result = loaded.Read(modelIndex, state)) != ClockResult::Ok)
                return finish(result);
            if (!state.streamedIn || !model.nativeModelInfo)
                return finish(ClockResult::NotInitialized);
            if (!M::QualifyProfile(image))
                return finish(ClockResult::UnsupportedBuild);
            C::Value context;
            if ((result = C::Capture(image, model, context)) != ClockResult::Ok)
                return finish(result);
            Snapshot first, second;
            if ((result = Capture(context, model, first)) != ClockResult::Ok)
                return finish(result);
            receipt.root = context.root;
            receipt.map = first.map;
            receipt.row = first.rowsAddress + static_cast<std::uintptr_t>(first.selected) * P::RowBytes;
            receipt.before = first.rows[static_cast<std::size_t>(first.selected)].count;
            if ((delta < 0 && !receipt.before) || (delta > 0 && receipt.before == (std::numeric_limits<std::uint32_t>::max)()))
                return finish(ClockResult::InvalidArgument);
            // Reaching zero enters native type/streaming/removal work. Until
            // that complete operation is callable, the interior slice refuses.
            if (delta < 0 && receipt.before == 1)
                return finish(ClockResult::UnsupportedBuild);
            receipt.target = delta > 0 ? receipt.before + 1 : receipt.before - 1;
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                (result = C::Recheck(image, model, context)) != ClockResult::Ok || (result = Capture(context, model, second)) != ClockResult::Ok)
                return finish(result);
            if (!Same(first, second, false, 0) || !M::QualifyProfile(image))
                return finish(ClockResult::BindingChanged);
            if ((result = owner()) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_MODEL_REFERENCE_EXCHANGE)
            const auto entry = exchange ? exchange : &MTA_DE_MODEL_REFERENCE_EXCHANGE;
#else
            const auto entry = exchange ? exchange : &NativeExchange;
#endif
            // One compare/exchange closes the final count race without
            // replaying accepted native intent or guessing at another row.
            receipt.entered = true;
            result = entry(receipt.row + 4, receipt.before, receipt.target, receipt.observed, receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok && receipt.observed == receipt.before;
            if (result != ClockResult::Ok)
                return finish(result);
            if (!receipt.accepted)
                return finish(ClockResult::Busy);
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                (result = C::Recheck(image, model, context)) != ClockResult::Ok || (result = Capture(context, model, second)) != ClockResult::Ok)
                return finish(result);
            if (!Same(first, second, true, receipt.target) || !M::QualifyProfile(image))
                return finish(ClockResult::BindingChanged);
            return finish(owner());
        }
        catch (...)
        {
            // Snapshot storage failure cannot justify replay or undoing a
            // count already accepted by native memory.
            return finish(ClockResult::ReadFailure);
        }
    }
}
