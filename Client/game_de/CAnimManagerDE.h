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
class CGame;
namespace mta::game_de
{
    class CAnimManagerDE : public CAnimManager
    {
    public:
        std::unique_ptr<CAnimBlock> GetAnimationBlock(int) override;
        std::unique_ptr<CAnimBlock> GetAnimationBlock(const char*) override;
        std::unique_ptr<CAnimBlock> GetAnimBlock(CAnimBlockSAInterface*) override;
        int                         GetNumAnimations() override;
        int                         GetNumAnimBlocks() override;
        int                         GetAnimationBlockIndex(const char*) override;
        const char*                 GetGateWayBlockName() const override;
        const char*                 GetGateWayAnimationName() const override;

    protected:
        CAnimManagerDE(NativeAnimationManagerAccess& value, std::uint64_t generation) noexcept : native(value), ownerGeneration(generation) {}
        // Factory products borrow these exact providers. They must outlive
        // every returned block; no global game or second native owner is used.
        CAnimManagerDE(NativeAnimationManagerAccess&, NativeAnimationBlockAccess&, CGame&, std::uint64_t);

    private:
        void                          RequireFactory(const char*) const;
        std::unique_ptr<CAnimBlock>   MakeBlock(const NativeAnimationBlockIdentity&, const char*);
        NativeAnimationManagerCounts  Read(const char*) const;
        NativeAnimationManagerAccess& native;
        const std::uint64_t           ownerGeneration;
        NativeAnimationBlockAccess*   blockAccess = nullptr;
        CGame*                        gameProvider = nullptr;
    };
    // These factories create only borrowed CPU block facades. The remaining
    // hierarchy/association services and other manager SDK operations
    // retain their genuine contracts; a complete game is still separate.
    static_assert(std::is_abstract_v<CAnimManagerDE> && std::is_base_of_v<CAnimManager, CAnimManagerDE>);
}
