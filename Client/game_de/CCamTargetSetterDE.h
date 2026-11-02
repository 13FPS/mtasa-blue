/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Issue bounded MTA camera targets through the existing native owner
 ****************************************************************************/
#pragma once
#include "CCamTargetDE.h"
namespace mta::game_de
{
    // This completes the thirteen CCam methods on a retained logical row.
    // Construction remains an enclosing owner's protected operation; it does
    // not publish CCamera::GetCam or a CGame factory. Pools and provider must
    // outlive this typed object, with registry withdrawal serialized as before.
    class CCamTargetSetterDE : public CCamTargetDE
    {
    public:
        void SetTargetEntity(CEntity* entity) override;
        ~CCamTargetSetterDE() noexcept = default;

    protected:
        CCamTargetSetterDE(std::uint32_t row, CPoolsDE& pools, NativeGameTimingAccess& owner)
            : CCamTargetDE(row, pools, owner), setterPools(pools), setterOwner(owner)
        {
        }

    private:
        friend class CCameraCamsDE;
        ClockResult ValidatePublication() const noexcept;
        ClockResult CheckProducerDrain() const noexcept;
        static void Require(ClockResult);
        // These are the identical references already checked by the base
        // before row acquisition, not a second owner or an owned pool.
        CPoolsDE&               setterPools;
        NativeGameTimingAccess& setterOwner;
    };
    static_assert(!std::is_abstract_v<CCamTargetSetterDE> && std::is_base_of_v<CCamTargetDE, CCamTargetSetterDE> &&
                  !std::is_copy_constructible_v<CCamTargetSetterDE> && !std::is_move_constructible_v<CCamTargetSetterDE> &&
                  !std::has_virtual_destructor_v<CCamTargetSetterDE>);
}
