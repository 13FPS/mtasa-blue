/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual fresh opaque model SDK getters without a layout or native cache
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ModelObjectFixture.generated.h"

using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&ModelObjectObserver::GetInterface), decltype(&CModelInfoDE::GetInterface)>);
static_assert(std::is_same_v<decltype(&ModelObjectObserver::GetRwObject), decltype(&CModelInfoDE::GetRwObject)>);
static_assert(std::is_abstract_v<CModelInfoDE> && !std::is_abstract_v<ModelObjectObserver>);

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    template <class Action>
    void Refuses(Action action, const char* operation, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message = error.what();
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = message.find(operation) != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused, "raw getter did not preserve its owner refusal");
    }
    struct Access final : NativeModelClassificationAccess
    {
        std::uint32_t  interfaceKey = 0, renderKey = 0, *mutateKey = nullptr;
        std::uintptr_t interfaceTag = 0, renderTag = 0;
        ClockResult    interfaceStatus = ClockResult::Ok, renderStatus = ClockResult::Ok;
        unsigned       interfaces = 0, renders = 0, classifications = 0, archives = 0;

        ClockResult ReadModelInterface(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            ++interfaces;
            if (mutateKey)
            {
                *mutateKey = 0;
                mutateKey = nullptr;
            }
            interfaceKey = key;
            output = interfaceTag;
            return interfaceStatus;
        }
        ClockResult ReadModelRender(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            ++renders;
            if (mutateKey)
            {
                *mutateKey = 0;
                mutateKey = nullptr;
            }
            renderKey = key;
            output = renderTag;
            return renderStatus;
        }
        ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++classifications;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept override
        {
            ++classifications;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept override
        {
            ++archives;
            return ClockResult::NotInitialized;
        }
    };
}

int main()
{
    try
    {
        Access              owner;
        std::uint32_t       callerKey = 37;
        ModelObjectObserver receiver(callerKey, owner);
        CModelInfo&         model = receiver;
        Check(owner.interfaces == 0 && owner.renders == 0, "construction queried/cached a native model");
        owner.renderTag = 1;
        Check(reinterpret_cast<std::uintptr_t>(model.GetRwObject()) == 1, "first render observation depended on a prior interface query");
        Check(owner.renders == 1 && owner.interfaces == 0, "render getter chained through another SDK query");
        owner.interfaceTag = 0;
        Check(model.GetInterface() == nullptr, "valid absent interface did not remain null");
        owner.interfaceTag = 0x1234;
        Check(reinterpret_cast<std::uintptr_t>(model.GetInterface()) == 0x1234, "interface getter reused a cached null");
        owner.interfaceTag = 0;
        Check(model.GetInterface() == nullptr, "interface getter reused a cached nonnull tag");

        for (const auto tag : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0x80000000u}, (std::numeric_limits<std::uintptr_t>::max)(),
                               static_cast<std::uintptr_t>(0xfedcba9876543210ull)})
        {
            owner.interfaceTag = tag;
            owner.renderTag = tag;
            owner.mutateKey = &callerKey;
            auto before = owner.interfaces;
            Check(reinterpret_cast<std::uintptr_t>(model.GetInterface()) == tag, "interface null/poison tag bits changed");
            Check(owner.interfaces == before + 1 && owner.interfaceKey == 37 && callerKey == 0, "interface query narrowed/repeated/tore its immutable key");
            before = owner.renders;
            Check(reinterpret_cast<std::uintptr_t>(model.GetRwObject()) == tag, "render null/poison tag bits changed");
            Check(owner.renders == before + 1 && owner.renderKey == 37, "render query was not fresh and direct");
        }
        Check(model.GetModel() == 37, "raw getters changed the CPU model index");

        // The SDK forwards every full key once; only the actual native reader
        // may reject its proven DFF domain. These spies grant no native bank,
        // RwObject layout, readable pointee or out-of-domain resource authority.
        for (const auto key : {std::uint32_t{0}, std::uint32_t{19999}, std::uint32_t{20000}, std::uint32_t{26315}, std::uint32_t{0xffff},
                               std::uint32_t{0x10000}, std::uint32_t{0x80000001}, std::uint32_t{0xffffffff}})
        {
            ModelObjectObserver other(key, owner);
            owner.interfaceTag = 1;
            owner.renderTag = 0;
            other.GetInterface();
            Check(owner.interfaceKey == key, "interface key was narrowed or clamped in SDK");
            Check(other.GetRwObject() == nullptr && owner.renderKey == key, "render key/null was replaced by an SDK domain rule");
        }
        for (const auto failure :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.interfaceStatus = failure;
            owner.renderStatus = failure;
            owner.interfaceTag = 0;
            owner.renderTag = 0;
            const auto interfaces = owner.interfaces, renders = owner.renders;
            Refuses([&] { model.GetInterface(); }, "GetInterface", failure);
            Check(owner.interfaces == interfaces + 1 && owner.renders == renders, "interface refusal retried or made a render query");
            Refuses([&] { model.GetRwObject(); }, "GetRwObject", failure);
            Check(owner.renders == renders + 1 && owner.interfaces == interfaces + 1, "render refusal retried or refreshed another handle");
        }
        Check(owner.archives == 0 && owner.classifications == 0, "raw getters imposed archive/type/classification prerequisites");
        NativeModelClassificationAccess defaults;
        ModelObjectObserver             missing(7, defaults);
        Refuses([&] { missing.GetInterface(); }, "GetInterface", ClockResult::NotInitialized);
        Refuses([&] { missing.GetRwObject(); }, "GetRwObject", ClockResult::NotInitialized);
        std::puts("Actual MTA model interface/render SDK fixture completed");
        std::printf("Passed %u actual MTA model interface/render SDK checks (%zu-bit; opaque dispatch only)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Model interface/render SDK failure after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
