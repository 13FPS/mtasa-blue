/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the current camera entry without ped bounds or CPU frustum math
 ****************************************************************************/
#include "NativeCameraSphereVisibleCalls.h"
#include "NativeCameraSphereVisibleProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <type_traits>
namespace mta::game_de::NativeCameraSphereVisibleCalls
{
    namespace
    {
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraSphereVisibleProfile;
        static_assert(std::is_standard_layout_v<CVector> && std::is_trivially_copyable_v<CVector> && sizeof(CVector) == 12 && alignof(CVector) == 4);
        bool Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return D::Profile(image);
        }
        bool Camera(std::uintptr_t image) noexcept
        {
            std::uintptr_t                  matrix = 0;
            std::array<unsigned char, 0x40> local{};
            // The loaded typed world owns this exact static native camera.
            // Check only immediate current storage; native may update its
            // matrix/planes, so their prior contents are not lifetime tokens.
            return NativeImageAccess::ImagePages(image, image + P::CameraRva, P::CameraBytes, true) &&
                   NativeImageAccess::ImagePages(image, image + P::DepthRva, 8) &&
                   NativeImageAccess::ReadImage(image, image + P::CameraRva + 0x18, &matrix, sizeof(matrix)) &&
                   (!matrix || D::Copy(matrix, local.data(), local.size()));
        }
        ClockResult Capture(std::uintptr_t image, CheckOwner owner, D::Value& graph) noexcept
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image) || !Camera(image))
                return ClockResult::BindingChanged;
            if ((result = D::Capture(image, graph)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, graph) || !Profile(image) || !Camera(image))
                return ClockResult::BindingChanged;
            return D::Owner(image, owner);
        }
#if !defined(MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t camera, const CVector& center, float radius, bool& value,
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
    ClockResult Read(std::uintptr_t image, const CVector& source, float radius, bool& output, CheckOwner owner, Receipt* receiptOutput, Invoke invoke) noexcept
    {
        CVector center;
        std::memcpy(&center, &source, sizeof(center));
        Receipt receipt;
        std::memcpy(&receipt.center, &center, sizeof(center));
        std::memcpy(&receipt.radius, &radius, sizeof(radius));
        receipt.camera = image + P::CameraRva;
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
        D::Value first, current;
        if ((result = Capture(image, owner, first)) != ClockResult::Ok || (result = Capture(image, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!D::Same(first, current, true))
            return finish(ClockResult::BindingChanged);
        receipt.root = first.root;
        receipt.gameMode = first.gameMode;
        receipt.level = first.level;
        receipt.world = first.world;
#if defined(MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        bool value = false;
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), receipt.camera, center, radius, value, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        receipt.value = value;
        // Reacquire authority around the one genuine native call. Accept
        // coherent new camera matrices/planes and world mode; never restore
        // earlier native state or publish an unacknowledged boolean.
        if ((result = Capture(image, owner, current)) != ClockResult::Ok)
            return finish(result);
        if (!D::Same(first, current, false))
            return finish(ClockResult::BindingChanged);
        output = value;
        return finish(ClockResult::Ok);
    }
}
