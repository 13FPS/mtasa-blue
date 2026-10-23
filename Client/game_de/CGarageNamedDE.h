/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual named garage type and door properties through native ownership
 ****************************************************************************/
#pragma once
#include "NativeNamedGarageAccess.h"
#include <windows.h>
#include <game/CGarage.h>
#include <type_traits>

namespace mta::game_de
{
    // A name is explicit borrowed-runtime lookup identity, not an SDK numeric
    // garage ID or a lease on a native row. All eight SDK properties share
    // this one explicit name; numeric garage factories remain separate.
    class CGarageNamedDE : public CGarage
    {
    public:
        void SetType(BYTE type) override;
        BYTE GetType() const override;
        void GetPosition(CVector& position) const override;
        void GetDirection(float& x1, float& y1, float& x2, float& y2) const override;
        void GetSize(float& height, float& width, float& depth) const override;
        void GetBoundingBox(float& left, float& right, float& front, float& back) const override;
        void SetOpen(bool open) override;
        bool IsOpen() const override;

    protected:
        CGarageNamedDE(NativeNamedGarageAccess&, const NativeGarageName&);

    private:
        NativeGarageGeometry     ReadGeometry(const char* operation) const;
        static void              Require(ClockResult, const char* operation);
        NativeNamedGarageAccess& native;
        const NativeGarageName   name;
    };
    static_assert(!std::is_abstract_v<CGarageNamedDE> && std::is_base_of_v<CGarage, CGarageNamedDE>);
}
