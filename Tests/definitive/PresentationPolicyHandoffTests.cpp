/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real Core consumer and SDK mixin with a controlled provider DLL
 *****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include "../../Client/core/CGameAdapterModule.h"
#include "../../Client/game_de/CGamePresentationPolicyDE.h"
#include "PresentationPolicyHandoffFixture.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unrelated()
    {
        throw std::runtime_error("unrelated SDK obligation reached presentation fixture");
    }
}
// CGame declares these unrelated slots non-pure without providing their
// out-of-line bodies here. The intermediate real mixin vtable still needs
// those symbols even though the concrete rejecting fixture overrides them.
// Keep this fixture-only definition rejecting; no jetpack behavior is tested.
void CGame::SetJetpackWeaponEnabled(eWeaponType, bool)
{
    fixture::Unrelated();
}
bool CGame::GetJetpackWeaponEnabled(eWeaponType)
{
    fixture::Unrelated();
}

#include "PresentationPolicyGameFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    using namespace PresentationHandoffFixture;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    template <typename Function>
    Function Resolve(HMODULE module, const char* name)
    {
        const auto address = GetProcAddress(module, name);
        Check(address != nullptr, name);
        Function function = nullptr;
        static_assert(sizeof(function) == sizeof(address));
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }
    struct Library
    {
        HMODULE value;
        explicit Library(const wchar_t* path) : value(LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32))
        {
            Check(value != nullptr, "load explicit owned fixture/backend DLL");
        }
        ~Library()
        {
            if (value)
                FreeLibrary(value);
        }
    };
    struct Identity
    {
        alignas(void*) std::array<unsigned char, sizeof(void*)> bytes{};
        CCoreInterface* Get() noexcept { return reinterpret_cast<CCoreInterface*>(bytes.data()); }
    };
    template <typename Function>
    bool Throws(Function&& function)
    {
        try
        {
            function();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        return false;
    }

    void Consumer(const wchar_t* full, const wchar_t* partial)
    {
        Identity identity;
        {
            Library    library(partial);
            const auto reader = Resolve<Read>(library.value, "ReadPresentationHandoffFixtureV1");
            const auto read = [&]
            {
                Snapshot value;
                reader(&value);
                return value;
            };
            CGameAdapterModule module;
            Check(module.Start(identity.Get(), std::filesystem::absolute(partial).wstring()), "Core owns partial-group fixture through real Start");
            PresentationPolicyStateV1 state;
            Check(!module.BeginPresentationPolicy(state) && !read().begins, "partial optional group never admits Begin");
            module.Stop();
            Check(!module.IsStarted(), "older/partial provider preserves ordinary adapter shutdown");
        }
        Library    library(full);
        const auto control = Resolve<SetControl>(library.value, "PresentationHandoffFixtureControlV1");
        const auto reader = Resolve<Read>(library.value, "ReadPresentationHandoffFixtureV1");
        const auto read = [&]
        {
            Snapshot value;
            reader(&value);
            return value;
        };
        {
            CGameAdapterModule module;
            Check(module.Start(identity.Get(), std::filesystem::absolute(full).wstring()), "actual Core Start owns complete provider group");
            NativePresentationPolicyBorrow borrow;
            Check(!module.BorrowPresentationPolicy(borrow) && !borrow.Complete(), "no constructor borrow before installed acknowledgement");
            PresentationPolicyStateV1 state;
            Check(module.BeginPresentationPolicy(state) && InstalledPresentationProvider(state, state.generation, GetCurrentThreadId()),
                  "actual Core Begin accepts only healthy installed current producer state");
            control(UnpinnedRead);
            Check(!module.BorrowPresentationPolicy(borrow) && !borrow.Complete(), "Core refuses an unpinned constructor provider");
            control(MalformedPinRead);
            Check(!module.BorrowPresentationPolicy(borrow) && !borrow.Complete(), "Core refuses malformed pinned value instead of accepting truthiness");
            control(Normal);
            Check(module.BorrowPresentationPolicy(borrow) && borrow.Complete(), "actual Core produces typed constructor borrow");
            Check(borrow.ownerClient && !borrow.ownerClient->clientGeneration, "Core retains stable unbound client slot");
            control(PoisonRead);
            Check(Throws([&] { NativePresentationPolicyAccess refused(borrow, &identity); }), "stale healthy snapshot cannot admit poisoned provider");
            control(UnpinnedRead);
            Check(Throws([&] { NativePresentationPolicyAccess refused(borrow, &identity); }), "constructor independently refuses provider pin loss");
            control(MalformedPinRead);
            Check(Throws([&] { NativePresentationPolicyAccess refused(borrow, &identity); }), "constructor independently refuses malformed pin state");
            control(Normal);
            {
                fixture::PresentationPolicyGame game(borrow);
                CGame*                          sdk = &game;
                Check(read().binds == 1 && borrow.ownerClient->clientIdentity == reinterpret_cast<std::uintptr_t>(&game),
                      "actual SDK mixin constructor binds its genuine this identity");
                sdk->DisableVSync();
                Check(read().disables == 1 && read().policy.requested, "actual inherited SDK void dispatch reaches stamped export");
                Check(!module.Reset() && !read().resets, "Core Reset refuses while constructed client borrows its slot");
                NativePresentationPolicyBorrow second;
                Check(!module.BorrowPresentationPolicy(second), "second current client cannot be borrowed");
                auto wrong = *borrow.ownerClient;
                ++wrong.clientGeneration;
                Check(borrow.functions.disable(PresentationPolicyVersionV1, borrow.adapter, borrow.core, &wrong) == Status::InvalidArgument,
                      "changed client stamp cannot dispatch");
                bool        wrongThreadRefused = false;
                std::thread other([&] { wrongThreadRefused = Throws([&] { sdk->DisableVSync(); }); });
                other.join();
                Check(wrongThreadRefused && read().disables == 1, "different producer thread cannot use SDK handoff");
                Check(module.RetirePresentationClient() && !borrow.ownerClient->clientGeneration, "Core can revoke exact current client before Clock teardown");
                Check(Throws([&] { sdk->DisableVSync(); }) && read().disables == 1, "void SDK call refuses after Core revocation");
            }
            Check(read().retires == 1, "destructor does not re-retire Core-revoked generation");
            Check(Throws([&] { NativePresentationPolicyAccess consumed(borrow, &identity); }),
                  "consumed constructor handoff cannot be reused after client retirement");
            Check(module.EndPresentationPolicy(), "installed provider retires after client retirement");
            Check(Throws([&] { NativePresentationPolicyAccess stale(borrow, &identity); }), "old borrow cannot reconstruct after provider End");
            Check(module.BeginPresentationPolicy(state), "new provider lifetime can begin after actual End");
            Check(Throws([&] { NativePresentationPolicyAccess stale(borrow, &identity); }), "old borrow cannot renew on newer provider generation");
            Check(module.EndPresentationPolicy(), "new empty provider retires");
            module.Stop();
            Check(!module.IsStarted(), "actual Core Stop unloads retired provider");
        }
        {
            CGameAdapterModule module;
            Check(module.Start(identity.Get(), std::filesystem::absolute(full).wstring()), "Core owns failed-Begin fixture");
            control(FailBegin);
            PresentationPolicyStateV1 state;
            Check(!module.BeginPresentationPolicy(state) && state.generation && state.pendingCleanup, "failed Begin preserves actual cleanup generation");
            NativePresentationPolicyBorrow borrow;
            Check(!module.BorrowPresentationPolicy(borrow), "failed installed admission cannot construct SDK client");
            const auto begins = read().begins;
            Check(!module.BeginPresentationPolicy(state) && read().begins == begins, "failed Begin debt blocks replacement admission");
            control(FailEnd);
            Check(!module.EndPresentationPolicy(), "failed End retains Core provider debt");
            control(Normal);
            Check(module.EndPresentationPolicy(), "same retained generation can retire on retry");
            Check(module.BeginPresentationPolicy(state) && module.BorrowPresentationPolicy(borrow), "new client borrow follows successful cleanup only");
            control(BindReturnsStampThenFails);
            Check(Throws([&] { NativePresentationPolicyAccess failed(borrow, &identity); }) && borrow.ownerClient->clientGeneration,
                  "failed CGame construction preserves returned genuine client stamp in Core");
            control(FailRetire);
            module.Stop();
            Check(module.IsStarted() && !read().shutdowns, "failed client retire retains adapter/module before Shutdown");
            control(Normal);
            module.Stop();
            const auto final = read();
            Check(!module.IsStarted() && final.retireOrder && final.retireOrder < final.shutdownOrder,
                  "cleanup retry retires exact constructor stamp before adapter Shutdown");
        }
        {
            CGameAdapterModule module;
            Check(module.Start(identity.Get(), std::filesystem::absolute(full).wstring()), "Core owns destructor-retirement fixture");
            PresentationPolicyStateV1      state;
            NativePresentationPolicyBorrow borrow;
            Check(module.BeginPresentationPolicy(state) && module.BorrowPresentationPolicy(borrow), "destructor fixture owns actual borrow");
            {
                fixture::PresentationPolicyGame game(borrow);
                control(FailRetire);
            }
            Check(borrow.ownerClient->clientGeneration && !module.Reset(), "failed destructor leaves stamp, and Reset cannot erase it");
            control(Normal);
            module.Stop();
            Check(!module.IsStarted() && read().retireOrder < read().shutdownOrder, "Core retires failed destructor debt before Shutdown");
        }
        std::unique_ptr<fixture::PresentationPolicyGame> late;
        NativePresentationPolicyBorrow                   escaped;
        const auto                                       createStorage = Resolve<CreateStorage>(library.value, "CreatePresentationHandoffStorageV1");
        PresentationPolicyBorrowLifetime                 dllStorage;
        Check(createStorage(&dllStorage) && HeapValidate(GetProcessHeap(), 0, dllStorage.operator->()) != FALSE,
              "actual SDK borrow storage allocated inside separate MT provider belongs to Windows process heap");
        dllStorage->revoked = true;
        auto retainedStorage = dllStorage;
        dllStorage.reset();
        {
            CGameAdapterModule module;
            Check(module.Start(identity.Get(), std::filesystem::absolute(full).wstring()), "Core owns late-wrapper lifetime fixture");
            PresentationPolicyStateV1 state;
            Check(module.BeginPresentationPolicy(state) && module.BorrowPresentationPolicy(escaped), "late wrapper has current Core handoff");
            late = std::make_unique<fixture::PresentationPolicyGame>(escaped);
            module.Stop();
            Check(!module.IsStarted() && escaped.lifetime->revoked, "Core Stop revokes stable producer storage before unloading its provider");
        }
        Check(FreeLibrary(library.value) != FALSE, "release the fixture observer's final DLL reference");
        library.value = nullptr;
        Check(!GetModuleHandleW(L"presentation-fixture-full.dll"), "controlled provider DLL is really unloaded before late SDK access");
        Check(retainedStorage && retainedStorage->revoked && HeapValidate(GetProcessHeap(), 0, retainedStorage.operator->()) != FALSE,
              "provider-created process-heap storage remains valid after its actual allocating MT DLL unloads");
        retainedStorage.reset();
        Check(!retainedStorage, "host releases provider-created SDK storage after DLL unload without CRT ownership or Core code");
        CGame* lateSdk = late.get();
        Check(Throws([&] { lateSdk->DisableVSync(); }), "late wrapper refuses without dereferencing destroyed Core or calling unloaded exports");
        late.reset();
        Check(escaped.lifetime->revoked && !escaped.ownerClient->clientGeneration, "late wrapper destructor consumes only still-live revoked storage");
    }

    void ActualBackend(const wchar_t* path)
    {
        Library                     module(path);
        const auto                  factory = Resolve<decltype(&CreateGameAdapter)>(module.value, "CreateGameAdapter");
        PresentationPolicyExportsV1 functions;
        functions.begin = Resolve<decltype(functions.begin)>(module.value, "BeginGamePresentationPolicyV1");
        functions.bind = Resolve<decltype(functions.bind)>(module.value, "BindGamePresentationClientV1");
        functions.disable = Resolve<decltype(functions.disable)>(module.value, "DisableGamePresentationVSyncV1");
        functions.retire = Resolve<decltype(functions.retire)>(module.value, "RetireGamePresentationClientV1");
        functions.read = Resolve<decltype(functions.read)>(module.value, "ReadGamePresentationPolicyV1");
        functions.end = Resolve<decltype(functions.end)>(module.value, "EndGamePresentationPolicyV1");
        Identity      core, different;
        IGameAdapter* adapter = factory(AdapterAbiVersion);
        Check(adapter && adapter->Initialize(core.Get()) == Status::Ok && functions.Complete(),
              "actual backend initializes Core producer with complete exports");
        PresentationPolicyStateV1 state;
        Check(functions.begin(0, adapter, core.Get(), &state) == Status::InvalidArgument, "actual backend rejects ABI mismatch");
        Check(functions.begin(1, reinterpret_cast<IGameAdapter*>(std::uintptr_t{1}), core.Get(), &state) == Status::InvalidArgument,
              "actual registry rejects arbitrary adapter before dereference");
        Check(functions.begin(1, adapter, different.Get(), &state) == Status::InvalidArgument, "actual registry rejects changed Core identity");
        Status      other = Status::Ok;
        std::thread thread([&] { other = functions.begin(1, adapter, core.Get(), &state); });
        thread.join();
        Check(other == Status::WrongThread, "actual registry rejects foreign initialization producer");
        Check(functions.end(1, adapter, core.Get(), 0) == Status::Ok, "actual empty policy End is compatible with ordinary adapter lifetime");
        Check(functions.begin(1, adapter, core.Get(), &state) != Status::Ok, "actual provider refuses ordinary owned test executable as GTA");
        Check(functions.end(1, adapter, core.Get(), state.generation) == Status::Ok, "actual refused generation retires through genuine owner");
        Check(!(adapter->GetCapabilities() & static_cast<std::uint64_t>(AdapterCapability::GameInterface)) && !adapter->GetGameInterface(),
              "presentation exports do not advertise unavailable full factory");
        Check(adapter->Shutdown() == Status::Ok, "actual no-debt shutdown preserves adapter lifecycle");
        adapter->Release();
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 4, "full fixture, partial fixture and pinned normal backend required");
        static_assert(sizeof(PresentationPolicyClientV1) == 40 && sizeof(PresentationPolicyStateV1) == 88);
        static_assert(std::is_abstract_v<CGamePresentationPolicyDE>);
        Consumer(argv[1], argv[2]);
        ActualBackend(argv[3]);
        std::printf("Passed %u focused actual Core presentation handoff checks\n", checks);
        std::puts(
            "actualCorePresentationExportGroupTested=1\nactualCorePresentationConstructionBorrowTested=1\nactualSdkPresentationMixinTested="
            "1\nactualCorePresentationRetirementTested=1\nactualPresentationBackendRefusalsTested=1\nactualPresentationProcessHeapLifetimeTested=1");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
