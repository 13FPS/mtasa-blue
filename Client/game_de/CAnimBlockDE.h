/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual animation-block metadata through one borrowed native owner
 ****************************************************************************/
#pragma once
#include "NativeAnimationBlockAccess.h"
#include <cstddef>
#include <game/CAnimBlock.h>
#include <type_traits>
namespace mta::game_de
{
    class CAnimBlockDE : public CAnimBlock
    {
    public:
        char*       GetName() override;
        int         GetIndex() override;
        bool        IsLoaded() override;
        int         GetIDOffset() override;
        std::size_t GetAnimationCount() override;

    protected:
        CAnimBlockDE(NativeAnimationBlockAccess&, const NativeAnimationBlockIdentity&);

    private:
        NativeAnimationBlockValues         Read(const char* operation) const;
        NativeAnimationBlockAccess&        native;
        const NativeAnimationBlockIdentity identity;
    };
    // Native refs/requests and classic raw interfaces still require their real
    // contracts. A borrowed name and metadata are not a complete factory.
    static_assert(std::is_abstract_v<CAnimBlockDE> && std::is_base_of_v<CAnimBlock, CAnimBlockDE>);
}
