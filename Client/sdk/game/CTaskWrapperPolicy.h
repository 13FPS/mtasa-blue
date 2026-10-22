/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA task wrapper selection and ordered registry retirement
 *****************************************************************************/
#pragma once
#include "TaskTypes.h"
#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace MtaGame
{
    class CTaskWrapperPolicy final
    {
    public:
        // These identify existing MTA wrapper classes, not executable entries,
        // native vtables or a promise that a port can construct those classes.
        enum class Factory : std::uint8_t
        {
            Base,
            GangDriveBy,
            UseGun,
            Fight,
            UseMobilePhone,
            RunAnim,
            RunNamedAnim,
            Die,
            StealthKill,
            Sunbathe,
            Swim,
            CarSetPassenger,
            CarSetDriver,
            CarSetOut,
            EnterCarDriver,
            EnterCarPassenger,
            EnterBoatDriver,
            LeaveCar,
            WanderStandard,
            IkChain,
            IkLookAt,
            IkManager,
            Climb,
            JetPack,
            Choking,
            Duck
        };

        struct Member
        {
            eTaskType type;
            Factory   factory;
        };

        // Preserve the actual classic factory's order and exact specialization
        // set. Unsupported/unknown logical task types use its real base wrapper;
        // this table does not manufacture native task ownership or constructors.
        inline static constexpr std::array<Member, 25> Members = {{
            {TASK_SIMPLE_GANG_DRIVEBY, Factory::GangDriveBy},
            {TASK_SIMPLE_USE_GUN, Factory::UseGun},
            {TASK_SIMPLE_FIGHT, Factory::Fight},
            {TASK_COMPLEX_USE_MOBILE_PHONE, Factory::UseMobilePhone},
            {TASK_SIMPLE_ANIM, Factory::RunAnim},
            {TASK_SIMPLE_NAMED_ANIM, Factory::RunNamedAnim},
            {TASK_COMPLEX_DIE, Factory::Die},
            {TASK_SIMPLE_STEALTH_KILL, Factory::StealthKill},
            {TASK_COMPLEX_SUNBATHE, Factory::Sunbathe},
            {TASK_SIMPLE_SWIM, Factory::Swim},
            {TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER, Factory::CarSetPassenger},
            {TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER, Factory::CarSetDriver},
            {TASK_SIMPLE_CAR_SET_PED_OUT, Factory::CarSetOut},
            {TASK_COMPLEX_ENTER_CAR_AS_DRIVER, Factory::EnterCarDriver},
            {TASK_COMPLEX_ENTER_CAR_AS_PASSENGER, Factory::EnterCarPassenger},
            {TASK_COMPLEX_ENTER_BOAT_AS_DRIVER, Factory::EnterBoatDriver},
            {TASK_COMPLEX_LEAVE_CAR, Factory::LeaveCar},
            {TASK_COMPLEX_WANDER, Factory::WanderStandard},
            {TASK_SIMPLE_IK_CHAIN, Factory::IkChain},
            {TASK_SIMPLE_IK_LOOK_AT, Factory::IkLookAt},
            {TASK_SIMPLE_IK_MANAGER, Factory::IkManager},
            {TASK_SIMPLE_CLIMB, Factory::Climb},
            {TASK_SIMPLE_JETPACK, Factory::JetPack},
            {TASK_SIMPLE_CHOKING, Factory::Choking},
            {TASK_SIMPLE_DUCK, Factory::Duck},
        }};

        static constexpr Factory SelectFactory(int type) noexcept
        {
            for (const auto& member : Members)
                if (member.type == type)
                    return member.factory;
            return Factory::Base;
        }

        template <class Entries, class Native, class NativeOf, class WrapperOf>
        static auto FindCached(const Entries& entries, Native native, NativeOf&& nativeOf, WrapperOf&& wrapperOf)
            -> std::decay_t<decltype(wrapperOf(*entries.begin()))>
        {
            // Native validation must already have happened in the real caller.
            // Skip a matching null wrapper, but retain insertion order and allow
            // duplicates exactly as the classic AddTask/GetTask list does.
            for (auto* entry : entries)
                if (nativeOf(entry) == native)
                    if (auto* wrapper = wrapperOf(entry))
                        return wrapper;
            return nullptr;
        }

        template <class Entries, class Native, class NativeOf, class WrapperOf, class DeleteEntry, class RetireWrapper>
        static bool RetireFirst(Entries& entries, Native native, NativeOf&& nativeOf, WrapperOf&& wrapperOf, DeleteEntry&& deleteEntry,
                                RetireWrapper&& retireWrapper)
        {
            for (auto iter = entries.begin(); iter != entries.end(); ++iter)
            {
                auto* entry = *iter;
                if (nativeOf(entry) != native)
                    continue;
                auto* wrapper = wrapperOf(entry);
                // Remove the entry before wrapper destruction: its destructor
                // can reenter the registry through native deletion callbacks.
                // A null wrapper still consumes the FIRST matching entry here,
                // unlike cached lookup; do not merge these two search rules.
                deleteEntry(entry);
                entries.erase(iter);
                if (wrapper)
                    retireWrapper(wrapper);
                return true;
            }
            return false;
        }
    };
}
