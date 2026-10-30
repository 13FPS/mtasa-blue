/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve current camera scalar reads and bounded atomic FOV writes
 ****************************************************************************/
#include "NativeCameraDataMemory.h"
#include "NativeCameraFovWrite.h"
#include <intrin.h>
#include "NativeCameraDataProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de::NativeCameraDataMemory
{
    namespace
    {
        namespace P = NativeCameraDataProfile;
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

        template <class T>
        ClockResult ReadScalar(std::uintptr_t image, std::uintptr_t rva, CheckOwner owner, T& output) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            if (!D::TryEnter())
                return ClockResult::Busy;
            D::Scope scope;
            auto     result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!Storage(image))
                return ClockResult::BindingChanged;
            D::Value firstWorld, currentWorld, confirmedWorld;
            if ((result = D::Capture(image, firstWorld)) != ClockResult::Ok)
                return result;
            T first{}, second{}, confirmed{};
            if (!Image::ReadImage(image, image + rva, &first, sizeof(first), true))
                return ClockResult::ReadFailure;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = D::Capture(image, currentWorld)) != ClockResult::Ok)
                return result;
            if (!Image::ReadImage(image, image + rva, &second, sizeof(second), true))
                return ClockResult::ReadFailure;
            if (first != second || !D::Same(firstWorld, currentWorld, true) || !D::Fence(image, currentWorld) || !Profile(image) || !Storage(image))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = D::Capture(image, confirmedWorld)) != ClockResult::Ok)
                return result;
            if (!Image::ReadImage(image, image + rva, &confirmed, sizeof(confirmed), true))
                return ClockResult::ReadFailure;
            // No floating-point comparison belongs here: a stable NaN,
            // infinity or signed zero is still the native raw FOV observation.
            // Recheck after the last supplied owner callback before publishing.
            if (second != confirmed || !D::Same(firstWorld, confirmedWorld, true) || !D::Fence(image, confirmedWorld) || !Profile(image) || !Storage(image))
                return ClockResult::BindingChanged;
            output = confirmed;
            return ClockResult::Ok;
        }
    }

    ClockResult ReadActive(std::uintptr_t image, CheckOwner owner, std::uint8_t& output) noexcept
    {
        // GetActiveCam observes the whole BYTE. Values outside the constructed
        // row domain remain observable and must not silently become row zero.
        return ReadScalar(image, P::ActiveRva, owner, output);
    }

    ClockResult ReadFov(std::uintptr_t image, std::uint32_t logicalRow, CheckOwner owner, std::uint32_t& rawBits) noexcept
    {
        // Logical wrappers name a constructed row, independently of the
        // changing active selector. No raw caller index can escape this array.
        if (logicalRow >= P::RowCount)
            return ClockResult::InvalidArgument;
        return ReadScalar(image, P::CameraRva + P::ArrayOffset + logicalRow * P::RowBytes + P::FovOffset, owner, rawBits);
    }
}

namespace mta::game_de
{
    namespace CameraFovWriteDetail
    {
        namespace M = NativeCameraDataMemory;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        using CheckOwner = NativeCameraFovWriteCalls::CheckOwner;

