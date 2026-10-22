/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse real MTA model predicates without publishing an incomplete model factory
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <game/CModelInfo.h>
#include "NativeModelClassificationAccess.h"
#include <type_traits>
namespace mta::game_de
{
    // The remaining model, reference, render/collision and mutation operations
    // stay pure. This is a reusable SDK slice, not a CGame model factory or a
    // classic-model/RwObject pointer cast from DE's metadata bank.
    class CModelInfoDE : public CModelInfo
    {
    public:
        DWORD          GetModel() override { return model; }
        eModelInfoType GetModelType() override;
        bool           IsVehicle() const override;
        bool           IsAllocatedInArchive() const noexcept override;
        bool           IsBoat() override;
        bool           IsCar() override;
        bool           IsPlane() override;
        bool           IsHeli() override;

    protected:
        CModelInfoDE(std::uint32_t index, NativeModelClassificationAccess& owner) noexcept : model(index), access(owner) {}

    private:
        bool                             Query(NativeModelVehiclePredicate);
        eModelInfoType                   QueryType() const;
        const std::uint32_t              model;
        NativeModelClassificationAccess& access;
    };
    static_assert(sizeof(DWORD) == sizeof(std::uint32_t));
    static_assert(std::is_abstract_v<CModelInfoDE> && std::is_base_of_v<CModelInfo, CModelInfoDE>);
}
