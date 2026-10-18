/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Scoped native local movement routing for the build-pinned DE trial
 *****************************************************************************/

#include "NativeMovementInput.h"
#include "NativeInputRouting.h"
#include "NativePadReader.h"
#include "FrontendInputLeasePolicy.h"
#include "NativeServiceAdmission.h"

#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <vector>
#include <enums/PedState.h>
#include <windows.h>
#include <tlhelp32.h>
#include <detours.h>

namespace mta::game_de
{
    namespace
    {
        using Failure = NativeMovementTrialFailure;
        namespace Admission = NativeServiceAdmission;
        using ControlFunction = void (*)(void*);
        using MembershipFunction = bool (*)(void*, std::int32_t);
        using QueryFunction = std::int32_t (*)(void*, std::int32_t, float*);

        constexpr std::uintptr_t controlRva = 0x124eaf0;
        constexpr std::uintptr_t membershipRva = 0xc09750;
        constexpr std::uintptr_t availabilityRva = 0xc097d0;
        constexpr std::uintptr_t queryRva = 0xc09990;
        constexpr std::uintptr_t padRva = 0x53e8810;
        constexpr ULONGLONG      maximumTrialMs = 35000;
        constexpr ULONGLONG      maximumInputAgeMs = 2000;
        constexpr ULONGLONG      maximumEligibilityAgeMs = 250;
        constexpr unsigned       exclusivePadAccess = 1u << 31;
        static_assert(PedState::PED_IDLE == 1 && PedState::PED_DIE == 0x36 && PedState::PED_DEAD == 0x37);

        struct BindingIdentity
        {
            std::uintptr_t ped = 0, provider = 0;
        };

        struct EligibilityPublication
        {
            NativeMovementEligibility eligibility;
            BindingIdentity           identity;
            std::uint64_t             generation = 0;
            ULONGLONG                 refreshed = 0;
            bool                      valid = false;
        };

        struct Counters
        {
            std::atomic<std::uint64_t> callbacks{0}, steps{0}, queries{0}, neutral{0}, restored{0}, restoreFailures{0}, foreign{0}, nested{0};
            std::atomic<std::uint32_t> reason{0};
            std::atomic<std::uint32_t> lastMoveState{0};
        };

        struct Runtime
        {
            SRWLOCK                    mailboxLock = SRWLOCK_INIT;
            SRWLOCK                    eligibilityLock = SRWLOCK_INIT;
            EligibilityPublication     eligibilityPublication;
            std::atomic<bool>          apiBusy{false};
            NativePadReader            reader;
            std::atomic<bool>          bound{false}, active{false}, hooked{false};
            std::atomic<bool>          modulePinned{false};
            std::atomic<unsigned>      scopes{0}, inFlight{0};
            std::atomic<unsigned>      padAccess{0};
            std::atomic<std::uint64_t> generation{0};
            std::atomic<std::uint64_t> admissionGeneration{0}, admissionSerial{0};
            DWORD                      producerThread = 0;
            std::atomic<DWORD>         engineThread{0};
            std::uintptr_t             image = 0, ped = 0, provider = 0;
            ULONGLONG                  started = 0, refreshed = 0;
            MtaInputState              frame;
            // Historical movement trials retain their absolute 35-second
            // limit. Only an explicitly admitted neutral frontend uses short,
            // renewable leases checked independently on the engine callback.
            bool                     frontendLease = false;
            HWND                     frontendWindow = nullptr;
            FrontendInputLeasePolicy frontendPolicy;
            // The adopted callback owns this snapshot while in flight. End
            // may retry it only after suspending threads and proving quiescence;
            // an exited engine thread must not take the recovery bytes with it.
            NativeInputRouting::PadTransaction transaction;
            std::uint64_t                      scopeGeneration = 0;
            std::int32_t                       scopeProviderIndex = 0;
            std::uintptr_t                     scopeAlternate = 0;
            Counters                           counters;
            ControlFunction                    originalControl = nullptr;
            MembershipFunction                 originalMembership = nullptr, originalAvailability = nullptr;
            QueryFunction                      originalQuery = nullptr;
        };

        // Hooks must never retain an adapter instance. A failed detachment may
        // outlive Core's owner, so this process lifetime state and the pinned
        // module retain every trampoline and restoration snapshot they need.
        Runtime runtime;

        bool ReleaseAdmission(std::uint64_t generation) noexcept
        {
            const auto owner = runtime.admissionGeneration.load();
            if (!owner)
                return true;
            if (!generation || generation != runtime.generation.load() || generation != owner)
                return false;
            if (!Admission::Release({Admission::Category::Input, runtime.admissionSerial.load()}))
                return false;
            runtime.admissionSerial.store(0);
            runtime.admissionGeneration.store(0);
            return true;
        }

        struct Scope
        {
            MtaInputState    frame;
            CControllerState consumed;
            std::uint64_t    consumedGeneration = 0;
            std::uint64_t    generation = 0;
            std::int32_t     providerIndex = 0;
            std::uintptr_t   alternate = 0;
            bool             routed = false;
            unsigned         callbackDepth = 0;
        };
        thread_local Scope scope;

        void ClearEligibility() noexcept
        {
            AcquireSRWLockExclusive(&runtime.eligibilityLock);
            runtime.eligibilityPublication = {};
            ReleaseSRWLockExclusive(&runtime.eligibilityLock);
        }

