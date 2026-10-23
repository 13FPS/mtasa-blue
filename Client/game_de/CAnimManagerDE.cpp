/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native counter and missing-name facts without fallback
 ****************************************************************************/
#include "CAnimManagerDE.h"
#include <game/CAnimGatewayNames.h>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE animation manager owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
        }
    }
    NativeAnimationManagerCounts CAnimManagerDE::Read(const char* operation) const
    {
        NativeAnimationManagerCounts next;
        Require(native.ReadAnimationManagerCounts(ownerGeneration, next), operation);
        return next;
    }
    int CAnimManagerDE::GetNumAnimations()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetNumAnimations").animations;
    }
    int CAnimManagerDE::GetNumAnimBlocks()
    {
        return Read("GetNumAnimBlocks").blocks;
    }
    // Gateway names belong to MTA itself. A native observation refusal must
    // not turn these authored constants into missing or synthetic data.
    const char* CAnimManagerDE::GetGateWayBlockName() const
    {
        return MtaGame::CAnimGatewayNames::GetBlockName();
    }
    const char* CAnimManagerDE::GetGateWayAnimationName() const
    {
        return MtaGame::CAnimGatewayNames::GetAnimationName();
    }
    int CAnimManagerDE::GetAnimationBlockIndex(const char* name)
    {
        if (!name)
            throw std::invalid_argument("DE animation block lookup requires a name");
        NativeAnimationBlockName copied{};
        std::size_t              i = 0;
        for (; i != copied.size(); ++i)
        {
            copied[i] = static_cast<unsigned char>(name[i]);
            if (!copied[i])
                break;
        }
        if (i == copied.size())
            throw std::invalid_argument("DE animation block lookup requires a terminated name within16bytes");
        std::int32_t index = -1;
        Require(native.FindAnimationBlockIndex(ownerGeneration, copied, index), "GetAnimationBlockIndex");
        if (index < -1)
            throw std::runtime_error("DE animation manager owner returned an invalid missing-name sentinel");
        // The genuine named native caller returns -1 for a missing block. Do
        // not turn an owner failure into that valid, observable result.
        return index;
    }
}
