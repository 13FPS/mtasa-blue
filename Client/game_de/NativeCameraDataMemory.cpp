/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve current camera scalar reads and bounded atomic FOV writes
 ****************************************************************************/
#include "NativeCameraDataMemory.h"
#include "NativeCameraFovWrite.h"
#include "NativeCameraDirection.h"
#include "NativeCameraDirectionProfile.h"
#include "NativeCameraRowBorrowProfile.h"
#include "NativeCameraHistoryProfile.h"
// CVector's inline SDK helpers require the existing shared float constants.
#include <SharedUtil.Defines.h>
#include <CVector.h>
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

namespace mta::game_de
{
    namespace CameraDirectionDetail
    {
        namespace M = NativeCameraDataMemory;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraDirectionProfile;
        namespace I = NativeImageAccess;
        using CheckOwner = NativeCameraDirectionCalls::CheckOwner;
        using Bits = NativeCameraDirectionCalls::Bits;
        bool Same(const Bits& a, const Bits& b) noexcept
        {
            return a.horizontal == b.horizontal && a.vertical == b.vertical;
        }
        bool Same(std::uint16_t a, std::uint16_t b) noexcept
        {
            return a == b;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            // Add new source roles only here. Unchanged FOV, active-index and
            // Restore operations continue to use their original qualifications.
            if (!M::Profile(image))
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!I::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > P::ImageBytes - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!I::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != P::ImageBytes || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto                           sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto                           sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (sectionsRva > P::ImageBytes || sectionsBytes > P::ImageBytes - sectionsRva ||
                !I::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            for (const auto& pin : Q::Pins)
            {
                bool section = false;
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    section =
                        section || I::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE);
                std::array<unsigned char, Q::MaximumPinBytes> actual{};
                if (!section || !pin.size || pin.size > actual.size() || !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool ReadPair(std::uintptr_t image, std::uintptr_t row, Bits& pair) noexcept
        {
            return I::ReadImage(image, row + Q::HorizontalOffset, &pair.horizontal, sizeof(pair.horizontal), true) &&
                   I::ReadImage(image, row + Q::VerticalOffset, &pair.vertical, sizeof(pair.vertical), true);
        }
        template <class T, class Read>
        ClockResult Sample(std::uintptr_t image, CheckOwner owner, CheckOwner afterApplicationOwner, D::Value& world, T& bits, Read read) noexcept
        {
            auto result = afterApplicationOwner ? afterApplicationOwner() : ClockResult::Ok;
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!M::Storage(image))
                return ClockResult::BindingChanged;
            D::Value nextWorld;
            T        next{};
            if ((result = D::Capture(image, nextWorld)) != ClockResult::Ok)
                return result;
            if (!read(next))
                return ClockResult::ReadFailure;
            if (!D::Fence(image, nextWorld) || !Profile(image) || !M::Storage(image))
                return ClockResult::BindingChanged;
            world = nextWorld;
            bits = next;
            return ClockResult::Ok;
        }
        template <class T, class Read>
        ClockResult Observe(std::uintptr_t image, std::uint32_t logicalRow, CheckOwner owner, T& output, Read read) noexcept
        {
            if (logicalRow >= P::RowCount)
                return ClockResult::InvalidArgument;
            if (!owner)
                return ClockResult::NotInitialized;
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
                return ClockResult::UnsupportedBuild;
            if (!D::TryEnter())
                return ClockResult::Busy;
            D::Scope scope;
            D::Value firstWorld, secondWorld, confirmedWorld;
            T        first{}, second{}, confirmed{};
            auto     result = Sample(image, owner, nullptr, firstWorld, first, read);
            if (result != ClockResult::Ok)
                return result;
            if ((result = Sample(image, owner, nullptr, secondWorld, second, read)) != ClockResult::Ok)
                return result;
            if (!Same(first, second) || !D::Same(firstWorld, secondWorld, true))
                return ClockResult::BindingChanged;
            if ((result = Sample(image, owner, nullptr, confirmedWorld, confirmed, read)) != ClockResult::Ok)
                return result;
            if (!Same(first, confirmed) || !D::Same(firstWorld, confirmedWorld, true) || !D::Same(secondWorld, confirmedWorld, true))
                return ClockResult::BindingChanged;
            output = confirmed;
            return ClockResult::Ok;
        }
        // Unconditional real delegate remains available to meaningful fixtures
        // even when they replace only the final pair exchange boundary.
        ClockResult NativeExchange(std::uintptr_t target, std::uint32_t expected, std::uint32_t replacement, std::uint32_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            return CameraFovWriteDetail::NativeExchange(target, expected, replacement, observed, exception);
        }
    }
}
namespace mta::game_de::NativeCameraDataMemory
{
    ClockResult ReadMode(std::uintptr_t image, std::uint32_t logicalRow, CheckOwner owner, std::uint16_t& rawMode) noexcept
    {
        namespace C = CameraDirectionDetail;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraDirectionProfile;
        const auto target = image + P::CameraRva + P::ArrayOffset + logicalRow * static_cast<std::uintptr_t>(P::RowBytes) + Q::ModeOffset;
        return C::Observe(image, logicalRow, owner, rawMode,
                          [&](std::uint16_t& next) { return NativeImageAccess::ReadImage(image, target, &next, sizeof(next), true); });
    }
    ClockResult ReadDirection(std::uintptr_t image, std::uint32_t logicalRow, CheckOwner owner, std::uint32_t& horizontal, std::uint32_t& vertical) noexcept
    {
        namespace C = CameraDirectionDetail;
        namespace P = NativeCameraDataProfile;
        const auto row = image + P::CameraRva + P::ArrayOffset + logicalRow * static_cast<std::uintptr_t>(P::RowBytes);
        C::Bits    pair;
        const auto result = C::Observe(image, logicalRow, owner, pair, [&](C::Bits& next) { return C::ReadPair(image, row, next); });
        if (result == ClockResult::Ok)
        {
            horizontal = pair.horizontal;
            vertical = pair.vertical;
        }
        return result;
    }
}
namespace mta::game_de
{
    struct NativeCameraDirection::Data
    {
        NativeStreamingCompletionContext::Value beforeWorld, observedWorld, confirmedWorld;
        Bits                                    before, observed, confirmed;
        Receipt                                 receipt;
    };
    NativeCameraDirection::NativeCameraDirection() noexcept = default;
    NativeCameraDirection::~NativeCameraDirection() = default;
    ClockResult NativeCameraDirection::Set(std::uintptr_t image, std::uint32_t row, std::uint32_t wrappedH, std::uint32_t wrappedV, CheckOwner owner,
                                           CheckOwner afterApplicationOwner, Receipt* output, Exchange exchange) noexcept
    {
        return Run(image, row, nullptr, Bits{wrappedH, wrappedV}, owner, afterApplicationOwner, output, exchange);
    }
    ClockResult NativeCameraDirection::Repair(std::uintptr_t image, std::uint32_t row, std::uint32_t expectedH, std::uint32_t expectedV, std::uint32_t wrappedH,
                                              std::uint32_t wrappedV, CheckOwner owner, CheckOwner afterApplicationOwner, Receipt* output,
                                              Exchange exchange) noexcept
    {
        const Bits expected{expectedH, expectedV};
        return Run(image, row, &expected, Bits{wrappedH, wrappedV}, owner, afterApplicationOwner, output, exchange);
    }
    ClockResult NativeCameraDirection::Run(std::uintptr_t image, std::uint32_t logicalRow, const Bits* expected, const Bits& replacement, CheckOwner owner,
                                           CheckOwner afterApplicationOwner, Receipt* output, Exchange exchange) noexcept
    {
        namespace C = CameraDirectionDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraDirectionProfile;
        Receipt receipt;
        receipt.image = image;
        receipt.logicalRow = logicalRow;
        receipt.replacement = replacement;
        receipt.getter = receipt.repair = expected != nullptr;
        if (expected)
            receipt.expected = *expected;
        bool       ownsAttempt = false;
        const auto finish = [&](ClockResult result)
        {
            if ((receipt.horizontal.entered || receipt.vertical.entered) && !receipt.knownNoEffect && result != ClockResult::Ok)
                debt.store(true);
            receipt.result = result;
            receipt.debt = debt.load();
            // A refusal after debt cannot overwrite the private journal that
            // caused it. Only this admitted attempt may update its own record.
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
        if (logicalRow >= P::RowCount)
            return finish(ClockResult::InvalidArgument);
        if (!owner || !afterApplicationOwner)
            return finish(ClockResult::NotInitialized);
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
            return finish(ClockResult::UnsupportedBuild);
        const auto row = image + P::CameraRva + P::ArrayOffset + logicalRow * P::RowBytes;
        receipt.horizontal.target = row + Q::HorizontalOffset;
        receipt.vertical.target = row + Q::VerticalOffset;
        if (receipt.horizontal.target % alignof(std::uint32_t) || receipt.vertical.target % alignof(std::uint32_t))
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
        const auto read = [&](Bits& next) { return C::ReadPair(image, row, next); };
        auto       result = C::Sample(image, owner, afterApplicationOwner, data->beforeWorld, data->before, read);
        if (result != ClockResult::Ok)
            return finish(result);
        if ((result = C::Sample(image, owner, afterApplicationOwner, data->observedWorld, data->observed, read)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->before, data->observed) || !D::Same(data->beforeWorld, data->observedWorld, true))
            return finish(ClockResult::BindingChanged);
        receipt.original = data->before;
        receipt.root = data->beforeWorld.root;
        receipt.world = data->beforeWorld.world;
        // Conditional getter repair is tied to the exact pair observed by the
        // SDK before it computed its outputs. A later current pair is not ours.
        if (expected && !C::Same(*expected, data->before))
            return finish(ClockResult::BindingChanged);
        receipt.horizontal.before = data->before.horizontal;
        receipt.vertical.before = data->before.vertical;
        receipt.horizontal.replacement = replacement.horizontal;
        receipt.vertical.replacement = replacement.vertical;
#if defined(MTA_DE_CAMERA_DIRECTION_EXCHANGE)
        const auto primitive = exchange ? exchange : &MTA_DE_CAMERA_DIRECTION_EXCHANGE;
#else
        const auto primitive = exchange ? exchange : &C::NativeExchange;
#endif
        const auto enter = [&](NativeCameraDirectionCalls::Step& step)
        {
            step.entered = true;
            const auto status = primitive(step.target, step.before, step.replacement, step.observed, step.exceptionCode);
            if (status != ClockResult::Ok)
                return status;
            step.returned = true;
            if (step.observed != step.before)
                return ClockResult::Busy;
            step.accepted = true;
            return ClockResult::Ok;
        };
        result = enter(receipt.horizontal);
        if (result != ClockResult::Ok)
        {
            if (receipt.horizontal.returned && !receipt.horizontal.accepted)
            {
                receipt.knownNoEffect = true;
                data.reset();
            }
            return finish(result);
        }
        const Bits prefix{replacement.horizontal, data->before.vertical};
        if ((result = C::Sample(image, owner, afterApplicationOwner, data->observedWorld, data->observed, read)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(prefix, data->observed) || !D::Same(data->beforeWorld, data->observedWorld, true))
            return finish(ClockResult::BindingChanged);
        if ((result = C::Sample(image, owner, afterApplicationOwner, data->confirmedWorld, data->confirmed, read)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(prefix, data->confirmed) || !D::Same(data->beforeWorld, data->confirmedWorld, true) ||
            !D::Same(data->observedWorld, data->confirmedWorld, true))
            return finish(ClockResult::BindingChanged);
        result = enter(receipt.vertical);
        if (result != ClockResult::Ok)
            return finish(result);
        if ((result = C::Sample(image, owner, afterApplicationOwner, data->observedWorld, data->observed, read)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(replacement, data->observed) || !D::Same(data->beforeWorld, data->observedWorld, true))
            return finish(ClockResult::BindingChanged);
        if ((result = C::Sample(image, owner, afterApplicationOwner, data->confirmedWorld, data->confirmed, read)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(replacement, data->confirmed) || !D::Same(data->beforeWorld, data->confirmedWorld, true) ||
            !D::Same(data->observedWorld, data->confirmedWorld, true))
            return finish(ClockResult::BindingChanged);
        receipt.after = data->confirmed;
        receipt.horizontal.after = data->confirmed.horizontal;
        receipt.vertical.after = data->confirmed.vertical;
        receipt.effectsVerified = true;
        data.reset();
        return finish(ClockResult::Ok);
    }
}

namespace mta::game_de
{
    namespace CameraRowBorrowDetail
    {
        namespace M = NativeCameraDataMemory;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraRowBorrowProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace I = NativeImageAccess;
        using CheckOwner = NativeCameraDataMemory::CheckOwner;

        // These are the actual SDK vector fields, not copies with a setter
        // hidden behind them. Check the float-triple ABI before lending one.
        static_assert(std::is_standard_layout_v<CVector> && sizeof(CVector) == 12 && alignof(CVector) == 4 && offsetof(CVector, fX) == 0 &&
                      offsetof(CVector, fY) == 4 && offsetof(CVector, fZ) == 8);

        bool Profile(std::uintptr_t image) noexcept
        {
            if (!M::Profile(image))
                return false;
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!I::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 || static_cast<std::size_t>(dos.e_lfanew) > P::ImageBytes - sizeof(nt) ||
                !I::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto at = static_cast<std::size_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (at > P::ImageBytes || bytes > P::ImageBytes - at || !I::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            for (const auto& pin : Q::Pins)
            {
                bool admitted = false;
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    admitted =
                        admitted || I::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE);
                std::array<unsigned char, Q::MaximumPinBytes> current{};
                if (!admitted || !pin.size || pin.size > current.size() || !I::ReadImage(image, image + pin.rva, current.data(), pin.size, false, true) ||
                    std::memcmp(current.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }

        bool Fields(std::uintptr_t image, std::uint32_t logicalRow) noexcept
        {
            if (logicalRow >= P::RowCount || !M::Storage(image))
                return false;
            const auto rowOffset = P::ArrayOffset + static_cast<std::size_t>(logicalRow) * P::RowBytes;
            for (const auto offset : Q::VectorOffsets)
            {
                if (offset > P::RowBytes || sizeof(CVector) > P::RowBytes - offset || rowOffset > P::CameraBytes || offset > P::CameraBytes - rowOffset ||
                    sizeof(CVector) > P::CameraBytes - rowOffset - offset)
                    return false;
                const auto address = image + P::CameraRva + rowOffset + offset;
                if (address % alignof(CVector) || !I::ImagePages(image, address, sizeof(CVector), true))
                    return false;
            }
            // Mutable vector contents are deliberately not inspected. A real
            // SDK caller may be acquiring these pointers in order to replace
            // nonfinite or otherwise unwanted camera values in place.
            return true;
        }

        ClockResult Sample(std::uintptr_t image, std::uint32_t row, CheckOwner owner, CheckOwner afterApplicationOwner, D::Value& output) noexcept
        {
            auto result = afterApplicationOwner();
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!Fields(image, row))
                return ClockResult::BindingChanged;
            D::Value next;
            if ((result = D::Capture(image, next)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, next) || !Profile(image) || !Fields(image, row))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
    }
}

namespace mta::game_de::NativeCameraDataMemory
{
    ClockResult AcquireRowBorrow(std::uintptr_t image, std::uint32_t row, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                                 CheckOwner owner, CheckOwner afterApplicationOwner, NativeCameraRowBorrow::Token& output) noexcept
    {
        namespace C = CameraRowBorrowDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        if (row >= P::RowCount || output)
            return ClockResult::InvalidArgument;
        if (!owner || !afterApplicationOwner)
            return ClockResult::NotInitialized;
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
            return ClockResult::UnsupportedBuild;
        if (identity.image != image)
            return ClockResult::OwnerConflict;
        if (!D::TryEnter())
            return ClockResult::Busy;
        D::Scope scope;
        D::Value first, current;
        auto     result = C::Sample(image, row, owner, afterApplicationOwner, first);
        if (result != ClockResult::Ok)
            return result;
        // The ledger calls this while preparing its one admission commit. All
        // fallible native checks stay before that commit, so a rejected row
        // cannot strand a constructor hold on the engine callback thread.
        const auto check = [&]() noexcept -> ClockResult
        {
            const auto status = C::Sample(image, row, owner, afterApplicationOwner, current);
            if (status != ClockResult::Ok)
                return status;
            return D::Same(first, current, true) ? ClockResult::Ok : ClockResult::BindingChanged;
        };
        return ledger.AcquireChecked(identity, row, check, output);
    }

    ClockResult ReadRowVector(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                              const NativeCameraRowBorrow::Token& token, CameraRowVectorKind kind, CheckOwner owner, CheckOwner afterApplicationOwner,
                              CVector*& output) noexcept
    {
        namespace C = CameraRowBorrowDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraRowBorrowProfile;
        const auto selection = static_cast<std::uint32_t>(kind);
        if (!token || token.Row() >= P::RowCount || selection >= Q::VectorOffsets.size())
            return ClockResult::InvalidArgument;
        if (!owner || !afterApplicationOwner)
            return ClockResult::NotInitialized;
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
            return ClockResult::UnsupportedBuild;
        if (identity.image != image)
            return ClockResult::OwnerConflict;
        if (!D::TryEnter())
            return ClockResult::Busy;
        D::Scope scope;
        auto     result = ledger.Validate(token, identity);
        if (result != ClockResult::Ok)
            return result;
        const auto row = token.Row();
        D::Value   first, current, confirmed;
        if ((result = C::Sample(image, row, owner, afterApplicationOwner, first)) != ClockResult::Ok ||
            (result = C::Sample(image, row, owner, afterApplicationOwner, current)) != ClockResult::Ok)
            return result;
        if (!D::Same(first, current, true))
            return ClockResult::BindingChanged;
        if ((result = C::Sample(image, row, owner, afterApplicationOwner, confirmed)) != ClockResult::Ok)
            return result;
        if (!D::Same(first, confirmed, true) || !D::Same(current, confirmed, true))
            return ClockResult::BindingChanged;
        if ((result = ledger.Validate(token, identity)) != ClockResult::Ok)
            return result;
        // The same private client still owns retention after the final native
        // bookend. The host must drain that client synchronously before End;
        // this pointer is never valid beyond the owning wrapper's lifetime.
        output = reinterpret_cast<CVector*>(image + P::CameraRva + P::ArrayOffset + static_cast<std::size_t>(row) * P::RowBytes + Q::VectorOffsets[selection]);
        return ClockResult::Ok;
    }
}

namespace mta::game_de
{
    namespace CameraHistoryDetail
    {
        namespace C = CameraRowBorrowDetail;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraHistoryProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace I = NativeImageAccess;
        using CheckOwner = NativeCameraDataMemory::CheckOwner;

        static_assert(Q::VectorBytes == sizeof(CVector) && Q::ArrayBytes == Q::VectorCount * sizeof(CVector));

        bool Profile(std::uintptr_t image) noexcept
        {
            // The existing row profile still owns its five vector roles. Only
            // this history path admits the additional temporal consumer body.
            if (!C::Profile(image))
                return false;
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!I::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 || static_cast<std::size_t>(dos.e_lfanew) > P::ImageBytes - sizeof(nt) ||
                !I::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto at = static_cast<std::size_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (at > P::ImageBytes || bytes > P::ImageBytes - at || !I::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            for (const auto& pin : Q::Pins)
            {
                bool admitted = false;
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    admitted =
                        admitted || I::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE);
                std::array<unsigned char, Q::MaximumPinBytes> current{};
                if (!admitted || !pin.size || pin.size > current.size() || !I::ReadImage(image, image + pin.rva, current.data(), pin.size, false, true) ||
                    std::memcmp(current.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }

        bool Storage(std::uintptr_t image, std::uint32_t row) noexcept
        {
            if (row >= P::RowCount || !C::Fields(image, row))
                return false;
            const auto offset = P::ArrayOffset + static_cast<std::size_t>(row) * P::RowBytes;
            if (Q::HistoryOffset > P::RowBytes || Q::ArrayBytes > P::RowBytes - Q::HistoryOffset || offset > P::CameraBytes ||
                Q::HistoryOffset > P::CameraBytes - offset || Q::ArrayBytes > P::CameraBytes - offset - Q::HistoryOffset)
                return false;
            const auto target = image + P::CameraRva + offset + Q::HistoryOffset;
            // The SDK exposes the array start. Validate all four constructed
            // triples, even though the existing gravity caller uses only0/1.
            // Timestamps begin immediately after this span and are not lent.
            return !(target % alignof(CVector)) && I::ImagePages(image, target, Q::ArrayBytes, true);
        }

        ClockResult Sample(std::uintptr_t image, std::uint32_t row, CheckOwner owner, CheckOwner afterApplicationOwner, D::Value& output) noexcept
        {
            D::Value   next;
            const auto result = C::Sample(image, row, owner, afterApplicationOwner, next);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!Storage(image, row) || !D::Fence(image, next))
                return ClockResult::BindingChanged;
            if (!Profile(image) || !Storage(image, row) || !D::Fence(image, next))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
    }
}

namespace mta::game_de::NativeCameraDataMemory
{
    ClockResult ReadRowHistory(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                               const NativeCameraRowBorrow::Token& token, CheckOwner owner, CheckOwner afterApplicationOwner, CVector*& output) noexcept
    {
        namespace C = CameraHistoryDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraDataProfile;
        namespace Q = NativeCameraHistoryProfile;
        if (!token || token.Row() >= P::RowCount)
            return ClockResult::InvalidArgument;
        if (!owner || !afterApplicationOwner)
            return ClockResult::NotInitialized;
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageBytes)
            return ClockResult::UnsupportedBuild;
        if (identity.image != image)
            return ClockResult::OwnerConflict;
        if (!D::TryEnter())
            return ClockResult::Busy;
        D::Scope scope;
        auto     result = ledger.Validate(token, identity);
        if (result != ClockResult::Ok)
            return result;
        const auto row = token.Row();
        D::Value   first, current, confirmed;
        if ((result = C::Sample(image, row, owner, afterApplicationOwner, first)) != ClockResult::Ok ||
            (result = C::Sample(image, row, owner, afterApplicationOwner, current)) != ClockResult::Ok)
            return result;
        if (!D::Same(first, current, true))
            return ClockResult::BindingChanged;
        if ((result = C::Sample(image, row, owner, afterApplicationOwner, confirmed)) != ClockResult::Ok)
            return result;
        if (!D::Same(first, confirmed, true) || !D::Same(current, confirmed, true))
            return ClockResult::BindingChanged;
        if ((result = ledger.Validate(token, identity)) != ClockResult::Ok)
            return result;
        // This is the native array under the same live client, not reconstructed
        // history. No timestamp, count, normalization or camera routine runs.
        output = reinterpret_cast<CVector*>(image + P::CameraRva + P::ArrayOffset + static_cast<std::size_t>(row) * P::RowBytes + Q::HistoryOffset);
        return ClockResult::Ok;
    }
}
