/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify actual native loader-name operands through MTA's CGame SDK
 *****************************************************************************/
#pragma once
#include <game/CGameAdapter.h>
#include <atomic>

namespace mta::game_de
{
    class NativeDataFileNamesReader final
    {
    public:
        Status Bind() noexcept;
        Status Read(bool& verified) const noexcept;
        void   Reset() noexcept;
        bool   IsBound() const noexcept;

    private:
        // Binding grants caller-thread read ownership only. It cannot be used
        // to open game files, invoke loaders or alter native filename operands.
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
