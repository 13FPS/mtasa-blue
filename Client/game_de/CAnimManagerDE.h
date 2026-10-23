/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual animation-manager observations through one borrowed owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerAccess.h"
// The common SDK expects BYTE/DWORD from the classic PCH. Standalone DE
// consumers need the same Windows types in both x86 and x64 compilation.
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <game/CAnimManager.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimManagerDE : public CAnimManager
    {
    public:
        int GetNumAnimations() override;
        int GetNumAnimBlocks() override;
        int GetAnimationBlockIndex(const char*) override;

    protected:
        CAnimManagerDE(NativeAnimationManagerAccess& value, std::uint64_t generation) noexcept : native(value), ownerGeneration(generation) {}

    private:
        NativeAnimationManagerCounts  Read(const char*) const;
        NativeAnimationManagerAccess& native;
        const std::uint64_t           ownerGeneration;
    };
    // Registering/loading/refcounting and actual block/hierarchy factories keep
    // their genuine mandatory contracts. Three observations do not publish a
    // complete manager or replace any classic raw animation interface.
    static_assert(std::is_abstract_v<CAnimManagerDE> && std::is_base_of_v<CAnimManager, CAnimManagerDE>);
}
