/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Dispatch the actual MTA player-model predicate through its current owner
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ModelObjectFixture.generated.h"

using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&ModelObjectObserver::IsPlayerModel), decltype(&CModelInfoDE::IsPlayerModel)>);
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
    void Refuses(Action action, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE model owner refused IsPlayerModel: ") + std::to_string(static_cast<unsigned>(expected));
        }
        Check(refused, "actual player-model SDK refusal must retain its explicit operation and status");
    }
    struct Access final : NativeModelClassificationAccess
    {
        unsigned      calls = 0, unrelated = 0;
        std::uint32_t key = 0, *mutateKey = nullptr;
        bool          value = false;
        ClockResult   status = ClockResult::Ok;
        ClockResult   ReadModelPlayerPredicate(std::uint32_t fullKey, bool& output) noexcept override
        {
            ++calls;
            key = fullKey;
            if (mutateKey)
            {
                *mutateKey = 0;
                mutateKey = nullptr;
            }
            // Write even on refusal to ensure a stale/changed temporary never
            // becomes a successful SDK boolean when the owner reports failure.
            output = value;
            return status;
        }
        ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++unrelated;
            return ClockResult::UnsupportedBuild;
        }
        ClockResult ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept override
        {
            ++unrelated;
            return ClockResult::UnsupportedBuild;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept override
        {
            ++unrelated;
            return ClockResult::UnsupportedBuild;
        }
        ClockResult ReadModelInterface(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::UnsupportedBuild;
        }
        ClockResult ReadModelRender(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::UnsupportedBuild;
        }
    };
}

int main()
{
    try
    {
        Access              owner;
        std::uint32_t       originalKey = 37;
        ModelObjectObserver receiver(originalKey, owner);
        CModelInfo&         model = receiver;
        Check(owner.calls == 0, "construction must not query or cache native player identity");
        for (const auto identity : {false, true, false, true})
        {
            owner.value = identity;
            const auto before = owner.calls;
            owner.mutateKey = &originalKey;
            Check(model.IsPlayerModel() == identity, "actual SDK dispatch must preserve current successful false and true values");
            Check(owner.calls == before + 1 && owner.key == 37 && originalKey == 0,
                  "actual SDK identity call must be fresh/direct and keep its immutable copied model key");
        }
        Check(model.GetModel() == 37, "player identity observation must not alter the model facade's key");
        // Full-key admission belongs to the native reader, so the SDK may not
        // invent a classic player ID interval or narrow large unsigned values.
        for (const auto key : {std::uint32_t{0}, std::uint32_t{7}, std::uint32_t{19999}, std::uint32_t{20000}, std::uint32_t{26315}, std::uint32_t{0xffff},
                               std::uint32_t{0x10000}, std::uint32_t{0x80000001}, std::uint32_t{0xffffffff}})
        {
            ModelObjectObserver other(key, owner);
            CModelInfo&         sdk = other;
            owner.value = true;
            const auto before = owner.calls;
            Check(sdk.IsPlayerModel() && owner.key == key && owner.calls == before + 1,
                  "actual SDK forwards each full unsigned key exactly once without player-ID or class gating");
        }
        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
            for (const auto temporary : {false, true})
            {
                owner.status = status;
                owner.value = temporary;
                const auto before = owner.calls;
                Refuses([&] { model.IsPlayerModel(); }, status);
                Check(owner.calls == before + 1 && owner.key == 37, "owner refusal must not trigger retries, fallback or index mutation");
            }
        Check(owner.unrelated == 0, "player identity dispatch must not chain through type/archive/render/interface observations");
        NativeModelClassificationAccess defaults;
        bool                            untouched = true;
        Check(defaults.ReadModelPlayerPredicate(7, untouched) == ClockResult::NotInitialized && untouched,
              "an old owner preserves output and explicitly refuses the newly introduced capability");
        ModelObjectObserver missing(7, defaults);
        CModelInfo&         unsupported = missing;
        Refuses([&] { unsupported.IsPlayerModel(); }, ClockResult::NotInitialized);
        std::puts("Actual MTA model player identity SDK fixture completed");
        std::printf("Passed %u actual MTA model player identity SDK checks (%zu-bit; predicate dispatch only)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Model player identity SDK failure after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
