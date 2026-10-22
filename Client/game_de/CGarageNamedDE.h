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
    // garage ID or a lease on a native row. Geometry remains unimplemented.
    class CGarageNamedDE : public CGarage
    {
    public:
        void SetType(BYTE type) override;
        BYTE GetType() const override;
        void SetOpen(bool open) override;
        bool IsOpen() const override;

    protected:
        CGarageNamedDE(NativeNamedGarageAccess&, const NativeGarageName&);

    private:
        static void              Require(ClockResult, const char* operation);
        NativeNamedGarageAccess& native;
        const NativeGarageName   name;
    };
    static_assert(std::is_abstract_v<CGarageNamedDE> && std::is_base_of_v<CGarage, CGarageNamedDE>);
}