        bool TryPublishEligibility(const NativeMovementEligibility& eligibility, const BindingIdentity& identity) noexcept
        {
            // A simulation callback must never wait for a producer which a
            // Detours transaction may suspend. A missed publication is safe:
            // the producer rejects old evidence after the short freshness bound.
            if (!TryAcquireSRWLockExclusive(&runtime.eligibilityLock))
                return false;
            const bool publish = runtime.bound.load() && runtime.active.load() && identity.ped == runtime.ped && identity.provider == runtime.provider &&
                                 eligibility.knownMask == NativeMovementEligibilityMask && eligibility.flags == NativeMovementEligibilityMask &&
                                 eligibility.eligible;
            if (publish)
                runtime.eligibilityPublication = {eligibility, identity, runtime.generation.load(), GetTickCount64(), true};
            ReleaseSRWLockExclusive(&runtime.eligibilityLock);
            return publish;
        }

        bool ReadPublishedEligibility(std::uint64_t generation, NativeMovementEligibility& eligibility) noexcept
        {
            AcquireSRWLockShared(&runtime.eligibilityLock);
            const auto publication = runtime.eligibilityPublication;
            ReleaseSRWLockShared(&runtime.eligibilityLock);
            const auto now = GetTickCount64();
            if (!generation || generation != runtime.generation.load() || !runtime.bound.load() || !runtime.active.load() || !publication.valid ||
                publication.generation != generation || publication.identity.ped != runtime.ped || publication.identity.provider != runtime.provider ||
                publication.eligibility.knownMask != NativeMovementEligibilityMask || publication.eligibility.flags != NativeMovementEligibilityMask ||
                !publication.eligibility.eligible || now < publication.refreshed || now - publication.refreshed > maximumEligibilityAgeMs)
                return false;
            eligibility = publication.eligibility;
            return true;
        }

        // API calls never wait for another API operation, and callbacks never
        // acquire this gate. Detours can suspend threads without stranding a
        // waiter on a lock held by the producer doing the transaction.
        struct ApiOperation
        {
            bool acquired;
            ApiOperation() noexcept : acquired(!runtime.apiBusy.exchange(true)) {}
            ~ApiOperation()
            {
                if (acquired)
                    runtime.apiBusy.store(false);
            }
        };

        void Fail(Failure reason) noexcept
        {
            runtime.active.store(false);
            if (runtime.frontendLease)
                runtime.frontendPolicy.Revoke(FrontendInputLeaseReason::NativeFailure);
            std::uint32_t expected = 0;
            runtime.counters.reason.compare_exchange_strong(expected, static_cast<std::uint32_t>(reason));
        }

        bool Pages(std::uintptr_t address, std::size_t count, bool writable = false, bool executable = false) noexcept
        {
            if (!address || !count || count > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + count;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const DWORD protection = info.Protect & 0xff;
                const bool  read = protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
                                  protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                const bool write = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
                const bool execute = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                if (!read || (writable && !write) || (executable && !execute))
                    return false;
                const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - region || region + info.RegionSize <= address)
                    return false;
                address = (region + info.RegionSize < end) ? region + info.RegionSize : end;
            }
            return true;
        }

