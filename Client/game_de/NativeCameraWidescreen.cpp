/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep configured mode distinct from deferred widescreen processing
 ****************************************************************************/
#include "NativeCameraWidescreen.h"
#include "NativeCameraWidescreenProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <intrin.h>
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace CameraWidescreenDetail
    {
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraWidescreenProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace Image = NativeImageAccess;
        bool ImageLayout(std::uintptr_t image) noexcept
        {
            if (sizeof(void*) != 8 || !image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > P::ImageBytes - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != P::ImageBytes || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto                           sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto                           sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (sectionsRva > P::ImageBytes || sectionsBytes > P::ImageBytes - sectionsRva ||
                !Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (Image::SectionContains(sections[i], rva, bytes, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(P::CameraRva, P::CameraBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::CrtPointerRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::CameraVtableRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : P::Pins)
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
            for (const auto& pin : Q::Pins)
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
            if (!contains(Q::GroupSlotRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                !contains(Q::SelectorRva, sizeof(std::uint8_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !contains(Q::CaseSlotRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                return false;
            for (const auto& group : P::Groups)
                if (!contains(group.slotRva, sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) ||
                    !contains(group.tableSlotRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    return false;
            return true;
        }

        bool Profile(std::uintptr_t image) noexcept
        {
            if (!ImageLayout(image))
                return false;
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto& group : P::Groups)
            {
                std::uintptr_t entry = 0;
                std::uint32_t  branch = 0;
                if (!Image::ReadImage(image, image + group.slotRva, &entry, sizeof(entry)) || entry != image + group.entryRva ||
                    !Image::ReadImage(image, image + group.tableSlotRva, &branch, sizeof(branch)) || branch != group.branchRva)
                    return false;
            }
            for (const auto& pin : Q::Pins)
            {
                std::array<unsigned char, Q::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            std::uintptr_t groupEntry = 0;
            std::uint32_t  branch = 0;
            std::uint8_t   selector = 0;
            if (!Image::ReadImage(image, image + Q::GroupSlotRva, &groupEntry, sizeof(groupEntry)) || groupEntry != image + Q::GroupEntryRva ||
                !Image::ReadImage(image, image + Q::SelectorRva, &selector, sizeof(selector), false, true) || selector != Q::Selector ||
                !Image::ReadImage(image, image + Q::CaseSlotRva, &branch, sizeof(branch), false, true) || branch != Q::CaseRva)
                return false;
            std::uintptr_t initializer = 0;
            // Normalize the relocated CRT pointer. Preferred-base disk bytes
            // would reject the genuine same image when Windows rebases it.
            return Image::ReadImage(image, image + P::CrtPointerRva, &initializer, sizeof(initializer)) && initializer == image + P::CrtWrapperRva &&
                   D::Profile(image);
        }

        bool Storage(std::uintptr_t image) noexcept
        {
            std::uintptr_t vtable = 0;
            // The native constructor owns these static rows. Verify current
            // image storage and camera type without freezing mutable contents
            // or demanding the unrelated matrix/ped/target dependencies.
            return Image::ImagePages(image, image + P::CameraRva, P::CameraBytes, true) &&
                   Image::ReadImage(image, image + P::CameraRva, &vtable, sizeof(vtable), true) && vtable == image + P::CameraVtableRva;
        }

        struct Snapshot
        {
            D::Value                                    world;
            std::array<unsigned char, Q::NeighborBytes> flags{};
        };
        bool Same(const Snapshot& a, const Snapshot& b) noexcept
        {
            return a.flags == b.flags && D::Same(a.world, b.world, true);
        }
        ClockResult Sample(std::uintptr_t image, NativeCameraWidescreenCalls::CheckOwner owner, NativeCameraWidescreenCalls::CheckOwner afterApplicationOwner,
                           Snapshot& output) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
                return ClockResult::UnsupportedBuild;
            auto result = afterApplicationOwner();
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!Storage(image))
                return ClockResult::BindingChanged;
            Snapshot next;
            if ((result = D::Capture(image, next.world)) != ClockResult::Ok)
                return result;
            if (!Image::ReadImage(image, image + P::CameraRva + Q::NeighborOffset, next.flags.data(), next.flags.size(), true))
                return ClockResult::ReadFailure;
            if (next.flags[Q::ModeOffset - Q::NeighborOffset] > 1)
                return ClockResult::BindingChanged;
            if ((result = afterApplicationOwner()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, next.world) || !Profile(image) || !Storage(image))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false); }
        };
#if !defined(MTA_DE_CAMERA_WIDESCREEN_EXCHANGE)
        ClockResult Exchange(std::uintptr_t target, std::uint8_t expected, std::uint8_t replacement, std::uint8_t& observed, bool& accepted,
                             std::uint32_t& exception) noexcept
        {
            __try
            {
                observed = static_cast<std::uint8_t>(
                    _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(target), static_cast<char>(replacement), static_cast<char>(expected)));
                accepted = observed == expected;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::WriteFailure;
            }
        }
#endif
    }
    struct NativeCameraWidescreen::Data
    {
        CameraWidescreenDetail::Snapshot before, expected, observed, confirmed;
        Receipt                          journal;
    };
    NativeCameraWidescreen::NativeCameraWidescreen() noexcept = default;
    NativeCameraWidescreen::~NativeCameraWidescreen() = default;
    ClockResult NativeCameraWidescreen::Read(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, bool& output) noexcept
    {
        namespace C = CameraWidescreenDetail;
        namespace D = NativeStreamingCompletionContext;
        if (busy.exchange(true))
            return ClockResult::Busy;
        C::BusyScope busyScope{busy};
        if (debt.load())
            return ClockResult::RestoreFailure;
        if (!owner || !afterApplicationOwner)
            return ClockResult::NotInitialized;
        if (!D::TryEnter())
            return ClockResult::Busy;
        D::Scope    scope;
        C::Snapshot first, current, confirmed;
        auto        result = C::Sample(image, owner, afterApplicationOwner, first);
        if (result != ClockResult::Ok || (result = C::Sample(image, owner, afterApplicationOwner, current)) != ClockResult::Ok ||
            (result = C::Sample(image, owner, afterApplicationOwner, confirmed)) != ClockResult::Ok)
            return result;
        if (!C::Same(first, current) || !C::Same(current, confirmed))
            return ClockResult::BindingChanged;
        // Read the configured byte, never the pending-off flag or derived
        // border/FOV values. Refusal never replaces the caller's bool.
        output = confirmed.flags[NativeCameraWidescreenProfile::ModeOffset - NativeCameraWidescreenProfile::NeighborOffset] != 0;
        return ClockResult::Ok;
    }
    ClockResult NativeCameraWidescreen::Write(std::uintptr_t image, bool mode, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* output,
                                              Exchange exchange) noexcept
    {
        namespace C = CameraWidescreenDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraWidescreenProfile;
        // Fix the actual MTA bool assignment before admission callbacks.
        const std::uint8_t requested = mode ? 1 : 0;
        Receipt            receipt;
        receipt.image = image;
        receipt.replacement = requested;
        bool       ownsBusy = false;
        const auto finish = [&](ClockResult result)
        {
            const bool effects = receipt.accepted || (receipt.entered && !receipt.returned);
            if (effects && !receipt.effectsVerified)
            {
                debt.store(true);
                receipt.result = result;
                receipt.debt = true;
                if (data)
                    data->journal = receipt;
            }
            receipt.debt = debt.load();
            receipt.result = result;
            if (ownsBusy && !receipt.debt)
                data.reset();
            if (output)
                *output = receipt;
            return result;
        };
        if (busy.exchange(true))
            return finish(ClockResult::Busy);
        ownsBusy = true;
        C::BusyScope busyScope{busy};
        if (debt.load())
            return finish(ClockResult::RestoreFailure);
        if (!owner || !afterApplicationOwner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            data = std::make_unique<Data>();
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        auto result = C::Sample(image, owner, afterApplicationOwner, data->before);
        if (result != ClockResult::Ok || (result = C::Sample(image, owner, afterApplicationOwner, data->observed)) != ClockResult::Ok ||
            (result = C::Sample(image, owner, afterApplicationOwner, data->confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->before, data->observed) || !C::Same(data->observed, data->confirmed))
            return finish(ClockResult::BindingChanged);
        constexpr auto index = Q::ModeOffset - Q::NeighborOffset;
        receipt.target = image + P::CameraRva + Q::ModeOffset;
        receipt.before = data->before.flags[index];
        receipt.root = data->before.world.root;
        receipt.world = data->before.world.world;
        data->expected = data->before;
        data->expected.flags[index] = requested;
#if defined(MTA_DE_CAMERA_WIDESCREEN_EXCHANGE)
        const auto write = exchange ? exchange : &MTA_DE_CAMERA_WIDESCREEN_EXCHANGE;
#else
        const auto write = exchange ? exchange : &C::Exchange;
#endif
        receipt.entered = true;
        result = write(receipt.target, receipt.before, requested, receipt.observed, receipt.accepted, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if (!receipt.accepted)
        {
            receipt.knownNoEffect = true;
            return finish(ClockResult::BindingChanged);
        }
        if (receipt.observed != receipt.before)
            return finish(ClockResult::BindingChanged);
        if ((result = C::Sample(image, owner, afterApplicationOwner, data->observed)) != ClockResult::Ok ||
            (result = C::Sample(image, owner, afterApplicationOwner, data->confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->expected, data->observed) || !C::Same(data->observed, data->confirmed))
            return finish(ClockResult::BindingChanged);
        receipt.after = data->confirmed.flags[index];
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
