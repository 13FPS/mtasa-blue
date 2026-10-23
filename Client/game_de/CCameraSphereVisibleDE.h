/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual const camera sphere method on the common SDK
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

    protected:
        explicit CCameraSphereVisibleDE(NativeCameraSphereVisibleAccess& owner) noexcept : cameraOwner(owner) {}

    private:
        NativeCameraSphereVisibleAccess& cameraOwner;
    };
    // One native observation supplies neither a full camera nor Reset/Fade,
    // target or transform ownership. All remaining actual SDK methods stay pure.
    static_assert(std::is_abstract_v<CCameraSphereVisibleDE> && std::is_base_of_v<CCamera, CCameraSphereVisibleDE>);
}
