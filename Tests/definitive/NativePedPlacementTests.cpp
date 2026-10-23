/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real ped/matrix XYZ storage with explicit persistent write prefixes
 ****************************************************************************/
#define main PlacementHistoricalReferenceMain
#include "NativePedReferenceTests.cpp"
#undef main
#include "../../Client/game_de/NativePedPlacementMemory.h"
#include "../../Client/game_de/NativePedPoseMutation.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include <functional>
#include <thread>
#include <intrin.h>
namespace PlacementHost
{
    unsigned              exchanges = 0, failBefore = 0, failAfter = 0;
    std::function<void()> before, after;
    bool                  Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++exchanges;
        if (failBefore == exchanges)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        LONG old = 0, next = 0;
        std::memcpy(&old, &expected, 4);
        std::memcpy(&next, &target, 4);
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), next, old);
        std::memcpy(&observed, &previous, 4);
        if (after)
            after();
        if (failAfter == exchanges)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
    void Reset()
    {
        exchanges = failBefore = failAfter = 0;
        before = after = {};
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
    }
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
#define MTA_DE_PED_PLACEMENT_EXCHANGE PlacementHost::Exchange
#include "../../Client/game_de/NativePedPlacementMemory.cpp"
#undef MTA_DE_PED_PLACEMENT_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "EntityPlacementPolicyTests.inl"
namespace
{
    struct PlacementBytes
    {
        std::array<unsigned char, NativePedPoolProfile::ObjectStride> ped;
        std::array<unsigned char, 0x58>                               matrix;
    };
    PlacementBytes SnapshotPlacement(ReferenceFixture& fixture)
    {
        PlacementBytes bytes;
        std::memcpy(bytes.ped.data(), fixture.transform.pool.objects, bytes.ped.size());
        std::memcpy(bytes.matrix.data(), fixture.transform.matrix, bytes.matrix.size());
        return bytes;
    }
    void PlacementNeighbors(const PlacementBytes& before, const PlacementBytes& after, bool matrix)
    {
        for (std::size_t i = 0; i < before.ped.size(); ++i)
            if (matrix || i < 8 || i >= 20)
                Check(before.ped[i] == after.ped[i], "position transaction preserves every unowned ped byte");
        for (std::size_t i = 0; i < before.matrix.size(); ++i)
            if (!matrix || i < 0x30 || i >= 0x3c)
                Check(before.matrix[i] == after.matrix[i], "basis, matrix padding and current back-reference remain unowned");
    }
    void ResetPlacement(ReferenceFixture& fixture, bool matrix, bool player = false)
    {
        fixture.Reset();
        fixture.transform.Store(
            0, fixture.transform.image.image + (player ? NativePedTransformProfile::PlayerVtableRva : NativePedTransformProfile::PedVtableRva));
        fixture.transform.Store(NativePedTransformProfile::MatrixPointerOffset,
                                matrix ? reinterpret_cast<std::uintptr_t>(fixture.transform.matrix) : std::uintptr_t{0});
        fixture.transform.Store(0x3a, std::uint16_t{17});
        fixture.transform.Store(0x6a, static_cast<unsigned char>(3));
        for (std::size_t row = 0; row != 4; ++row)
            fixture.transform.Matrix(row * 16 + 12, std::uint32_t{0xa53178ef});
        PlacementHost::Reset();
    }
    CVector PositionAt(std::uintptr_t address)
    {
        CVector value;
        std::memcpy(&value, reinterpret_cast<void*>(address), sizeof(value));
        return value;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "placement evidence remains inside its explicit workspace directory");
    RunEntityPlacementPolicyTests();
    ReferenceFixture fixture(argv[1]);
    PoolTestHost::image = ReferenceTestHost::image = fixture.transform.image.image;
    for (const auto& pin : NativePedMetadataProfile::Code)
        fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const NativePedPlacementMemory::Binding binding{fixture.transform.image.image};
    NativePedPlacementMemory                memory(binding, &PoolTestHost::Owner);
    Check(NativePedPlacementMemory::Revalidate(binding), "existing exact transform/metadata/native admission profiles qualify position storage");
    NativePedPlacementMemory::Receipt receipt;
    NativePedPoolReader::Value        ped;
    const auto                        resolve = [&]
    {
        Check(NativePedPoolReader(binding, &PoolTestHost::Owner).Resolve(1, ped) == ClockResult::Ok,
              "each position test starts from a fresh occupied ped observation");
        PlacementHost::Reset();
    };
    const CVector target(-0.0f, (std::numeric_limits<float>::max)(), -3.25f);
    for (const bool matrix : {false, true})
        for (const bool player : {false, true})
        {
            ResetPlacement(fixture, matrix, player);
            resolve();
            const auto before = SnapshotPlacement(fixture);
            Check(memory.Set(ped, target, &receipt) == ClockResult::Ok && receipt.entered && receipt.verified && receipt.attemptedMask == 7 &&
                      receipt.appliedMask == 7 && receipt.observedMask == 7 && !receipt.foreignMask && PlacementHost::exchanges == 3 &&
                      receipt.address == (matrix ? reinterpret_cast<std::uintptr_t>(fixture.transform.matrix) + 0x30 : ped.nativePed + 8) &&
                      receipt.modelIndex == 17,
                  "qualified generic/player inline/matrix positions commit each raw XYZ DWORD once without native teleport or unit conversion");
            const auto actual = PositionAt(receipt.address);
            Check(std::memcmp(&actual, &target, sizeof(target)) == 0,
                  "position preserves signed zero and every finite target bit, without getter repair bounds");
            PlacementNeighbors(before, SnapshotPlacement(fixture), matrix);
            PlacementHost::Reset();
            const auto* alias = reinterpret_cast<const CVector*>(receipt.address);
            Check(memory.Set(ped, *alias, &receipt) == ClockResult::Ok && receipt.verified && !receipt.entered && !receipt.attemptedMask &&
                      receipt.unchangedMask == 7 && !PlacementHost::exchanges,
                  "whole-source capture preserves exact self-alias and unchanged values without inventing a write");
        }
    for (const auto model : {std::uint16_t{449}, std::uint16_t{537}, std::uint16_t{538}, std::uint16_t{569}, std::uint16_t{570}, std::uint16_t{590}})
    {
        ResetPlacement(fixture, true);
        fixture.transform.Store(0x3a, model);
        resolve();
        const auto before = SnapshotPlacement(fixture);
        Check(memory.Set(ped, target, &receipt) == ClockResult::UnsupportedBuild && !receipt.entered && !PlacementHost::exchanges,
              "unqualified classic train post-effects are refused before any component mutation");
        Check(before.ped == SnapshotPlacement(fixture).ped && before.matrix == SnapshotPlacement(fixture).matrix,
              "train refusal preserves all placement storage");
    }
    for (const auto bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        ResetPlacement(fixture, true);
        resolve();
        const auto before = SnapshotPlacement(fixture);
        Check(memory.Set(ped, CVector(1, bad, 3), &receipt) == ClockResult::InvalidArgument && !receipt.entered && !PlacementHost::exchanges &&
                  before.ped == SnapshotPlacement(fixture).ped && before.matrix == SnapshotPlacement(fixture).matrix,
              "native position primitive independently refuses nonfinite target storage");
    }
    for (unsigned failure = 1; failure != 4; ++failure)
    {
        ResetPlacement(fixture, true);
        resolve();
        PlacementHost::failAfter = failure;
        Check(memory.Set(ped, target, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.verified &&
                  receipt.appliedMask == static_cast<std::uint8_t>((1u << (failure - 1)) - 1) && PlacementHost::exchanges == failure,
              "unknown acknowledgement retains the exact accepted XYZ prefix and never retries a component");
        const auto    actual = PositionAt(receipt.address);
        const CVector original(10, 20, 30);
        const auto*   actualBits = reinterpret_cast<const unsigned char*>(&actual);
        const auto*   targetBits = reinterpret_cast<const unsigned char*>(&target);
        const auto*   originalBits = reinterpret_cast<const unsigned char*>(&original);
        for (unsigned component = 0; component != 3; ++component)
            Check(std::memcmp(actualBits + component * 4, (component < failure ? targetBits : originalBits) + component * 4, 4) == 0,
                  "fault receipt never rolls back an unknown accepted component or writes the remaining suffix");
    }
    ResetPlacement(fixture, true);
    resolve();
    PlacementHost::failBefore = 1;
    Check(memory.Set(ped, target, &receipt) == ClockResult::WriteFailure && receipt.entered && !receipt.appliedMask && receipt.attemptedMask == 1 &&
              PositionAt(receipt.address) == CVector(10, 20, 30),
          "a final-boundary fault is entered even when no store acknowledgement exists");
    ResetPlacement(fixture, true);
    resolve();
    PlacementHost::before = [&]
    {
        NativePedPlacementMemory::Receipt nested;
        Check(memory.Set(ped, CVector(1, 2, 3), &nested) == ClockResult::Busy && !nested.entered,
              "a nested adapter pose mutation cannot overlap the admitted component transaction");
    };
    Check(memory.Set(ped, target, &receipt) == ClockResult::Ok && PlacementHost::exchanges == 3,
          "the shared CPU pose scope releases after successful native storage");
    for (unsigned change = 0; change != 5; ++change)
    {
        ResetPlacement(fixture, true);
        resolve();
        PlacementHost::after = [&]
        {
            if (PlacementHost::exchanges != 1)
                return;
            if (change == 0)
                fixture.transform.Matrix(0x38, std::uint32_t{0xa53178ef});
            if (change == 1)
                fixture.transform.Store(NativePedTransformProfile::MatrixPointerOffset, std::uintptr_t{0});
            if (change == 2)
                fixture.transform.Store(0x3a, std::uint16_t{449});
            if (change == 3)
                fixture.transform.Matrix(NativePedTransformProfile::MatrixOwnerOffset, std::uintptr_t{0});
            if (change == 4)
                PoolTestHost::failAt = PoolTestHost::reads + 1;
        };
        Check(memory.Set(ped, target, &receipt) != ClockResult::Ok && receipt.entered && receipt.appliedMask == 1 && PlacementHost::exchanges == 1 &&
                  !receipt.verified,
              "changed suffix, matrix identity/owner/model or next read stops after the accepted prefix without retarget or replay");
        Check(std::memcmp(fixture.transform.matrix + 0x30, &target, 4) == 0, "accepted original matrix X remains a persistent effect after late refusal");
    }
    ResetPlacement(fixture, true);
    resolve();
    PoolTestHost::ownerResult = ClockResult::Expired;
    Check(memory.Set(ped, target, &receipt) == ClockResult::Expired && !receipt.entered && !PlacementHost::exchanges,
          "position storage cannot borrow an expired callback through the old numeric pool observation");
    ResetPlacement(fixture, true);
    resolve();
    ClockResult foreign = ClockResult::Ok;
    std::thread other([&] { foreign = memory.Set(ped, target, &receipt); });
    other.join();
    Check(foreign == ClockResult::WrongThread && !receipt.entered && !PlacementHost::exchanges, "actual native GameThread identity remains mandatory");
    ResetPlacement(fixture, true);
    resolve();
    {
        NativePedPoseMutation scope;
        Check(scope && memory.Set(ped, target, &receipt) == ClockResult::Busy && !receipt.entered,
              "the same mutation scope used by native Teleport excludes placement before its first CAS");
    }
    Check(memory.Set(ped, target, &receipt) == ClockResult::Ok, "pose scope releases on bounded refusal without preventing later normal placement");
    std::puts("Production native ped placement primitive fixture completed");
    std::printf("Passed %u native ped placement checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
