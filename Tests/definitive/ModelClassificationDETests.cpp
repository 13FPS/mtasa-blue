/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA model predicate virtual dispatch without a native model factory
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Access final : NativeModelClassificationAccess
    {
        unsigned                    calls = 0;
        std::uint32_t               key = 0;
        NativeModelVehiclePredicate predicate = NativeModelVehiclePredicate::Boat;
        ClockResult                 status = ClockResult::Ok;
        bool                        value = false;
        unsigned                    typeCalls = 0;
        unsigned char               rawType = 0;
        bool                        hasModel = true;
        ClockResult                 ReadModelType(std::uint32_t model, NativeModelTypeObservation& output) noexcept override
        {
            ++typeCalls;
            key = model;
            if (status == ClockResult::Ok)
                output = {hasModel, rawType};
            return status;
        }
        ClockResult ReadModelVehiclePredicate(std::uint32_t model, NativeModelVehiclePredicate selected, bool& output) noexcept override
        {
            ++calls;
            key = model;
            predicate = selected;
            if (status == ClockResult::Ok)
                output = value;
            return status;
        }
    };
    struct Model final : CModelInfoDE
    {
        Model(std::uint32_t key, NativeModelClassificationAccess& access) : CModelInfoDE(key, access) {}
        [[noreturn]] static void         Unavailable() { throw std::logic_error("unrelated model SDK method in classification fixture"); }
        class CBaseModelInfoSAInterface* GetInterface() override { Unavailable(); }
        bool                             IsPlayerModel() override { Unavailable(); }
        bool                             IsTrain() override { Unavailable(); }
        bool                             IsBike() override { Unavailable(); }
        bool                             IsFakePlane() override { Unavailable(); }
        bool                             IsMonsterTruck() override { Unavailable(); }
        bool                             IsQuadBike() override { Unavailable(); }
        bool                             IsBmx() override { Unavailable(); }
        bool                             IsTrailer() override { Unavailable(); }
        char*                            GetNameIfVehicle() override { Unavailable(); }
        BYTE                             GetVehicleType() const noexcept override { std::abort(); }
        void                             Request(EModelRequestType requestType, const char* szTag /* = NULL*/) override
        {
            static_cast<void>(requestType);
            static_cast<void>(szTag);
            Unavailable();
        }
        bool           IsLoaded() override { Unavailable(); }
        unsigned short GetFlags() override { Unavailable(); }
        unsigned short GetOriginalFlags() override { Unavailable(); }
        void           SetFlags(unsigned short usFlags) override
        {
            static_cast<void>(usFlags);
            Unavailable();
        }
        void SetIdeFlags(unsigned int uiFlags) override
        {
            static_cast<void>(uiFlags);
            Unavailable();
        }
        bool GetIdeFlag(eModelIdeFlag eFlag) override
        {
            static_cast<void>(eFlag);
            Unavailable();
        }
        void SetIdeFlag(eModelIdeFlag eFlag, bool bState) override
        {
            static_cast<void>(eFlag);
            static_cast<void>(bState);
            Unavailable();
        }
        CBoundingBox*    GetBoundingBox() override { Unavailable(); }
        bool             IsValid() override { Unavailable(); }
        mutable unsigned archiveCalls = 0;
        bool             archiveValid = true;
        bool             IsAllocatedInArchive() const noexcept override
        {
            ++archiveCalls;
            return archiveValid;
        }
        unsigned short GetTextureDictionaryID() override { Unavailable(); }
        void           SetTextureDictionaryID(unsigned short usTxdId) override
        {
            static_cast<void>(usTxdId);
            Unavailable();
        }
        void  ResetTextureDictionaryID() override { Unavailable(); }
        float GetLODDistance() override { Unavailable(); }
        float GetOriginalLODDistance() override { Unavailable(); }
        void  SetLODDistance(float fDistance, bool bOverrideMaxDistance = false) override
        {
            static_cast<void>(fDistance);
            static_cast<void>(bOverrideMaxDistance);
            Unavailable();
        }
        void RestreamIPL() override { Unavailable(); }
        bool GetTime(char& hourOn, char& hourOff) override
        {
            static_cast<void>(hourOn);
            static_cast<void>(hourOff);
            Unavailable();
        }
        bool SetTime(char hourOn, char hourOff) override
        {
            static_cast<void>(hourOn);
            static_cast<void>(hourOff);
            Unavailable();
        }
        void ModelAddRef(EModelRequestType requestType, const char* szTag /* = NULL*/) override
        {
            static_cast<void>(requestType);
            static_cast<void>(szTag);
            Unavailable();
        }
        void  RemoveRef() override { Unavailable(); }
        int   GetRefCount() override { Unavailable(); }
        bool  ForceUnload() override { Unavailable(); }
        bool  UnloadUnused() override { Unavailable(); }
        void  DeallocateModel() override { Unavailable(); }
        float GetDistanceFromCentreOfMassToBaseOfModel() override { Unavailable(); }
        void  SetAlphaTransparencyEnabled(bool bEnabled) override
        {
            static_cast<void>(bEnabled);
            Unavailable();
        }
        bool  IsAlphaTransparencyEnabled() override { Unavailable(); }
        void  ResetAlphaTransparency() override { Unavailable(); }
        short GetAvailableVehicleMod(unsigned short usSlot) override
        {
            static_cast<void>(usSlot);
            Unavailable();
        }
        bool IsUpgradeAvailable(eVehicleUpgradePosn posn) override
        {
            static_cast<void>(posn);
            Unavailable();
        }
        void SetCustomCarPlateText(const char* szText) override
        {
            static_cast<void>(szText);
            Unavailable();
        }
        unsigned int GetNumRemaps() override { Unavailable(); }
        void*        GetVehicleSuspensionData() override { Unavailable(); }
        void*        SetVehicleSuspensionData(void* pSuspensionLines) override
        {
            static_cast<void>(pSuspensionLines);
            Unavailable();
        }
        CVector GetVehicleExhaustFumesPosition() override { Unavailable(); }
        void    SetVehicleExhaustFumesPosition(const CVector& position) override
        {
            static_cast<void>(position);
            Unavailable();
        }
        CVector GetVehicleDummyDefaultPosition(VehicleDummies::Enum eDummy) override
        {
            static_cast<void>(eDummy);
            Unavailable();
        }
        CVector GetVehicleDummyPosition(VehicleDummies::Enum eDummy) override
        {
            static_cast<void>(eDummy);
            Unavailable();
        }
        bool GetVehicleDummyPositions(std::array<CVector, static_cast<std::size_t>(VehicleDummies::VEHICLE_DUMMY_COUNT)>& positions) const override
        {
            static_cast<void>(positions);
            Unavailable();
        }
        void SetVehicleDummyPosition(VehicleDummies::Enum eDummy, const CVector& vecPosition) override
        {
            static_cast<void>(eDummy);
            static_cast<void>(vecPosition);
            Unavailable();
        }
        void ResetVehicleDummies(bool bRemoveFromDummiesMap) override
        {
            static_cast<void>(bRemoveFromDummiesMap);
            Unavailable();
        }
        float GetVehicleWheelSize(ResizableVehicleWheelGroup::Enum eWheelGroup) override
        {
            static_cast<void>(eWheelGroup);
            Unavailable();
        }
        void SetVehicleWheelSize(ResizableVehicleWheelGroup::Enum eWheelGroup, float fWheelSize) override
        {
            static_cast<void>(eWheelGroup);
            static_cast<void>(fWheelSize);
            Unavailable();
        }
        void ResetVehicleWheelSizes(std::pair<float, float>* defaultSizes = nullptr) override
        {
            static_cast<void>(defaultSizes);
            Unavailable();
        }
        void InitialiseSupportedUpgrades(RpClump* pClump) override
        {
            static_cast<void>(pClump);
            Unavailable();
        }
        void GetVoice(short* psVoiceType, short* psVoice) override
        {
            static_cast<void>(psVoiceType);
            static_cast<void>(psVoice);
            Unavailable();
        }
        void GetVoice(const char** pszVoiceType, const char** szVoice) override
        {
            static_cast<void>(pszVoiceType);
            static_cast<void>(szVoice);
            Unavailable();
        }
        void SetVoice(short sVoiceType, short sVoice) override
        {
            static_cast<void>(sVoiceType);
            static_cast<void>(sVoice);
            Unavailable();
        }
        void SetVoice(const char* szVoiceType, const char* szVoice) override
        {
            static_cast<void>(szVoiceType);
            static_cast<void>(szVoice);
            Unavailable();
        }
        bool SetCustomModel(RpClump* pClump) override
        {
            static_cast<void>(pClump);
            Unavailable();
        }
        void RestoreOriginalModel() override { Unavailable(); }
        void SetColModel(CColModel* pColModel) override
        {
            static_cast<void>(pColModel);
            Unavailable();
        }
        void      RestoreColModel() override { Unavailable(); }
        void      AddColRef() override { Unavailable(); }
        void      RemoveColRef() override { Unavailable(); }
        void      MakeCustomModel() override { Unavailable(); }
        RwObject* GetRwObject() override { Unavailable(); }
        void      MakePedModel(const char* szTexture) override
        {
            static_cast<void>(szTexture);
            Unavailable();
        }
        void MakeObjectModel(unsigned short usBaseID) override
        {
            static_cast<void>(usBaseID);
            Unavailable();
        }
        void MakeObjectDamageableModel(std::uint16_t baseID) override
        {
            static_cast<void>(baseID);
            Unavailable();
        }
        void MakeVehicleAutomobile(unsigned short usBaseID) override
        {
            static_cast<void>(usBaseID);
            Unavailable();
        }
        void MakeTimedObjectModel(unsigned short usBaseID) override
        {
            static_cast<void>(usBaseID);
            Unavailable();
        }
        void MakeClumpModel(unsigned short usBaseID) override
        {
            static_cast<void>(usBaseID);
            Unavailable();
        }
        SVehicleSupportedUpgrades GetVehicleSupportedUpgrades() override { Unavailable(); }
        void                      ResetSupportedUpgrades() override { Unavailable(); }
        void                      SetObjectPropertiesGroup(unsigned short usObjectGroup) override
        {
            static_cast<void>(usObjectGroup);
            Unavailable();
        }
        unsigned short GetObjectPropertiesGroup() override { Unavailable(); }
        void           RestoreObjectPropertiesGroup() override { Unavailable(); }
        bool           IsTowableBy(CModelInfo* towingModel) override
        {
            static_cast<void>(towingModel);
            Unavailable();
        }
        unsigned int GetParentID() override { Unavailable(); }
        bool         IsDynamic() override { Unavailable(); }
        bool         IsDamageableAtomic() override { Unavailable(); }
    };
    bool Query(CModelInfo& model, NativeModelVehiclePredicate predicate)
    {
        switch (predicate)
        {
            case NativeModelVehiclePredicate::Boat:
                return model.IsBoat();
            case NativeModelVehiclePredicate::Car:
                return model.IsCar();
            case NativeModelVehiclePredicate::Plane:
                return model.IsPlane();
            case NativeModelVehiclePredicate::Heli:
                return model.IsHeli();
        }
        throw std::logic_error("invalid fixture predicate");
    }
}
int main()
{
    try
    {
        Access access;
        for (const auto key : std::array<std::uint32_t, 6>{0, 17, 19999, 20000, 65536, 0xffffffff})
        {
            Model       model(key, access);
            CModelInfo& sdk = model;
            Check(sdk.GetModel() == key, "actual MTA model identifier preserves the entire DWORD without indexing native arrays");
            for (const auto& entry : NativeModelClassificationProfile::Entries)
                for (bool value : {false, true})
                {
                    access.value = value;
                    const auto calls = access.calls;
                    Check(Query(sdk, entry.predicate) == value && access.calls == calls + 1 && access.key == key && access.predicate == entry.predicate,
                          "actual SDK classifier method forwards full key and exact named predicate once, preserving true and false");
                }
            for (unsigned char rawType : std::array<unsigned char, 8>{0, 1, 3, 5, 6, 7, 8, 255})
            {
                access.rawType = rawType;
                const auto types = access.typeCalls;
                Check(static_cast<unsigned char>(sdk.GetModelType()) == rawType && access.typeCalls == types + 1 && access.key == key,
                      "actual SDK GetModelType preserves the current native AL byte without vtable identity substitution");
                const auto beforeType = access.typeCalls, beforeArchive = model.archiveCalls;
                const bool vehicle = sdk.IsVehicle();
                Check(vehicle == (key < 20000 && rawType == 6) && access.typeCalls == beforeType + (key < 20000 ? 1 : 0) &&
                          model.archiveCalls == beforeArchive + (key < 20000 ? 1 : 0),
                      "actual MTA IsVehicle preserves model bounds and archive admission before native type comparison");
            }
            access.hasModel = false;
            Check(sdk.GetModelType() == eModelInfoType::UNKNOWN, "actual classic absent-interface GetModelType maps only qualified absence to UNKNOWN");
            access.hasModel = true;
            model.archiveValid = false;
            const auto beforeType = access.typeCalls;
            Check(!sdk.IsVehicle() && access.typeCalls == beforeType, "unallocated archive cannot be replaced by a known vehicle-type byte");
            model.archiveValid = true;
            for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
            {
                access.status = static_cast<ClockResult>(status);
                const auto typeCalls = access.typeCalls;
                bool       typeRefused = false;
                try
                {
                    (void)sdk.GetModelType();
                }
                catch (const std::runtime_error&)
                {
                    typeRefused = true;
                }
                Check(typeRefused && access.typeCalls == typeCalls + 1, "actual GetModelType preserves owner failure without fabricating Unknown or retry");
                for (const auto& entry : NativeModelClassificationProfile::Entries)
                {
                    const auto calls = access.calls;
                    bool       refused = false;
                    try
                    {
                        (void)Query(sdk, entry.predicate);
                    }
                    catch (const std::runtime_error& e)
                    {
                        refused = std::string(e.what()).find("DE model owner refused vehicle predicate") != std::string::npos;
                    }
                    Check(refused && access.calls == calls + 1, "all owner refusals remain explicit SDK errors without a negative result or retry");
                }
            }
            access.status = ClockResult::Ok;
        }
        NativeModelClassificationAccess absent;
        Model                           unbound(17, absent);
        bool                            refused = false;
        try
        {
            (void)unbound.IsBoat();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, "old/default owner cannot fake a supported false model classification");
        refused = false;
        try
        {
            (void)unbound.GetModelType();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, "unbound raw metadata type cannot manufacture an SDK enum result");
        std::puts("Production MTA model type SDK fixture completed");
        std::puts("Production MTA model classification SDK fixture completed");
        std::printf("Passed %u actual MTA model-classification SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Model classification SDK failed after %u checks: %s\n", checks, e.what());
        return 1;
    }
}
