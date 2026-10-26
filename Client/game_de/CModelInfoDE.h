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
    // Remaining reference, collision and mutation contracts stay pure. The
    // raw handles below are current opaque observations, not classic layouts
    // or a complete CGame model factory/native lifetime lease.
    class CModelInfoDE : public CModelInfo
    {
    public:
        CBaseModelInfoSAInterface* GetInterface() override;
        RwObject*                  GetRwObject() override;
        DWORD                      GetModel() override { return model; }
        eModelInfoType             GetModelType() override;
        bool                       IsVehicle() const override;
        bool                       IsAllocatedInArchive() const override;
        bool                       IsBoat() override;
        bool                       IsCar() override;
        bool                       IsPlane() override;
        bool                       IsHeli() override;
        bool                       IsTowableBy(CModelInfo* towingModel) override;

    protected:
        // Request dependencies consume the native getter's EAX result. Keep
        // that observation in this same owner rather than dereferencing an
        // opaque interface as a classic model or caching the animation value.
        std::uint32_t GetAnimFileIndex();

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
