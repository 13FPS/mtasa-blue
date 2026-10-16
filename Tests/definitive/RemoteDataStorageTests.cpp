/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify shared SDK storage and the unchanged classic pad ABI
 *****************************************************************************/

#include "../../Client/game_de/RemoteDataStorage.h"
#if !defined(_WIN64)
    #include "../../Client/multiplayer_sa/CRemoteDataSA.h"
#endif
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace
{
    using namespace mta::game_de;
    int checks = 0;

    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }

    template <class Value>
    bool ZeroBytes(const Value& value)
    {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t index = 0; index < sizeof(value); ++index)
            if (bytes[index])
                return false;
        return true;
    }

    CControllerState Controller(short first)
    {
        // Cover every canonical field, including MTA-only logical extras.
        // These CPU values never bind or write any engine controller.
        static_assert(sizeof(CControllerState) == 24 * sizeof(short));
        CControllerState state;
        for (std::size_t index = 0; index < 24; ++index)
        {
            const auto value = static_cast<short>(first + static_cast<short>(index));
            std::memcpy(reinterpret_cast<unsigned char*>(&state) + index * sizeof(short), &value, sizeof(value));
        }
        return state;
    }

    bool Same(const CControllerState& left, const CControllerState& right)
    {
        return std::memcmp(&left, &right, sizeof(left)) == 0;
    }

    void TestSdkStorage()
    {
        static_assert(!std::has_virtual_destructor_v<CRemoteDataStorage>);
        static_assert(std::is_base_of_v<CRemoteDataStorage, RemoteDataStorage>);
        static_assert(sizeof(RemotePadStorage) == 96 && alignof(RemotePadStorage) == alignof(CControllerState));
        // Ownership stays concrete; consumers only borrow the stable SDK view.
        auto                storage = std::make_unique<RemoteDataStorage>();
        CRemoteDataStorage* sdk = storage.get();
        auto*               current = sdk->CurrentControllerState();
        auto*               previous = sdk->LastControllerState();
        auto*               shots = sdk->ShotSyncData();
        auto*               stats = sdk->Stats();
        Check(current == &storage->m_pad.NewState && previous == &storage->m_pad.OldState, "SDK controller pointers alias the owned pad");
        Check(shots == &storage->m_shotSyncData && stats == &storage->m_stats, "SDK metadata pointers alias actual MTA storage");
        Check(ZeroBytes(*current) && ZeroBytes(*previous) && ZeroBytes(*stats), "new MTA controller histories and stats are zero");
        Check(sdk->GetCameraRotation() == 0.0f && storage->m_fGravity == 0.008f, "existing camera/gravity defaults are retained");
        Check(!sdk->GetAkimboTargetUp() && !storage->ProcessPlayerWeapon() && !shots->m_bRemoteBulletSyncVectorsValid,
              "existing remote-state boolean defaults are retained");
        Check(shots->m_fArmDirectionX == 0.0f && shots->m_fArmDirectionY == 0.0f && !shots->m_bUseOrigin,
              "new DE owner initializes its additional CPU-only shot fields");

        const MtaInputState input{Controller(101), Controller(-124)};
        storage->WriteInput(input);
        Check(Same(*current, input.current) && Same(*previous, input.previous), "all canonical current/previous fields retain independent values");
        const auto copied = storage->ReadInput();
        Check(Same(copied.current, input.current) && Same(copied.previous, input.previous), "public frame copies preserve both controller histories");
        current->LeftStickX = -37;
        previous->m_bRadioTrackSkip = 63;
        const auto mutated = storage->ReadInput();
        Check(mutated.current.LeftStickX == -37 && mutated.previous.m_bRadioTrackSkip == 63, "SDK interior writes feed the actual frame publisher");
        Check(mutated.previous.LeftStickX == input.previous.LeftStickX && mutated.current.m_bRadioTrackSkip == input.current.m_bRadioTrackSkip,
              "SDK current and previous state cannot alias each other");

        sdk->SetCameraRotation(1.5f);
        sdk->SetGravity(-0.2f);
        sdk->SetProcessPlayerWeapon(true);
        sdk->SetAkimboTarget(CVector(3.0f, -4.0f, 5.0f));
        sdk->SetAkimboTargetUp(true);
        stats->StatTypesFloat[0] = 9.0f;
        stats->StatTypesInt[MAX_INT_STATS - 1] = 27;
        shots->m_bRemoteBulletSyncVectorsValid = true;
        Check(sdk->GetCameraRotation() == 1.5f && storage->m_fGravity == -0.2f && storage->ProcessPlayerWeapon(),
              "ordinary MTA metadata accessors retain their values");
        Check(&sdk->GetAkimboTarget() == &storage->m_vecAkimboTarget && sdk->GetAkimboTarget().fY == -4.0f && sdk->GetAkimboTargetUp(),
              "akimbo accessor returns the owned MTA vector by reference");

        RemoteDataStorage other;
        Check(ZeroBytes(*other.CurrentControllerState()) && ZeroBytes(*other.LastControllerState()) && ZeroBytes(*other.Stats()),
              "independent player storage cannot share controller or stat data");
        Check(other.GetCameraRotation() == 0.0f && !other.ProcessPlayerWeapon() && !other.ShotSyncData()->m_bRemoteBulletSyncVectorsValid,
              "independent player metadata cannot adopt another player's state");
        storage->WriteInput(MtaInputState{});
        Check(current == sdk->CurrentControllerState() && previous == sdk->LastControllerState() && stats == sdk->Stats() && shots == sdk->ShotSyncData(),
              "resetting a controller history preserves all cached SDK interior pointers");
        Check(ZeroBytes(*current) && ZeroBytes(*previous), "controller reset clears both canonical histories");
        Check(stats->StatTypesFloat[0] == 9.0f && stats->StatTypesInt[MAX_INT_STATS - 1] == 27 && shots->m_bRemoteBulletSyncVectorsValid,
              "controller reset cannot erase unrelated per-player MTA metadata");
    }

    void TestCanonicalHistory()
    {
        RemoteDataStorage                  storage;
        MtaInput::DirectionHistory         directions;
        CControllerState                   expectedPrevious;
        const std::array<std::uint64_t, 8> actions = {MtaInput::ActionBit(FORWARDS),
                                                      MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(SPRINT),
                                                      MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(WALK),
                                                      MtaInput::ActionBit(LEFT),
                                                      MtaInput::ActionBit(LEFT) | MtaInput::ActionBit(RIGHT),
                                                      MtaInput::ActionBit(BACKWARDS),
                                                      0,
                                                      0};
        auto*                              stableCurrent = storage.CurrentControllerState();
        auto*                              stablePrevious = storage.LastControllerState();
        for (const auto held : actions)
        {
            const auto          nextCurrent = MtaInput::BuildControllerState(held, {}, directions);
            const MtaInputState next{nextCurrent, *storage.CurrentControllerState()};
            storage.WriteInput(next);
            const auto published = storage.ReadInput();
            Check(Same(published.current, nextCurrent) && Same(published.previous, expectedPrevious),
                  "MTA canonical action synthesis retains the previous submitted frame");
            Check(stableCurrent == storage.CurrentControllerState() && stablePrevious == storage.LastControllerState(),
                  "successive frame publication keeps cached SDK state addresses stable");
            expectedPrevious = nextCurrent;
        }
    }

