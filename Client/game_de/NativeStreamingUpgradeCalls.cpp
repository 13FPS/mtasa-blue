/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual model-context admission for both native upgrade effects
 ****************************************************************************/
#include "NativeStreamingUpgradeCalls.h"
#include "NativeStreamingModelContext.h"
#include <windows.h>
#include <game/CModelRequestPolicy.h>
#include <array>
namespace mta::game_de::NativeStreamingUpgradeCalls
{
    namespace
    {
        namespace Context = NativeStreamingModelContext;
        struct Model
        {
            NativeModelBankReader::Value   bank;
            NativeModelLoadedReader::Value loaded;
            Context::Value                 context;
        };
        ClockResult Capture(std::uintptr_t image, std::uint32_t model, CheckOwner owner, Model& output) noexcept
        {
            Model                   next;
            NativeModelBankReader   bank({image}, owner);
            NativeModelLoadedReader loaded({image}, owner);
            auto                    result = bank.Read(model, next.bank);
            if (result != ClockResult::Ok)
                return result;
            if (!next.bank.nativeModelInfo)
                return ClockResult::NotInitialized;
            if ((result = loaded.Read(model, next.loaded)) != ClockResult::Ok)
                return result;
            if (next.loaded.rawLoadState > 4)
                return ClockResult::BindingChanged;
            if ((result = Context::Capture(image, next.bank, next.context)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        ClockResult Recheck(std::uintptr_t image, CheckOwner owner, const Model& model, bool before) noexcept
        {
            auto result = NativeModelBankReader({image}, owner).Recheck(model.bank);
            if (result != ClockResult::Ok)
                return result;
            if (before && (result = NativeModelLoadedReader({image}, owner).Recheck(model.loaded)) != ClockResult::Ok)
                return result;
            return Context::Recheck(image, model.bank, model.context);
        }
#if !defined(MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(static_cast<std::int32_t>(model), flags);
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
    ClockResult Request(std::uintptr_t image, std::uint32_t modelIndex, std::uint32_t flags, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        Receipt receipt;
        receipt.entryRva = NativeStreamingUpgradeProfile::RequestRva;
        receipt.modelIndex = modelIndex;
        receipt.flags = flags;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!MtaGame::CModelRequestPolicy::IsUpgradeModel(modelIndex))
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        NativeStreamingUpgradeReader        reader({image}, owner);
        NativeStreamingUpgradeReader::Links links;
        auto                                result = reader.ReadLinks(modelIndex, links);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.companionIndex = links.companionIndex;
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::UnsupportedBuild);
        std::array<Model, 2> models{};
        std::size_t          count = 1;
        if ((result = Capture(image, modelIndex, owner, models[0])) != ClockResult::Ok)
            return finish(result);
        receipt.selectedStateBefore = models[0].loaded.rawLoadState;
        if (links.companionIndex >= 0)
        {
            count = 2;
            if ((result = Capture(image, static_cast<std::uint32_t>(links.companionIndex), owner, models[1])) != ClockResult::Ok)
                return finish(result);
            receipt.companionStateBefore = models[1].loaded.rawLoadState;
            if (models[0].context.root != models[1].context.root || models[0].context.provider != models[1].context.provider ||
                models[0].context.rows != models[1].context.rows || models[0].context.count != models[1].context.count)
                return finish(ClockResult::BindingChanged);
        }
        for (std::size_t index = 0; index < count; ++index)
            if ((result = Recheck(image, owner, models[index], true)) != ClockResult::Ok)
                return finish(result);
        if ((result = reader.RecheckLinks(links)) != ClockResult::Ok)
            return finish(result);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::UnsupportedBuild);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        receipt.root = models[0].context.root;
        receipt.provider = models[0].context.provider;
        const auto function = reinterpret_cast<NativeFunction>(image + receipt.entryRva);
#if defined(MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(function, modelIndex, flags, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        for (std::size_t index = 0; index < count; ++index)
        {
            NativeModelLoadedReader::Value after;
            if ((result = NativeModelLoadedReader({image}, owner).Read(models[index].bank.modelIndex, after)) != ClockResult::Ok)
                return finish(result);
            if (index == 0)
                receipt.selectedStateAfter = after.rawLoadState;
            else
                receipt.companionStateAfter = after.rawLoadState;
            if ((result = Recheck(image, owner, models[index], false)) != ClockResult::Ok)
                return finish(result);
        }
        if ((result = reader.RecheckLinks(links)) != ClockResult::Ok)
            return finish(result);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::BindingChanged);
        // Real asynchronous requests can already have changed flags, queues
        // and dependencies. A lost bookend is a stopped-owner result, never
        // permission to rewind either model or issue the companion again.
        return finish(owner());
    }
}
