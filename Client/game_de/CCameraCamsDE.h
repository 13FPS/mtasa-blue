/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Publish stable actual Cam products without a complete camera factory
 ****************************************************************************/
#pragma once
#include "CCameraSphereVisibleDE.h"
#include "CCamTargetSetterDE.h"
#include <array>
#include <atomic>
#include <memory>
namespace mta::game_de
{
    // Publication and all SDK borrows are serialized by the enclosing owner.
    // Close revokes new publication; that owner then quiesces its borrowers,
    // stops native issuance and drains on the Begin/End producer. Pools and
    // provider outlive this facade and every borrowed CCam/vector pointer.
    class CCameraCamsDE : public CCameraSphereVisibleDE
    {
    public:
        CCam*       GetCam(BYTE index) override;
        void        CloseCamPublication() noexcept { publicationClosed.store(true); }
        ClockResult DrainCamsOnProducer() noexcept;
        ~CCameraCamsDE() noexcept;
        CCameraCamsDE(const CCameraCamsDE&) = delete;
        CCameraCamsDE& operator=(const CCameraCamsDE&) = delete;
        CCameraCamsDE(CCameraCamsDE&&) = delete;
        CCameraCamsDE& operator=(CCameraCamsDE&&) = delete;

    protected:
        CCameraCamsDE(CPoolsDE& pools, NativeGameTimingAccess& owner);

    private:
        static void                                        Require(ClockResult, const char*);
        CPoolsDE&                                          camPools;
        NativeGameTimingAccess&                            camOwner;
        std::array<std::unique_ptr<CCamTargetSetterDE>, 3> products{};
        std::atomic<bool>                                  publicationClosed{false};
    };
    // Thirty actual camera methods remain pure. This is neither a complete
    // CCamera nor a CGame::GetCamera publication, and base deletion is invalid.
    static_assert(std::is_abstract_v<CCameraCamsDE> && std::is_base_of_v<CCameraSphereVisibleDE, CCameraCamsDE> &&
                  !std::is_copy_constructible_v<CCameraCamsDE> && !std::is_move_constructible_v<CCameraCamsDE> &&
                  !std::has_virtual_destructor_v<CCameraCamsDE>);
}
