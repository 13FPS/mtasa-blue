/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Map a current camera target only through an existing same-owner ped
 ****************************************************************************/
#pragma once
#include "CCamRowDE.h"
namespace mta::game_de
{
    class CPoolsDE;
    class NativeGameTimingAccess;
    // The same enclosing owner keeps pools/provider alive and serializes pool
    // withdrawal with this nonowning SDK entity result. The camera's row hold
    // retains its native backend, not a second ped or pool ownership claim.
    class CCamTargetDE : public CCamRowDE
    {
    public:
        CEntity* GetTargetEntity() const override;

    protected:
        CCamTargetDE(std::uint32_t row, CPoolsDE& pools, NativeGameTimingAccess& owner);

    private:
        static NativeGameTimingAccess& RequireOwner(CPoolsDE&, NativeGameTimingAccess&);
        static void                    Require(ClockResult, const char*);
        CPoolsDE&                      targetPools;
        NativeGameTimingAccess&        targetOwner;
    };
    // The target setter is still a separate native-cell effect contract. A
    // checked getter cannot publish a complete GetCam or CGame factory.
    static_assert(std::is_abstract_v<CCamTargetDE> && std::is_base_of_v<CCamRowDE, CCamTargetDE>);
}
