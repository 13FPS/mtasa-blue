/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA model predicate virtual dispatch without a native model factory
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include <array>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
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
        unsigned                    archiveCalls = 0;
        ClockResult                 archiveStatus = ClockResult::Ok;
        bool                        archiveValid = true;
        bool                        poisonArchiveOnRefusal = false, poisonTypeOnRefusal = false, traceEnabled = false;
        std::array<unsigned, 4>     events{};
        unsigned                    eventCount = 0;
        void                        Mark(unsigned event) noexcept
        {
            if (!traceEnabled)
                return;
            if (eventCount == events.size())
                std::abort();
            events[eventCount++] = event;
        }
        void ExpectOrder(std::initializer_list<unsigned> expected)
        {
            Check(eventCount == expected.size(), "archive/type observation has only the source-required calls");
            unsigned index = 0;
            for (const auto event : expected)
                Check(events[index++] == event, "actual archive admission precedes the native type observation");
            eventCount = 0;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t model, bool& output) noexcept override
        {
            Mark(1);
            ++archiveCalls;
            key = model;
            // Refused output is intentionally poisoned in the focused scope:
            // the SDK must validate status before consuming the observation.
            if (archiveStatus == ClockResult::Ok || poisonArchiveOnRefusal)
                output = archiveValid;
            return archiveStatus;
        }

        ClockResult ReadModelType(std::uint32_t model, NativeModelTypeObservation& output) noexcept override
        {
            Mark(2);
            ++typeCalls;
            key = model;
            if (status == ClockResult::Ok || poisonTypeOnRefusal)
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
        CBoundingBox*  GetBoundingBox() override { Unavailable(); }
        bool           IsValid() override { Unavailable(); }
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
    static_assert(!noexcept(std::declval<const CModelInfo&>().IsAllocatedInArchive()));
    static_assert(!noexcept(std::declval<const CModelInfoDE&>().IsAllocatedInArchive()));
    static_assert(std::is_same_v<decltype(&Model::IsAllocatedInArchive), bool (CModelInfoDE::*)() const>);
    static_assert(std::is_same_v<decltype(&Model::IsVehicle), bool (CModelInfoDE::*)() const>);

    template <class Operation>
    void RefusesStatus(Operation operation, ClockResult status, const char* prefix)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            Check(std::string(error.what()) == std::string(prefix) + std::to_string(static_cast<unsigned>(status)),
                  "owner refusal preserves its exact SDK operation and status without returning a negative result");
            return;
        }
        Check(false, "owner refusal must throw instead of consuming a poisoned observation");
    }

    void ArchiveExceptionChecks()
    {
        Access access;
        access.traceEnabled = true;
        for (const auto key : std::array<std::uint32_t, 6>{0, 17, 19999, 20000, 65536, 0xffffffff})
        {
            Model       model(key, access);
            CModelInfo& sdk = model;
            for (const bool allocated : {false, true})
            {
                access.archiveValid = allocated;
                const auto archives = access.archiveCalls, types = access.typeCalls;
                Check(sdk.IsAllocatedInArchive() == allocated && access.archiveCalls == archives + 1 && access.typeCalls == types && access.key == key,
                      "successful archive observation forwards the full key once and preserves both actual boolean values");
                access.ExpectOrder({1});
            }
        }
        for (const auto key : std::array<std::uint32_t, 3>{20000, 65536, 0xffffffff})
        {
            Model model(key, access);
            access.archiveStatus = access.status = ClockResult::Expired;
            const auto archives = access.archiveCalls, types = access.typeCalls;
            Check(!model.IsVehicle() && access.archiveCalls == archives && access.typeCalls == types,
                  "model bound rejects IsVehicle on CPU before either archive or type owner can refuse");
            access.ExpectOrder({});
        }
        access.archiveStatus = access.status = ClockResult::Ok;
        for (const auto key : std::array<std::uint32_t, 3>{0, 17, 19999})
        {
            Model model(key, access);
            access.archiveValid = false;
            access.rawType = 6;
            const auto types = access.typeCalls;
            Check(!model.IsVehicle() && access.typeCalls == types, "false archive admission stops IsVehicle before a known vehicle byte");
            access.ExpectOrder({1});
            access.archiveValid = true;
            access.hasModel = false;
            Check(!model.IsVehicle(), "qualified type absence remains a genuine nonvehicle result after archive admission");
            access.ExpectOrder({1, 2});
            access.hasModel = true;
            access.rawType = 5;
            Check(!model.IsVehicle(), "qualified nonvehicle type remains false after exactly one ordered type observation");
            access.ExpectOrder({1, 2});
            access.rawType = 6;
            Check(model.IsVehicle() && access.key == key, "vehicle type6 becomes true only after successful archive admission");
            access.ExpectOrder({1, 2});
        }
        Model model(17, access);
        access.poisonArchiveOnRefusal = access.poisonTypeOnRefusal = true;
        access.archiveValid = true;
        access.hasModel = true;
        access.rawType = 6;
        for (unsigned raw = 1; raw <= static_cast<unsigned>(ClockResult::OriginalException); ++raw)
        {
            const auto status = static_cast<ClockResult>(raw);
            access.archiveStatus = status;
            const auto types = access.typeCalls, archives = access.archiveCalls;
            RefusesStatus([&] { static_cast<void>(model.IsAllocatedInArchive()); }, status, "DE model owner refused archive allocation: ");
            Check(access.archiveCalls == archives + 1 && access.typeCalls == types, "every archive refusal is one call and does not query type");
            access.ExpectOrder({1});
            RefusesStatus([&] { static_cast<void>(model.IsVehicle()); }, status, "DE model owner refused archive allocation: ");
            Check(access.archiveCalls == archives + 2 && access.typeCalls == types, "IsVehicle archive refusal cannot consume poisoned true or enter type");
            access.ExpectOrder({1});
            access.archiveStatus = ClockResult::Ok;
            access.status = status;
            RefusesStatus([&] { static_cast<void>(model.IsVehicle()); }, status, "DE model owner refused model type: ");
            Check(access.archiveCalls == archives + 3 && access.typeCalls == types + 1,
                  "type owner refusal propagates after one admitted archive and type call");
            access.ExpectOrder({1, 2});
        }
        NativeModelClassificationAccess missing;
        Model                           unbound(17, missing);
        RefusesStatus([&] { static_cast<void>(unbound.IsAllocatedInArchive()); }, ClockResult::NotInitialized, "DE model owner refused archive allocation: ");
        RefusesStatus([&] { static_cast<void>(unbound.IsVehicle()); }, ClockResult::NotInitialized, "DE model owner refused archive allocation: ");
    }
}
int main(int argc, char** argv)
{
    try
    {
        const bool archiveOnly = argc == 2 && std::string(argv[1]) == "--archive-exceptions";
        Check(argc == 1 || archiveOnly, "model SDK fixture accepts only the explicit archive exception scope");
        if (!archiveOnly)
        {
            Access access;
            for (const auto key : std::array<std::uint32_t, 6>{0, 17, 19999, 20000, 65536, 0xffffffff})
            {
                Model       model(key, access);
                CModelInfo& sdk = model;
                Check(sdk.GetModel() == key, "actual MTA model identifier preserves the entire DWORD without indexing native arrays");
                for (bool allocated : {false, true})
                {
                    access.archiveValid = allocated;
                    const auto before = access.archiveCalls;
                    Check(sdk.IsAllocatedInArchive() == allocated && access.archiveCalls == before + 1 && access.key == key,
                          "actual SDK forwards complete archive key once and preserves genuine true and false owner results");
                }

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
                    const auto beforeType = access.typeCalls, beforeArchive = access.archiveCalls;
                    const bool vehicle = sdk.IsVehicle();
                    Check(vehicle == (key < 20000 && rawType == 6) && access.typeCalls == beforeType + (key < 20000 ? 1 : 0) &&
                              access.archiveCalls == beforeArchive + (key < 20000 ? 1 : 0),
                          "actual MTA IsVehicle preserves model bounds and archive admission before native type comparison");
                }
                access.hasModel = false;
                Check(sdk.GetModelType() == eModelInfoType::UNKNOWN, "actual classic absent-interface GetModelType maps only qualified absence to UNKNOWN");
                access.hasModel = true;
                access.archiveValid = false;
                const auto beforeType = access.typeCalls;
                Check(!sdk.IsVehicle() && access.typeCalls == beforeType, "unallocated archive cannot be replaced by a known vehicle-type byte");
                access.archiveValid = true;
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
        }
        ArchiveExceptionChecks();
        std::puts("Production MTA model archive exception SDK fixture completed");
        if (archiveOnly)
            std::printf("Passed %u actual MTA model archive-exception SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        else
        {
            std::puts("Production MTA model archive SDK fixture completed");
            std::puts("Production MTA model type SDK fixture completed");
            std::puts("Production MTA model classification SDK fixture completed");
            std::printf("Passed %u actual MTA model-classification SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Model classification SDK failed after %u checks: %s\n", checks, e.what());
        return 1;
    }
}