        bool Read(std::uintptr_t address, void* result, std::size_t count) noexcept
        {
            SIZE_T copied = 0;
            return Pages(address, count) && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), result, count, &copied) &&
                   copied == count;
        }

        template <typename T>
        bool Read(std::uintptr_t address, T& result) noexcept
        {
            return Read(address, &result, sizeof(result));
        }

        bool WritePad(const NativeInputRouting::PadStateBytes& bytes) noexcept
        {
            const auto address = runtime.image + padRva + NativePadCurrentOffset;
            SIZE_T     copied = 0;
            // Only these two adjacent controller blocks are owned. Mouse data,
            // native physical sources and their pending edge queues stay outside
            // the write range, even when a trial or restoration fails.
            return Pages(address, bytes.size(), true) &&
                   WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), bytes.data(), bytes.size(), &copied) && copied == bytes.size();
        }

        bool ReadContext(std::int32_t& index, std::uintptr_t& alternate) noexcept
        {
            return Read(runtime.image + 0x5726120, index) && Read(runtime.image + 0x5067940, alternate);
        }

        template <std::size_t N>
        bool PinAt(std::uintptr_t image, std::uintptr_t rva, const std::array<unsigned char, N>& expected) noexcept
        {
            std::array<unsigned char, N> actual{};
            return Pages(image + rva, N, false, true) && Read(image + rva, actual) && actual == expected;
        }

        bool CheckEligibilityPins(std::uintptr_t image) noexcept
        {
            // These instructions establish the state member independently of
            // move state, and the native input gate independently of the public
            // SDK's incompatible enclosing CPad layout. They are outside every
            // Detours entry, so they remain checkable while this trial is active.
            constexpr std::array<unsigned char, 5>  baseConstructor = {0xe8, 0x59, 0xb8, 0xfc, 0xff};
            constexpr std::array<unsigned char, 10> idleState = {0xc7, 0x87, 0xfc, 0x05, 0, 0, 1, 0, 0, 0};
            constexpr std::array<unsigned char, 10> onFoot = {0xf7, 0x83, 0x34, 0x06, 0, 0, 0, 1, 0, 0};
            constexpr std::array<unsigned char, 4>  controlsEnabled = {0x66, 0x39, 0x79, 0x30};
            return PinAt(image, 0x124e002, baseConstructor) && PinAt(image, 0x1219cce, idleState) && PinAt(image, 0x124eb5e, onFoot) &&
                   PinAt(image, 0x11579bf, controlsEnabled);
        }

        bool ReadLocalEligibility(std::uintptr_t image, const BindingIdentity& expectedIdentity, void* candidate, NativeMovementEligibility& eligibility,
                                  BindingIdentity& identity) noexcept
        {
            std::uintptr_t ped = 0, vtable = 0, callback = 0, info = 0, provider = 0, providerVtable = 0, alternate = 0;
            std::int32_t   selector = -1, index = -1;
            std::uint32_t  pedFlags = 0, pedState = 0;
            std::uint16_t  disabledControls = 0;
            unsigned char  playerIndex = 0;
            if (!image || !CheckEligibilityPins(image) || !Read(image + 0x53eb720, ped) || !ped || (expectedIdentity.ped && ped != expectedIdentity.ped) ||
                (candidate && reinterpret_cast<std::uintptr_t>(candidate) != ped) || !Read(ped, vtable) || vtable != image + 0x4319730 ||
                !Read(vtable + 0x50, callback) || callback != image + controlRva || !Read(ped + 0x5f0, info) || info != image + 0x53eb728 ||
                !Read(ped + 0x7e0, selector) || selector != 0 || !Read(image + 0x522bcf6, playerIndex) || playerIndex != 0 || !Read(image + 0x5726120, index) ||
                index != 0 || !Read(image + 0x5067940, alternate) || alternate != image + padRva || !Read(image + 0x5726100, provider) || !provider ||
                (expectedIdentity.provider && provider != expectedIdentity.provider) || !Read(provider, providerVtable) ||
                providerVtable != image + 0x4226b20 || !Read(ped + 0x634, pedFlags) || !Read(ped + 0x5fc, pedState) ||
                !Read(image + padRva + 0x30, disabledControls))
                return false;
            const std::array<std::uintptr_t, 7> expected = {0xc09750, 0xc097d0, 0xc09810, 0xc09950, 0xc09970, 0xc09ae0, 0xae3c00};
            std::array<std::uintptr_t, 7>       actual{};
            if (!Read(providerVtable, actual.data(), sizeof(actual)))
                return false;
            for (std::size_t i = 0; i < actual.size(); ++i)
                if (actual[i] != image + expected[i])
                    return false;
            // Type2 is checked as code, rather than invoking an unowned native
            // virtual method merely to decide whether the trial can start.
            constexpr std::array<unsigned char, 6> type2 = {0xb8, 0x02, 0, 0, 0, 0xc3};
            std::array<unsigned char, 6>           typeCode{};
            if (!Read(actual[6], typeCode) || typeCode != type2)
                return false;
            // Producer reads are observations rather than a lock on the game.
            // Recheck the identity roots before publishing one coherent result;
            // the callback repeats this same guard immediately before each write.
            std::uintptr_t pedAgain = 0, providerAgain = 0, alternateAgain = 0;
            std::int32_t   indexAgain = -1, selectorAgain = -1;
            std::uint32_t  flagsAgain = 0, stateAgain = 0;
            std::uint16_t  disabledAgain = 0;
            unsigned char  playerAgain = 0;
            // Only the verified vehicle bit contributes to this admission.
            // Unrelated ped flags can change without invalidating on-foot
            // eligibility; state and the complete controls gate remain stable.
            if (!Read(image + 0x53eb720, pedAgain) || pedAgain != ped || !Read(image + 0x5726100, providerAgain) || providerAgain != provider ||
                !Read(image + 0x5726120, indexAgain) || indexAgain != index || !Read(image + 0x5067940, alternateAgain) || alternateAgain != alternate ||
                !Read(ped + 0x7e0, selectorAgain) || selectorAgain != selector || !Read(image + 0x522bcf6, playerAgain) || playerAgain != playerIndex ||
                !Read(ped + 0x634, flagsAgain) || ((flagsAgain ^ pedFlags) & 0x100) || !Read(ped + 0x5fc, stateAgain) || stateAgain != pedState ||
                !Read(image + padRva + 0x30, disabledAgain) || disabledAgain != disabledControls ||
                !Pages(image + padRva + NativePadCurrentOffset, sizeof(CControllerState) * 2, true))
                return false;

            NativeMovementEligibility next{};
            next.knownMask = NativeMovementEligibilityMask;
            next.flags = static_cast<std::uint32_t>(NativeMovementEligibilityFlag::LocalIdentity);
            next.rawPedState = pedState;
            next.rawDisabledControls = disabledControls;
            // Only IDLE is admitted. Die/dead recognition does not claim full
            // task or health discovery; every other state, including vehicle
            // transitions and unknown states, is refused before synthesis/write.
            next.context.inVehicle = (pedFlags & 0x100) != 0;
            next.context.isDead = pedState == PedState::PED_DIE || pedState == PedState::PED_DEAD;
            if (!next.context.inVehicle)
                next.flags |= static_cast<std::uint32_t>(NativeMovementEligibilityFlag::OnFoot);
            if (pedState == PedState::PED_IDLE)
                next.flags |= static_cast<std::uint32_t>(NativeMovementEligibilityFlag::MovementState);
            if (!disabledControls)
                next.flags |= static_cast<std::uint32_t>(NativeMovementEligibilityFlag::ControlsEnabled);
            next.eligible = next.flags == next.knownMask;
            eligibility = next;
            identity = {ped, provider};
            return true;
        }

        bool CheckBinding(void* candidate = nullptr, bool capture = false, NativeMovementEligibility* observed = nullptr) noexcept
        {
            NativeMovementEligibility eligibility{};
            BindingIdentity           identity{};
            if (!ReadLocalEligibility(runtime.image, {runtime.ped, runtime.provider}, candidate, eligibility, identity))
                return false;
            if (!eligibility.eligible)
            {
                Fail(eligibility.context.inVehicle ? Failure::Vehicle : Failure::IneligibleContext);
                return false;
            }
            // The identity is captured before hooks are installed. Validation
            // during a session must not race producer reads by rewriting it.
            if (capture)
            {
                runtime.ped = identity.ped;
                runtime.provider = identity.provider;
            }
            if (observed)
                *observed = eligibility;
            // Ambient globals are temporarily borrowed for other peds between
            // callbacks. Only the adopted local callback may refresh active
            // producer evidence; it still executes the full guard before every
            // write and after the original call instead of trusting this cache.
            if (!capture && scope.callbackDepth && runtime.engineThread.load() == GetCurrentThreadId())
                TryPublishEligibility(eligibility, identity);
            return true;
        }

        template <std::size_t N>
        bool Pin(std::uintptr_t rva, const std::array<unsigned char, N>& expected) noexcept
        {
            return PinAt(runtime.image, rva, expected);
        }

        bool CheckEntryPins() noexcept
        {
            constexpr std::array<unsigned char, 16> control = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x20, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x48, 0x8d};
            constexpr std::array<unsigned char, 16> membership = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                                  0xec, 0x20, 0x48, 0x63, 0xda, 0x48, 0x8b, 0xf9};
            constexpr std::array<unsigned char, 14> availability = {0x8d, 0x42, 0xdb, 0x83, 0xf8, 0x01, 0x76, 0x2f, 0x80, 0x3d, 0x55, 0xc9, 0xb1, 0x04};
            constexpr std::array<unsigned char, 24> query = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89,
                                                             0x74, 0x24, 0x18, 0x48, 0x89, 0x7c, 0x24, 0x20, 0x41, 0x56, 0x41, 0x57};
            return Pin(controlRva, control) && Pin(membershipRva, membership) && Pin(availabilityRva, availability) && Pin(queryRva, query);
        }

        bool FrontendWindowFocused(HWND window) noexcept
        {
            DWORD      process = 0, foregroundProcess = 0;
            const auto foreground = GetForegroundWindow();
            return window && IsWindow(window) && GetAncestor(window, GA_ROOT) == window && GetWindowThreadProcessId(window, &process) &&
                   process == GetCurrentProcessId() && foreground && GetWindowThreadProcessId(foreground, &foregroundProcess) && foregroundProcess == process &&
                   GetAncestor(foreground, GA_ROOT) == window;
        }

        bool Fresh(ULONGLONG refreshed) noexcept
        {
            const auto now = GetTickCount64();
            if (runtime.frontendLease)
            {
                if (!runtime.frontendPolicy.Check(now, FrontendWindowFocused(runtime.frontendWindow)))
                {
                    Fail(Failure::Expired);
                    return false;
                }
                return true;
            }
            if (now - runtime.started >= maximumTrialMs)
            {
                Fail(Failure::Expired);
                return false;
            }
            if (now - refreshed > maximumInputAgeMs)
            {
                Fail(Failure::StaleInput);
                return false;
            }
            return true;
        }

        bool RestoreScope() noexcept
        {
            scope.routed = false;
            if (!runtime.transaction.Active())
                return true;
            NativeInputRouting::PadStateBytes original{}, observed{};
            if (!runtime.transaction.RestoreBytes(original) || !WritePad(original) ||
                !Read(runtime.image + padRva + NativePadCurrentOffset, observed.data(), observed.size()) || !runtime.transaction.ConfirmRestored(observed))
            {
                runtime.counters.restoreFailures.fetch_add(1);
                Fail(Failure::RestoreFailure);
                // The original snapshot remains owned for an explicit retry.
                // Keeping the module/hook state alive is safer than discarding
                // the only bytes which can put local input back.
                return false;
            }
            runtime.counters.restored.fetch_add(1);
            runtime.scopes.store(0);
            runtime.padAccess.store(0);
            std::int32_t   index = -1;
            std::uintptr_t alternate = 0;
            if (!ReadContext(index, alternate) || index != runtime.scopeProviderIndex || alternate != runtime.scopeAlternate)
                Fail(Failure::ContextChanged);
            return true;
        }

        bool PrepareScope(void* ped) noexcept
        {
            if (!runtime.active.load())
                return false;
            // CLEO and the native simulation run on different threads. A
            // callback takes one coherent publication without ever blocking
            // behind a producer which Detours may need to suspend.
            if (!TryAcquireSRWLockShared(&runtime.mailboxLock))
                return false;
            scope.frame = runtime.frame;
            const auto refreshed = runtime.refreshed;
            ReleaseSRWLockShared(&runtime.mailboxLock);
            if (!Fresh(refreshed))
                return false;
            if (!CheckBinding(ped))
            {
                Fail(Failure::BindingChanged);
                return false;
            }
            unsigned expected = 0;
            if (!runtime.padAccess.compare_exchange_strong(expected, exclusivePadAccess))
            {
                Fail(Failure::WrongThread);
                return false;
            }
            expected = 0;
            if (!runtime.scopes.compare_exchange_strong(expected, 1))
            {
                runtime.padAccess.store(0);
                runtime.counters.nested.fetch_add(1);
                Fail(Failure::NestedCallback);
                return false;
            }
            NativeInputRouting::PadStateBytes physical{}, replacement{};
            scope.generation = runtime.generation.load();
            if (scope.consumedGeneration != scope.generation)
            {
                scope.consumedGeneration = scope.generation;
                scope.consumed = scope.frame.previous;
            }
            scope.frame.previous = scope.consumed;
            if (!ReadContext(scope.providerIndex, scope.alternate) ||
                !Read(runtime.image + padRva + NativePadCurrentOffset, physical.data(), physical.size()) ||
                !runtime.transaction.Prepare(physical, scope.frame, replacement))
            {
                runtime.scopes.store(0);
                runtime.padAccess.store(0);
                Fail(Failure::ReadFailure);
                return false;
            }
            runtime.scopeGeneration = scope.generation;
            runtime.scopeProviderIndex = scope.providerIndex;
            runtime.scopeAlternate = scope.alternate;
            // Count ownership before applying: even a partial failed write has
            // a restoration obligation, which terminal metrics must include.
            runtime.counters.steps.fetch_add(1);
            if (!WritePad(replacement))
            {
                Fail(Failure::WriteFailure);
                RestoreScope();
                return false;
            }
            scope.routed = true;
            return true;
        }

        bool OwnsQuery(void* provider) noexcept
        {
            if (!scope.routed || runtime.generation.load() != scope.generation || GetCurrentThreadId() != runtime.engineThread.load())
                return false;
            std::int32_t   index = -1;
            std::uintptr_t alternate = 0;
            if (!ReadContext(index, alternate) || index != scope.providerIndex || alternate != scope.alternate)
            {
                Fail(Failure::ContextChanged);
                RestoreScope();
                return false;
            }
            // Disarming prevents the next scope. An existing native call still
            // owns its replacement bytes until restoration, so queries must
            // remain authoritative throughout that call even after a fault.
            if (reinterpret_cast<std::uintptr_t>(provider) == runtime.provider)
                return true;
            Fail(Failure::BindingChanged);
            RestoreScope();
            return false;
        }

        bool MembershipBody(void* provider, std::int32_t action)
        {
            if (OwnsQuery(provider))
                return true;
            if (GetCurrentThreadId() == runtime.engineThread.load() && runtime.transaction.Active())
                return true;
            return runtime.originalMembership(provider, action);
        }

        bool AvailabilityBody(void* provider, std::int32_t action)
        {
            if (OwnsQuery(provider))
                return true;
            if (GetCurrentThreadId() == runtime.engineThread.load() && runtime.transaction.Active())
                return true;
            return runtime.originalAvailability(provider, action);
        }

        std::int32_t QueryBody(void* provider, std::int32_t action, float* value)
        {
            if (!OwnsQuery(provider))
            {
                if (GetCurrentThreadId() == runtime.engineThread.load() && runtime.transaction.Active())
                {
                    if (value)
                        *value = 0.0f;
                    return 1;
                }
                return runtime.originalQuery(provider, action, value);
            }
            NativeInputRouting::ActionSample sample{};
            if (!NativeInputRouting::QueryAction(scope.frame, static_cast<std::uint32_t>(action), sample))
            {
                Fail(Failure::InvalidState);
                sample = {0.0f, 1};
            }
            runtime.counters.queries.fetch_add(1);
            if (action != 2 && action != 0x1e && action != 0x1f)
                runtime.counters.neutral.fetch_add(1);
            if (value)
                *value = sample.value;
            return sample.state;
        }

        void ForwardControl(void* ped)
        {
            unsigned access = runtime.padAccess.load();
            for (;;)
            {
                if (access & exclusivePadAccess)
                {
                    // Emergency collision abort: a foreign original must not
                    // consume the adopted thread's temporary global pad. Skip
                    // this one call; that thread alone restores in finally.
                    Fail(Failure::WrongThread);
                    return;
                }
                if (runtime.padAccess.compare_exchange_weak(access, access + 1))
                    break;
            }
            __try
            {
                runtime.originalControl(ped);
            }
            __finally
            {
                runtime.padAccess.fetch_sub(1);
            }
        }

        void ControlBody(void* ped)
        {
            runtime.counters.callbacks.fetch_add(1);
            const DWORD currentThread = GetCurrentThreadId();
            DWORD       engineThread = runtime.engineThread.load();
            if (!engineThread && runtime.active.load() && reinterpret_cast<std::uintptr_t>(ped) == runtime.ped)
            {
                if (!CheckBinding(ped))
                {
                    Fail(Failure::BindingChanged);
                    ForwardControl(ped);
                    return;
                }
                // Only a natural callback with the full verified local player
                // identity may establish the simulation owner. Later callbacks
                // on other threads cannot inherit its pad or provider routing.
                runtime.engineThread.compare_exchange_strong(engineThread, currentThread);
                engineThread = runtime.engineThread.load();
            }
            if (currentThread != engineThread)
            {
                if (engineThread)
                    runtime.counters.foreign.fetch_add(1);
                ForwardControl(ped);
                return;
            }
            if (runtime.transaction.Active())
            {
                runtime.counters.nested.fetch_add(1);
                Fail(Failure::NestedCallback);
                if (!RestoreScope())
                    return;
                ForwardControl(ped);
                return;
            }
            if (!runtime.active.load() || reinterpret_cast<std::uintptr_t>(ped) != runtime.ped || !PrepareScope(ped))
            {
                // A partial apply followed by failed restoration must not run
                // another native consumer against stranded replacement bytes.
                if (runtime.transaction.Active())
                    return;
                ForwardControl(ped);
                return;
            }
            // Keep structured cleanup around the real native call: early
            // returns and SEH unwinding both restore local bytes, while the
            // original exception continues to the game's own handler.
            __try
            {
                runtime.originalControl(ped);
                std::uint32_t moveState = 0;
                if (CheckBinding(ped) && Read(runtime.ped + 0x600, moveState))
                    runtime.counters.lastMoveState.store(moveState);
                else
                    Fail(Failure::BindingChanged);
            }
            __finally
            {
                if (AbnormalTermination())
                    Fail(Failure::OriginalException);
                if (RestoreScope() && runtime.active.load())
                    scope.consumed = scope.frame.current;
            }
        }

        void HookControl(void* ped)
        {
            runtime.inFlight.fetch_add(1);
            ++scope.callbackDepth;
            __try
            {
                ControlBody(ped);
            }
            __finally
            {
                --scope.callbackDepth;
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool HookMembership(void* provider, std::int32_t action)
        {
            runtime.inFlight.fetch_add(1);
            __try
            {
                return MembershipBody(provider, action);
            }
            __finally
            {
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool HookAvailability(void* provider, std::int32_t action)
        {
            runtime.inFlight.fetch_add(1);
            __try
            {
                return AvailabilityBody(provider, action);
            }
            __finally
            {
                runtime.inFlight.fetch_sub(1);
            }
        }

        std::int32_t HookQuery(void* provider, std::int32_t action, float* value)
        {
            runtime.inFlight.fetch_add(1);
            __try
            {
                return QueryBody(provider, action, value);
            }
            __finally
            {
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool ChangeHooks(bool install, bool* pending = nullptr) noexcept
        {
            std::vector<HANDLE> threads;
            const HANDLE        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snapshot == INVALID_HANDLE_VALUE)
                return false;
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            bool enumerated = Thread32First(snapshot, &entry) != FALSE;
            bool ready = enumerated;
            try
            {
                while (enumerated)
                {
                    if (entry.th32OwnerProcessID == GetCurrentProcessId() && entry.th32ThreadID != GetCurrentThreadId())
                    {
                        HANDLE thread =
                            OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                        if (!thread)
                        {
                            ready = false;
                            break;
                        }
                        try
                        {
                            threads.push_back(thread);
                        }
                        catch (...)
                        {
                            CloseHandle(thread);
                            throw;
                        }
                    }
                    enumerated = Thread32Next(snapshot, &entry) != FALSE;
                }
            }
            catch (...)
            {
                ready = false;
            }
            CloseHandle(snapshot);
            bool transaction = false;
            if (ready && DetourTransactionBegin() == NO_ERROR)
            {
                transaction = true;
                ready = DetourUpdateThread(GetCurrentThread()) == NO_ERROR;
                for (HANDLE thread : threads)
                    ready = ready && DetourUpdateThread(thread) == NO_ERROR;
                // Registration suspends each target immediately. Check again
                // after suspension: a callback could have entered between End's
                // optimistic check and this transaction. Its trampoline must
                // remain alive until that whole hook has returned.
                if (ready && !install && runtime.inFlight.load())
                {
                    if (pending)
                        *pending = true;
                    ready = false;
                }
                if (ready && !install && runtime.scopes.load())
                    ready = runtime.scopeGeneration == runtime.generation.load() && RestoreScope();
                const auto change = install ? DetourAttach : DetourDetach;
                if (ready && (install || runtime.hooked.load()))
                    ready = change(reinterpret_cast<PVOID*>(&runtime.originalControl), reinterpret_cast<PVOID>(&HookControl)) == NO_ERROR &&
                            change(reinterpret_cast<PVOID*>(&runtime.originalMembership), reinterpret_cast<PVOID>(&HookMembership)) == NO_ERROR &&
                            change(reinterpret_cast<PVOID*>(&runtime.originalAvailability), reinterpret_cast<PVOID>(&HookAvailability)) == NO_ERROR &&
                            change(reinterpret_cast<PVOID*>(&runtime.originalQuery), reinterpret_cast<PVOID>(&HookQuery)) == NO_ERROR;
                if (ready)
                {
                    ready = DetourTransactionCommit() == NO_ERROR;
                    transaction = false;
                }
            }
            if (transaction)
                DetourTransactionAbort();
            for (HANDLE thread : threads)
                CloseHandle(thread);
            return ready;
        }

        void ClearCounters() noexcept
        {
            runtime.counters.callbacks.store(0);
            runtime.counters.steps.store(0);
            runtime.counters.queries.store(0);
            runtime.counters.neutral.store(0);
            runtime.counters.restored.store(0);
            runtime.counters.restoreFailures.store(0);
            runtime.counters.foreign.store(0);
            runtime.counters.nested.store(0);
            runtime.counters.reason.store(0);
            runtime.counters.lastMoveState.store(0);
        }
    }

    bool NativeMovementInput::Begin(const MtaInputState& state) noexcept
    {
        return BeginInternal(state, nullptr);
    }

    bool NativeMovementInput::ValidateFrontendLeaseRequest(const FrontendInputLeaseRequestV1& request) noexcept
    {
        return request.size == sizeof(request) && request.version == FrontendInputLeaseVersionV1 && !request.reserved[0] && !request.reserved[1] &&
               FrontendInputLeasePolicy::Neutral(request.neutral) && FrontendWindowFocused(static_cast<HWND>(request.window));
    }

    bool NativeMovementInput::BeginFrontendLease(const FrontendInputLeaseRequestV1& request) noexcept
    {
        return ValidateFrontendLeaseRequest(request) && BeginInternal(request.neutral, request.window);
    }

    bool NativeMovementInput::BeginInternal(const MtaInputState& state, void* frontendWindow) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || runtime.hooked.load() || runtime.scopes.load() || runtime.inFlight.load() || runtime.active.load() ||
            runtime.transaction.Active() || runtime.admissionSerial.load() || runtime.generation.load() == (std::numeric_limits<std::uint64_t>::max)())
            return false;
        Admission::Token claim;
        if (!Admission::TryAcquire(Admission::Category::Input, claim))
            return false;
        ClearCounters();
        generation = runtime.generation.fetch_add(1) + 1;
        runtime.admissionSerial.store(claim.serial);
        runtime.admissionGeneration.store(generation);
        bool setupAttempted = false;
        runtime.bound.store(false);
        ClearEligibility();
        runtime.producerThread = GetCurrentThreadId();
        runtime.frontendLease = frontendWindow != nullptr;
        runtime.frontendWindow = static_cast<HWND>(frontendWindow);
        runtime.frontendPolicy.Revoke(FrontendInputLeaseReason::Stopped);
        runtime.engineThread.store(0);
        runtime.ped = runtime.provider = 0;
        bool ready = NativeInputRouting::ValidateMovementState(state);
        if (runtime.frontendLease)
            ready = ready &&
                    runtime.frontendPolicy.Begin(generation, runtime.producerThread, GetTickCount64(), FrontendWindowFocused(runtime.frontendWindow), state);
        if (!ready)
            Fail(Failure::InvalidState);
        // Wrong executables fail before pinning the module or creating hooks.
        NativeDeviceState physicalSources;
        if (ready && (!runtime.reader.Bind() || !runtime.reader.ReadDeviceState(physicalSources)))
        {
            Fail(Failure::UnsupportedBuild);
            ready = false;
        }
        runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        NativeMovementEligibility initialEligibility;
        ULONGLONG                 initialEligibilityAt = 0;
        if (ready && (!CheckEntryPins() || !CheckBinding(nullptr, true, &initialEligibility)))
        {
            Fail(Failure::BindingChanged);
            ready = false;
        }
        if (ready)
            initialEligibilityAt = GetTickCount64();
        HMODULE retained = nullptr;
        if (ready)
        {
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookControl), &retained))
            {
                Fail(Failure::HookInstall);
                ready = false;
            }
            else
                runtime.modulePinned.store(true);
        }
        if (ready)
        {
            runtime.originalControl = reinterpret_cast<ControlFunction>(runtime.image + controlRva);
            runtime.originalMembership = reinterpret_cast<MembershipFunction>(runtime.image + membershipRva);
            runtime.originalAvailability = reinterpret_cast<MembershipFunction>(runtime.image + availabilityRva);
            runtime.originalQuery = reinterpret_cast<QueryFunction>(runtime.image + queryRva);
            setupAttempted = true;
            ready = ChangeHooks(true);
            if (!ready)
                Fail(Failure::HookInstall);
        }
        if (ready)
        {
            runtime.hooked.store(true);
            runtime.bound.store(true);
            runtime.frame = state;
            runtime.started = runtime.refreshed = GetTickCount64();
            if (runtime.frontendLease && !runtime.frontendPolicy.Check(runtime.started, FrontendWindowFocused(runtime.frontendWindow), true))
            {
                // Hooks may already exist if focus left during installation.
                // Preserve their ownership for End; never admit a first scope.
                Fail(Failure::Expired);
                ready = false;
            }
            // The first producer poll can precede the first engine callback.
            // Seed from the already successful pre-install guard, preserving
            // its real observation time rather than manufacturing a fresh read.
            AcquireSRWLockExclusive(&runtime.eligibilityLock);
            runtime.eligibilityPublication = {initialEligibility, {runtime.ped, runtime.provider}, generation, initialEligibilityAt, true};
            ReleaseSRWLockExclusive(&runtime.eligibilityLock);
            runtime.active.store(ready);
        }
        else
            runtime.reader.Reset();
        ReadState(finalState);
        hasFinalState = true;
        // A failed native transaction can retain code or recovery bytes. Only
        // refusals before setup began are known safe to release without End.
        if (!ready && !setupAttempted && !runtime.hooked.load() && !runtime.scopes.load() && !runtime.inFlight.load() && !runtime.transaction.Active())
            ReleaseAdmission(generation);
        return ready;
    }

    bool NativeMovementInput::Update(const MtaInputState& state) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        // Existing action publication must not renew the optional frontend
        // lease. Its exact-neutral/generation API is the only renewal authority.
        bool ready = generation && generation == runtime.generation.load() && runtime.active.load() && !runtime.frontendLease;
        if (ready && GetCurrentThreadId() != runtime.producerThread)
        {
            Fail(Failure::WrongThread);
            ready = false;
        }
        if (ready && !NativeInputRouting::ValidateMovementState(state))
        {
            Fail(Failure::InvalidState);
            ready = false;
        }
        if (ready)
        {
            AcquireSRWLockExclusive(&runtime.mailboxLock);
            ready = Fresh(runtime.refreshed) && runtime.active.load();
            if (ready)
            {
                // Submit may overlap native simulation. Only the next coherent
                // current frame is published; previous belongs to the engine
                // consumer and cannot be replayed by a script refresh.
                runtime.frame.current = state.current;
                runtime.refreshed = GetTickCount64();
            }
            ReleaseSRWLockExclusive(&runtime.mailboxLock);
        }
        return ready;
    }

    bool NativeMovementInput::IsFrontendLease() const noexcept
    {
        return generation && generation == runtime.generation.load() && runtime.frontendLease;
    }

    bool NativeMovementInput::RenewFrontendLease(std::uint64_t requestedGeneration, const MtaInputState& state) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || !IsFrontendLease() || requestedGeneration != generation || !runtime.active.load())
            return false;
        const bool renewed =
            runtime.frontendPolicy.Renew(requestedGeneration, GetCurrentThreadId(), GetTickCount64(), FrontendWindowFocused(runtime.frontendWindow), state);
        if (!renewed && runtime.frontendPolicy.Revoked())
            Fail(Failure::Expired);
        return renewed;
    }

    bool NativeMovementInput::ReadFrontendLease(FrontendInputLeaseStateV1& state) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || !IsFrontendLease() || GetCurrentThreadId() != runtime.producerThread)
            return false;
        // Reading may discover a stall before the next game callback. It can
        // revoke, never renew; already restored native bytes remain physical.
        if (runtime.active.load() && !runtime.frontendPolicy.Check(GetTickCount64(), FrontendWindowFocused(runtime.frontendWindow), true))
            Fail(Failure::Expired);
        auto next = runtime.frontendPolicy.Read();
        next.generation = generation;
        next.producerThread = runtime.producerThread;
        next.active = next.active && runtime.active.load();
        next.bound = runtime.bound.load();
        state = next;
        return true;
    }

    bool NativeMovementInput::EndFrontendLease(std::uint64_t requestedGeneration) noexcept
    {
        if (!IsFrontendLease() || !requestedGeneration || requestedGeneration != generation)
            return false;
        return End();
    }

    bool NativeMovementInput::End() noexcept
    {
        if (!generation || generation != runtime.generation.load())
            return true;
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        if (!generation || generation != runtime.generation.load())
        {
            return true;
        }
        runtime.active.store(false);
        ClearEligibility();
        if (scope.callbackDepth || GetCurrentThreadId() != runtime.producerThread)
        {
            Fail(scope.callbackDepth ? Failure::ScopeActive : Failure::WrongThread);
            return false;
        }
        if (runtime.frontendLease)
            runtime.frontendPolicy.Revoke(FrontendInputLeaseReason::Stopped);
        // Busy is ordinary asynchronous cleanup, not a permanent trial fault.
        // The producer retries after native finally has released its scope.
        if (runtime.inFlight.load())
            return false;
        bool       pending = false;
        const bool clean = ((!runtime.hooked.load() && !runtime.scopes.load()) || ChangeHooks(false, &pending)) && !runtime.transaction.Active();
        if (clean)
        {
            runtime.hooked.store(false);
            runtime.bound.store(false);
            runtime.reader.Reset();
        }
        else if (!pending && !runtime.counters.reason.load())
            Fail(Failure::HookRemove);
        ReadState(finalState);
        hasFinalState = true;
        return clean && ReleaseAdmission(generation);
    }

    bool NativeMovementInput::IsBound() const noexcept
    {
        return generation && generation == runtime.generation.load() && runtime.bound.load();
    }

    bool NativeMovementInput::IsActive() const noexcept
    {
        return IsBound() && runtime.active.load();
    }

    bool NativeMovementInput::ReadEligibility(NativeMovementEligibility& eligibility) const noexcept
    {
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        // Active producers cannot interpret another ped's borrowed global pad
        // context as local eligibility. Consume only recent, identity-bound
        // evidence from this generation's seed or authoritative engine callback.
        // ApiOperation excludes Begin/End and their Detours transactions while
        // the short shared-lock copy runs; the callback publisher never blocks.
        if (generation)
            return ReadPublishedEligibility(generation, eligibility);
        // Before Begin the adapter requires its exact-image reader binding.
        // This admission observation installs no hooks and owns no old cache.
        const auto      image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        BindingIdentity identity{};
        return ReadLocalEligibility(image, {}, nullptr, eligibility, identity);
    }

    void NativeMovementInput::ReadState(NativeMovementTrialState& state) const noexcept
    {
        state = hasFinalState ? finalState : NativeMovementTrialState{};
        // A successful PIN is process lifetime state even when installation
        // later fails or the adapter releases all trial ownership.
        state.modulePinned = runtime.modulePinned.load();
        if (!generation || generation != runtime.generation.load())
            return;
        state.bound = IsBound();
        state.active = IsActive();
        // Read completed effects before their monotone prerequisites. A
        // callback may advance between loads, but cannot make a snapshot claim
        // more restores than scopes, or more neutral routes than queries.
        state.restoredSteps = runtime.counters.restored.load();
        state.scopedSteps = runtime.counters.steps.load();
        state.neutralQueries = runtime.counters.neutral.load();
        state.routedQueries = runtime.counters.queries.load();
        state.restoreFailures = runtime.counters.restoreFailures.load();
        state.foreignThreadCallbacks = runtime.counters.foreign.load();
        state.nestedCallbacks = runtime.counters.nested.load();
        state.callbackCount = runtime.counters.callbacks.load();
        state.failureReason = runtime.counters.reason.load();
        state.lastMoveState = runtime.counters.lastMoveState.load();
    }
}
