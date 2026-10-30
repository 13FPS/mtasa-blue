/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual CCam FOV policies on an immutable logical native row
 ****************************************************************************/
#pragma once
#include "NativeCameraSphereVisibleAccess.h"
#include <game/CCam.h>
#include <type_traits>
namespace mta::game_de
{
    class CCamFovDE : public CCam
    {
    public:
        float GetFOV() const override;
        void  SetFOV(float value) override;

    protected:
        // A logical immutable index is re-admitted by the same native owner
        // on each read. It exposes no native Cam pointer or writable vectors.
        CCamFovDE(std::uint32_t index, NativeCameraSphereVisibleAccess& owner) noexcept : logicalRow(index), cameraOwner(owner) {}

    private:
        const std::uint32_t              logicalRow;
        NativeCameraSphereVisibleAccess& cameraOwner;
    };
    // A scalar observation cannot manufacture the remaining actual Cam SDK
    // contracts or a complete CCamera::GetCam/CGame camera factory.
    static_assert(std::is_abstract_v<CCamFovDE> && std::is_base_of_v<CCam, CCamFovDE>);
}
