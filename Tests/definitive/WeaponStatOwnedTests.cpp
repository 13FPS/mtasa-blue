/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise real owned weapon-stat SDK objects and shared MTA policy
 ****************************************************************************/
#include <windows.h>
#include <cstdint>
#include <game/CWeaponStatOwned.h>
#include "../../Client/game_de/CWeaponStatFactoryDE.h"
#include "WeaponStatDefaultsGolden.h"

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            std::exit(EXIT_FAILURE);
        }
    }

    std::uint32_t FloatBits(float value)
    {
        std::uint32_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    void Word(std::uint64_t& digest, std::uint32_t value)
    {
        for (unsigned shift = 0; shift != 32; shift += 8)
        {
            digest ^= (value >> shift) & 0xff;
            digest *= 1099511628211ull;
        }
    }

    std::uint64_t DefaultDigest(CWeaponStat& value)
    {
        std::uint64_t digest = 14695981039346656037ull;
        Word(digest, static_cast<std::uint32_t>(value.GetFireType()));
        Word(digest, FloatBits(value.GetTargetRange()));
        Word(digest, FloatBits(value.GetWeaponRange()));
        Word(digest, static_cast<std::uint32_t>(value.GetModel()));
        Word(digest, static_cast<std::uint32_t>(value.GetModel2()));
        Word(digest, static_cast<std::uint32_t>(value.GetSlot()));
        Word(digest, static_cast<std::uint32_t>(value.GetFlags()));
        Word(digest, static_cast<std::uint32_t>(value.GetAnimGroup()));
        Word(digest, static_cast<std::uint32_t>(value.GetMaximumClipAmmo()));
        Word(digest, static_cast<std::uint32_t>(value.GetDamagePerHit()));
        Word(digest, FloatBits(value.GetFireOffset()->fX));
        Word(digest, FloatBits(value.GetFireOffset()->fY));
        Word(digest, FloatBits(value.GetFireOffset()->fZ));
        Word(digest, static_cast<std::uint32_t>(value.GetSkill()));
        Word(digest, FloatBits(value.GetRequiredStatLevel()));
        Word(digest, FloatBits(value.GetAccuracy()));
        Word(digest, FloatBits(value.GetMoveSpeed()));
        Word(digest, FloatBits(value.GetWeaponAnimLoopStart()));
        Word(digest, FloatBits(value.GetWeaponAnimLoopStop()));
        Word(digest, FloatBits(value.GetWeaponAnimLoopFireTime()));
        Word(digest, FloatBits(value.GetWeaponAnim2LoopStart()));
        Word(digest, FloatBits(value.GetWeaponAnim2LoopStop()));
        Word(digest, FloatBits(value.GetWeaponAnim2LoopFireTime()));
        Word(digest, FloatBits(value.GetWeaponAnimBreakoutTime()));
        Word(digest, FloatBits(value.GetFiringSpeed()));
        Word(digest, FloatBits(value.GetRadius()));
        Word(digest, FloatBits(value.GetLifeSpan()));
        Word(digest, FloatBits(value.GetSpread()));
        Word(digest, static_cast<std::uint32_t>(value.GetAimOffsetIndex()));
        Word(digest, static_cast<std::uint32_t>(value.GetDefaultCombo()));
        Word(digest, static_cast<std::uint32_t>(value.GetCombosAvailable()));
        return digest;
    }

    struct Originals
    {
        DWORD        flags = 0;
        DWORD        animGroup = 109;
        unsigned     calls = 0;
        eWeaponType  requestedType = WEAPONTYPE_INVALID;
        eWeaponSkill requestedSkill = WEAPONSKILL_SPECIAL;

        static MtaGame::WeaponStatOriginal Lookup(void* context, eWeaponType weapon, eWeaponSkill skill)
        {
            auto& self = *static_cast<Originals*>(context);
            ++self.calls;
            self.requestedType = weapon;
            self.requestedSkill = skill;
            return {self.flags, self.animGroup};
        }
    };

    void CheckDefaults()
    {
        for (unsigned skill = 0; skill != 3; ++skill)
        {
            for (int weapon = WEAPONTYPE_PISTOL; weapon <= WEAPONTYPE_EXTINGUISHER; ++weapon)
            {
                const auto                type = static_cast<eWeaponType>(weapon);
                const auto                level = static_cast<eWeaponSkill>(skill);
                MtaGame::CWeaponStatOwned owned(type, level);
                CWeaponStat&              sdk = owned;
                Check(DefaultDigest(sdk) == goldenDefaults[skill][weapon - WEAPONTYPE_PISTOL],
                      "every default SDK field matches pre-extraction MTA golden data");
                Check(sdk.GetWeaponType() == type && sdk.GetWeaponSkillLevel() == level, "factory metadata retains the requested type and level");

                // The real DE factory is intentionally an unlisted CPU object,
                // unlike CGame::GetWeaponInfo's separate native-bank wrapper.
                std::unique_ptr<MtaGame::CWeaponStatOwned> created(static_cast<MtaGame::CWeaponStatOwned*>(mta::game_de::CreateWeaponStatDE(type, level)));
                Check(created && DefaultDigest(*created) == goldenDefaults[skill][weapon - WEAPONTYPE_PISTOL],
                      "DE factory returns the complete actual owned SDK object");
                Check(created->GetFireOffset() != sdk.GetFireOffset(), "independent factory objects never share mutable vectors");

                sdk.SetWeaponRange(-87.25f);
                sdk.GetFireOffset()->fX = 912.5f;
                Check(MtaGame::CWeaponStatDefaults::Load(&sdk, type, level), "shared loader retains MTA success convention");
                Check(DefaultDigest(sdk) == goldenDefaults[skill][weapon - WEAPONTYPE_PISTOL], "shared loader resets every field through the actual SDK");
                Check(DefaultDigest(*created) == goldenDefaults[skill][weapon - WEAPONTYPE_PISTOL], "mutating and resetting one object preserves the other");
            }
        }

        MtaGame::CWeaponStatOwned sentinel(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        const auto                before = DefaultDigest(sentinel);
        for (int weapon = -1; weapon <= WEAPONTYPE_INVALID; ++weapon)
        {
            for (unsigned skill = 0; skill <= 255; ++skill)
            {
                const bool covered = weapon >= WEAPONTYPE_PISTOL && weapon <= WEAPONTYPE_EXTINGUISHER && skill < 3;
                const auto type = static_cast<eWeaponType>(weapon);
                const auto level = static_cast<eWeaponSkill>(skill);
                Check((MtaGame::CWeaponStatDefaults::Select(type, level) != nullptr) == covered,
                      "exact default lookup rejects malformed and unsupported type/skill combinations");
                if (!covered)
                {
                    Check(MtaGame::CWeaponStatDefaults::Load(&sentinel, type, level), "unsupported load preserves the existing success convention");
                    Check(DefaultDigest(sentinel) == before, "unsupported load does not mutate its destination");
                }
            }
        }
        Check(MtaGame::CWeaponStatDefaults::Select(static_cast<eWeaponType>(INT_MAX), WEAPONSKILL_STD) == nullptr,
              "large enum cast cannot index a default table");
        Check(MtaGame::CWeaponStatDefaults::Load(nullptr, WEAPONTYPE_PISTOL, WEAPONSKILL_STD), "null destination retains the existing loader convention");

        for (const int weapon : {-1, 0, 21, 43, 255, INT_MAX})
        {
            MtaGame::CWeaponStatOwned empty(static_cast<eWeaponType>(weapon), WEAPONSKILL_SPECIAL);
            MtaGame::CWeaponStatOwned another(static_cast<eWeaponType>(weapon), WEAPONSKILL_SPECIAL);
            Check(DefaultDigest(empty) == DefaultDigest(another), "unsupported factory data has deterministic value initialization");
            Check(empty.GetMaximumClipAmmo() == 0 && empty.GetFlags() == 0 && empty.GetModel() == 0 && empty.GetFireOffset()->fX == 0,
                  "unsupported unlisted requests retain zero CPU data");
            Check(empty.GetWeaponType() == static_cast<eWeaponType>(weapon) && empty.GetWeaponSkillLevel() == WEAPONSKILL_SPECIAL,
                  "unsupported factory metadata is preserved independently of row skill");
        }
    }

    void CheckSdkFields()
    {
        MtaGame::CWeaponStatOwned object(WEAPONTYPE_PISTOL, WEAPONSKILL_STD);
        CWeaponStat&              sdk = object;
        struct FloatField
        {
            void (CWeaponStat::*set)(float);
            float (CWeaponStat::*get)();
        };
        const FloatField fields[] = {{&CWeaponStat::SetWeaponRange, &CWeaponStat::GetWeaponRange},
                                     {&CWeaponStat::SetTargetRange, &CWeaponStat::GetTargetRange},
                                     {&CWeaponStat::SetAccuracy, &CWeaponStat::GetAccuracy},
                                     {&CWeaponStat::SetMoveSpeed, &CWeaponStat::GetMoveSpeed},
                                     {&CWeaponStat::SetFiringSpeed, &CWeaponStat::GetFiringSpeed},
                                     {&CWeaponStat::SetRadius, &CWeaponStat::GetRadius},
                                     {&CWeaponStat::SetLifeSpan, &CWeaponStat::GetLifeSpan},
                                     {&CWeaponStat::SetSpread, &CWeaponStat::GetSpread},
                                     {&CWeaponStat::SetAnimBreakoutTime, &CWeaponStat::GetAnimBreakoutTime},
                                     {&CWeaponStat::SetWeaponAnimLoopStart, &CWeaponStat::GetWeaponAnimLoopStart},
                                     {&CWeaponStat::SetWeaponAnimLoopStop, &CWeaponStat::GetWeaponAnimLoopStop},
                                     {&CWeaponStat::SetWeaponAnimLoopFireTime, &CWeaponStat::GetWeaponAnimLoopFireTime},
                                     {&CWeaponStat::SetWeaponAnim2LoopStart, &CWeaponStat::GetWeaponAnim2LoopStart},
                                     {&CWeaponStat::SetWeaponAnim2LoopStop, &CWeaponStat::GetWeaponAnim2LoopStop},
                                     {&CWeaponStat::SetWeaponAnim2LoopFireTime, &CWeaponStat::GetWeaponAnim2LoopFireTime},
                                     {&CWeaponStat::SetWeaponAnimBreakoutTime, &CWeaponStat::GetWeaponAnimBreakoutTime},
                                     {&CWeaponStat::SetWeaponSpeed, &CWeaponStat::GetWeaponSpeed},
                                     {&CWeaponStat::SetWeaponRadius, &CWeaponStat::GetWeaponRadius}};
        const float      scalars[] = {-123.5f, -0.0f, 0.0f, 1.0f, 1024.75f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
        for (const auto& field : fields)
        {
            for (const float value : scalars)
            {
                (sdk.*field.set)(value);
                Check(FloatBits((sdk.*field.get)()) == FloatBits(value), "owned CPU float fields retain exact SDK values without pretending native validation");
            }
        }

        sdk.SetWeaponSpeed(17.5f);
        Check(sdk.GetFiringSpeed() == 17.5f, "firing speed and weapon speed are aliases");
        sdk.SetFiringSpeed(-3.5f);
        Check(sdk.GetWeaponSpeed() == -3.5f, "speed alias works in both directions");
        sdk.SetWeaponRadius(31.0f);
        Check(sdk.GetRadius() == 31.0f, "radius alias shares storage");
        sdk.SetRadius(-2.0f);
        Check(sdk.GetWeaponRadius() == -2.0f, "radius alias works in both directions");
        sdk.SetAnimBreakoutTime(8.5f);
        Check(sdk.GetWeaponAnimBreakoutTime() == 8.5f, "breakout alias shares storage");
        sdk.SetWeaponAnimBreakoutTime(4.25f);
        Check(sdk.GetAnimBreakoutTime() == 4.25f, "breakout alias works in both directions");

        const short shorts[] = {SHRT_MIN, -1, 0, 1, SHRT_MAX};
        for (const short value : shorts)
        {
            sdk.SetMaximumClipAmmo(value);
            Check(sdk.GetMaximumClipAmmo() == value, "clip count retains signed short domain");
            sdk.SetDamagePerHit(value);
            Check(sdk.GetDamagePerHit() == value, "damage retains signed short domain");
            sdk.SetAimOffsetIndex(value);
            Check(sdk.GetAimOffsetIndex() == value, "owned aim index is CPU data, not a native indexed write");
        }
        for (unsigned value = 0; value <= 255; ++value)
        {
            sdk.SetDefaultCombo(static_cast<BYTE>(value));
            Check(static_cast<unsigned>(sdk.GetDefaultCombo()) == value, "default combo retains full byte domain");
            sdk.SetCombosAvailable(static_cast<BYTE>(value));
            Check(static_cast<unsigned>(sdk.GetCombosAvailable()) == value, "combo count retains full byte domain");
            sdk.SetSkill(static_cast<eWeaponSkill>(value));
            Check(sdk.GetSkill() == static_cast<eWeaponSkill>(value), "row skill stores independent CPU metadata");
            sdk.SetWeaponSkillLevel(static_cast<eWeaponSkill>(255 - value));
            Check(sdk.GetWeaponSkillLevel() == static_cast<eWeaponSkill>(255 - value) && sdk.GetSkill() == static_cast<eWeaponSkill>(value),
                  "factory metadata and value-record skill remain distinct");
        }
        for (const int value : {INT_MIN, -1, 0, 1, INT_MAX})
        {
            sdk.SetModel(value);
            Check(sdk.GetModel() == static_cast<eWeaponModel>(value), "model stores full CPU integer without native dereference");
            sdk.SetModel2(value);
            Check(sdk.GetModel2() == static_cast<eWeaponModel>(value), "second model stores independent CPU integer");
            sdk.SetRequiredStatLevel(value);
            Check(sdk.GetRequiredStatLevel() == static_cast<float>(value), "integer required stat uses original integer storage and float getter");
            sdk.SetSlot(static_cast<eWeaponSlot>(value));
            Check(sdk.GetSlot() == static_cast<eWeaponSlot>(value), "owned slot is data and does not mutate native slot arrays");
            sdk.SetFireType(static_cast<eFireType>(value));
            Check(sdk.GetFireType() == static_cast<eFireType>(value), "owned fire type preserves SDK assignment semantics");
            sdk.SetWeaponType(static_cast<eWeaponType>(value));
            Check(sdk.GetWeaponType() == static_cast<eWeaponType>(value), "type metadata retains actual SDK enum assignment");
        }
        for (const float value : {-16384.75f, -0.5f, 0.0f, 3.75f, 16777216.0f})
        {
            sdk.SetRequiredStatLevel(value);
            Check(sdk.GetRequiredStatLevel() == static_cast<float>(static_cast<int>(value)),
                  "finite representable float required stat truncates exactly as classic MTA");
        }
        for (const DWORD value : {0ul, 1ul, 17ul, 0x80000000ul, 0xfffffffful})
        {
            sdk.SetAnimGroup(value);
            Check(sdk.GetAnimGroup() == value, "animation group retains full owned CPU DWORD domain");
            sdk.SetFlags(static_cast<int>(value));
            Check(static_cast<DWORD>(sdk.GetFlags()) == value, "direct initialization retains all flag bits");
        }

        CVector* borrowed = sdk.GetFireOffset();
        CVector  source(1.25f, -2.5f, 9.75f);
        sdk.SetFireOffset(&source);
        Check(sdk.GetFireOffset() == borrowed && borrowed->fX == 1.25f && borrowed->fY == -2.5f && borrowed->fZ == 9.75f,
              "owned SDK returns stable mutable borrowed vector storage");
        source.fX = 55;
        Check(borrowed->fX == 1.25f, "vector setter copies the caller rather than retaining a temporary pointer");
        borrowed->fY = 12.5f;
        Check(sdk.GetFireOffset()->fY == 12.5f, "ordinary SDK borrowed-pointer mutation reaches the owned record");
    }

    void CheckFlags()
    {
        for (int weapon = 0; weapon <= WEAPONTYPE_INVALID; ++weapon)
        {
            Originals                 originals;
            MtaGame::CWeaponStatOwned object(static_cast<eWeaponType>(weapon), WEAPONSKILL_STD, Originals::Lookup, &originals);
            CWeaponStat&              sdk = object;
            Check(originals.calls == 0, "default initialization bypasses transition fixups");
            sdk.SetFlags(0);
            sdk.SetAnimGroup(201);
            sdk.SetFlagBits(WEAPONTYPE_TWIN_PISTOLS | 0x80000000ul);
            const bool dualSupported = (weapon >= WEAPONTYPE_PISTOL && weapon <= WEAPONTYPE_SNIPERRIFLE) || weapon == WEAPONTYPE_MINIGUN;
            Check(sdk.GetAnimGroup() == (dualSupported ? 17ul : 201ul), "dual flag chooses MTA sawnoff animation only for supported weapon types");
            Check(originals.calls == 0 && sdk.IsFlagSet(0x80000000ul), "dual enable preserves foreign bits without looking up an original");
            sdk.ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
            Check(sdk.GetAnimGroup() == (dualSupported ? originals.animGroup : 201ul),
                  "dual clear restores current provider animation only for supported types");
            Check(originals.calls == (dualSupported ? 1u : 0u), "original callback remains lazy");
            Check(static_cast<DWORD>(sdk.GetFlags()) == 0x80000000ul, "dual transitions preserve unrelated high flag bit");
        }

        for (const DWORD allowed : {0ul, static_cast<DWORD>(WEAPONTYPE_ANIM_RELOAD), static_cast<DWORD>(WEAPONTYPE_ANIM_CROUCHFIRE),
                                    static_cast<DWORD>(WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_ANIM_CROUCHFIRE)})
        {
            Originals originals;
            originals.flags = allowed;
            MtaGame::CWeaponStatOwned object(WEAPONTYPE_PISTOL, WEAPONSKILL_STD, Originals::Lookup, &originals);
            CWeaponStat&              sdk = object;
            sdk.SetFlags(0x40000000);
            sdk.SetFlagBits(WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_ANIM_CROUCHFIRE);
            Check(static_cast<DWORD>(sdk.GetFlags()) == (allowed | 0x40000000ul), "reload and crouch enable are independently limited by original support");
            Check(originals.calls == 2, "each newly requested constrained flag reads its real original source");
            originals.flags = 0;
            originals.calls = 0;
            sdk.SetFlagBits(allowed);
            Check(static_cast<DWORD>(sdk.GetFlags()) == (allowed | 0x40000000ul) && originals.calls == 0,
                  "existing animation flags survive later original changes without unnecessary lookups");
            sdk.ToggleFlagBits(allowed);
            Check(static_cast<DWORD>(sdk.GetFlags()) == 0x40000000ul && originals.calls == 0, "clearing animation flags needs no original lookup");
            sdk.ToggleFlagBits(WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_ANIM_CROUCHFIRE);
            Check(static_cast<DWORD>(sdk.GetFlags()) == 0x40000000ul, "toggle-on uses the same current original restrictions");
        }

        Originals                 originals;
        MtaGame::CWeaponStatOwned object(WEAPONTYPE_PISTOL, WEAPONSKILL_STD, Originals::Lookup, &originals);
        CWeaponStat&              sdk = object;
        sdk.SetFlags(WEAPONTYPE_TWIN_PISTOLS);
        originals.animGroup = 81;
        sdk.ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(sdk.GetAnimGroup() == 81, "dual clear reads mutable originals at the transition");
        sdk.SetWeaponType(WEAPONTYPE_MINIGUN);
        sdk.SetWeaponSkillLevel(WEAPONSKILL_PRO);
        sdk.SetFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        originals.animGroup = 91;
        sdk.ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
        Check(sdk.GetAnimGroup() == 91 && originals.requestedType == WEAPONTYPE_MINIGUN && originals.requestedSkill == WEAPONSKILL_PRO,
              "lookup follows current factory metadata rather than cached constructor values");
        originals.calls = 0;
        sdk.SetFlags(-1);
        Check(originals.calls == 0 && sdk.GetFlags() == -1 && sdk.GetAnimGroup() == 91, "direct SetFlags deliberately bypasses animation fixups");
        sdk.SetFlags(0);
        for (unsigned bit = 0; bit != 32; ++bit)
        {
            const DWORD flag = DWORD{1} << bit;
            if (flag & (WEAPONTYPE_TWIN_PISTOLS | WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_ANIM_CROUCHFIRE))
                continue;
            sdk.SetFlagBits(flag);
            Check(sdk.IsFlagSet(flag), "ordinary flag bit can be set");
            sdk.ToggleFlagBits(flag);
            Check(!sdk.IsFlagSet(flag), "ordinary flag bit toggles without policy loss");
            sdk.SetFlagBits(flag);
            sdk.ClearFlagBits(flag);
            Check(!sdk.IsFlagSet(flag), "ordinary flag bit clears without touching another bit");
        }
        Check(originals.calls == 0, "unrelated flag changes never consult the original provider");

        for (unsigned skill = 0; skill != 3; ++skill)
        {
            for (int weapon = 22; weapon <= 42; ++weapon)
            {
                MtaGame::CWeaponStatOwned actual(static_cast<eWeaponType>(weapon), static_cast<eWeaponSkill>(skill));
                const int                 baseline = actual.GetFlags();
                const DWORD               group = actual.GetAnimGroup();
                actual.SetFlags(0);
                actual.SetFlagBits(WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_ANIM_CROUCHFIRE);
                Check(actual.GetFlags() == (baseline & (WEAPONTYPE_ANIM_RELOAD | WEAPONTYPE_ANIM_CROUCHFIRE)),
                      "default original provider uses the shared MTA baseline for constrained flags");
                actual.SetFlagBits(WEAPONTYPE_TWIN_PISTOLS);
                actual.ClearFlagBits(WEAPONTYPE_TWIN_PISTOLS);
                if ((weapon >= 22 && weapon <= 34) || weapon == 38)
                    Check(actual.GetAnimGroup() == group, "default original provider restores the shared MTA baseline animation");
            }
        }
    }
}

int main()
{
    static_assert(!std::is_abstract_v<MtaGame::CWeaponStatOwned>);
    static_assert(sizeof(MtaGame::WeaponStatData) == 112 && alignof(MtaGame::WeaponStatData) == 4);
    CheckDefaults();
    CheckSdkFields();
    CheckFlags();
    std::printf("Passed %u shared MTA owned weapon-stat checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
