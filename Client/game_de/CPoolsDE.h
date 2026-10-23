/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the actual MTA ped registry on existing registered DE wrappers
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <game/CPools.h>
#include <game/CPedRegistryPolicy.h>
#include "NativePedPoolReader.h"
#include "NativePedReferenceToken.h"
#include <array>
#include <cstdint>
#include <type_traits>

namespace mta::game_de
{
    class CPedVitalsDE;
    class NativeGameTimingAccess;
    template <class ConcretePed>
    class CPedBorrowedFactoryDE;

    // This is the real SDK lookup/count slice, not a complete native pool or
    // factory. All construction/deletion and non-ped pool methods remain pure.
    class CPoolsDE : public CPools
    {
    public:
        static constexpr std::size_t PedCapacity = NativePedPoolProfile::NativeCapacity;

        // A future enclosing factory owns this guard separately from its
        // borrowed complete wrapper: pool and wrapper outlive the guard, and
        // the factory retires the guard before destroying the wrapper. This
        // withdraws only CPU registry identity, never native reference cells.
        // Like the existing mutable SDK rows, factory/guard/row operations are
        // serialized by the enclosing owner; callers cannot race withdrawal or
        // wrapper destruction against an outstanding borrowed SDK access.
        class BorrowedPedRegistration final
        {
        public:
            BorrowedPedRegistration() noexcept = default;
            ~BorrowedPedRegistration();
            BorrowedPedRegistration(const BorrowedPedRegistration&) = delete;
            BorrowedPedRegistration& operator=(const BorrowedPedRegistration&) = delete;
            BorrowedPedRegistration(BorrowedPedRegistration&&) = delete;
            BorrowedPedRegistration& operator=(BorrowedPedRegistration&&) = delete;
            ClockResult              Reset() noexcept;
            bool                     IsBound() const noexcept { return registry != nullptr; }

        private:
            friend class CPoolsDE;
            CPoolsDE*     registry = nullptr;
            std::size_t   index = PedCapacity;
            std::uint64_t version = 0;
        };

        SClientEntity<CPed>* GetPed(DWORD* nativeInterface) override;
        SClientEntity<CPed>* GetPed(std::size_t index) override;
        CPed*                GetPedFromRef(DWORD handle) override;
        CClientEntity*       GetPedClientEntity(CPed* wrapper) override;
        // These are actual CPU registration bookkeeping, not native pool
        // occupancy/readiness. Reset deliberately preserves existing rows.
        unsigned long GetPedCount() override { return pedRegistrationCount; }
        void          ResetPedPoolCount() override;
        void          InvalidateLocalPlayerClientEntity() override;

        // Status-bearing internal counterparts preserve outputs on refusal.
        // Successful null means a qualified absent native/registered identity;
        // missing callback authority is never converted into SDK absence.
        ClockResult ReadPedFromRef(std::uint32_t handle, CPed*&) noexcept;
        ClockResult ReadPedByIndex(std::size_t index, SClientEntity<CPed>*&) noexcept;
        ClockResult ReadPedByAddress(std::uintptr_t nativeAddress, SClientEntity<CPed>*&) noexcept;
        // The exact CPU wrapper identity selects one existing controlled row;
        // its current token/owner still admits the nullable client association.
        ClockResult ReadPedClientEntity(CPed* wrapper, CClientEntity*&) noexcept;
        // Select the same controlled row by exact CPU wrapper identity. The
        // copied token adds no registration and exposes no native pointer;
        // its consumer still needs current native callback admission.
        ClockResult ReadBorrowedPedReference(CPed* wrapper, NativePedReferenceToken&) noexcept;

        CPoolsDE(const CPoolsDE&) = delete;
        CPoolsDE& operator=(const CPoolsDE&) = delete;

    protected:
        explicit CPoolsDE(NativeGameTimingAccess& owner) noexcept : pedRegistryOwner(owner) {}
        virtual ~CPoolsDE();
        // Only an enclosing factory can admit a genuine derived DE wrapper.
        // Its immutable token and vitals owner must match this exact owner;
        // a generic CPed pointer or a numeric native address cannot register.
        ClockResult BindBorrowedPed(CPedVitalsDE&, CClientEntity*, BorrowedPedRegistration&) noexcept;

    private:
        // One enclosing typed factory may compose this same registry with the
        // existing controller. These checks grant no native lifetime authority;
        // they only prevent adopting a differently bound wrapper or deleting
        // an owned wrapper while any mutable canonical row still names it.
        template <class ConcretePed>
        friend class CPedBorrowedFactoryDE;
        bool HasExactBorrowedPedBinding(const CPedVitalsDE&, const NativePedReferenceToken&) const noexcept;
        bool HasBorrowedPedLink(const CPed&) const noexcept;
        struct Control
        {
            BorrowedPedRegistration* registration = nullptr;
            CPed*                    expectedPed = nullptr;
            CClientEntity*           expectedClient = nullptr;
            NativePedReferenceToken  reference{};
            std::uint64_t            version = 0;
        };
        ClockResult                                  ReadControlled(std::size_t index, NativePedPoolReader::Value&) noexcept;
        ClockResult                                  Detach(BorrowedPedRegistration&) noexcept;
        static void                                  Require(ClockResult, const char*);
        static bool                                  Same(const NativePedPoolReader::Value&, const NativePedPoolReader::Value&) noexcept;
        bool                                         Matches(std::size_t) const noexcept;
        NativeGameTimingAccess&                      pedRegistryOwner;
        std::array<SClientEntity<CPed>, PedCapacity> pedLinks{};
        std::array<Control, PedCapacity>             pedControls{};
        unsigned long                                pedRegistrationCount = 0;
    };
    static_assert(std::is_abstract_v<CPoolsDE> && std::is_base_of_v<CPools, CPoolsDE>);
}
