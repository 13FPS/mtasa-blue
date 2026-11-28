/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the bounded actual camera operations on the common SDK
 *****************************************************************************/
#pragma once
#include "NativeCameraSphereVisibleAccess.h"
#include <SharedUtil.IntTypes.h>
// The shared entity header retains its legacy unscoped bone-enum forward
// declaration. Keep that existing MSVC include contract local to the SDK.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CCamera.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <type_traits>

namespace mta::game_de
{
    class CCameraSphereVisibleDE : public CCamera
    {
    public:
        bool IsSphereVisible(CVector* center, float radius) const override;
        BYTE GetActiveCam() override;
        void Restore() override;
        void RestoreWithJumpCut() override;
        bool IsInTransition() const override;
        void ResetCameraClip() override;

    protected:
        explicit CCameraSphereVisibleDE(NativeCameraSphereVisibleAccess& owner) noexcept : cameraOwner(owner) {}

    private:
        NativeCameraSphereVisibleAccess& cameraOwner;
    };
    // Reading the transition state and applying clipping still require a backend.
    static_assert(std::is_abstract_v<CCameraSphereVisibleDE> && std::is_base_of_v<CCamera, CCameraSphereVisibleDE>);
}
