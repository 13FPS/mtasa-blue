/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Call the actual DE frontend/loading transaction rather than state8
 *****************************************************************************/
#include "NativeStartGameCalls.h"
#include "NativeStartGameProfile.h"
#include "NativeBorrowedGcProfile.h"
#include "NativeApplicationFrameProfile.h"
#include "NativeGameStateProfile.h"
#include "NativeGamePauseProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeStartGameCalls
{
    namespace
    {
        namespace Profile = NativeStartGameProfile;
        namespace Gc = NativeBorrowedGcProfile;
        namespace Image = NativeImageAccess;
        bool CurrentImage(std::uintptr_t image) noexcept
        {
            return sizeof(void*) == 8 && image && image <= (std::numeric_limits<std::uintptr_t>::max)() - NativeGameStateProfile::imageSize &&
                   image == reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        }
        template <typename T>
        bool Data(std::uintptr_t image, std::uintptr_t rva, T& output) noexcept
        {
            return Image::ReadImage(image, image + rva, &output, sizeof(output), true);
        }
        bool Code(std::uintptr_t image, std::uintptr_t rva, const unsigned char* expected, std::size_t size) noexcept
        {
            std::array<unsigned char, 160> actual{};
            return size && size <= actual.size() && Image::ReadImage(image, image + rva, actual.data(), size, false, true) &&
                   !std::memcmp(actual.data(), expected, size);
        }
        bool Pins(std::uintptr_t image) noexcept
        {
            for (const auto& pin : Profile::Code)
                if (!Code(image, pin.rva, pin.bytes.data(), pin.size))
                    return false;
            for (const auto& pointer : Profile::Pointers)
            {
                std::uintptr_t actual = 0;
                if (!Image::ReadImage(image, image + pointer.slotRva, &actual, sizeof(actual)) || actual != image + pointer.targetRva)
                    return false;
            }
            std::array<char, sizeof(Profile::Name)> name{};
            if (!Image::ReadImage(image, image + Profile::NameRva, name.data(), name.size()) || std::memcmp(name.data(), Profile::Name, name.size()))
                return false;
            // Reuse the established native dispatch semantics for state7; the
            // SDK enum's integer order alone cannot authorize this transition.
            for (const auto& pin : NativeGameStateProfile::pins)
            {
                std::array<unsigned char, 160> actual{};
                if (pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool Receiver(std::uintptr_t image, void* receiver) noexcept
        {
            const auto     address = reinterpret_cast<std::uintptr_t>(receiver);
            std::uintptr_t root = 0, vtable = 0;
            if (!address || address % alignof(std::uintptr_t) || !Data(image, NativeApplicationFrameProfile::rootRva, root) || root != address)
                return false;
            MEMORY_BASIC_INFORMATION pages{};
            if (VirtualQuery(receiver, &pages, sizeof(pages)) != sizeof(pages) || pages.State != MEM_COMMIT ||
                (pages.Type != MEM_PRIVATE && pages.Type != MEM_MAPPED) || (pages.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return false;
            const auto protection = pages.Protect & 0xff;
            if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                return false;
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), receiver, &vtable, sizeof(vtable), &copied) && copied == sizeof(vtable) &&
                   vtable == image + Profile::ReceiverVtableRva;
        }
        bool GcZero(std::uintptr_t image, LONG& marker) noexcept
        {
            static_assert(Gc::markerBytes == sizeof(LONG) && Gc::markerRva % alignof(LONG) == 0);
            if (!Image::ImagePages(image, image + Gc::markerRva, sizeof(marker), true))
                return false;
            __try
            {
                // Reproduce the exact native same-GameThread UObject check.
                // This preserves a foreign nonzero marker; it does not acquire
                // the separate off-thread GC guard or change async counters.
                marker = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(image + Gc::markerRva), 0, 0);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
#if !defined(MTA_DE_START_GAME_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, void* receiver, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(receiver);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
        bool Observation(std::uintptr_t image, std::uint32_t& state, unsigned char& request, std::uint32_t& selection) noexcept
        {
            return Data(image, NativeGameStateProfile::stateRva, state) && Data(image, Profile::RequestRva, request) &&
                   Data(image, Profile::SelectionRva, selection);
        }
    }

    ClockResult CheckThread(std::uintptr_t image) noexcept
    {
        if (!CurrentImage(image))
            return ClockResult::BindingChanged;
        // The last three native route pins establish the ID producer and its
        // independent UObject registry consumer. These are not guessed globals.
        for (const auto& pin : Profile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                if (!Code(image, pin.rva, pin.bytes.data(), pin.size))
                    return ClockResult::BindingChanged;
        for (const auto& pin : Gc::pins)
            if (!Code(image, pin.rva, pin.bytes, pin.size))
                return ClockResult::BindingChanged;
        unsigned char initialized = 0;
        DWORD         thread = 0;
        if (!Data(image, Profile::ThreadInitializedRva, initialized) || !Data(image, Profile::ThreadIdRva, thread))
            return ClockResult::ReadFailure;
        if (!initialized || !thread)
            return ClockResult::NotInitialized;
        if (initialized != 1)
            return ClockResult::BindingChanged;
        if (thread != GetCurrentThreadId())
            return ClockResult::WrongThread;
        LONG marker = -1;
        if (!GcZero(image, marker))
            return ClockResult::ReadFailure;
        if (marker)
            return ClockResult::Busy;
        unsigned char initializedAfter = 0;
        DWORD         threadAfter = 0;
        if (!Data(image, Profile::ThreadInitializedRva, initializedAfter) || !Data(image, Profile::ThreadIdRva, threadAfter) ||
            initializedAfter != initialized || threadAfter != thread || !CurrentImage(image))
            return ClockResult::BindingChanged;
        return ClockResult::Ok;
    }

    ClockResult Apply(std::uintptr_t image, void* receiver, CheckOwner checkOwner, ClaimEntry claimEntry, Receipt* receiptOutput, Invoke invoke) noexcept
    {
        Receipt    receipt;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!receiver || !checkOwner || !claimEntry)
            return finish(ClockResult::InvalidArgument);
        auto result = CheckThread(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = checkOwner(receiver);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Pins(image) || !Receiver(image, receiver) || !Observation(image, receipt.nativeStateBefore, receipt.requestBefore, receipt.selectionBefore))
            return finish(ClockResult::BindingChanged);
        if (receipt.nativeStateBefore != 7)
            return finish(receipt.nativeStateBefore >= 8 && receipt.nativeStateBefore <= 9 ? ClockResult::Busy : ClockResult::NotInitialized);
        if (receipt.requestBefore)
            return finish(ClockResult::Busy);
        result = CheckThread(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = checkOwner(receiver);
        if (result != ClockResult::Ok)
            return finish(result);
        std::uint32_t state = 0, selection = 0;
        unsigned char request = 0;
        if (!Receiver(image, receiver) || !Observation(image, state, request, selection) || state != receipt.nativeStateBefore ||
            request != receipt.requestBefore || selection != receipt.selectionBefore)
            return finish(ClockResult::BindingChanged);
        // Claim performs its final full owner/world/provider/UI admission and
        // atomically consumes this generation's sole attempt only here. Failed
        // argument/provenance/prerequisite checks above never consume it.
        result = claimEntry(receiver);
        if (result != ClockResult::Ok)
            return finish(result);
        const auto function = reinterpret_cast<NativeFunction>(image + Profile::EntryRva);
        receipt.entered = true;
#if defined(MTA_DE_START_GAME_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_START_GAME_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        result = entry(function, receiver, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if (!CurrentImage(image) || !Pins(image) || !Receiver(image, receiver) ||
            !Observation(image, receipt.nativeStateAfter, receipt.requestAfter, receipt.selectionAfter) ||
            !Data(image, NativeGamePauseProfile::userPauseRva, receipt.userPauseAfter))
            return finish(ClockResult::BindingChanged);
        result = CheckThread(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = checkOwner(receiver);
        if (result != ClockResult::Ok)
            return finish(result);
        // These are the actual entry's request/selection/menu-close effects.
        // Loading continues in native frames; do not force state8 or require
        // the previous menu/world graph to survive this legitimate transition.
        return finish(receipt.requestAfter == 1 && receipt.selectionAfter == 0 && receipt.userPauseAfter == 0 && receipt.nativeStateAfter >= 7 &&
                              receipt.nativeStateAfter <= 9
                          ? ClockResult::Ok
                          : ClockResult::BindingChanged);
    }
}
