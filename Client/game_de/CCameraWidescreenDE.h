/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's configured widescreen bool policy on the same owner
 ****************************************************************************/
#pragma once
#include "CCameraCamsDE.h"
namespace mta::game_de
{
    // This adds the actual configured-mode Set/Get pair. It does not derive
    // mode from visible bars or cache a setter request while the engine may
    // change that mode. Existing Cam products still require explicit producer
    // drain before this typed facade is destroyed; no factory is published.
    class CCameraWidescreenDE : public CCameraCamsDE
    {
    public:
        void SetWidescreen(bool enabled) override;
        bool GetWidescreen() override;
        ~CCameraWidescreenDE() noexcept = default;

    protected:
        CCameraWidescreenDE(CPoolsDE& pools, NativeGameTimingAccess& owner);

    private:
        static void                      Require(ClockResult, const char*);
        NativeCameraSphereVisibleAccess& widescreenOwner;
    };
    // Seven actual methods are implemented; the other twenty-eight remain
    // pure. A completed bool pair is not a complete CCamera/CGame factory.
    static_assert(std::is_abstract_v<CCameraWidescreenDE> && std::is_base_of_v<CCameraCamsDE, CCameraWidescreenDE> &&
                  !std::is_copy_constructible_v<CCameraWidescreenDE> && !std::is_move_constructible_v<CCameraWidescreenDE> &&
                  !std::has_virtual_destructor_v<CCameraWidescreenDE>);
}
