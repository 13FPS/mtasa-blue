/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional Core-producer handoff to a genuine native present owner
 *****************************************************************************/
#pragma once
#include "CGameClockService.h"
#include <cstdint>
#include <type_traits>
#include <atomic>
#include <windows.h>
#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace mta::game_de
{
    inline constexpr std::uint32_t PresentationPolicyVersionV1 = 1;

    // This identifies a constructed client; it is not a native viewport lease.
    // The callback never dereferences clientIdentity or retains a CGame object.
    struct PresentationPolicyClientV1
    {
        std::uint32_t size = sizeof(PresentationPolicyClientV1), version = PresentationPolicyVersionV1;
        std::uint64_t providerGeneration = 0, clientGeneration = 0, clientIdentity = 0;
        std::uint32_t producerThread = 0, reserved = 0;
    };
    struct PresentationPolicyStateV1
    {
        std::uint32_t size = sizeof(PresentationPolicyStateV1), version = PresentationPolicyVersionV1;
        std::uint64_t generation = 0;
        std::uint32_t producerThread = 0, hooked = 0, active = 0, retiring = 0, requested = 0;
        std::uint32_t clientActive = 0, poisoned = 0, pendingCleanup = 0, modulePinned = 0;
        ClockResult   failure = ClockResult::NotInitialized;
        std::uint64_t callbacks = 0, applied = 0, rejected = 0;
        std::uint32_t reserved[2]{};
    };
    static_assert(std::is_standard_layout_v<PresentationPolicyClientV1> && std::is_trivially_copyable_v<PresentationPolicyClientV1>);
    static_assert(std::is_standard_layout_v<PresentationPolicyStateV1> && std::is_trivially_copyable_v<PresentationPolicyStateV1>);
    static_assert(sizeof(PresentationPolicyClientV1) == 40 && sizeof(PresentationPolicyStateV1) == 88);

    // This complete optional export group preserves IGameAdapter's existing
    // vtable. A real installed provider precedes the constructor/client borrow;
    // the existing null full-CGame factory is not promoted by these exports.
#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    #define MTA_PRESENTATION_POLICY_EXPORT __declspec(dllexport)
#else
    #define MTA_PRESENTATION_POLICY_EXPORT
#endif
    extern "C" MTA_PRESENTATION_POLICY_EXPORT Status BeginGamePresentationPolicyV1(std::uint32_t, IGameAdapter*, CCoreInterface*,
                                                                                   PresentationPolicyStateV1*) noexcept;
    extern "C" MTA_PRESENTATION_POLICY_EXPORT Status BindGamePresentationClientV1(std::uint32_t, IGameAdapter*, CCoreInterface*, std::uint64_t, const void*,
                                                                                  PresentationPolicyClientV1*) noexcept;
    extern "C" MTA_PRESENTATION_POLICY_EXPORT Status DisableGamePresentationVSyncV1(std::uint32_t, IGameAdapter*, CCoreInterface*,
                                                                                    const PresentationPolicyClientV1*) noexcept;
    extern "C" MTA_PRESENTATION_POLICY_EXPORT Status RetireGamePresentationClientV1(std::uint32_t, IGameAdapter*, CCoreInterface*,
                                                                                    const PresentationPolicyClientV1*) noexcept;
    extern "C" MTA_PRESENTATION_POLICY_EXPORT Status ReadGamePresentationPolicyV1(std::uint32_t, IGameAdapter*, CCoreInterface*,
                                                                                  PresentationPolicyStateV1*) noexcept;
    extern "C" MTA_PRESENTATION_POLICY_EXPORT Status EndGamePresentationPolicyV1(std::uint32_t, IGameAdapter*, CCoreInterface*, std::uint64_t) noexcept;
#undef MTA_PRESENTATION_POLICY_EXPORT

    // Copy the complete group while Core owns the DLL and before hook admission.
    // No callback performs a loader lookup or calls back through this value.
    struct PresentationPolicyExportsV1
    {
        decltype(&BeginGamePresentationPolicyV1)  begin = nullptr;
        decltype(&BindGamePresentationClientV1)   bind = nullptr;
        decltype(&DisableGamePresentationVSyncV1) disable = nullptr;
        decltype(&RetireGamePresentationClientV1) retire = nullptr;
        decltype(&ReadGamePresentationPolicyV1)   read = nullptr;
        decltype(&EndGamePresentationPolicyV1)    end = nullptr;
        bool                                      Complete() const noexcept { return begin && bind && disable && retire && read && end; }
    };

    inline bool SamePresentationClient(const PresentationPolicyClientV1& left, const PresentationPolicyClientV1& right) noexcept
    {
        return left.size == sizeof(left) && right.size == sizeof(right) && left.version == PresentationPolicyVersionV1 &&
               right.version == PresentationPolicyVersionV1 && left.providerGeneration == right.providerGeneration &&
               left.clientGeneration == right.clientGeneration && left.clientIdentity == right.clientIdentity && left.producerThread == right.producerThread &&
               !left.reserved && !right.reserved;
    }

    inline bool InstalledPresentationProvider(const PresentationPolicyStateV1& state, std::uint64_t generation, std::uint32_t producer) noexcept
    {
        return state.size == sizeof(state) && state.version == PresentationPolicyVersionV1 && generation && state.generation == generation && producer &&
               state.producerThread == producer && state.hooked == 1 && state.active == 1 && state.modulePinned == 1 && !state.retiring && !state.poisoned &&
               !state.pendingCleanup && state.failure == ClockResult::Ok && !state.reserved[0] && !state.reserved[1];
    }

    // Producer-only storage is allocated before hook admission. A late CGame
    // destructor can still observe revocation after Core/DLL retirement without
    // dereferencing destroyed Core storage or calling its unloaded exports.
    // Native callbacks retain neither this shared state nor an object pointer.
    struct PresentationPolicyBorrowState
    {
        std::atomic<std::uint32_t> references{1};
        PresentationPolicyClientV1 client;
        bool                       revoked = false, consumed = false;
    };
    // Intrusive ownership keeps every release operation inline in its current
    // caller. The Windows process heap is independent of each DLL's /MT CRT.
    // A std::shared_ptr control-block vtable/deleter created in Core
    // could otherwise call unloaded Core code from a late CGame destructor.
    class PresentationPolicyBorrowLifetime
    {
    public:
        PresentationPolicyBorrowLifetime() noexcept = default;
        PresentationPolicyBorrowLifetime(const PresentationPolicyBorrowLifetime& other) noexcept : state(other.state)
        {
            if (state && state->references.fetch_add(1) == (std::numeric_limits<std::uint32_t>::max)())
                std::terminate();
        }
        PresentationPolicyBorrowLifetime(PresentationPolicyBorrowLifetime&& other) noexcept : state(std::exchange(other.state, nullptr)) {}
        PresentationPolicyBorrowLifetime& operator=(PresentationPolicyBorrowLifetime other) noexcept
        {
            std::swap(state, other.state);
            return *this;
        }
        ~PresentationPolicyBorrowLifetime()
        {
            if (state && state->references.fetch_sub(1) == 1)
            {
                state->~PresentationPolicyBorrowState();
                HeapFree(GetProcessHeap(), 0, state);
            }
        }
        static PresentationPolicyBorrowLifetime Create() noexcept
        {
            auto* memory = HeapAlloc(GetProcessHeap(), 0, sizeof(PresentationPolicyBorrowState));
            return PresentationPolicyBorrowLifetime(memory ? new (memory) PresentationPolicyBorrowState : nullptr);
        }
        void                           reset() noexcept { *this = {}; }
        explicit                       operator bool() const noexcept { return state != nullptr; }
        PresentationPolicyBorrowState* operator->() const noexcept { return state; }

    private:
        explicit PresentationPolicyBorrowLifetime(PresentationPolicyBorrowState* value) noexcept : state(value) {}
        PresentationPolicyBorrowState* state = nullptr;
    };
    struct NativePresentationPolicyBorrow
    {
        PresentationPolicyExportsV1      functions;
        IGameAdapter*                    adapter = nullptr;
        CCoreInterface*                  core = nullptr;
        std::uint64_t                    providerGeneration = 0;
        std::uint32_t                    producerThread = 0;
        PresentationPolicyClientV1*      ownerClient = nullptr;
        PresentationPolicyBorrowLifetime lifetime;
        bool                             Complete() const noexcept
        {
            return functions.Complete() && adapter && core && providerGeneration && producerThread && lifetime && ownerClient == &lifetime->client;
        }
    };
}
