/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual SDK distance limits, float bits and original-cache reads
 ****************************************************************************/
#include "CModelInfoLodDE.h"
#include <game/CModelLodDistancePolicy.h>
#include <cstring>
#include <stdexcept>
#include <string>
namespace mta::game_de::ModelLodSdk
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE ") + operation + " refused: " + std::to_string(static_cast<unsigned>(result)));
        }
        float Decode(std::uint32_t bits) noexcept
        {
            float value = 0;
            static_assert(sizeof(value) == sizeof(bits));
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
    }
    NativeModelClassificationAccess& RequireSameOwner(NativeModelClassificationAccess& classification, NativeModelLodAccess& lod)
    {
        // An existing model key cannot be rebound by passing a different
        // writer beside its classification/time/wheel/group/dummy owners.
        if (dynamic_cast<NativeModelLodAccess*>(&classification) != &lod)
            throw std::runtime_error("DE model LOD methods require the same classification owner");
        return classification;
    }
    float Get(NativeModelLodAccess& owner, std::uint32_t model)
    {
        std::uint32_t bits = 0;
        Require(owner.ReadModelLodDistance(model, bits), "GetLODDistance");
        return Decode(bits);
    }
    float GetOriginal(NativeModelLodAccess& owner, std::uint32_t model)
    {
        std::uint32_t bits = 0;
        // The native owner reads its actual model-ID default cache. The live
        // distance is never substituted when that cache has no original.
        Require(owner.ReadOriginalModelLodDistance(model, bits), "GetOriginalLODDistance");
        return Decode(bits);
    }
    void Set(NativeModelLodAccess& owner, std::uint32_t model, float distance, bool overrideMaxDistance)
    {
        // Share the exact classic std::min argument order before owner access.
        // This keeps signed zero, negative values and NaN payloads while only
        // limiting values above 325 when the caller did not request override.
        const float   limited = MtaGame::CModelLodDistancePolicy::Limit(distance, overrideMaxDistance);
        std::uint32_t bits = 0;
        std::memcpy(&bits, &limited, sizeof(bits));
        Require(owner.SetModelLodDistance(model, bits), "SetLODDistance");
    }
}
