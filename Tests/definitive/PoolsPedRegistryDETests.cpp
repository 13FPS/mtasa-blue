/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual partial CPools SDK with controlled borrowed wrapper identity
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include "../../Client/game_de/CPoolsDE.h"
#include "../../Client/game_de/CPedVitalsDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include "PedPoolRegistryFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool SameToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    struct Owner final : NativeGameTimingAccess
    {
        struct Row
        {
            NativePedReferenceToken    token;
            NativePedPoolReader::Value value;
            bool                       present = true;
        };
        std::array<Row, 2>                                                rows{};
        std::array<unsigned char, 9 * NativePedPoolProfile::ObjectStride> nativeBytes{};
        std::array<unsigned char, NativePedPoolProfile::NativeCapacity>   nativeFlags{};
        ClockResult           ownerStatus = ClockResult::Ok, probeStatus = ClockResult::Ok, referenceStatus = ClockResult::Ok;
        unsigned              probes = 0, references = 0, acquisitions = 0, releases = 0;
        std::function<void()> afterReference;
        Owner()
        {
            rows[0].token = {9, 77, 37};
            rows[1].token = {10, 77, 38};
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                auto& value = rows[i].value;
                value.handle = i ? 0x87f : 1;
                value.slot = i ? 8 : 0;
                value.generation = static_cast<unsigned char>(value.handle);
                value.pool = reinterpret_cast<std::uintptr_t>(this);
                value.objects = reinterpret_cast<std::uintptr_t>(nativeBytes.data());
                value.flags = reinterpret_cast<std::uintptr_t>(nativeFlags.data());
                value.nativePed = value.objects + value.slot * NativePedPoolProfile::ObjectStride;
                value.capacity = static_cast<std::int32_t>(NativePedPoolProfile::NativeCapacity);
                value.cursor = -1;
                value.ownsAllocations = 1;
            }
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult CheckPedPoolAccess() noexcept override { return ownerStatus; }
        ClockResult ProbePedPoolHandle(std::uint32_t handle, bool& present, NativePedPoolReader::Value& output) noexcept override
        {
            ++probes;
            if (ownerStatus != ClockResult::Ok)
                return ownerStatus;
            if (probeStatus != ClockResult::Ok)
                return probeStatus;
            if (handle > 0x7fffffff)
            {
                present = false;
                return ClockResult::Ok;
            }
            if (handle >> 8 >= NativePedPoolProfile::NativeCapacity)
                return ClockResult::InvalidArgument;
            for (const auto& row : rows)
                if (row.present && row.value.handle == handle)
                {
                    output = row.value;
                    present = true;
                    return ClockResult::Ok;
                }
            present = false;
            return ClockResult::Ok;
        }
        ClockResult ResolvePedReference(const NativePedReferenceToken& token, NativePedPoolReader::Value& output) noexcept override
        {
            ++references;
            if (ownerStatus != ClockResult::Ok)
                return ownerStatus;
            if (referenceStatus != ClockResult::Ok)
                return referenceStatus;
            for (const auto& row : rows)
                if (row.present && SameToken(token, row.token))
                {
                    output = row.value;
                    if (afterReference)
                    {
                        auto action = std::move(afterReference);
                        afterReference = {};
                        action();
                    }
                    return ClockResult::Ok;
                }
            return ClockResult::OwnerConflict;
        }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& expected) noexcept override
        {
            if (ownerStatus != ClockResult::Ok)
                return ownerStatus;
            for (const auto& row : rows)
                if (row.present && row.value.handle == expected.handle && row.value.nativePed == expected.nativePed)
                    return ClockResult::Ok;
            return ClockResult::BindingChanged;
        }
        ClockResult AcquirePedReference(std::uint32_t, NativePedReferenceToken&) noexcept override
        {
            ++acquisitions;
            return ClockResult::NotInitialized;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken&) noexcept override
        {
            ++releases;
            return ClockResult::NotInitialized;
        }
    };
    struct ClientIdentity
    {
        unsigned value;
    };
    CClientEntity* Client(ClientIdentity& value)
    {
        // Association identity only: this fixture never constructs/invokes a
        // client object or treats a native address as an SDK ped wrapper.
        return reinterpret_cast<CClientEntity*>(&value);
    }
}
int main()
{
    static_assert(std::is_abstract_v<CPoolsDE> && std::is_abstract_v<CPedVitalsDE>);
    static_assert(!std::is_copy_constructible_v<CPoolsDE::BorrowedPedRegistration> && !std::is_move_constructible_v<CPoolsDE::BorrowedPedRegistration>);
    Owner                             owner, otherOwner;
    pool_registry_fixture::Pools      pool(owner);
    unsigned                          destroyed = 0;
    CPoolsDE::BorrowedPedRegistration registration;
    auto                              wrapper =
        std::make_unique<pool_registry_fixture::Ped>(owner, owner.rows[0].token,
                                                     [&]
                                                     {
                                                         ++destroyed;
                                                         Check(!registration.IsBound() && pool.GetPedCount() == 0,
                                                               "enclosing factory retires the borrowed CPU link before destroying its concrete test wrapper");
                                                     });
    pool_registry_fixture::Ped foreignWrapper(otherOwner, otherOwner.rows[0].token);
    CPed* const                sdkWrapper = wrapper.get();
    ClientIdentity             clientA{1}, clientB{2};
    Check(static_cast<void*>(sdkWrapper) != static_cast<void*>(wrapper.get()), "registration uses ordinary actual SDK multiple-inheritance base adjustment");
    Check(pool.BindBorrowedPed(foreignWrapper, Client(clientA), registration) == ClockResult::OwnerConflict && !registration.IsBound() && !pool.GetPedCount(),
          "generic or differently owned wrappers cannot manufacture a same-owner pool association");
    owner.ownerStatus = ClockResult::NotInitialized;
    Check(pool.BindBorrowedPed(*wrapper, Client(clientA), registration) == ClockResult::NotInitialized && !registration.IsBound(),
          "a matching immutable token outside its owner still cannot register a wrapper");
    owner.ownerStatus = ClockResult::Ok;
    Check(pool.BindBorrowedPed(*wrapper, Client(clientA), registration) == ClockResult::Ok && registration.IsBound() && pool.GetPedCount() == 1,
          "factory-only admission binds the actual existing wrapper and token in one canonical SDK registry");
    CPoolsDE::BorrowedPedRegistration competing;
    Check(pool.BindBorrowedPed(*wrapper, Client(clientB), competing) == ClockResult::Busy && !competing.IsBound() && pool.GetPedCount() == 1,
          "a live controlled registration cannot be overwritten by another guard or client association");
    CPools& sdk = pool;
    auto*   pair = sdk.GetPed(std::size_t{0});
    Check(pair && pair->pEntity == sdkWrapper && pair->pClientEntity == Client(clientA) && sdk.GetPedFromRef(1) == sdkWrapper,
          "actual CPools virtual dispatch returns the registered SDK base identity after genuine resolution");
    Check(sdk.GetPed(reinterpret_cast<DWORD*>(owner.rows[0].value.nativePed)) == pair,
          "native-address lookup compares fresh registered token facts and returns the same canonical pair");
    const auto probes = owner.probes, refs = owner.references;
    Check(sdk.GetPedFromRef(0x87f) == nullptr && owner.probes == probes + 1 && owner.references == refs && sdk.GetPedCount() == 1,
          "genuine native resolver runs first even when that occupied native index has no SDK row");
    Check(sdk.GetPedFromRef(0xffffffff) == nullptr && sdk.GetPedFromRef(0x80000000) == nullptr,
          "actual SDK lookup preserves every proved signed-negative native null branch without indexing a registry row");
    CPed* invalidOutput = &foreignWrapper;
    Check(pool.ReadPedFromRef(static_cast<std::uint32_t>(NativePedPoolProfile::NativeCapacity << 8), invalidOutput) == ClockResult::InvalidArgument &&
              invalidOutput == &foreignWrapper,
          "nonnegative out-of-capacity handles remain explicit unsupported input rather than inventing an unproved native null branch");
    owner.rows[0].present = false;
    Check(sdk.GetPedFromRef(1) == nullptr && sdk.GetPedCount() == 1,
          "qualified native absence returns null while retaining the distinct CPU registration count");
    owner.rows[0].present = true;
    for (auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::Stopped, ClockResult::OwnerConflict, ClockResult::ReadFailure})
    {
        owner.ownerStatus = status;
        CPed* output = &foreignWrapper;
        Check(pool.ReadPedFromRef(1, output) == status && output == &foreignWrapper && pool.GetPedCount() == 1,
              "generic owner refusal preserves outputs and never fabricates an absent SDK ped or zero native count");
        bool threw = false;
        try
        {
            sdk.GetPedFromRef(1);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw, "actual pointer-returning SDK methods expose owner refusal rather than successful null");
    }
    owner.ownerStatus = ClockResult::Ok;
    owner.referenceStatus = ClockResult::OwnerConflict;
    CPed* output = &foreignWrapper;
    Check(pool.ReadPedFromRef(1, output) == ClockResult::OwnerConflict && output == &foreignWrapper,
          "a current raw native handle does not replace the wrapper's revoked registered-cell token");
    owner.referenceStatus = ClockResult::Ok;
    owner.afterReference = [&] { pair->pClientEntity = Client(clientB); };
    Check(pool.ReadPedFromRef(1, output) == ClockResult::BindingChanged && output == &foreignWrapper,
          "a caller mutation during the actual token observation refuses the mixed canonical association");
    Check(registration.Reset() == ClockResult::BindingChanged && !registration.IsBound() && pair->pEntity == sdkWrapper &&
              pair->pClientEntity == Client(clientB) && pool.GetPedCount() == 1,
          "retired guard cannot clear a foreign client association or decrement its unrelated count");
    Check(pool.ReadPedFromRef(1, output) == ClockResult::BindingChanged && output == &foreignWrapper,
          "uncontrolled public pairs are never published as qualified DE wrappers");
    *pair = {nullptr, nullptr};
    pool.ResetPedPoolCount();
    CPoolsDE::BorrowedPedRegistration later;
    Check(pool.BindBorrowedPed(*wrapper, Client(clientA), later) == ClockResult::Ok && pool.GetPedCount() == 1 && registration.Reset() == ClockResult::Ok &&
              sdk.GetPedFromRef(1) == sdkWrapper,
          "an old withdrawn guard cannot retire a later version of the canonical registration");
    pool.InvalidateLocalPlayerClientEntity();
    Check(!pair->pClientEntity && sdk.GetPedFromRef(1) == sdkWrapper,
          "actual shared local-client invalidation preserves wrapper and controlled token identity");
    pool.ResetPedPoolCount();
    Check(!pool.GetPedCount() && sdk.GetPedFromRef(1) == sdkWrapper && later.IsBound(),
          "actual shared count-only reset leaves the same registered SDK slot available");
    Check(later.Reset() == ClockResult::Ok && pool.GetPedCount() == (std::numeric_limits<unsigned long>::max)(),
          "borrow withdrawal retains the shared established unsigned decrement after a count-only reset");
    pool.ResetPedPoolCount();
    Check(pool.BindBorrowedPed(*wrapper, Client(clientA), registration) == ClockResult::Ok, "withdrawn guard may own a new admitted row version");
    pair->pEntity = &foreignWrapper;
    Check(registration.Reset() == ClockResult::BindingChanged && pair->pEntity == &foreignWrapper && pair->pClientEntity == Client(clientA),
          "guard never clears a caller-replaced SDK wrapper identity");
    *pair = {nullptr, nullptr};
    pool.ResetPedPoolCount();
    Check(pool.BindBorrowedPed(*wrapper, nullptr, registration) == ClockResult::Ok, "own final borrow is independently admitted");
    owner.ownerStatus = ClockResult::Stopped;
    Check(registration.Reset() == ClockResult::Ok && !pool.GetPedCount() && !registration.IsBound() && !pair->pEntity && !pair->pClientEntity,
          "CPU-only borrow withdrawal remains possible after native owner stop without any native cleanup call");
    wrapper.reset();
    Check(destroyed == 1 && !owner.acquisitions && !owner.releases,
          "the enclosing caller owns actual destruction and the sole native reference controller is never duplicated by the pool");
    std::puts("Production actual DE borrowed ped registry SDK fixture completed");
    std::printf("Passed %u actual DE ped registry SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