        // Kept available even when a fixture supplies the final exchange
        // boundary: meaningful tests delegate to this exact real DWORD CAS.
        ClockResult NativeExchange(std::uintptr_t target, std::uint32_t expected, std::uint32_t replacement, std::uint32_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(std::uint32_t));
            LONG compare = 0, desired = 0;
            std::memcpy(&compare, &expected, sizeof(compare));
            std::memcpy(&desired, &replacement, sizeof(desired));
            __try
            {
                const auto prior = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(target), desired, compare);
                std::memcpy(&observed, &prior, sizeof(observed));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Sample(std::uintptr_t image, std::uintptr_t target, CheckOwner owner, CheckOwner afterApplicationFrameOwner, D::Value& world,
                           std::uint32_t& bits) noexcept
        {
            auto result = afterApplicationFrameOwner();
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            // Reuse the very same private PE/source/constructed-storage
            // qualification as the unchanged scalar readers. No raw pointer
            // or alternate weak camera layout can authorize this write.
            if (!M::Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!M::Storage(image))
                return ClockResult::BindingChanged;
            D::Value      nextWorld;
            std::uint32_t next = 0;
            if ((result = D::Capture(image, nextWorld)) != ClockResult::Ok)
                return result;
            if (!NativeImageAccess::ReadImage(image, target, &next, sizeof(next), true))
                return ClockResult::ReadFailure;
            if (!D::Fence(image, nextWorld) || !M::Profile(image) || !M::Storage(image))
                return ClockResult::BindingChanged;
            world = nextWorld;
            bits = next;
            return ClockResult::Ok;
        }
    }
    struct NativeCameraFovWrite::Data
    {
        NativeStreamingCompletionContext::Value beforeWorld, observedWorld, confirmedWorld;
        std::uint32_t                           before = 0, observed = 0, confirmed = 0;
        NativeCameraFovWriteCalls::Receipt      receipt;
    };
    NativeCameraFovWrite::NativeCameraFovWrite() noexcept = default;
    NativeCameraFovWrite::~NativeCameraFovWrite() = default;
    ClockResult NativeCameraFovWrite::Write(std::uintptr_t image, std::uint32_t logicalRow, std::uint32_t replacement, CheckOwner owner,
                                            CheckOwner afterApplicationFrameOwner, Receipt* output, Exchange exchange) noexcept
    {
        namespace C = CameraFovWriteDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        Receipt receipt;
        receipt.image = image;
        receipt.logicalRow = logicalRow;
        receipt.replacement = replacement;
        bool       ownsAttempt = false;
        const auto finish = [&](ClockResult result)
        {
            if (receipt.entered && !receipt.knownNoEffect && result != ClockResult::Ok)
                debt.store(true);
            receipt.result = result;
            receipt.debt = debt.load();
            // A rejected/reentrant later request cannot replace the exact
            // private journal belonging to an earlier uncertain exchange.
            if (ownsAttempt && data)
                data->receipt = receipt;
            if (output)
                *output = receipt;
            return result;
        };
        if (busy.exchange(true))
            return finish(ClockResult::Busy);
        struct BusyScope
        {
            std::atomic<bool>& value;
            ~BusyScope() { value.store(false); }
        } busyScope{busy};
        if (debt.load())
            return finish(ClockResult::RestoreFailure);
        // Positive IEEE754 float bits are ordered as unsigned DWORDs. This
        // exact open interval rejects both zero signs, negatives,180,NaN and
        // infinities without touching or normalizing the old native bits.
        static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
        if (logicalRow >= P::RowCount || !replacement || replacement >= 0x43340000u)
            return finish(ClockResult::InvalidArgument);
        if (!owner || !afterApplicationFrameOwner)
            return finish(ClockResult::NotInitialized);
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
            return finish(ClockResult::UnsupportedBuild);
        receipt.target = image + P::CameraRva + P::ArrayOffset + logicalRow * P::RowBytes + P::FovOffset;
        if (receipt.target % alignof(std::uint32_t))
            return finish(ClockResult::BindingChanged);
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
        ownsAttempt = true;
        auto result = C::Sample(image, receipt.target, owner, afterApplicationFrameOwner, data->beforeWorld, data->before);
        if (result != ClockResult::Ok)
            return finish(result);
        if ((result = C::Sample(image, receipt.target, owner, afterApplicationFrameOwner, data->observedWorld, data->observed)) != ClockResult::Ok)
            return finish(result);
        if (data->before != data->observed || !D::Same(data->beforeWorld, data->observedWorld, true))
            return finish(ClockResult::BindingChanged);
        receipt.root = data->beforeWorld.root;
        receipt.world = data->beforeWorld.world;
        receipt.before = data->before;
#if defined(MTA_DE_CAMERA_FOV_EXCHANGE)
        const auto primitive = exchange ? exchange : &MTA_DE_CAMERA_FOV_EXCHANGE;
#else
        const auto primitive = exchange ? exchange : &C::NativeExchange;
#endif
        receipt.entered = true;
        result = primitive(receipt.target, receipt.before, replacement, receipt.observed, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if (receipt.observed != receipt.before)
        {
            // CAS returned a distinct current DWORD and therefore wrote
            // nothing. Preserve that drift, release only this CPU attempt,
            // and leave a later explicit request possible without any debt.
            receipt.knownNoEffect = true;
            data.reset();
            return finish(ClockResult::Busy);
        }
        receipt.accepted = true;
        if ((result = C::Sample(image, receipt.target, owner, afterApplicationFrameOwner, data->observedWorld, data->observed)) != ClockResult::Ok)
            return finish(result);
        if (data->observed != replacement || !D::Same(data->beforeWorld, data->observedWorld, true))
            return finish(ClockResult::BindingChanged);
        if ((result = C::Sample(image, receipt.target, owner, afterApplicationFrameOwner, data->confirmedWorld, data->confirmed)) != ClockResult::Ok)
            return finish(result);
        if (data->confirmed != replacement || !D::Same(data->beforeWorld, data->confirmedWorld, true) ||
            !D::Same(data->observedWorld, data->confirmedWorld, true))
            return finish(ClockResult::BindingChanged);
        receipt.after = data->confirmed;
        receipt.effectsVerified = true;
        data.reset();
        return finish(ClockResult::Ok);
    }
}
