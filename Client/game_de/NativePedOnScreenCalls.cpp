/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse native bound helpers and camera visibility in exact named order
 ****************************************************************************/
#include "NativePedOnScreenCalls.h"
#include "NativePedOnScreenProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeModelBankReader.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <type_traits>
namespace mta::game_de::NativePedOnScreenCalls
{
    namespace
    {
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativePedOnScreenProfile;
        static_assert(std::is_standard_layout_v<CVector> && std::is_trivially_copyable_v<CVector> && sizeof(CVector) == 12 && alignof(CVector) == 4);
        bool Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            std::uintptr_t group = 0;
            std::uint32_t  branch = 0;
            unsigned char  selector = 0;
            return NativeImageAccess::ReadImage(image, image + P::GroupSlotRva, &group, sizeof(group)) && group == image + P::GroupRva &&
                   NativeImageAccess::ReadImage(image, image + P::RemapRva, &selector, sizeof(selector)) && selector == P::Selector &&
                   NativeImageAccess::ReadImage(image, image + P::TableSlotRva, &branch, sizeof(branch)) && branch == P::BranchRva &&
                   NativePedMetadataMemory::Revalidate({image}) && NativeModelBankReader::Revalidate({image}) && D::Profile(image);
        }
        bool Camera(std::uintptr_t image) noexcept
        {
            std::uintptr_t                  matrix = 0;
            std::array<unsigned char, 0x40> copied{};
            // State9 and the current typed world admit the existing engine
            // camera. Check its immediate native storage, not a second camera
            // adapter/token or frozen frustum values that native may update.
            return NativeImageAccess::ImagePages(image, image + P::CameraRva, P::CameraBytes, true) &&
                   NativeImageAccess::ImagePages(image, image + P::DepthRva, 8) &&
                   NativeImageAccess::ImagePages(image, image + P::FallbackCollisionRva, 0x28) &&
                   NativeImageAccess::ReadImage(image, image + P::CameraRva + 0x18, &matrix, sizeof(matrix)) &&
                   (!matrix || D::Copy(matrix, copied.data(), copied.size()));
        }
        struct Context
        {
            D::Value                     graph;
            NativePedMetadata            metadata;
            NativeModelBankReader::Value model;
        };
        ClockResult Capture(std::uintptr_t image, const Ped& expected, CheckOwner owner, Context& value) noexcept
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image) || !Camera(image))
                return ClockResult::BindingChanged;
            if ((result = NativePedMetadataMemory({image}, owner).Read(expected, value.metadata)) != ClockResult::Ok ||
                (result = NativeModelBankReader({image}, owner).Read(value.metadata.modelIndex, value.model)) != ClockResult::Ok ||
                (result = D::Capture(image, value.graph)) != ClockResult::Ok)
                return result;
            // The native signed-WORD lookup must remain inside the actual
            // model bank. Null is a proved native fallback, not modelzero.
            // Native owns any model/UEasset resolution behind that association.
            if ((result = NativeModelBankReader({image}, owner).Recheck(value.model)) != ClockResult::Ok ||
                (result = NativePedPoolReader({image}, owner).Recheck(expected)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, value.graph) || !Profile(image) || !Camera(image))
                return ClockResult::BindingChanged;
            return D::Owner(image, owner);
        }
        bool Same(const Context& a, const Context& b, bool sameMode) noexcept
        {
            return D::Same(a.graph, b.graph, sameMode) && a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType &&
                   a.model.slotAddress == b.model.slotAddress && a.model.nativeModelInfo == b.model.nativeModelInfo;
        }
#if !defined(MTA_DE_PED_ONSCREEN_NATIVE_INVOKE)
        ClockResult NativeCall(RadiusFunction function, std::uintptr_t ped, float& radius, std::uint32_t& exception) noexcept
        {
            __try
            {
                radius = function(ped);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult NativeCall(CenterFunction function, std::uintptr_t ped, CVector& center, CVector*& returned, std::uint32_t& exception) noexcept
        {
            __try
            {
                returned = function(ped, &center);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult NativeCall(ScreenFunction function, std::uintptr_t camera, const CVector& center, float radius, bool& value,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                value = function(camera, &center, radius);
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
    ClockResult Read(std::uintptr_t image, const Ped& source, bool& output, CheckOwner owner, Receipt* receiptOutput, const Invoke* invoke) noexcept
    {
        // Copy the resolved external receiver before callbacks can mutate the
        // caller's Value. No current-player substitution or native pointer is
        // borrowed from a previous callback by this composed operation.
        const Ped expected = source;
        Receipt   receipt;
        receipt.handle = expected.handle;
        receipt.nativePed = expected.nativePed;
        receipt.camera = image + NativePedOnScreenProfile::CameraRva;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        auto     result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        Context first, current;
        if ((result = Capture(image, expected, owner, first)) != ClockResult::Ok || (result = Capture(image, expected, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, true))
            return finish(ClockResult::BindingChanged);
        receipt.root = first.graph.root;
        receipt.gameMode = first.graph.gameMode;
        receipt.level = first.graph.level;
        receipt.world = first.graph.world;
        receipt.modelIndex = first.metadata.modelIndex;
#if defined(MTA_DE_PED_ONSCREEN_NATIVE_INVOKE)
        const auto radiusEntry = invoke && invoke->radius ? invoke->radius : static_cast<RadiusInvoke>(&MTA_DE_PED_ONSCREEN_NATIVE_INVOKE);
        const auto centerEntry = invoke && invoke->center ? invoke->center : static_cast<CenterInvoke>(&MTA_DE_PED_ONSCREEN_NATIVE_INVOKE);
        const auto screenEntry = invoke && invoke->screen ? invoke->screen : static_cast<ScreenInvoke>(&MTA_DE_PED_ONSCREEN_NATIVE_INVOKE);
#else
        const auto radiusEntry = invoke && invoke->radius ? invoke->radius : static_cast<RadiusInvoke>(&NativeCall);
        const auto centerEntry = invoke && invoke->center ? invoke->center : static_cast<CenterInvoke>(&NativeCall);
        const auto screenEntry = invoke && invoke->screen ? invoke->screen : static_cast<ScreenInvoke>(&NativeCall);
#endif
        float    radius = 0;
        CVector  center;
        CVector* returnedCenter = nullptr;
        bool     value = false;
        receipt.entered = true;
        result = radiusEntry(reinterpret_cast<RadiusFunction>(image + P::RadiusRva), expected.nativePed, radius, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.completedMask = 1;
        receipt.radius = radius;
        if ((result = Capture(image, expected, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, false))
            return finish(ClockResult::BindingChanged);
        result = centerEntry(reinterpret_cast<CenterFunction>(image + P::CenterRva), expected.nativePed, center, returnedCenter, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.completedMask = 3;
        receipt.center = center;
        if (returnedCenter != &center)
            return finish(ClockResult::BindingChanged);
        if ((result = Capture(image, expected, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, false))
            return finish(ClockResult::BindingChanged);
        result = screenEntry(reinterpret_cast<ScreenFunction>(image + P::ScreenRva), receipt.camera, center, radius, value, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.completedMask = 7;
        receipt.returned = true;
        receipt.value = value;
        // Reacquire current world/ped/model association after native effects.
        // Do not demand equal native cached bounds, camera matrices or frusta,
        // and never roll back lazy asset changes on failed acknowledgement.
        if ((result = Capture(image, expected, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!Same(first, current, false))
            return finish(ClockResult::BindingChanged);
        output = value;
        return finish(ClockResult::Ok);
    }
}
