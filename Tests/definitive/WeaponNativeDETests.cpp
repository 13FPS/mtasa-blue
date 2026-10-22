/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual borrowed CWeapon SDK dispatch and native refusal boundaries
 *****************************************************************************/
#include "../../Client/game_de/CWeaponNativeDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unqualified weapon SDK effect reached fixture\n", stderr);
        std::abort();
    }
}

// The generated subclass rejects the remaining SDK, without overriding the
// actual production eight-field routes or borrowed owner/slot getters.
#include "WeaponNativeFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool ok, const char* why)
    {
        ++checks;
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }
    template <class Enum>
    std::uint32_t Raw(const Enum& value)
    {
        static_assert(sizeof(value) == sizeof(std::uint32_t));
        std::uint32_t raw;
        std::memcpy(&raw, &value, sizeof(raw));
        return raw;
    }
    bool Same(const NativePedReferenceToken& left, const NativePedReferenceToken& right)
    {
        return left.version == right.version && left.ownerGeneration == right.ownerGeneration && left.slot == right.slot;
    }
    template <class Action>
    void Refuses(Action action, const char* operation, ClockResult result)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text = error.what();
            refused = text.find(std::string("DE CWeapon ") + operation + " unavailable") != std::string::npos &&
                      text.find("status " + std::to_string(static_cast<std::uint32_t>(result))) != std::string::npos;
        }
        Check(refused, "every native refusal must name the actual SDK operation and status");
    }

    struct ReceiverBase : NativeGameTimingAccess
    {
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
    };
    struct Receiver final : ReceiverBase
    {
        std::array<std::uint32_t, 4> fields{};
        std::array<unsigned, 8>      calls{};
        NativePedReferenceToken      lastToken;
        std::uint32_t                lastSlot = 999, lastValue = 0;
        ClockResult                  result = ClockResult::Ok;
        bool                         acceptedFailure = false;
        CWeaponInfo*                 info = nullptr;
        unsigned                     infoCalls = 0;
        eWeaponSkill                 lastSkill = WEAPONSKILL_STD;

        ClockResult Read(unsigned field, const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t& output) noexcept
        {
            ++calls[field * 2];
            lastToken = token;
            lastSlot = slot;
            // Even an ill-behaved receiver can fill its temporary on refusal.
            // The actual SDK wrapper must throw before publishing a return.
            output = fields[field];
            return slot < 13 ? result : ClockResult::InvalidArgument;
        }
        ClockResult Write(unsigned field, const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t value) noexcept
        {
            ++calls[field * 2 + 1];
            lastToken = token;
            lastSlot = slot;
            lastValue = value;
            const auto status = slot < 13 ? result : ClockResult::InvalidArgument;
            if (status == ClockResult::Ok || acceptedFailure)
                fields[field] = value;
            return status;
        }
        ClockResult ReadPedWeaponType(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t& value) noexcept override
        {
            return Read(0, token, slot, value);
        }
        ClockResult SetPedWeaponType(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t value) noexcept override
        {
            return Write(0, token, slot, value);
        }
        ClockResult ReadPedWeaponState(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t& value) noexcept override
        {
            return Read(1, token, slot, value);
        }
        ClockResult SetPedWeaponState(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t value) noexcept override
        {
            return Write(1, token, slot, value);
        }
        ClockResult ReadPedWeaponAmmoInClip(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t& value) noexcept override
        {
            return Read(2, token, slot, value);
        }
        ClockResult SetPedWeaponAmmoInClip(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t value) noexcept override
        {
            return Write(2, token, slot, value);
        }
        ClockResult ReadPedWeaponAmmoTotal(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t& value) noexcept override
        {
            return Read(3, token, slot, value);
        }
        ClockResult SetPedWeaponAmmoTotal(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t value) noexcept override
        {
            return Write(3, token, slot, value);
        }
        ClockResult ReadPedWeaponInfo(const NativePedReferenceToken& token, std::uint32_t slot, eWeaponSkill skill, CWeaponInfo*& value) noexcept override
        {
            ++infoCalls;
            lastToken = token;
            lastSlot = slot;
            lastSkill = skill;
            value = info;
            return slot < 13 ? result : ClockResult::InvalidArgument;
        }
    };

    std::uint32_t Read(CWeapon& weapon, unsigned field)
    {
        switch (field)
        {
            case 0:
                return Raw(weapon.GetType());
            case 1:
                return Raw(weapon.GetState());
            case 2:
                return weapon.GetAmmoInClip();
            case 3:
                return weapon.GetAmmoTotal();
            default:
                std::abort();
        }
    }
    void Write(CWeapon& weapon, unsigned field, std::uint32_t bits)
    {
        switch (field)
        {
            case 0:
                weapon.SetType(static_cast<eWeaponType>(bits));
                break;
            case 1:
                weapon.SetState(static_cast<eWeaponState>(bits));
                break;
            case 2:
                weapon.SetAmmoInClip(bits);
                break;
            case 3:
                weapon.SetAmmoTotal(bits);
                break;
            default:
                std::abort();
        }
    }
    constexpr std::array<const char*, 8> Operations = {"GetType",       "SetType",       "GetState",     "SetState",
                                                       "GetAmmoInClip", "SetAmmoInClip", "GetAmmoTotal", "SetAmmoTotal"};
}

