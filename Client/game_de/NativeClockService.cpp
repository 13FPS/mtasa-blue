/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded native ownership for MTA's SDK clock implementation
 *****************************************************************************/

#include "NativeClockService.h"
#include "CClockDE.h"
#include "NativeClockProfile.h"
#include "NativeClockMemory.h"
#include "NativeGameTimingAccess.h"
#include "NativeGameTimingProfile.h"
#include "NativeGamePauseProfile.h"
#include "NativeApplicationFrameProfile.h"
#include "NativeClassChainAccess.h"
#include "NativeImageAccess.h"
#include "NativeServiceAdmission.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <detours.h>

#if !defined(DETOURS_MTA_VM_TRANSACTION_NODES)
    #error The native clock requires Detours externally-owned suspension support.
#endif

#pragma intrinsic(_ReturnAddress)

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeClockProfile;
        namespace Image = NativeImageAccess;
        namespace Admission = NativeServiceAdmission;
        namespace TimingProfile = NativeGameTimingProfile;
        namespace PauseProfile = NativeGamePauseProfile;
        namespace FrameProfile = NativeApplicationFrameProfile;
        namespace UiMapProfile = NativeUiClassMapProfile;
        using ContinuationFunction = void (*)();
        using ApplicationFrameFunction = FrameProfile::FrameFunction;
        constexpr std::size_t    imageSize = Profile::imageSize;
        constexpr std::uintptr_t hoursRva = Profile::hoursRva, minutesRva = Profile::minutesRva, secondsRva = Profile::secondsRva;
        constexpr std::uintptr_t lastChangeRva = Profile::lastChangeRva, nativeTimerRva = Profile::nativeTimerRva,
                                 minuteDurationRva = Profile::minuteDurationRva;
        constexpr std::uintptr_t atomicPatchRva = Profile::patchRva & ~std::uintptr_t{7};
        constexpr std::size_t    atomicPatchOffset = Profile::patchRva - atomicPatchRva;
        static_assert(atomicPatchOffset + Profile::originalPatch.size() <= sizeof(LONG64));

        class NativeAccess final : public NativeClockAccess, public NativeGameTimingAccess
        {
        public:
            ClockResult Read(ClockValues& values) noexcept override;
            ClockResult Set(std::uint8_t hour, std::uint8_t minute) noexcept override;
            ClockResult Freeze(bool frozen) noexcept override;
            ClockResult Frozen(bool& frozen) const noexcept override;
            ClockResult SetTimeScale(float value) noexcept override;
            ClockResult SetMinuteDuration(std::uint32_t value) noexcept override;
            ClockResult Pause(bool value) noexcept override;
        };

        struct Counters
        {
            std::atomic<std::uint64_t> callbacks{0}, qualified{0}, foreign{0}, nested{0};
            std::atomic<std::uint64_t> reads{0}, sets{0}, freezes{0}, unfreezes{0}, resets{0};
            std::atomic<std::uint64_t> renewals{0}, restoreAttempts{0}, restoreFailures{0};
        };

        struct PeerThread
        {
            HANDLE handle = nullptr;
            bool   suspended = false;
        };

        struct ClockPublication
        {
            std::atomic<std::uint64_t> version{0};
            std::atomic<std::uint32_t> hour{0}, minute{0}, seconds{0}, nativeTimer{0}, lastMinuteChange{0}, millisecondsPerGameMinute{0};
            std::atomic<std::uint64_t> lastCompletedSequence{0};
            std::atomic<ClockResult>   lastCommandResult{ClockResult::NotInitialized};
        };

        struct Runtime
        {
            SRWLOCK           mailboxLock = SRWLOCK_INIT;
            std::atomic<bool> apiBusy{false}, bound{false}, hooked{false}, active{false}, ready{false}, stopping{false}, restored{true};
            std::atomic<bool> modulePinned{false}, captured{false}, patchInstalled{false}, patchRecovery{false}, dataRecovery{false};
            std::atomic<bool> peerBusy{false}, peersRetained{false};
            std::atomic<bool> nativeTouched{false};
            // Timing-only writes own their own originals. They must never
            // trigger the displayed-clock rewind used by a real CClock Set.
            std::atomic<unsigned>      timingCaptured{0}, timingDirty{0}, timingRecovery{0};
            std::atomic<std::uint64_t> timingGeneration{0};
            float                      originalTimeScale = 0;
            std::uint32_t              originalMinuteDuration = 0;
            std::uint8_t               originalUserPause = 0;
            bool                       timingMode = false;
            std::atomic<bool>          timerHooked{false}, timerReady{false};
            GameTimingHandler          timingHandler = nullptr;
            void*                      timingContext = nullptr;
            bool                       frameMode = false;
            std::atomic<bool>          frameHooked{false};
            ApplicationFrameHandler    frameHandler = nullptr;
            void*                      frameContext = nullptr;
            ApplicationFrameFunction   originalFrame = nullptr;
            std::atomic<unsigned>      inFlight{0};
            std::atomic<std::uint32_t> retainedPeerThreads{0};
            std::atomic<DWORD>         engineThread{0};
            std::atomic<std::uint64_t> generation{0}, refreshed{0}, lastSubmitted{0}, pendingSequence{0};
            std::atomic<std::uint64_t> admissionGeneration{0}, admissionSerial{0};
            std::atomic<std::uint64_t> lastCompletedSequence{0};
            std::atomic<ClockResult>   failure{ClockResult::Ok};
            std::atomic<ClockResult>   lastCommandResult{ClockResult::NotInitialized};
            DWORD                      producerThread = 0;
            std::uintptr_t             image = 0;
            ULONGLONG                  started = 0;
            std::uint32_t              maximumDuration = 0;
            ClockCommandV1             pending;
            bool                       hasPending = false;
            ClockValues                original, observed;
            ClockPublication           publishedClock;
            ContinuationFunction       originalContinuation = nullptr;
            ContinuationFunction       originalTimer = nullptr;
            DWORD                      patchProtection = 0;
            std::atomic<bool>          protectionPending{false};
            std::vector<PeerThread>    recoveryPeers;
            NativeAccess               access;
            CClockDE                   clock{access};
            Counters                   counters;
        };

        // A failed code/data restoration cannot discard its owner on DLL
        // shutdown. The pinned module and process-lifetime state retain the
        // trampoline, original bytes and clock snapshot until cleanup succeeds.
        Runtime                    runtime;
        thread_local unsigned      callbackDepth = 0;
        thread_local std::uint64_t timingCallbackGeneration = 0;
        thread_local bool          timingTimerScope = false;
        thread_local std::uint64_t frameCallbackGeneration = 0;
        thread_local void*         frameCallbackReceiver = nullptr;
        constexpr unsigned         timingScaleBit = 1, timingMinuteBit = 2, timingPauseBit = 4;

        bool ReleaseAdmission(std::uint64_t generation) noexcept
        {
            const auto owner = runtime.admissionGeneration.load();
            if (!owner)
                return true;
            if (!generation || generation != runtime.generation.load() || generation != owner)
                return false;
            if (!Admission::Release({Admission::Category::Clock, runtime.admissionSerial.load()}))
                return false;
            runtime.admissionSerial.store(0);
            runtime.admissionGeneration.store(0);
            return true;
        }

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

        void Stop(ClockResult reason) noexcept
        {
            runtime.active.store(false);
            runtime.stopping.store(true);
            if (reason != ClockResult::Ok && reason != ClockResult::Stopped)
            {
                auto expected = ClockResult::Ok;
                runtime.failure.compare_exchange_strong(expected, reason);
            }
        }

        bool Fresh() noexcept
        {
            const auto now = GetTickCount64();
            const auto refreshed = runtime.refreshed.load();
            return now >= runtime.started && now - runtime.started <= runtime.maximumDuration && now >= refreshed && now - refreshed <= ClockServiceLeaseMsV1;
        }

        template <typename T>
        bool ReadValue(std::uintptr_t rva, T& value) noexcept
        {
            return Image::ReadImage(runtime.image, runtime.image + rva, &value, sizeof(value), true);
        }

        template <typename T>
        bool WriteValue(std::uintptr_t rva, const T& value) noexcept
        {
            SIZE_T copied = 0;
            T      observed{};
            return Image::ImagePages(runtime.image, runtime.image + rva, sizeof(value), true) &&
                   WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(runtime.image + rva), &value, sizeof(value), &copied) &&
                   copied == sizeof(value) && ReadValue(rva, observed) && observed == value;
        }

        template <std::size_t N>
        bool CheckCode(std::uintptr_t rva, const std::array<unsigned char, N>& expected) noexcept
        {
            std::array<unsigned char, N> actual{};
            return Image::ReadImage(runtime.image, runtime.image + rva, actual.data(), actual.size(), false, true) && actual == expected;
        }

        bool CheckImage(bool uiClassMap = false) noexcept
        {
            if (!runtime.image || runtime.image > (std::numeric_limits<std::uintptr_t>::max)() - imageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(runtime.image, runtime.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > imageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!Image::ReadImage(runtime.image, runtime.image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != imageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > imageSize || sectionsBytes > imageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(runtime.image, runtime.image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, bool executable)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, IMAGE_SCN_MEM_READ | (executable ? IMAGE_SCN_MEM_EXECUTE : IMAGE_SCN_MEM_WRITE),
                                               executable ? 0 : IMAGE_SCN_MEM_EXECUTE))
                        return true;
                return false;
            };
            if (runtime.frameMode)
            {
                if (!contains(FrameProfile::rootRva, sizeof(std::uintptr_t), false) || !contains(FrameProfile::stateRva, sizeof(std::uint32_t), false))
                    return false;
                for (const auto& pin : FrameProfile::pins)
                {
                    bool code = false;
                    for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                        code |= Image::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE);
                    if (!code)
                        return false;
                }
                if (uiClassMap)
                    for (const auto& pin : UiMapProfile::pins)
                    {
                        bool present = false;
                        for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                            present |= Image::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                                                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE));
                        if (!present)
                            return false;
                    }
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], FrameProfile::slotRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ,
                                               IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                        return true;
                return false;
            }
            return contains(Profile::clockCodeRva, Profile::clockCode.size(), true) && contains(Profile::hookEntryRva, Profile::hookEntry.size(), true) &&
                   contains(Profile::hookTailRva, Profile::hookTail.size(), true) && contains(Profile::clockCallRva, Profile::clockCall.size(), true) &&
                   contains(Profile::frameCallRva, Profile::frameCall.size(), true) && contains(Profile::getterCaseRva, Profile::getterCase.size(), true) &&
                   contains(hoursRva, 1, false) && contains(minutesRva, 1, false) && contains(secondsRva, 2, false) && contains(lastChangeRva, 4, false) &&
                   contains(nativeTimerRva, 4, false) && contains(minuteDurationRva, 4, false) &&
                   (!runtime.timingMode ||
                    (contains(PauseProfile::entryRva, PauseProfile::entry.size(), true) && contains(PauseProfile::tailRva, PauseProfile::tail.size(), true) &&
                     contains(PauseProfile::pauseReadsRva, PauseProfile::pauseReads.size(), true) &&
                     contains(PauseProfile::pauseBranchRva, PauseProfile::pauseBranch.size(), true) &&
                     contains(PauseProfile::frameRva, PauseProfile::frame.size(), true) &&
                     contains(PauseProfile::returnRva, PauseProfile::returnCode.size(), true) &&
                     contains(PauseProfile::normalCallRva, PauseProfile::normalCall.size(), true) &&
                     contains(PauseProfile::alternateCallRva, PauseProfile::alternateCall.size(), true) &&
                     contains(PauseProfile::gameplayTailRva, PauseProfile::gameplayTail.size(), true) && contains(PauseProfile::userPauseRva, 1, false)));
        }

        bool UiMapPinPages(std::uintptr_t address, std::size_t bytes, bool code) noexcept
        {
            if (!Image::ImagePages(runtime.image, address, bytes, false, code))
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION region{};
                const DWORD              expected = code ? PAGE_EXECUTE_READ : PAGE_READONLY;
                if (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region) || (region.Protect & 0xff) != expected)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
                if (region.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + region.RegionSize <= address)
                    return false;
                address = base + region.RegionSize < end ? base + region.RegionSize : end;
            }
            return true;
        }

        bool CheckBinding(bool entryBeforeHook = false, bool allowRecoveryPatch = false, bool uiClassMap = false) noexcept
        {
            if (runtime.frameMode)
            {
                if (!CheckImage(uiClassMap))
                    return false;
                for (const auto& pin : FrameProfile::pins)
                {
                    // Only our own function-entry detour may differ. Retain
                    // every caller, dispatch, constructor and tail identity pin.
                    if (pin.rva == FrameProfile::entryRva && runtime.frameHooked.load() && !entryBeforeHook)
                        continue;
                    std::array<unsigned char, FrameProfile::maximumPinBytes> actual{};
                    if (pin.size > actual.size() || !Image::ReadImage(runtime.image, runtime.image + pin.rva, actual.data(), pin.size, false, true) ||
                        std::memcmp(actual.data(), pin.bytes, pin.size) != 0)
                        return false;
                }
                std::uintptr_t target = 0;
                if (!Image::ReadImage(runtime.image, runtime.image + FrameProfile::slotRva, &target, sizeof(target)) ||
                    target != runtime.image + FrameProfile::entryRva)
                    return false;
                // Normal frame observations remain independent of dictionary
                // support; only this requested lookup qualifies all its pins.
                if (uiClassMap)
                    for (const auto& pin : UiMapProfile::pins)
                    {
                        std::array<unsigned char, UiMapProfile::maximumPinBytes> actual{};
                        if (pin.size > actual.size() || !UiMapPinPages(runtime.image + pin.rva, pin.size, pin.code) ||
                            !Image::ReadImage(runtime.image, runtime.image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                            std::memcmp(actual.data(), pin.bytes, pin.size) != 0)
                            return false;
                    }
                return true;
            }
            if (!CheckImage() || !CheckCode(Profile::hookTailRva, Profile::hookTail) || !CheckCode(Profile::clockCallRva, Profile::clockCall) ||
                !CheckCode(Profile::frameCallRva, Profile::frameCall) || !CheckCode(Profile::getterCaseRva, Profile::getterCase) ||
                (entryBeforeHook && !CheckCode(Profile::hookEntryRva, Profile::hookEntry)))
                return false;
            if (runtime.timingMode &&
                ((!runtime.timerHooked.load() && !CheckCode(PauseProfile::entryRva, PauseProfile::entry)) ||
                 !CheckCode(PauseProfile::tailRva, PauseProfile::tail) || !CheckCode(PauseProfile::pauseReadsRva, PauseProfile::pauseReads) ||
                 !CheckCode(PauseProfile::pauseBranchRva, PauseProfile::pauseBranch) || !CheckCode(PauseProfile::frameRva, PauseProfile::frame) ||
                 !CheckCode(PauseProfile::returnRva, PauseProfile::returnCode) || !CheckCode(PauseProfile::normalCallRva, PauseProfile::normalCall) ||
                 !CheckCode(PauseProfile::alternateCallRva, PauseProfile::alternateCall) ||
                 !CheckCode(PauseProfile::gameplayTailRva, PauseProfile::gameplayTail)))
                return false;
            std::array<unsigned char, Profile::clockCode.size()> actual{};
            if (!Image::ReadImage(runtime.image, runtime.image + Profile::clockCodeRva, actual.data(), actual.size(), false, true))
                return false;
            auto           expected = Profile::clockCode;
            constexpr auto patchOffset = Profile::patchRva - Profile::clockCodeRva;
            if (allowRecoveryPatch && runtime.patchRecovery.load())
                std::memcpy(actual.data() + patchOffset, expected.data() + patchOffset, Profile::originalPatch.size());
            else if (runtime.patchInstalled.load())
                std::memcpy(expected.data() + patchOffset, Profile::frozenPatch.data(), Profile::frozenPatch.size());
            return actual == expected;
        }

        bool RawRead(ClockValues& output) noexcept
        {
            return NativeClockMemory::Read(runtime.image, output);
        }

        bool CheckTimingBinding() noexcept
        {
            // Mutations qualify their actual init/update/script dataflow as
            // well as the existing exact-image clock binding and page checks.
            return CheckCode(TimingProfile::initializeRva, TimingProfile::initialize) &&
                   CheckCode(TimingProfile::elapsedMultiplierRva, TimingProfile::elapsedMultiplier) &&
                   CheckCode(TimingProfile::scriptScaleRva, TimingProfile::scriptScale) &&
                   Image::ImagePages(runtime.image, runtime.image + TimingProfile::timeScaleRva, sizeof(float), true);
        }

        ClockResult CheckOwner() noexcept
        {
            if (runtime.frameMode || !runtime.bound.load() || !runtime.ready.load())
                return ClockResult::NotInitialized;
            if (!callbackDepth || GetCurrentThreadId() != runtime.engineThread.load())
                return ClockResult::WrongThread;
            if (!runtime.active.load() || runtime.stopping.load() || !Fresh())
                return ClockResult::Expired;
            if (runtime.patchRecovery.load())
                return ClockResult::PatchFailure;
            return CheckBinding() ? ClockResult::Ok : ClockResult::BindingChanged;
        }

        ClockResult CheckTimingOwner() noexcept
        {
            const auto owner = timingTimerScope && runtime.timingMode && runtime.timerHooked.load() && runtime.timerReady.load()
                                   ? (!runtime.bound.load()                                           ? ClockResult::NotInitialized
                                      : GetCurrentThreadId() != runtime.engineThread.load()           ? ClockResult::WrongThread
                                      : !runtime.active.load() || runtime.stopping.load() || !Fresh() ? ClockResult::Expired
                                      : runtime.patchRecovery.load()                                  ? ClockResult::PatchFailure
                                      : CheckBinding()                                                ? ClockResult::Ok
                                                                                                      : ClockResult::BindingChanged)
                                   : CheckOwner();
            if (owner != ClockResult::Ok)
                return owner;
            if (callbackDepth != 1)
                return ClockResult::WrongThread;
            const auto current = runtime.generation.load();
            if (!timingCallbackGeneration || timingCallbackGeneration != current || runtime.admissionGeneration.load() != current ||
                !runtime.admissionSerial.load() || (runtime.timingGeneration.load() && runtime.timingGeneration.load() != current))
                return ClockResult::OwnerConflict;
            return CheckTimingBinding() ? ClockResult::Ok : ClockResult::BindingChanged;
        }

        bool ResumePeer(PeerThread& thread) noexcept
        {
            if (thread.suspended && ResumeThread(thread.handle) == static_cast<DWORD>(-1) && WaitForSingleObject(thread.handle, 0) != WAIT_OBJECT_0)
                return false;
            thread.suspended = false;
            if (thread.handle)
                CloseHandle(thread.handle);
            thread.handle = nullptr;
            return true;
        }

        bool RetryPeerHolds() noexcept
        {
            if (runtime.peerBusy.exchange(true))
                return false;
            bool          clean = true;
            std::uint32_t retained = 0;
            for (auto& thread : runtime.recoveryPeers)
                if (thread.handle && !ResumePeer(thread))
                {
                    clean = false;
                    ++retained;
                }
            if (clean)
                runtime.recoveryPeers.clear();
            runtime.peersRetained.store(!clean);
            runtime.retainedPeerThreads.store(retained);
            runtime.peerBusy.store(false);
            return clean;
        }

        bool RetryPeerHoldsBounded() noexcept
        {
            // The producer may be one of the parked peers. Retry here before
            // native continuation can need its allocator/GUI locks; permanent
            // OS failure remains explicit retained recovery, never success.
            for (unsigned attempt = 0; attempt != 8 && runtime.peersRetained.load(); ++attempt)
                RetryPeerHolds();
            return !runtime.peersRetained.load();
        }

        class PeerHold
        {
        public:
            ~PeerHold() { Release(); }

            bool Prepare() noexcept
            {
                if (runtime.peerBusy.exchange(true))
                    return false;
                ownsGate = true;
                if (runtime.peersRetained.load() || !runtime.recoveryPeers.empty())
                    return false;
                // Any old empty capacity is retired before parking peers. A
                // failed resume can then transfer handles with a no-allocation
                // swap, even when the suspended producer cannot run cleanup.
                std::vector<PeerThread>{}.swap(runtime.recoveryPeers);
                const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
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
                            const auto handle =
                                OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION | SYNCHRONIZE, FALSE,
                                           entry.th32ThreadID);
                            if (!handle)
                            {
                                ready = false;
                                break;
                            }
                            try
                            {
                                threads.push_back({handle, false});
                            }
                            catch (...)
                            {
                                CloseHandle(handle);
                                throw;
                            }
                        }
                        SetLastError(ERROR_SUCCESS);
                        enumerated = Thread32Next(snapshot, &entry) != FALSE;
                        if (!enumerated && GetLastError() != ERROR_NO_MORE_FILES)
                            ready = false;
                    }
                }
                catch (...)
                {
                    ready = false;
                }
                CloseHandle(snapshot);
                return ready;
            }

            bool Suspend() noexcept
            {
                // Allocate/open every record first. Once peers are parked this
                // path takes no heap or mailbox lock which a parked peer owns.
                for (auto& thread : threads)
                {
                    if (SuspendThread(thread.handle) == static_cast<DWORD>(-1))
                        return false;
                    thread.suspended = true;
                }
                for (const auto& thread : threads)
                {
                    CONTEXT context{};
                    context.ContextFlags = CONTEXT_CONTROL;
                    if (!GetThreadContext(thread.handle, &context))
                        return false;
                    // A raw patch has no Detours relocation map. Reject a peer
                    // anywhere in the clock span, including the old short JG
                    // instruction inside the proposed five-byte replacement.
                    // Known script/init/serialization writer windows are also
                    // excluded; this is not ownership of arbitrary mod writers.
                    if ((context.Rip >= runtime.image + Profile::clockCodeRva && context.Rip < runtime.image + Profile::continuationRva) ||
                        (context.Rip >= runtime.image + 0x112ce10 && context.Rip < runtime.image + 0x112cee6) ||
                        (context.Rip >= runtime.image + 0x1146790 && context.Rip < runtime.image + 0x1147045) ||
                        (context.Rip >= runtime.image + 0x114a1b0 && context.Rip < runtime.image + 0x114a92f) ||
                        (context.Rip >= runtime.image + 0x10d9880 && context.Rip < runtime.image + 0x10d9900) ||
                        (context.Rip >= runtime.image + 0x12c9400 && context.Rip < runtime.image + 0x12c9640) ||
                        (context.Rip >= runtime.image + TimingProfile::initializeRva &&
                         context.Rip < runtime.image + TimingProfile::initializeRva + TimingProfile::initialize.size()) ||
                        (context.Rip >= runtime.image + TimingProfile::scriptScaleRva &&
                         context.Rip < runtime.image + TimingProfile::scriptScaleRva + TimingProfile::scriptScale.size()) ||
                        (context.Rip >= runtime.image + 0x1186920 && context.Rip < runtime.image + 0x1186bbc))
                        return false;
                    // Chained unwind fragments belong to one application
                    // method. A peer anywhere in its body can return through
                    // the entry trampoline, so include the entire method.
                    if (runtime.frameMode && context.Rip >= runtime.image + FrameProfile::entryRva &&
                        context.Rip < runtime.image + FrameProfile::functionEndRva)
                        return false;
                }
                return true;
            }

            bool Release() noexcept
            {
                if (!ownsGate)
                    return true;
                bool          clean = true;
                std::uint32_t retained = 0;
                for (auto& thread : threads)
                    if (thread.handle && !ResumePeer(thread))
                    {
                        clean = false;
                        ++retained;
                    }
                if (!clean)
                {
                    runtime.recoveryPeers.swap(threads);
                    runtime.peersRetained.store(true);
                    runtime.retainedPeerThreads.store(retained);
                    Stop(ClockResult::RestoreFailure);
                }
                runtime.peerBusy.store(false);
                ownsGate = false;
                return clean;
            }

            bool RegisterDetours() const noexcept
            {
                for (const auto& thread : threads)
                    if (!thread.suspended || DetourUpdateThreadExternallySuspended(thread.handle) != NO_ERROR)
                        return false;
                return true;
            }

        private:
            std::vector<PeerThread> threads;
            bool                    ownsGate = false;
        };

        bool ReadPatch(std::array<unsigned char, 5>& bytes) noexcept
        {
            return Image::ReadImage(runtime.image, runtime.image + Profile::patchRva, bytes.data(), bytes.size(), false, true);
        }

        bool ExchangeCode(LONG64 expected, LONG64 desired) noexcept
        {
            __try
            {
                return InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(runtime.image + atomicPatchRva), desired, expected) == expected;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool WritePatch(const std::array<unsigned char, 5>& bytes) noexcept
        {
            if (bytes != Profile::originalPatch && bytes != Profile::frozenPatch)
                return false;
            // One aligned atomic store changes the five logical branch bytes
            // while preserving the preceding three instruction bytes. A failed
            // store cannot leave a torn JMP which a later frame could execute.
            std::array<unsigned char, sizeof(LONG64)> before{}, original{}, frozen{}, desired{}, observed{};
            constexpr auto                            codeOffset = atomicPatchRva - Profile::clockCodeRva;
            std::memcpy(original.data(), Profile::clockCode.data() + codeOffset, original.size());
            frozen = original;
            std::memcpy(frozen.data() + atomicPatchOffset, Profile::frozenPatch.data(), Profile::frozenPatch.size());
            if (!Image::ReadImage(runtime.image, runtime.image + atomicPatchRva, before.data(), before.size(), false, true) ||
                (before != original && before != frozen))
                return false;
            desired = before;
            std::memcpy(desired.data() + atomicPatchOffset, bytes.data(), bytes.size());
            LONG64 expectedValue = 0, desiredValue = 0;
            std::memcpy(&expectedValue, before.data(), sizeof(expectedValue));
            std::memcpy(&desiredValue, desired.data(), sizeof(desiredValue));
            return ExchangeCode(expectedValue, desiredValue) &&
                   Image::ReadImage(runtime.image, runtime.image + atomicPatchRva, observed.data(), observed.size(), false, true) && observed == desired &&
                   FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(runtime.image + atomicPatchRva), desired.size());
        }

        ClockResult PatchWhileHeld(bool frozen) noexcept
        {
            if (!runtime.patchRecovery.load() && !runtime.protectionPending.load() && runtime.patchInstalled.load() == frozen)
                return ClockResult::Ok;
            if (!Profile::clockBoundaryReviewed)
                return ClockResult::UnsupportedBuild;
            std::array<unsigned char, 5> before{};
            if (!ReadPatch(before) ||
                (!runtime.patchRecovery.load() && before != (runtime.patchInstalled.load() ? Profile::frozenPatch : Profile::originalPatch)))
                return ClockResult::BindingChanged;
            DWORD oldProtection = 0;
            runtime.nativeTouched.store(true);
            if (!VirtualProtect(reinterpret_cast<void*>(runtime.image + atomicPatchRva), sizeof(LONG64), PAGE_EXECUTE_READWRITE, &oldProtection))
                return ClockResult::PatchFailure;
            if (!runtime.protectionPending)
                runtime.patchProtection = oldProtection;
            runtime.protectionPending = true;
            runtime.patchRecovery.store(true);
            const auto& desired = frozen ? Profile::frozenPatch : Profile::originalPatch;
            bool        changed = WritePatch(desired);
            bool        cacheConfirmed = changed;
            if (changed)
                runtime.patchInstalled.store(frozen);
            else if (WritePatch(Profile::originalPatch))
            {
                runtime.patchInstalled.store(false);
                cacheConfirmed = true;
            }
            DWORD      ignored = 0;
            const bool protectedAgain =
                VirtualProtect(reinterpret_cast<void*>(runtime.image + atomicPatchRva), sizeof(LONG64), runtime.patchProtection, &ignored) != FALSE;
            if (protectedAgain)
                runtime.protectionPending = false;
            std::array<unsigned char, 5> after{};
            const bool                   known = ReadPatch(after) && after == (runtime.patchInstalled.load() ? Profile::frozenPatch : Profile::originalPatch);
            if (known && protectedAgain && cacheConfirmed)
                runtime.patchRecovery.store(false);
            return changed && known && protectedAgain && cacheConfirmed ? ClockResult::Ok : ClockResult::PatchFailure;
        }

        ClockResult ChangeFreezePatch(bool frozen) noexcept
        {
            if (!callbackDepth || GetCurrentThreadId() != runtime.engineThread.load())
                return ClockResult::WrongThread;
            if (!CheckBinding(false, true))
                return ClockResult::BindingChanged;
            // Read/Set-only owners can always acknowledge an unchanged native
            // branch. An unreviewed boundary never acquires write permission.
            if (!runtime.patchRecovery.load() && !runtime.protectionPending.load() && runtime.patchInstalled.load() == frozen)
                return ClockResult::Ok;
            if (!Profile::clockBoundaryReviewed)
                return ClockResult::UnsupportedBuild;
            PeerHold peers;
            if (!peers.Prepare() || !peers.Suspend())
                return ClockResult::Busy;
            const auto owner = CheckOwner();
            const auto result = owner == ClockResult::Ok ? PatchWhileHeld(frozen) : owner;
            const auto completed = peers.Release() ? result : ClockResult::RestoreFailure;
            if (completed == ClockResult::PatchFailure || completed == ClockResult::RestoreFailure)
                Stop(completed);
            return completed;
        }

        ClockResult NativeAccess::Read(ClockValues& output) noexcept
        {
            const auto status = CheckOwner();
            if (status != ClockResult::Ok)
                return status;
            ClockValues value;
            if (!RawRead(value))
                return ClockResult::ReadFailure;
            output = value;
            runtime.observed = value;
            runtime.counters.reads.fetch_add(1);
            return ClockResult::Ok;
        }

        ClockResult NativeAccess::Set(std::uint8_t hour, std::uint8_t minute) noexcept
        {
            const auto status = CheckOwner();
            if (status != ClockResult::Ok)
                return status;
            if (hour > 23 || minute > 60)
                return ClockResult::InvalidArgument;
            std::uint32_t timer = 0;
            PeerHold      peers;
            if (!peers.Prepare() || !peers.Suspend())
                return ClockResult::Busy;
            const auto owner = CheckOwner();
            if (owner != ClockResult::Ok)
                return owner;
            if (!ReadValue(nativeTimerRva, timer))
                return ClockResult::ReadFailure;
            // Classic CClock writes these three values only. Calling DE's
            // normalized script setter would also mutate seconds/calendar.
            runtime.nativeTouched.store(true);
            runtime.dataRecovery.store(true);
            const bool written = WriteValue(hoursRva, hour) && WriteValue(minutesRva, minute) && WriteValue(lastChangeRva, timer);
            const bool released = peers.Release();
            if (!written || !released)
            {
                const auto failure = released ? ClockResult::WriteFailure : ClockResult::RestoreFailure;
                Stop(failure);
                return failure;
            }
            runtime.counters.sets.fetch_add(1);
            return ClockResult::Ok;
        }

        ClockResult NativeAccess::Freeze(bool frozen) noexcept
        {
            const auto status = CheckOwner();
            if (status != ClockResult::Ok)
                return status;
            runtime.nativeTouched.store(true);
            const auto changed = ChangeFreezePatch(frozen);
            if (changed == ClockResult::Ok)
            {
                if (frozen)
                    runtime.counters.freezes.fetch_add(1);
                else
                    runtime.counters.unfreezes.fetch_add(1);
            }
            return changed;
        }

        ClockResult NativeAccess::Frozen(bool& frozen) const noexcept
        {
            const auto status = CheckOwner();
            if (status != ClockResult::Ok)
                return status;
            frozen = runtime.patchInstalled.load();
            return ClockResult::Ok;
        }

        template <typename T>
        bool TimingValueValid(const T& value) noexcept
        {
            if constexpr (std::is_same_v<T, float>)
                return std::isfinite(value);
            else if constexpr (std::is_same_v<T, std::uint8_t>)
                return value <= 1;
            else
                return value && value <= 3600000;
        }

        template <typename T>
        bool TimingReadback(std::uintptr_t rva, const T& expected) noexcept
        {
            T observed{};
            // In particular, preserve the native float's original signed-zero
            // bits instead of accepting a merely numerically equal readback.
            return ReadValue(rva, observed) && std::memcmp(&observed, &expected, sizeof(T)) == 0;
        }

        template <typename T>
        ClockResult SetTimingValue(std::uintptr_t rva, const T& value, unsigned bit, T& original) noexcept
        {
            auto owner = CheckTimingOwner();
            if (owner != ClockResult::Ok)
            {
                if (owner == ClockResult::BindingChanged)
                    Stop(owner);
                return owner;
            }
            if (!TimingValueValid(value))
                return ClockResult::InvalidArgument;
            PeerHold peers;
            if (!peers.Prepare() || !peers.Suspend())
                return ClockResult::Busy;
            owner = CheckTimingOwner();
            if (owner != ClockResult::Ok)
                return owner;
            if (!(runtime.timingCaptured.load() & bit))
            {
                T captured{};
                if (!ReadValue(rva, captured) || !TimingValueValid(captured))
                {
                    Stop(ClockResult::ReadFailure);
                    return ClockResult::ReadFailure;
                }
                original = captured;
                runtime.timingGeneration.store(timingCallbackGeneration);
                runtime.timingCaptured.fetch_or(bit);
            }
            // A failed/partial OS write is already a restoration obligation.
            // Neither a later call nor End may discard this original value.
            runtime.timingDirty.fetch_or(bit);
            runtime.timingRecovery.fetch_or(bit);
            runtime.restored.store(false);
            const bool written = WriteValue(rva, value) && TimingReadback(rva, value);
            const bool released = peers.Release();
            if (!written || !released)
            {
                const auto failure = released ? ClockResult::WriteFailure : ClockResult::RestoreFailure;
                Stop(failure);
                return failure;
            }
            runtime.timingRecovery.fetch_and(~bit);
            return ClockResult::Ok;
        }

        ClockResult NativeAccess::SetTimeScale(float value) noexcept
        {
            return SetTimingValue(TimingProfile::timeScaleRva, value, timingScaleBit, runtime.originalTimeScale);
        }

        ClockResult NativeAccess::SetMinuteDuration(std::uint32_t value) noexcept
        {
            return SetTimingValue(minuteDurationRva, value, timingMinuteBit, runtime.originalMinuteDuration);
        }

        ClockResult NativeAccess::Pause(bool value) noexcept
        {
            // Legacy clock ownership has no callback once user pause skips the
            // weather continuation. It therefore cannot acquire pause debt.
            if (!runtime.timingMode || !runtime.timerHooked.load() || !runtime.timerReady.load())
                return ClockResult::NotInitialized;
            return SetTimingValue(PauseProfile::userPauseRva, static_cast<std::uint8_t>(value), timingPauseBit, runtime.originalUserPause);
        }

        bool RestoreOriginal() noexcept
        {
            runtime.counters.restoreAttempts.fetch_add(1);
            bool       clean = callbackDepth && GetCurrentThreadId() == runtime.engineThread.load() && CheckBinding(false, true);
            const bool clockChanged = runtime.nativeTouched.load() || runtime.dataRecovery.load() || runtime.patchInstalled.load() ||
                                      runtime.patchRecovery.load() || runtime.protectionPending.load();
            const auto timingChanged = runtime.timingDirty.load() | runtime.timingRecovery.load();
            if (timingChanged)
                clean = clean && callbackDepth == 1 && timingCallbackGeneration == runtime.generation.load() &&
                        runtime.timingGeneration.load() == timingCallbackGeneration && runtime.admissionGeneration.load() == timingCallbackGeneration &&
                        runtime.admissionSerial.load() && (runtime.timingCaptured.load() & timingChanged) == timingChanged && CheckTimingBinding();
            if (clean && !clockChanged && !timingChanged)
            {
                ClockValues current;
                if (!runtime.captured.load() || RawRead(current))
                {
                    // A captured read owns lifetime, not time. Retire it using
                    // current verified values so a read-only reference never
                    // rewinds the game clock or writes a restoration snapshot.
                    if (runtime.captured.load())
                        runtime.observed = current;
                    runtime.restored.store(true);
                    runtime.ready.store(false);
                    runtime.timerReady.store(false);
                    return true;
                }
                runtime.counters.restoreFailures.fetch_add(1);
                Stop(ClockResult::RestoreFailure);
                return false;
            }
            PeerHold peers;
            if (clean)
                clean = peers.Prepare() && peers.Suspend() && CheckBinding(false, true) && (!timingChanged || CheckTimingBinding());
            std::uint32_t timer = 0;
            if (clean && clockChanged)
                clean = ReadValue(nativeTimerRva, timer);
            // Restoring pauses the trial's displayed time and preserves its
            // captured fractional minute. Native timer/calendar/store slots
            // are read-only and never rewound by this owner.
            const std::uint32_t phase = runtime.original.nativeTimer - runtime.original.lastMinuteChange;
            const std::uint32_t rebased = timer - phase;
            if (clean && clockChanged)
            {
                runtime.nativeTouched.store(true);
                runtime.dataRecovery.store(true);
                clean = WriteValue(hoursRva, runtime.original.hour) && WriteValue(minutesRva, runtime.original.minute) &&
                        WriteValue(secondsRva, runtime.original.seconds) && WriteValue(lastChangeRva, rebased);
            }
            if (clean && clockChanged)
                clean = PatchWhileHeld(false) == ClockResult::Ok;
            if (clean && (timingChanged & timingScaleBit))
            {
                runtime.timingRecovery.fetch_or(timingChanged);
                clean = WriteValue(TimingProfile::timeScaleRva, runtime.originalTimeScale) &&
                        TimingReadback(TimingProfile::timeScaleRva, runtime.originalTimeScale);
            }
            if (clean && (timingChanged & timingMinuteBit))
            {
                runtime.timingRecovery.fetch_or(timingChanged);
                clean = WriteValue(minuteDurationRva, runtime.originalMinuteDuration) && TimingReadback(minuteDurationRva, runtime.originalMinuteDuration);
            }
            if (clean && (timingChanged & timingPauseBit))
            {
                runtime.timingRecovery.fetch_or(timingChanged);
                clean =
                    WriteValue(PauseProfile::userPauseRva, runtime.originalUserPause) && TimingReadback(PauseProfile::userPauseRva, runtime.originalUserPause);
            }
            ClockValues observed;
            if (clean && runtime.captured.load())
                clean = RawRead(observed) && (!clockChanged || (observed.hour == runtime.original.hour && observed.minute == runtime.original.minute &&
                                                                observed.seconds == runtime.original.seconds && observed.lastMinuteChange == rebased));
            clean = peers.Release() && clean;
            if (clean)
            {
                if (runtime.captured.load())
                    runtime.observed = observed;
                runtime.dataRecovery.store(false);
                runtime.timingDirty.store(0);
                runtime.timingRecovery.store(0);
                runtime.timingCaptured.store(0);
                runtime.timingGeneration.store(0);
                runtime.restored.store(true);
                runtime.ready.store(false);
                runtime.timerReady.store(false);
                return true;
            }
            runtime.counters.restoreFailures.fetch_add(1);
            Stop(ClockResult::RestoreFailure);
            return false;
        }

        void Publish() noexcept
        {
            auto& state = runtime.publishedClock;
            state.version.fetch_add(1);
            state.hour.store(runtime.observed.hour);
            state.minute.store(runtime.observed.minute);
            state.seconds.store(runtime.observed.seconds);
            state.nativeTimer.store(runtime.observed.nativeTimer);
            state.lastMinuteChange.store(runtime.observed.lastMinuteChange);
            state.millisecondsPerGameMinute.store(runtime.observed.millisecondsPerGameMinute);
            // A completed SDK call can precede the callback's native readback.
            // Publish its acknowledgement with those observed values, never
            // pair a new Set result with the previous callback's clock values.
            state.lastCompletedSequence.store(runtime.lastCompletedSequence.load());
            state.lastCommandResult.store(runtime.lastCommandResult.load());
            state.version.fetch_add(1);
        }

        void CompleteCommand(const ClockCommandV1& command) noexcept
        {
            if (!NativeClockService::ValidateCommand(command) || command.generation != runtime.generation.load() ||
                command.sequence <= runtime.lastCompletedSequence.load())
            {
                Stop(ClockResult::OwnerConflict);
                return;
            }
            CClock& clock = runtime.clock;
            BYTE    hour = 0, minute = 0;
            switch (command.operation)
            {
                case ClockOperation::Read:
                    clock.Get(&hour, &minute);
                    break;
                case ClockOperation::Set:
                    clock.Set(static_cast<BYTE>(command.hour), static_cast<BYTE>(command.minute));
                    break;
                case ClockOperation::Freeze:
                    clock.SetTimeFrozen(true);
                    break;
                case ClockOperation::Unfreeze:
                    clock.SetTimeFrozen(false);
                    break;
                case ClockOperation::ResetTimeFrozen:
                    clock.ResetTimeFrozen();
                    break;
            }
            const auto result = runtime.clock.LastResult();
            if (command.operation == ClockOperation::ResetTimeFrozen && result == ClockResult::Ok)
                runtime.counters.resets.fetch_add(1);
            // The mailbox is already released before SDK/native operations.
            // Keep completion private until Publish pairs it with this
            // callback's native readback, including recovery-only callbacks.
            runtime.lastCommandResult.store(result);
            runtime.lastCompletedSequence.store(command.sequence);
            if (result != ClockResult::Ok)
                Stop(result);
        }

        void ClockBody(void* returnAddress) noexcept
        {
            if (reinterpret_cast<std::uintptr_t>(returnAddress) != runtime.image + Profile::clockCallRva + Profile::clockCall.size())
                return;
            runtime.counters.qualified.fetch_add(1);
            const auto current = GetCurrentThreadId();
            DWORD      owner = runtime.engineThread.load();
            if (!owner && runtime.active.load())
            {
                runtime.engineThread.compare_exchange_strong(owner, current);
                owner = runtime.engineThread.load();
            }
            if (!owner || owner != current)
            {
                if (owner)
                {
                    runtime.counters.foreign.fetch_add(1);
                    Stop(ClockResult::WrongThread);
                }
                return;
            }
            if (callbackDepth != 1)
            {
                runtime.counters.nested.fetch_add(1);
                Stop(ClockResult::Busy);
                return;
            }
            // A producer might itself be the peer whose resume failed. The
            // still-running native owner therefore retries debt before any
            // further transaction or allocator/GUI-dependent work.
            if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                return;
            if (runtime.active.load() && !Fresh())
                Stop(ClockResult::Expired);
            if (runtime.active.load() && !runtime.captured.load())
            {
                ClockValues original;
                if (!CheckBinding() || !RawRead(original) || original.minute >= 60 || original.seconds > 60 ||
                    static_cast<std::uint32_t>(original.nativeTimer - original.lastMinuteChange) > original.millisecondsPerGameMinute)
                    Stop(ClockResult::BindingChanged);
                else
                {
                    runtime.original = runtime.observed = original;
                    runtime.captured.store(true);
                    runtime.restored.store(false);
                    runtime.ready.store(true);
                }
            }
            // Only this matched natural continuation can grant timing writes.
            // A producer's reader Bind, matching OS thread, or an unrelated
            // call to the hooked function never creates this generation scope.
            if (runtime.captured.load())
                timingCallbackGeneration = runtime.generation.load();
            if (runtime.active.load() && runtime.ready.load())
            {
                ClockCommandV1 command;
                bool           pending = false;
                if (TryAcquireSRWLockExclusive(&runtime.mailboxLock))
                {
                    pending = runtime.hasPending;
                    if (pending)
                    {
                        command = runtime.pending;
                        runtime.hasPending = false;
                        runtime.pendingSequence.store(0);
                    }
                    ReleaseSRWLockExclusive(&runtime.mailboxLock);
                }
                if (pending)
                    CompleteCommand(command);
                if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                    return;
                ClockValues observed;
                if (runtime.active.load() && (!CheckBinding() || !RawRead(observed)))
                    Stop(ClockResult::BindingChanged);
                else if (runtime.active.load())
                    runtime.observed = observed;
            }
            if (runtime.stopping.load())
            {
                if (runtime.captured.load() && !runtime.restored.load())
                    RestoreOriginal();
                else if (!runtime.captured.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load())
                    runtime.restored.store(true);
            }
            if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                return;
            Publish();
        }

        void HookClockContinuation()
        {
            const auto returnAddress = _ReturnAddress();
            const auto previousTimingGeneration = timingCallbackGeneration;
            const auto previousTimerScope = timingTimerScope;
            timingCallbackGeneration = 0;
            timingTimerScope = false;
            runtime.inFlight.fetch_add(1);
            runtime.counters.callbacks.fetch_add(1);
            ++callbackDepth;
            __try
            {
                ClockBody(returnAddress);
                // A native continuation can take locks owned by a parked
                // producer. Do not enter it, or publish through SRW locks,
                // until every suspension owned by this transaction is retired.
                if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                    return;
                runtime.originalContinuation();
            }
            __finally
            {
                if (AbnormalTermination())
                    Stop(ClockResult::OriginalException);
                timingCallbackGeneration = previousTimingGeneration;
                timingTimerScope = previousTimerScope;
                --callbackDepth;
                runtime.inFlight.fetch_sub(1);
            }
        }

        void TimerBody(void* returnAddress)
        {
            const auto origin = reinterpret_cast<std::uintptr_t>(returnAddress);
            if (!runtime.timingMode || !runtime.timerHooked.load() ||
                (origin != runtime.image + PauseProfile::normalReturnRva && origin != runtime.image + PauseProfile::alternateReturnRva))
                return;
            runtime.counters.qualified.fetch_add(1);
            const auto current = GetCurrentThreadId();
            DWORD      owner = runtime.engineThread.load();
            if (!owner && runtime.active.load())
            {
                runtime.engineThread.compare_exchange_strong(owner, current);
                owner = runtime.engineThread.load();
            }
            if (!owner || owner != current)
            {
                if (owner)
                {
                    runtime.counters.foreign.fetch_add(1);
                    Stop(ClockResult::WrongThread);
                }
                return;
            }
            if (callbackDepth != 1)
            {
                runtime.counters.nested.fetch_add(1);
                Stop(ClockResult::Busy);
                return;
            }
            if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                return;
            if (runtime.active.load() && !Fresh())
                Stop(ClockResult::Expired);
            if (!CheckBinding(false, true))
                Stop(ClockResult::BindingChanged);
            else
            {
                // This is a real paused-frame owner, not a clock observation.
                // It neither captures CClock values nor admits its mailbox.
                timingTimerScope = true;
                timingCallbackGeneration = runtime.generation.load();
                if (runtime.active.load())
                    runtime.timerReady.store(true);
                if (runtime.active.load() && runtime.timerReady.load() && runtime.timingHandler)
                    runtime.timingHandler(runtime.access, runtime.timingContext);
                if (runtime.stopping.load())
                {
                    if ((runtime.captured.load() && !runtime.restored.load()) || runtime.timingDirty.load() || runtime.timingRecovery.load())
                        RestoreOriginal();
                    else
                    {
                        runtime.restored.store(true);
                        runtime.timerReady.store(false);
                    }
                }
            }
            if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                return;
            Publish();
        }

        void HookTimerUpdate()
        {
            const auto returnAddress = _ReturnAddress();
            const auto previousTimingGeneration = timingCallbackGeneration;
            const auto previousTimerScope = timingTimerScope;
            timingCallbackGeneration = 0;
            timingTimerScope = false;
            runtime.inFlight.fetch_add(1);
            runtime.counters.callbacks.fetch_add(1);
            ++callbackDepth;
            __try
            {
                // Every origin retains its original native update before any
                // client work. Only proven gameplay returns gain write scope.
                // Existing peer-resume debt must retire before native code can
                // need a parked producer's locks, as for the clock hook.
                if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                    return;
                runtime.originalTimer();
                TimerBody(returnAddress);
            }
            __finally
            {
                if (AbnormalTermination())
                    Stop(ClockResult::OriginalException);
                timingCallbackGeneration = previousTimingGeneration;
                timingTimerScope = previousTimerScope;
                --callbackDepth;
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool FrameReceiver(void* receiver) noexcept
        {
            std::uintptr_t root = 0, vtable = 0, rootAfter = 0;
            if (!receiver || !ReadValue(FrameProfile::rootRva, root) || root != reinterpret_cast<std::uintptr_t>(receiver))
                return false;
            MEMORY_BASIC_INFORMATION region{};
            if (VirtualQuery(receiver, &region, sizeof(region)) != sizeof(region) || region.State != MEM_COMMIT ||
                (region.Type != MEM_PRIVATE && region.Type != MEM_MAPPED) || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return false;
            const auto protection = region.Protect & 0xff;
            const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
            if ((protection != PAGE_READWRITE && protection != PAGE_WRITECOPY) || region.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base ||
                root < base || root > base + region.RegionSize || sizeof(vtable) > base + region.RegionSize - root)
                return false;
            SIZE_T copied = 0;
            // A natural invocation supplies the receiver. OS reads bookend its
            // identity without dereferencing or retaining any UObject child.
            return ReadProcessMemory(GetCurrentProcess(), receiver, &vtable, sizeof(vtable), &copied) && copied == sizeof(vtable) &&
                   vtable == runtime.image + FrameProfile::vtableRva && ReadValue(FrameProfile::rootRva, rootAfter) && rootAfter == root;
        }

        bool QualifiedApplicationFrame(std::uint64_t expectedGeneration) noexcept
        {
            return expectedGeneration && expectedGeneration == runtime.generation.load() && frameCallbackGeneration == expectedGeneration &&
                   frameCallbackReceiver && callbackDepth == 1 && runtime.frameMode && runtime.frameHooked.load() && runtime.bound.load() &&
                   runtime.ready.load() && runtime.active.load() && !runtime.stopping.load() && !runtime.peersRetained.load() &&
                   GetCurrentThreadId() == runtime.engineThread.load() && runtime.admissionGeneration.load() == expectedGeneration &&
                   runtime.admissionSerial.load() && Fresh();
        }

        bool UiMapHeapRead(std::uintptr_t address, void* output, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            auto       at = address;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION region{};
                if (VirtualQuery(reinterpret_cast<const void*>(at), &region, sizeof(region)) != sizeof(region) || region.State != MEM_COMMIT ||
                    (region.Type != MEM_PRIVATE && region.Type != MEM_MAPPED) || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = region.Protect & 0xff;
                if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
                if (region.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + region.RegionSize <= at)
                    return false;
                at = base + region.RegionSize < end ? base + region.RegionSize : end;
            }
            SIZE_T copied = 0;
            // The OS copy handles allocation/protection changes after the
            // page observation. It does not retain a UObject or its children.
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, bytes, &copied) && copied == bytes;
        }

        void InvokeFrameHandler(void* receiver, float delta, std::uint32_t state)
        {
            const auto previousGeneration = frameCallbackGeneration;
            const auto previousReceiver = frameCallbackReceiver;
            frameCallbackGeneration = runtime.generation.load();
            frameCallbackReceiver = receiver;
            __try
            {
                runtime.frameHandler({runtime.generation.load(), state, delta}, runtime.frameContext);
            }
            __finally
            {
                // Retire observation scope even when a client raises SEH. The
                // outer hook separately retains the original/trampoline owner.
                if (AbnormalTermination())
                    Stop(ClockResult::OriginalException);
                frameCallbackGeneration = previousGeneration;
                frameCallbackReceiver = previousReceiver;
            }
        }

        void FrameBody(void* receiver, float delta, void* returnAddress, bool receiverQualifiedBefore) noexcept
        {
            if (!runtime.frameMode || !runtime.frameHooked.load() ||
                reinterpret_cast<std::uintptr_t>(returnAddress) != runtime.image + FrameProfile::normalReturnRva)
                return;
            runtime.counters.qualified.fetch_add(1);
            if (callbackDepth != 1)
            {
                runtime.counters.nested.fetch_add(1);
                Stop(ClockResult::Busy);
                return;
            }
            if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                return;
            if (runtime.active.load() && !Fresh())
                Stop(ClockResult::Expired);
            if (!runtime.active.load() || runtime.stopping.load())
                return;
            std::uint32_t state = 0;
            if (!receiverQualifiedBefore || !FrameReceiver(receiver) || !CheckBinding() || !std::isfinite(delta) || !ReadValue(FrameProfile::stateRva, state) ||
                state > 9)
            {
                Stop(ClockResult::BindingChanged);
                return;
            }
            const auto current = GetCurrentThreadId();
            DWORD      owner = runtime.engineThread.load();
            if (!owner)
            {
                runtime.engineThread.compare_exchange_strong(owner, current);
                owner = runtime.engineThread.load();
            }
            if (owner != current)
            {
                runtime.counters.foreign.fetch_add(1);
                Stop(ClockResult::WrongThread);
                return;
            }
            runtime.ready.store(true);
            if (runtime.frameHandler)
            {
                try
                {
                    InvokeFrameHandler(receiver, delta, state);
                }
                catch (...)
                {
                    // Client observation failures cannot unwind into the
                    // engine's ordinary frame dispatch or grant writer scope.
                    Stop(ClockResult::OriginalException);
                }
            }
        }

        void HookApplicationFrame(void* receiver, float delta)
        {
            const auto returnAddress = _ReturnAddress();
            const auto previousTimingGeneration = timingCallbackGeneration;
            const auto previousTimerScope = timingTimerScope;
            const auto previousFrameGeneration = frameCallbackGeneration;
            const auto previousFrameReceiver = frameCallbackReceiver;
            timingCallbackGeneration = 0;
            timingTimerScope = false;
            frameCallbackGeneration = 0;
            frameCallbackReceiver = nullptr;
            runtime.inFlight.fetch_add(1);
            runtime.counters.callbacks.fetch_add(1);
            ++callbackDepth;
            __try
            {
                if (runtime.peersRetained.load() && !RetryPeerHoldsBounded())
                    return;
                const bool before = reinterpret_cast<std::uintptr_t>(returnAddress) == runtime.image + FrameProfile::normalReturnRva && FrameReceiver(receiver);
                // Preserve the actual Windows x64 this/float ABI and finish
                // the original frame exactly once before any MTA observation.
                // Loading-loop and unknown origins forward without admission.
                runtime.originalFrame(receiver, delta);
                FrameBody(receiver, delta, returnAddress, before);
            }
            __finally
            {
                if (AbnormalTermination())
                    Stop(ClockResult::OriginalException);
                timingCallbackGeneration = previousTimingGeneration;
                timingTimerScope = previousTimerScope;
                frameCallbackGeneration = previousFrameGeneration;
                frameCallbackReceiver = previousFrameReceiver;
                --callbackDepth;
                runtime.inFlight.fetch_sub(1);
            }
        }

        bool ChangeHooks(bool install) noexcept
        {
            // Detours owns relocation only for its function-entry edit. Raw
            // clock-branch changes use their own context-checked peer hold.
            PeerHold peers;
            if (!peers.Prepare() || !peers.Suspend())
                return false;
            bool ready = false;
            bool transaction = false;
            if (DetourTransactionBegin() == NO_ERROR)
            {
                transaction = true;
                ready = peers.RegisterDetours();
                if (ready && !install &&
                    (runtime.inFlight.load() || !runtime.restored.load() || runtime.patchInstalled.load() || runtime.patchRecovery.load() ||
                     runtime.dataRecovery.load() || runtime.timingDirty.load() || runtime.timingRecovery.load() || runtime.peersRetained.load() ||
                     runtime.protectionPending.load()))
                    ready = false;
                if (ready && !runtime.frameMode)
                    ready = (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.originalContinuation),
                                                                    reinterpret_cast<PVOID>(&HookClockContinuation)) == NO_ERROR;
                if (ready && runtime.timingMode)
                    ready = (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.originalTimer),
                                                                    reinterpret_cast<PVOID>(&HookTimerUpdate)) == NO_ERROR;
                if (ready && runtime.frameMode)
                    ready = (install ? DetourAttach : DetourDetach)(reinterpret_cast<PVOID*>(&runtime.originalFrame),
                                                                    reinterpret_cast<PVOID>(&HookApplicationFrame)) == NO_ERROR;
                if (ready)
                {
                    ready = DetourTransactionCommit() == NO_ERROR;
                    transaction = false;
                    // Commit changes native ownership before peers resume.
                    // Failed ResumeThread must not hide a live installed hook
                    // (or misreport a successfully detached hook as retained).
                    if (ready)
                    {
                        runtime.hooked.store(install && !runtime.frameMode);
                        runtime.timerHooked.store(install && runtime.timingMode);
                        runtime.frameHooked.store(install && runtime.frameMode);
                    }
                }
            }
            if (transaction)
                DetourTransactionAbort();
            return peers.Release() && ready;
        }

        void ClearCounters() noexcept
        {
            runtime.counters.callbacks.store(0);
            runtime.counters.qualified.store(0);
            runtime.counters.foreign.store(0);
            runtime.counters.nested.store(0);
            runtime.counters.reads.store(0);
            runtime.counters.sets.store(0);
            runtime.counters.freezes.store(0);
            runtime.counters.unfreezes.store(0);
            runtime.counters.resets.store(0);
            runtime.counters.renewals.store(0);
            runtime.counters.restoreAttempts.store(0);
            runtime.counters.restoreFailures.store(0);
        }

        bool ReadState(ClockServiceStateV1& state) noexcept
        {
            // Recovery diagnostics must remain callable even when the owner
            // retained a peer suspension. No producer/engine publication lock
            // may be acquired while that peer could own it.
            ClockServiceStateV1 next;
            bool                clockRead = false;
            const auto&         clock = runtime.publishedClock;
            for (unsigned retry = 0; retry != 32; ++retry)
            {
                const auto before = clock.version.load();
                if (before & 1)
                    continue;
                const auto hour = clock.hour.load(), minute = clock.minute.load(), seconds = clock.seconds.load();
                const auto timer = clock.nativeTimer.load(), lastChange = clock.lastMinuteChange.load(), duration = clock.millisecondsPerGameMinute.load();
                const auto completed = clock.lastCompletedSequence.load();
                const auto result = clock.lastCommandResult.load();
                if (clock.version.load() == before)
                {
                    next.hour = hour;
                    next.minute = minute;
                    next.seconds = seconds;
                    next.nativeTimer = timer;
                    next.lastMinuteChange = lastChange;
                    next.millisecondsPerGameMinute = duration;
                    next.lastCompletedSequence = completed;
                    next.lastCommandResult = result;
                    clockRead = true;
                    break;
                }
            }
            if (!clockRead)
                return false;
            next.generation = runtime.generation.load();
            next.producerThread = runtime.producerThread;
            next.engineThread = runtime.engineThread.load();
            next.bound = runtime.bound.load();
            next.hooked = runtime.hooked.load() || runtime.timerHooked.load() || runtime.frameHooked.load();
            next.active = runtime.active.load();
            next.ready = runtime.ready.load() && !runtime.timingRecovery.load();
            next.stopping = runtime.stopping.load();
            next.restored = runtime.restored.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load();
            next.frozen = runtime.patchInstalled.load();
            next.modulePinned = runtime.modulePinned.load();
            next.failure = runtime.failure.load();
            next.pendingSequence = runtime.pendingSequence.load();
            next.callbacks = runtime.counters.callbacks.load();
            next.qualifiedCallbacks = runtime.counters.qualified.load();
            next.foreignCallbacks = runtime.counters.foreign.load();
            next.nestedCallbacks = runtime.counters.nested.load();
            next.reads = runtime.counters.reads.load();
            next.sets = runtime.counters.sets.load();
            next.freezes = runtime.counters.freezes.load();
            next.unfreezes = runtime.counters.unfreezes.load();
            next.resets = runtime.counters.resets.load();
            next.renewals = runtime.counters.renewals.load();
            next.restoreAttempts = runtime.counters.restoreAttempts.load();
            next.restoreFailures = runtime.counters.restoreFailures.load();
            next.inFlight = runtime.inFlight.load();
            next.patchInstalled = runtime.patchInstalled.load();
            next.retainedPeerThreads = runtime.retainedPeerThreads.load();
            next.patchRecoveryPending = runtime.patchRecovery.load();
            next.dataRecoveryPending = runtime.dataRecovery.load() || runtime.timingDirty.load() || runtime.timingRecovery.load();
            next.protectionRestorePending = runtime.protectionPending.load();
            state = next;
            return true;
        }
    }

    bool NativeClockService::ValidateRequest(const ClockServiceRequestV1& request) noexcept
    {
        return request.size == sizeof(request) && request.version == ClockServiceVersionV1 && request.maximumDurationMs &&
               request.maximumDurationMs <= ClockServiceMaximumDurationMsV1 && !request.reserved[0] && !request.reserved[1] && !request.reserved[2];
    }

    bool NativeClockService::ValidateCommand(const ClockCommandV1& command) noexcept
    {
        if (command.size != sizeof(command) || command.version != ClockServiceVersionV1 || !command.generation || !command.sequence || command.reserved ||
            static_cast<std::uint32_t>(command.operation) > static_cast<std::uint32_t>(ClockOperation::ResetTimeFrozen))
            return false;
        return command.operation == ClockOperation::Set ? command.hour <= 23 && command.minute <= 60 : !command.hour && !command.minute;
    }

    bool NativeClockService::Begin(const ClockServiceRequestV1& request, ClockServiceStateV1& state) noexcept
    {
        return BeginOwner(request, state, false, nullptr, nullptr);
    }

    bool NativeClockService::BeginGameTiming(const ClockServiceRequestV1& request, ClockServiceStateV1& state, GameTimingHandler handler,
                                             void* context) noexcept
    {
        return BeginOwner(request, state, true, handler, context);
    }

    bool NativeClockService::BeginApplicationFrames(const ClockServiceRequestV1& request, ClockServiceStateV1& state, ApplicationFrameHandler handler,
                                                    void* context) noexcept
    {
        return BeginOwner(request, state, false, nullptr, context, true, handler);
    }

    bool NativeClockService::ReadStartupUiClassMap(std::uint64_t requestedGeneration, NativeUiClassMapAccess::Key key,
                                                   NativeUiClassMapAccess::Value& value) const noexcept
    {
        const auto qualified = [&]() noexcept { return requestedGeneration == generation && QualifiedApplicationFrame(generation); };
        if (!qualified() || !FrameReceiver(frameCallbackReceiver) || !CheckBinding(false, false, true))
            return false;
        const auto receiver = reinterpret_cast<std::uintptr_t>(frameCallbackReceiver);
        if (UiMapProfile::ownerOffset > (std::numeric_limits<std::uintptr_t>::max)() - receiver)
            return false;
        NativeUiClassMapAccess::Value next{};
        if (!NativeUiClassMapAccess(receiver + UiMapProfile::ownerOffset, &UiMapHeapRead).Lookup(key, next) || !qualified() ||
            !FrameReceiver(frameCallbackReceiver) || !CheckBinding(false, false, true))
            return false;
        value = next;
        return true;
    }

    bool NativeClockService::ReadStartupUiTransitionClass(std::uint64_t requestedGeneration, NativeUiClassMapAccess::Key key, bool& compatible) const noexcept
    {
        NativeUiClassMapAccess::Value first{}, second{};
        std::uintptr_t                baseBefore = 0, baseAfter = 0;
        bool                          next = false;
        // Each map observation independently qualifies this exact receiver's
        // post-original callback scope. A producer, nested call or old owner
        // cannot turn an earlier opaque class address into startup authority.
        if (!ReadStartupUiClassMap(requestedGeneration, key, first) || !first.keyPresent || !first.classAddress ||
            !Image::ReadImage(runtime.image, runtime.image + UiMapProfile::uiTransitionClassCacheRva, &baseBefore, sizeof(baseBefore)) || !baseBefore ||
            !NativeClassChainAccess(&UiMapHeapRead).IsChildOf(first.classAddress, baseBefore, next) ||
            !ReadStartupUiClassMap(requestedGeneration, key, second) || !second.keyPresent || second.classAddress != first.classAddress ||
            !Image::ReadImage(runtime.image, runtime.image + UiMapProfile::uiTransitionClassCacheRva, &baseAfter, sizeof(baseAfter)) ||
            baseAfter != baseBefore || requestedGeneration != generation || !QualifiedApplicationFrame(generation))
            return false;
        // No StaticClass call or widget creation occurs here. The caller gets
        // only a coherent relation; UObject lifetime remains a separate duty.
        compatible = next;
        return true;
    }

    bool NativeClockService::BeginOwner(const ClockServiceRequestV1& request, ClockServiceStateV1& state, bool timingMode, GameTimingHandler handler,
                                        void* context, bool frameMode, ApplicationFrameHandler frameHandler) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || !ValidateRequest(request) || runtime.hooked.load() || runtime.timerHooked.load() || runtime.frameHooked.load() ||
            runtime.inFlight.load() || runtime.captured.load() || runtime.patchInstalled.load() || runtime.patchRecovery.load() ||
            runtime.protectionPending.load() || runtime.dataRecovery.load() || runtime.timingDirty.load() || runtime.timingRecovery.load() ||
            runtime.peersRetained.load() || runtime.admissionSerial.load() || runtime.generation.load() == (std::numeric_limits<std::uint64_t>::max)())
            return false;
        Admission::Token claim;
        if (!Admission::TryAcquire(Admission::Category::Clock, claim))
            return false;
        ClearCounters();
        generation = runtime.generation.fetch_add(1) + 1;
        runtime.admissionSerial.store(claim.serial);
        runtime.admissionGeneration.store(generation);
        runtime.producerThread = GetCurrentThreadId();
        runtime.engineThread.store(0);
        runtime.started = GetTickCount64();
        runtime.refreshed.store(runtime.started);
        runtime.maximumDuration = request.maximumDurationMs;
        runtime.image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        runtime.bound.store(false);
        runtime.active.store(false);
        runtime.ready.store(false);
        runtime.stopping.store(false);
        runtime.restored.store(true);
        runtime.failure.store(ClockResult::Ok);
        runtime.pendingSequence.store(0);
        runtime.lastSubmitted.store(0);
        runtime.lastCompletedSequence.store(0);
        runtime.lastCommandResult.store(ClockResult::NotInitialized);
        runtime.hasPending = false;
        runtime.pending = {};
        runtime.original = {};
        runtime.observed = {};
        runtime.publishedClock.version.store(0);
        runtime.publishedClock.hour.store(0);
        runtime.publishedClock.minute.store(0);
        runtime.publishedClock.seconds.store(0);
        runtime.publishedClock.nativeTimer.store(0);
        runtime.publishedClock.lastMinuteChange.store(0);
        runtime.publishedClock.millisecondsPerGameMinute.store(0);
        runtime.publishedClock.lastCompletedSequence.store(0);
        runtime.publishedClock.lastCommandResult.store(ClockResult::NotInitialized);
        runtime.protectionPending = false;
        runtime.patchProtection = 0;
        runtime.nativeTouched.store(false);
        runtime.timingCaptured.store(0);
        runtime.timingDirty.store(0);
        runtime.timingRecovery.store(0);
        runtime.timingGeneration.store(0);
        runtime.originalTimeScale = 0;
        runtime.originalMinuteDuration = 0;
        runtime.originalUserPause = 0;
        runtime.timingMode = timingMode;
        runtime.timerReady.store(false);
        runtime.timingHandler = handler;
        runtime.timingContext = timingMode ? context : nullptr;
        runtime.frameMode = frameMode;
        runtime.frameHandler = frameHandler;
        runtime.frameContext = frameMode ? context : nullptr;
        runtime.originalFrame = nullptr;
        bool valid = Image::CheckExecutableBuild() && CheckBinding(true);
        if (!valid)
            Stop(ClockResult::UnsupportedBuild);
        HMODULE retained = nullptr;
        if (valid && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                         reinterpret_cast<LPCWSTR>(&HookClockContinuation), &retained))
        {
            Stop(ClockResult::PatchFailure);
            valid = false;
        }
        else if (valid)
            runtime.modulePinned.store(true);
        if (valid)
        {
            runtime.originalContinuation = frameMode ? nullptr : reinterpret_cast<ContinuationFunction>(runtime.image + Profile::hookEntryRva);
            runtime.originalTimer = timingMode ? reinterpret_cast<ContinuationFunction>(runtime.image + PauseProfile::entryRva) : nullptr;
            runtime.originalFrame = frameMode ? reinterpret_cast<ApplicationFrameFunction>(runtime.image + FrameProfile::entryRva) : nullptr;
            valid = ChangeHooks(true);
            if (valid)
            {
                runtime.started = GetTickCount64();
                runtime.refreshed.store(runtime.started);
                runtime.bound.store(true);
                runtime.active.store(true);
                if (!Fresh())
                {
                    Stop(ClockResult::Expired);
                    valid = false;
                }
            }
            else
                Stop(ClockResult::PatchFailure);
        }
        ReadState(finalState);
        hasFinalState = true;
        state = finalState;
        // Refused setup relinquishes admission only when every possible
        // native obligation is absent. Partial setup retains it until End.
        if (!valid && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.frameHooked.load() && !runtime.inFlight.load() &&
            !runtime.captured.load() && !runtime.patchInstalled.load() && !runtime.patchRecovery.load() && !runtime.protectionPending.load() &&
            !runtime.dataRecovery.load() && !runtime.peersRetained.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load() &&
            !runtime.peerBusy.load())
        {
            if (ReleaseAdmission(generation))
            {
                runtime.timingHandler = nullptr;
                runtime.timingContext = nullptr;
                runtime.frameHandler = nullptr;
                runtime.frameContext = nullptr;
            }
        }
        return valid;
    }

    bool NativeClockService::Submit(const ClockCommandV1& command) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || runtime.frameMode || !ValidateCommand(command) || !generation || command.generation != generation ||
            generation != runtime.generation.load() || GetCurrentThreadId() != runtime.producerThread || !runtime.active.load() || !runtime.ready.load() ||
            runtime.stopping.load())
            return false;
        if (!Fresh())
        {
            Stop(ClockResult::Expired);
            return false;
        }
        AcquireSRWLockExclusive(&runtime.mailboxLock);
        const bool accepted = !runtime.hasPending && command.sequence > runtime.lastSubmitted.load() && runtime.active.load();
        if (accepted)
        {
            runtime.pending = command;
            runtime.hasPending = true;
            runtime.lastSubmitted.store(command.sequence);
            runtime.pendingSequence.store(command.sequence);
        }
        ReleaseSRWLockExclusive(&runtime.mailboxLock);
        return accepted;
    }

    bool NativeClockService::Renew(std::uint64_t requestedGeneration) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || !generation || requestedGeneration != generation || generation != runtime.generation.load() ||
            GetCurrentThreadId() != runtime.producerThread || !runtime.active.load() || runtime.stopping.load())
            return false;
        if (!Fresh())
        {
            Stop(ClockResult::Expired);
            return false;
        }
        runtime.refreshed.store(GetTickCount64());
        runtime.counters.renewals.fetch_add(1);
        return true;
    }

    bool NativeClockService::Read(ClockServiceStateV1& state) noexcept
    {
        ApiOperation operation;
        if (!operation.acquired || !generation)
            return false;
        if (generation != runtime.generation.load())
        {
            if (hasFinalState)
            {
                state = finalState;
                return true;
            }
            return false;
        }
        if (GetCurrentThreadId() != runtime.producerThread)
            return false;
        if (runtime.active.load() && !Fresh())
            Stop(ClockResult::Expired);
        if (!ReadState(finalState))
            return false;
        hasFinalState = true;
        state = finalState;
        return true;
    }

    bool NativeClockService::End(std::uint64_t requestedGeneration) noexcept
    {
        if (!generation || requestedGeneration != generation)
            return false;
        return End();
    }

    bool NativeClockService::End() noexcept
    {
        if (!generation || generation != runtime.generation.load())
            return true;
        ApiOperation operation;
        if (!operation.acquired)
            return false;
        if (!generation || generation != runtime.generation.load())
            return true;
        if (GetCurrentThreadId() != runtime.producerThread || callbackDepth)
            return false;
        Stop(ClockResult::Stopped);
        // A failed resume may retain the very callback counted inFlight.
        // Retire that suspend debt first so its finally block can finish.
        if ((runtime.peersRetained.load() && !RetryPeerHoldsBounded()) || runtime.inFlight.load())
            return false;
        AcquireSRWLockExclusive(&runtime.mailboxLock);
        runtime.hasPending = false;
        runtime.pendingSequence.store(0);
        ReleaseSRWLockExclusive(&runtime.mailboxLock);
        // Cleanup is asynchronous: the producer must not restore native data
        // on its CLEO thread, or unload code beneath an engine callback.
        if ((runtime.captured.load() && !runtime.restored.load()) || runtime.patchInstalled.load() || runtime.patchRecovery.load() ||
            runtime.dataRecovery.load() || runtime.timingDirty.load() || runtime.timingRecovery.load() || runtime.inFlight.load() ||
            runtime.peersRetained.load() || runtime.protectionPending.load())
            return false;
        const bool clean = (!runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.frameHooked.load()) || ChangeHooks(false);
        if (clean)
        {
            runtime.hooked.store(false);
            runtime.frameHooked.store(false);
            runtime.bound.store(false);
            runtime.ready.store(false);
            runtime.timerReady.store(false);
            runtime.captured.store(false);
            runtime.timingHandler = nullptr;
            runtime.timingContext = nullptr;
            runtime.frameHandler = nullptr;
            runtime.frameContext = nullptr;
        }
        ReadState(finalState);
        hasFinalState = true;
        return clean && ReleaseAdmission(generation);
    }

    bool NativeClockService::IsRetained() const noexcept
    {
        return generation && generation == runtime.generation.load() &&
               (runtime.hooked.load() || runtime.timerHooked.load() || runtime.frameHooked.load() || runtime.inFlight.load() ||
                (runtime.captured.load() && !runtime.restored.load()) || runtime.patchInstalled.load() || runtime.patchRecovery.load() ||
                runtime.dataRecovery.load() || runtime.timingDirty.load() || runtime.timingRecovery.load() || runtime.peersRetained.load() ||
                runtime.protectionPending.load() || runtime.admissionSerial.load());
    }
}
