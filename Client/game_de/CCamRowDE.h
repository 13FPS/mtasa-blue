/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Return current native camera vectors under one retained row client
 ****************************************************************************/
#pragma once
#include "CCamFovDE.h"
#include "NativeCameraRowBorrow.h"
#include <type_traits>
namespace mta::game_de
{
    class CCamRowDE : public CCamFovDE
    {
    public:
        ~CCamRowDE() noexcept;
        CCamRowDE(const CCamRowDE&) = delete;
        CCamRowDE& operator=(const CCamRowDE&) = delete;
        CCamRowDE(CCamRowDE&&) = delete;
        CCamRowDE& operator=(CCamRowDE&&) = delete;
        CVector*   GetFront() const override;
        CVector*   GetUp() const override;
        CVector*   GetSource() const override;
        CVector*   GetFixedModeSource() const override;
        CVector*   GetFixedModeVector() const override;

    protected:
        // Acquire the same provider's row hold before any actual native vector
        // address can escape. This supplies no GetCam/CGame camera factory.
        CCamRowDE(std::uint32_t row, NativeCameraSphereVisibleAccess& provider);

    private:
        CVector*                         Read(CameraRowVectorKind, const char*) const;
        NativeCameraSphereVisibleAccess& rowOwner;
        NativeCameraRowBorrow::Token     token;
    };
    // The shared SDK has no virtual destructor. Only typed destruction is
    // supported here; History and both entity methods intentionally stay pure.
    static_assert(std::is_abstract_v<CCamRowDE> && std::is_base_of_v<CCamFovDE, CCamRowDE> && !std::is_copy_constructible_v<CCamRowDE> &&
                  !std::is_copy_assignable_v<CCamRowDE> && !std::is_move_constructible_v<CCamRowDE> && !std::is_move_assignable_v<CCamRowDE>);
}