int main()
{
    using namespace mta::game_de;
    static_assert(std::is_abstract_v<CWeaponPolicyDE> && std::is_abstract_v<CWeaponNativeDE> && !std::is_abstract_v<fixture::WeaponFixture>);
    static_assert(noexcept(std::declval<const CWeaponNativeDE&>().GetPed()) && noexcept(std::declval<const CWeaponNativeDE&>().GetSlot()));
    static_assert(std::is_same_v<decltype(std::declval<const CWeaponNativeDE&>().GetPed()), CPed*>);

    Receiver                receiver;
    NativePedReferenceToken sourceToken;
    sourceToken.version = UINT64_C(0x1122334455667788);
    sourceToken.ownerGeneration = UINT64_C(0x8877665544332211);
    sourceToken.slot = 987;
    const auto retained = sourceToken;
    // These owned addresses are CPU identity sentinels only. No CPed object
    // is claimed, dereferenced or passed into a native call by this fixture.
    std::array<std::uint64_t, 2> ownerStorage{};
    auto*                        owner = reinterpret_cast<CPed*>(&ownerStorage[0]);
    auto*                        otherOwner = reinterpret_cast<CPed*>(&ownerStorage[1]);
    fixture::WeaponFixture       weapon(receiver, sourceToken, owner, WEAPONSLOT_TYPE_SHOTGUN);
    CWeapon&                     sdk = weapon;
    sourceToken = {};
    Check(sdk.GetPed() == owner && sdk.GetSlot() == WEAPONSLOT_TYPE_SHOTGUN, "explicit borrowed CPU owner and slot survive independent token input changes");
    Check(receiver.calls == std::array<unsigned, 8>{}, "constructing and reading wrapper metadata never accesses native fields");
    for (unsigned slot = 0; slot <= WEAPONSLOT_MAX; ++slot)
    {
        fixture::WeaponFixture selected(receiver, retained, slot & 1 ? otherOwner : nullptr, static_cast<eWeaponSlot>(slot));
        Check(selected.GetPed() == (slot & 1 ? otherOwner : nullptr) && Raw(selected.GetSlot()) == slot,
              "all actual SDK slots including the explicit sentinel preserve independent owner metadata");
        if (slot < WEAPONSLOT_MAX)
        {
            selected.GetAmmoTotal();
            Check(receiver.lastSlot == slot && Same(receiver.lastToken, retained), "selected slot routes with the same retained reference token");
        }
        else
        {
            Refuses([&] { selected.GetAmmoTotal(); }, "GetAmmoTotal", ClockResult::InvalidArgument);
            Check(receiver.lastSlot == slot, "invalid native slot is not silently replaced with an equipped or default slot");
        }
    }

    const std::vector<std::uint32_t> patterns = {
        0, 1, 255, UINT32_C(0x80000000), UINT32_C(0xffffffff), UINT32_C(0x12345678), UINT32_C(0x7fc12345), UINT32_C(0x01020304)};
    const std::vector<std::uint32_t> types = {WEAPONTYPE_UNARMED, WEAPONTYPE_PISTOL, WEAPONTYPE_INVALID};
    const std::vector<std::uint32_t> states = {WEAPONSTATE_READY, WEAPONSTATE_FIRING, WEAPONSTATE_RELOADING, WEAPONSTATE_OUT_OF_AMMO,
                                               WEAPONSTATE_MELEE_MADECONTACT};
    for (unsigned field = 0; field != 4; ++field)
        for (const auto bits : field == 0 ? types : field == 1 ? states : patterns)
        {
            receiver.fields = {11, 22, 33, 44};
            const auto before = receiver.fields;
            const auto counts = receiver.calls;
            Write(sdk, field, bits);
            Check(receiver.calls[field * 2 + 1] == counts[field * 2 + 1] + 1 && receiver.fields[field] == bits && receiver.lastValue == bits &&
                      receiver.lastSlot == WEAPONSLOT_TYPE_SHOTGUN && Same(receiver.lastToken, retained),
                  "one SDK setter forwards its complete storage bits, the explicit slot and immutable token exactly once");
            for (unsigned neighbor = 0; neighbor != 4; ++neighbor)
                if (neighbor != field)
                    Check(receiver.fields[neighbor] == before[neighbor], "setting one raw field never clamps or repairs another field");
            Check(Read(sdk, field) == bits && receiver.calls[field * 2] == counts[field * 2] + 1,
                  "each successful getter exposes current raw receiver storage rather than cached wrapper defaults");
        }

    for (const auto value : {std::uint32_t{0}, std::uint32_t{200}, std::uint32_t{255}})
    {
        receiver.fields[0] = value;
        Check(Read(sdk, 0) == value, "typed weapon getter accepts the complete unfixed enum language range without a gameplay filter");
    }
    for (const auto value : {std::uint32_t{0}, std::uint32_t{4}, std::uint32_t{7}})
    {
        receiver.fields[1] = value;
        Check(Read(sdk, 1) == value, "typed state getter preserves representable unnamed enum values");
    }
    for (unsigned field = 0; field != 2; ++field)
        for (const auto value : {field == 0 ? std::uint32_t{256} : std::uint32_t{8}, UINT32_MAX})
        {
            receiver.fields[field] = value;
            const auto    before = receiver.calls;
            std::uint32_t output = UINT32_C(0xabcdef01);
            Refuses([&] { output = Read(sdk, field); }, Operations[field * 2], ClockResult::InvalidArgument);
            Check(output == UINT32_C(0xabcdef01) && receiver.calls[field * 2] == before[field * 2] + 1 && receiver.fields[field] == value,
                  "native DWORD outside an unfixed enum's language range refuses typed publication without clamp, retry or write");
        }

    // Pointer values here are receiver identity sentinels, never fake SDK
    // objects. The actual callback fixture separately uses the real80 bank.
    auto* infoIdentity = reinterpret_cast<CWeaponInfo*>(&ownerStorage[0]);
    auto* otherInfoIdentity = reinterpret_cast<CWeaponInfo*>(&ownerStorage[1]);
    for (unsigned skill = 0; skill != 256; ++skill)
    {
        receiver.info = skill & 1 ? infoIdentity : nullptr;
        const auto before = receiver.infoCalls;
        Check(sdk.GetInfo(static_cast<eWeaponSkill>(skill)) == receiver.info && receiver.infoCalls == before + 1 &&
                  receiver.lastSkill == static_cast<eWeaponSkill>(skill) && Same(receiver.lastToken, retained) && receiver.lastSlot == WEAPONSLOT_TYPE_SHOTGUN,
              "GetInfo forwards the complete fixed skill value and qualified pointer or null once without duplicate selection");
    }

    for (unsigned rawStatus = static_cast<unsigned>(ClockResult::InvalidArgument); rawStatus <= static_cast<unsigned>(ClockResult::OriginalException);
         ++rawStatus)
    {
        receiver.result = static_cast<ClockResult>(rawStatus);
        for (unsigned field = 0; field != 4; ++field)
        {
            const auto    counts = receiver.calls;
            std::uint32_t output = UINT32_C(0xabcdef01);
            Refuses([&] { output = Read(sdk, field); }, Operations[field * 2], receiver.result);
            Check(output == UINT32_C(0xabcdef01) && receiver.calls[field * 2] == counts[field * 2] + 1,
                  "failed getter never publishes a receiver temporary or retries with a new scope");
            const auto before = receiver.fields;
            Refuses([&] { Write(sdk, field, 0); }, Operations[field * 2 + 1], receiver.result);
            Check(receiver.fields == before && receiver.calls[field * 2 + 1] == counts[field * 2 + 1] + 1,
                  "pre-entry setter refusal has no fallback, fabricated success or second dispatch");
        }
        CWeaponInfo* infoOutput = otherInfoIdentity;
        receiver.info = infoIdentity;
        const auto infoBefore = receiver.infoCalls;
        Refuses([&] { infoOutput = sdk.GetInfo(WEAPONSKILL_STD); }, "GetInfo", receiver.result);
        Check(infoOutput == otherInfoIdentity && receiver.infoCalls == infoBefore + 1,
              "GetInfo never publishes a pointer temporary on owner refusal or retries a lookup");
        Check(sdk.GetPed() == owner && sdk.GetSlot() == WEAPONSLOT_TYPE_SHOTGUN, "CPU metadata remains independent of native callback refusal");
    }

    receiver.result = ClockResult::OriginalException;
    receiver.acceptedFailure = true;
    const std::array<std::uint32_t, 4> acceptedBits = {WEAPONTYPE_INVALID, WEAPONSTATE_MELEE_MADECONTACT, UINT32_MAX, UINT32_C(0x80000000)};
    for (unsigned field = 0; field != 4; ++field)
    {
        const auto counts = receiver.calls;
        {
            fixture::WeaponFixture accepted(receiver, retained, owner, WEAPONSLOT_TYPE_SHOTGUN);
            Refuses([&] { Write(accepted, field, acceptedBits[field]); }, Operations[field * 2 + 1], receiver.result);
        }
        Check(receiver.fields[field] == acceptedBits[field] && receiver.calls[field * 2 + 1] == counts[field * 2 + 1] + 1,
              "an accepted effect survives reported failure and CPU wrapper destruction without rollback or replay");
    }

    ReceiverBase           absent;
    fixture::WeaponFixture unavailable(absent, retained, nullptr, WEAPONSLOT_TYPE_HANDGUN);
    for (unsigned field = 0; field != 4; ++field)
    {
        Refuses([&] { Read(unavailable, field); }, Operations[field * 2], ClockResult::NotInitialized);
        Refuses([&] { Write(unavailable, field, 0); }, Operations[field * 2 + 1], ClockResult::NotInitialized);
    }
    Refuses([&] { unavailable.GetInfo(WEAPONSKILL_STD); }, "GetInfo", ClockResult::NotInitialized);
    Check(unavailable.GetPed() == nullptr && unavailable.GetSlot() == WEAPONSLOT_TYPE_HANDGUN,
          "default-refusing timing owners still preserve explicit borrowed CPU metadata");
    std::puts("Production actual CWeapon CPU metadata SDK fixture completed");
    std::puts("Production actual ped-owned CWeapon SDK slice fixture completed");
    std::printf("Passed %u actual ped-owned CWeapon SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
