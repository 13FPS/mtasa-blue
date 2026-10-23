/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve real world removal, native Euler placement and actor/world add
 ****************************************************************************/
#include "NativePedOrientationMemory.h"
#include "NativePedOrientationProfile.h"
#include "NativePedPoseContext.h"
#include "NativePedPoseMutation.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <utility>
namespace mta::game_de
{
    namespace PedOrientationDetail
    {
#if !defined(MTA_DE_PED_ORIENTATION_NATIVE_INVOKE)
        ClockResult NativeCall(NativePedOrientationMemory::Phase phase, const NativePedOrientationMemory::Entries& entries, void* ped, const CVector& radians,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                if (phase == NativePedOrientationMemory::Phase::Remove)
                    entries.remove(ped);
                else if (phase == NativePedOrientationMemory::Phase::Rotate)
                    entries.rotate(ped, radians.fX, radians.fY, radians.fZ);
                else
                    entries.add(ped);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    bool NativePedOrientationMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedTransformReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedOrientationMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativePedOrientationProfile;
        namespace I = NativeImageAccess;
        if (!NativePedPoseContext::Pins(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!I::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !I::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || !nt.FileHeader.NumberOfSections ||
            nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : P::Code)
        {
            bool contains = false;
            for (std::size_t i = 0; i < nt.FileHeader.NumberOfSections; ++i)
                if (I::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contains = true;
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains ||
                !I::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedOrientationMemory::Apply(std::uint32_t handle, const CVector& input, Receipt* output, Invoke invoke) const noexcept
    {
        namespace C = NativePedPoseContext;
        namespace P = NativePedOrientationProfile;
        Receipt receipt;
        receipt.handle = handle;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        // This is the same gate used by actual placement and Teleport. Native
        // engine internals are not redirected; only adapter recursion refuses.
        NativePedPoseMutation scope;
        if (!scope)
            return finish(ClockResult::Busy);
        auto result = C::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!C::Read(reinterpret_cast<std::uintptr_t>(&input), &receipt.requested, sizeof(receipt.requested)))
            return finish(ClockResult::InvalidArgument);
        try
        {
            C::Context current;
            result = C::Capture(binding, owner, handle, current, C::MatrixMode::OrInline);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.nativePed = current.transform.ped.nativePed;
            receipt.matrixBefore = current.transform.matrix;
            receipt.matrixAfter = receipt.matrixBefore;
            const Entries entries{reinterpret_cast<WorldFunction>(binding.image + P::RemoveRva), reinterpret_cast<RotateFunction>(binding.image + P::RotateRva),
                                  reinterpret_cast<WorldFunction>(binding.image + P::AddRva)};
#if defined(MTA_DE_PED_ORIENTATION_NATIVE_INVOKE)
            const auto call = invoke ? invoke : &MTA_DE_PED_ORIENTATION_NATIVE_INVOKE;
#else
            const auto call = invoke ? invoke : &PedOrientationDetail::NativeCall;
#endif
            for (unsigned index = 0; index != 3; ++index)
            {
                if (!Revalidate(binding))
                    return finish(ClockResult::BindingChanged);
                if ((result = C::Fence(binding, owner, current)) != ClockResult::Ok)
                    return finish(result);
                const auto phase = static_cast<Phase>(index);
                const auto mask = static_cast<std::uint8_t>(1u << index);
                receipt.entered = true;
                receipt.enteredMask |= mask;
                result = call(phase, entries, reinterpret_cast<void*>(receipt.nativePed), receipt.requested, receipt.exceptionCode);
                if (result != ClockResult::Ok)
                    return finish(result);
                receipt.returnedMask |= mask;
                C::Context after;
                result = C::Capture(binding, owner, handle, after, C::MatrixMode::OrInline);
                if (result != ClockResult::Ok)
                    return finish(result);
                if (!C::SameLifetime(current, after))
                    return finish(ClockResult::BindingChanged);
                // Remove legitimately retires old world/moving-list nodes;
                // Add may create new ones. Never inspect an old snapshot as
                // authority after entry or try to put its list bytes back.
                current = std::move(after);
                receipt.matrixAfter = current.transform.matrix;
            }
            receipt.afterObserved = true;
            return finish(C::Owner(binding, owner));
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