#if !defined(_WIN64)
    // Independent record of the old MSVC x86 data layout. Its explicit vfptr
    // models the unchanged SDK's single virtual base, rather than deriving a
    // second mock interface or obtaining expected offsets from the new class.
    struct ClassicLayoutBeforeExtraction
    {
        void*           sdkVtable;
        CPadSAInterface m_pad;
        float           m_fCameraRotation;
        CShotSyncData   m_shotSyncData;
        CVector         m_vecAkimboTarget;
        bool            m_bAkimboTargetUp;
        CStatsData      m_stats;
        float           m_fGravity;
        bool            m_bProcessPlayerWeapon;
    };

    void TestClassicLayout()
    {
        static_assert(sizeof(void*) == 4);
        static_assert(sizeof(CRemoteDataStorageSA) == sizeof(ClassicLayoutBeforeExtraction));
        static_assert(alignof(CRemoteDataStorageSA) == alignof(ClassicLayoutBeforeExtraction));
        CRemoteDataStorageSA          actual;
        ClassicLayoutBeforeExtraction expected{};
        const auto                    offset = [](const auto& object, const auto& member)
        { return reinterpret_cast<const unsigned char*>(&member) - reinterpret_cast<const unsigned char*>(&object); };
        Check(offset(actual, actual.m_pad) == offset(expected, expected.m_pad), "classic full pad offset matches its original x86 layout");
        Check(offset(actual, actual.m_fCameraRotation) == offset(expected, expected.m_fCameraRotation), "classic camera offset is unchanged");
        Check(offset(actual, actual.m_shotSyncData) == offset(expected, expected.m_shotSyncData), "classic shots offset is unchanged");
        Check(offset(actual, actual.m_vecAkimboTarget) == offset(expected, expected.m_vecAkimboTarget), "classic akimbo vector offset is unchanged");
        Check(offset(actual, actual.m_bAkimboTargetUp) == offset(expected, expected.m_bAkimboTargetUp), "classic akimbo flag offset is unchanged");
        Check(offset(actual, actual.m_stats) == offset(expected, expected.m_stats), "classic stats offset is unchanged");
        Check(offset(actual, actual.m_fGravity) == offset(expected, expected.m_fGravity), "classic gravity offset is unchanged");
        Check(offset(actual, actual.m_bProcessPlayerWeapon) == offset(expected, expected.m_bProcessPlayerWeapon), "classic weapon flag offset is unchanged");
        Check(sizeof(actual.m_pad) > sizeof(RemotePadStorage), "classic storage retains more than the DE canonical controller pair");
        Check(ZeroBytes(actual.m_pad), "classic constructor continues zeroing the entire pad including noncontroller fields");
        CRemoteDataStorage* sdk = &actual;
        auto*               current = sdk->CurrentControllerState();
        auto*               previous = sdk->LastControllerState();
        Check(current == &actual.m_pad.NewState && previous == &actual.m_pad.OldState, "classic SDK controllers still alias full-pad NewState/OldState");

        CPadSAInterface source;
        std::memset(&source, 0x5a, sizeof(source));
        source.NewState = Controller(-93);
        source.OldState = Controller(37);
        source.PCTempMouseState = Controller(68);
        source.PCTempKeyState = Controller(-72);
        source.PCTempJoyState = Controller(24);
        source.bHornHistory[0] = true;
        source.bHornHistory[4] = false;
        source.DisablePlayerControls = 0x1234;
        source.LastTimeTouched = 0x87654321;
        source.bDisablePlayerFireWeapon = true;
        // These are the same whole-pad transfers used by SwitchContext and
        // ReturnContextToLocalPlayer, including every noncontroller byte.
        std::memcpy(&actual.m_pad, &source, sizeof(source));
        CPadSAInterface restored;
        std::memcpy(&restored, &actual.m_pad, sizeof(restored));
        Check(std::memcmp(&source, &restored, sizeof(source)) == 0, "classic full-pad copy round trip preserves all bytes");
        Check(Same(*current, source.NewState) && Same(*previous, source.OldState), "cached classic SDK pointers observe full-pad context copies");
        Check(restored.LastTimeTouched == 0x87654321 && restored.DisablePlayerControls == 0x1234 && Same(restored.PCTempMouseState, source.PCTempMouseState) &&
                  restored.bHornHistory[0] && !restored.bHornHistory[4],
              "classic timestamps, disable flags, temporary inputs and horn history survive context copies");
        Check(current == sdk->CurrentControllerState() && previous == sdk->LastControllerState(),
              "classic full-pad copies preserve cached SDK state addresses");
    }
#endif
}

int main()
{
    try
    {
        TestSdkStorage();
        TestCanonicalHistory();
#if !defined(_WIN64)
        TestClassicLayout();
#endif
        std::cout << "Passed " << checks << " remote data storage checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Remote data storage failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
