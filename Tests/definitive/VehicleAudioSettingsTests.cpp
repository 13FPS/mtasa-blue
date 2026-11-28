// Compile the real entry types and application methods with owned vehicles.
#include <SharedUtil.IntTypes.h>
#include "../../Client/game_sa/CVehicleAudioSettingsEntrySA.h"
#include <cstddef>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace
{
    class ForeignEntry final : public CVehicleAudioSettingsEntry
    {
        // Independent SDK storage and accessors. No native record is embedded.
#define PROPERTY(Type, Name) \
private: \
    Type field##Name{}; \
\
public: \
    Type Get##Name() const noexcept override \
    { \
        return field##Name; \
    } \
    void Set##Name(Type value) noexcept override \
    { \
        field##Name = value; \
    }
        PROPERTY(float, HornVolumeDelta)
        PROPERTY(char, VehicleTypeForAudio)
        PROPERTY(char, RadioType)
        PROPERTY(char, RadioNum)
        PROPERTY(char, DoorSound)
        PROPERTY(char, EngineUpgrade)
        PROPERTY(float, HornHign)
        PROPERTY(char, HornTon)
        PROPERTY(float, FieldC)
        PROPERTY(float, BassEq)
        PROPERTY(char, BassSetting)
        PROPERTY(short, EngineOffSoundBankID)
        PROPERTY(short, EngineOnSoundBankID)
        PROPERTY(VehicleSoundType::Enum, SoundType)
#undef PROPERTY
    };

    const tVehicleAudioSettings* pNextVehicleAudioSettings = nullptr;
    struct CVehicle
    {
        virtual void ReinitAudio() = 0;
    };
    struct Entity
    {
        tVehicleAudioSettings m_nSettings{};
        bool                  hasInterface = true;
        Entity*               GetInterface() { return hasInterface ? this : nullptr; }
    };
    struct CVehicleSA final : CVehicle
    {
        Entity   entity;
        bool     hasEntity = true;
        unsigned restarts = 0;
        Entity*  GetVehicleAudioEntity() { return hasEntity ? &entity : nullptr; }
        void     ReinitAudio() override
        {
            ++restarts;
            entity.m_nSettings = *pNextVehicleAudioSettings;
        }
    };
    struct CVehicleAudioSettingsManagerSA
    {
        CVehicleAudioSettingsEntrySA m_nextSettings;
        bool                         HasPendingSettingsChange(CVehicle*, const CVehicleAudioSettingsEntry&) const noexcept;
        void                         SetNextSettings(const CVehicleAudioSettingsEntry*) noexcept;
    };
    struct Game
    {
        CVehicleAudioSettingsManagerSA manager;
        auto*                          GetVehicleAudioSettingsManager() { return &manager; }
    };
    Game* g_pGame = nullptr;
    struct CClientVehicle
    {
        CVehicle*                         m_pVehicle;
        const CVehicleAudioSettingsEntry& desired;
        const auto&                       GetAudioSettings() const { return desired; }
        void                              ApplyAudioSettings();
    };
#include "VehicleAudioApplication.generated.inl"

    void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <class Value>
    void TestProperty(void (CVehicleAudioSettingsEntry::*setter)(Value) noexcept, Value value)
    {
        ForeignEntry   desired;
        CVehicleSA     live;
        CClientVehicle client{&live, desired};
        (desired.*setter)(value);
        client.ApplyAudioSettings();
        Check(live.restarts == 1, "different field caused one restart");
        client.ApplyAudioSettings();
        Check(live.restarts == 1, "copied field stopped needing reinitialization");
        ForeignEntry                       roundTrip;
        const CVehicleAudioSettingsEntrySA current(&live.entity.m_nSettings);
        roundTrip.CopyFrom(current);
        Check(roundTrip.HasSamePlaybackSettings(desired), "round trip between different backends");
    }

    void Run()
    {
        Game game;
        g_pGame = &game;
        using Entry = CVehicleAudioSettingsEntry;
        TestProperty(&Entry::SetSoundType, VehicleSoundType::TRAIN);
        TestProperty(&Entry::SetEngineOnSoundBankID, short{42});
        TestProperty(&Entry::SetEngineOffSoundBankID, short{53});
        TestProperty(&Entry::SetBassSetting, char{1});
        TestProperty(&Entry::SetBassEq, 2.25f);
        TestProperty(&Entry::SetFieldC, -1.0f);
        TestProperty(&Entry::SetHornTon, char{7});
        TestProperty(&Entry::SetHornHign, 0.75f);
        TestProperty(&Entry::SetEngineUpgrade, char{3});
        TestProperty(&Entry::SetDoorSound, char{4});
        TestProperty(&Entry::SetRadioNum, char{6});
        TestProperty(&Entry::SetRadioType, char{2});
        TestProperty(&Entry::SetVehicleTypeForAudio, char{5});
        TestProperty(&Entry::SetHornVolumeDelta, -9.5f);
        TestProperty(&Entry::SetBassEq, std::numeric_limits<float>::quiet_NaN());

        ForeignEntry   desired;
        CVehicleSA     live;
        CClientVehicle client{&live, desired};
        // This byte sits strictly between native fields, and can come from a
        // different initialization history than the desired SDK entry.
        static_assert(offsetof(tVehicleAudioSettings, m_nEngineOnSoundBankId) > sizeof(VehicleSoundType::Enum));
        reinterpret_cast<unsigned char*>(&live.entity.m_nSettings)[sizeof(VehicleSoundType::Enum)] = 0x9b;
        desired.SetHornVolumeDelta(-0.0f);
        client.ApplyAudioSettings();
        Check(live.restarts == 0, "padding and negative zero ignored");
        desired.SetRadioType(2);
        desired.SetRadioNum(13);
        desired.SetHornVolumeDelta(-3.0f);
        client.ApplyAudioSettings();
        Check(live.restarts == 1 && live.entity.m_nSettings.m_fHornVolumeDelta == -3.0f && live.entity.m_nSettings.m_nRadioID == 13,
              "exact stored horn delta and configured station");
        live.entity.m_nSettings.m_nRadioID = 4;
        client.ApplyAudioSettings();
        Check(live.restarts == 1, "choosing a random station preserves playing audio");
        desired.SetRadioType(1);
        client.ApplyAudioSettings();
        Check(live.restarts == 2 && live.entity.m_nSettings.m_nRadioID == 13, "switching to fixed station reapplies configured value");

        CVehicleAudioSettingsEntrySA native;
        game.manager.SetNextSettings(&native);
        native.SetRadioNum(7);
        Check(pNextVehicleAudioSettings == &native.GetInterface() && pNextVehicleAudioSettings->m_nRadioID == 7, "native entry remains a live borrow");
        {
            ForeignEntry temporary;
            temporary.SetRadioNum(11);
            game.manager.SetNextSettings(&temporary);
            temporary.SetRadioNum(8);
        }
        Check(pNextVehicleAudioSettings == &game.manager.m_nextSettings.GetInterface() && pNextVehicleAudioSettings->m_nRadioID == 11,
              "foreign entry survives retirement in manager storage");
        live.hasEntity = false;
        Check(game.manager.HasPendingSettingsChange(&live, desired), "unavailable entity requires application");
        live.hasEntity = true;
        live.entity.hasInterface = false;
        Check(game.manager.HasPendingSettingsChange(&live, desired) && game.manager.HasPendingSettingsChange(nullptr, desired),
              "unavailable interface or vehicle requires application");
        g_pGame = nullptr;
        pNextVehicleAudioSettings = nullptr;
    }
}

int main()
{
    try
    {
        Run();
        std::puts("14 vehicle audio fields, cross-backend copy, restart counts, random station, padding and settings lifetimes passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
