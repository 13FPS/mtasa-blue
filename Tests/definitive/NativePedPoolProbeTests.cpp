/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified native absence without reading retired ped object data
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativePedPoolProbeHistoricalMain
#include "NativePedPoolReaderTests.cpp"
#undef main
#pragma pop_macro("main")

namespace
{
    unsigned probeObjectReads = 0;
    Pool*    probeChanging = nullptr;
    unsigned probeMutation = 0;
    void     ProbeSawObject()
    {
        ++probeObjectReads;
        PoolTestHost::after = &ProbeSawObject;
    }
    void ChangeProbeAbsence()
    {
        if (probeMutation == 0)
            probeChanging->flags[0] = 0x81;
        else if (probeMutation == 1)
            probeChanging->Put(20, std::int32_t{2});
        else
        {
            const auto root = reinterpret_cast<std::uintptr_t>(probeChanging->header);
            probeChanging->image.Patch(P::RootRva, &root, sizeof(root), PAGE_READWRITE);
        }
    }
}
int main(int argc, char** argv)
{
    Check(NativePedPoolProbeHistoricalMain(argc, argv) == EXIT_SUCCESS, "existing native Resolve/Recheck regression executes before the new presence contract");
    const auto directory = std::filesystem::path(argv[1]) / L"probe";
    Check(std::filesystem::create_directory(directory), "retain a distinct task-owned sparse probe image without copying GTA");
    OwnedImage image(directory);
    Pool       pool(image);
    PoolTestHost::image = image.image;
    NativePedPoolReader reader({image.image}, PoolTestHost::Owner);
    auto                value = Sentinel();
    bool                present = false;
    Check(reader.Probe(1, present, value) == ClockResult::Ok && present && value.handle == 1 && value.slot == 0 &&
              value.nativePed == reinterpret_cast<std::uintptr_t>(pool.objects),
          "current occupied native generation is distinct from a successful absence");
    const auto reads = PoolTestHost::reads;
    const auto ownerCalls = PoolTestHost::ownerCalls;
    for (unsigned fail = 1; fail <= reads; ++fail)
    {
        pool.Reset();
        PoolTestHost::failAt = fail;
        value = Sentinel();
        present = true;
        Check(reader.Probe(1, present, value) != ClockResult::Ok && present && Untouched(value),
              "every native observation failure preserves presence and opaque output");
    }
    for (unsigned revoke = 1; revoke <= ownerCalls; ++revoke)
    {
        pool.Reset();
        PoolTestHost::revokeAt = revoke;
        value = Sentinel();
        present = false;
        Check(reader.Probe(1, present, value) == ClockResult::WrongThread && !present && Untouched(value),
              "owner revocation remains refusal rather than successful native absence");
    }
    for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::Stopped, ClockResult::OwnerConflict})
        for (bool initial : {false, true})
        {
            pool.Reset();
            PoolTestHost::ownerResult = status;
            present = initial;
            value = Sentinel();
            Check(reader.Probe(1, present, value) == status && present == initial && Untouched(value),
                  "generic owner statuses never become plausible false/native-null observations");
        }
    for (std::uint32_t handle : {std::uint32_t{1}})
    {
        pool.Reset();
        pool.flags[0] = 0x80;
        std::memset(pool.objects, 0xcd, sizeof(std::uintptr_t));
        probeObjectReads = 0;
        PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(pool.objects);
        PoolTestHost::after = &ProbeSawObject;
        present = true;
        value = Sentinel();
        Check(reader.Probe(handle, present, value) == ClockResult::Ok && !present && Untouched(value) && !probeObjectReads,
              "valid live-domain handles observing free flags qualify the native mismatch without reading poisoned retired ped/vtable bytes");
    }
    for (std::uint32_t handle : {std::uint32_t{0x80}, std::uint32_t{0xff}, std::uint32_t{0x180}, std::uint32_t{0x3e7ff}, std::uint32_t{0x7fffffff}})
        for (bool initial : {false, true})
        {
            pool.Reset();
            const std::uintptr_t unusableRoot = 1;
            image.Patch(P::RootRva, &unusableRoot, sizeof(unusableRoot), PAGE_READWRITE);
            present = initial;
            value = Sentinel();
            Check(reader.Probe(handle, present, value) == ClockResult::InvalidArgument && present == initial && Untouched(value),
                  "crafted nonnegative empty-bit handles refuse before native pool access rather than claiming the helper returned null");
        }
    pool.Reset();
    present = true;
    value = Sentinel();
    Check(reader.Probe(2, present, value) == ClockResult::Ok && !present && Untouched(value),
          "wrong generation on an occupied native slot is qualified absence without publishing another generation's ped");
    for (std::uint32_t handle : {std::uint32_t{0x80000000}, std::uint32_t{0x80000001}, std::uint32_t{0xffffffff}})
    {
        pool.Reset();
        const std::uintptr_t unusableRoot = 1;
        image.Patch(P::RootRva, &unusableRoot, sizeof(unusableRoot), PAGE_READWRITE);
        present = true;
        value = Sentinel();
        Check(reader.Probe(handle, present, value) == ClockResult::Ok && !present && Untouched(value),
              "signed-negative DWORDs preserve the pinned native early null branch without reading pool/header/object data");
    }
    pool.Reset();
    present = true;
    value = Sentinel();
    Check(reader.Probe(static_cast<std::uint32_t>(P::NativeCapacity << 8), present, value) == ClockResult::InvalidArgument && present && Untouched(value),
          "out-of-capacity nonnegative input keeps explicit refusal because the native consumer proves no bounded null branch");
    for (unsigned change : {0u, 1u})
    {
        pool.Reset();
        pool.flags[0] = 0x80;
        probeChanging = &pool;
        probeMutation = change;
        PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(pool.flags);
        PoolTestHost::after = &ChangeProbeAbsence;
        present = true;
        value = Sentinel();
        Check(reader.Probe(1, present, value) == ClockResult::BindingChanged && present && Untouched(value),
              "two absent results do not qualify changed native slot flags or pool-header ownership bookends");
    }
    pool.Reset();
    const std::uintptr_t zero = 0;
    image.Patch(P::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
    present = true;
    value = Sentinel();
    Check(reader.Probe(1, present, value) == ClockResult::Ok && !present && Untouched(value),
          "stable qualified null native root reports absence without manufacturing an SDK local player");
    probeChanging = &pool;
    probeMutation = 2;
    PoolTestHost::watched = image.image + P::RootRva;
    PoolTestHost::after = &ChangeProbeAbsence;
    present = false;
    value = Sentinel();
    Check(reader.Probe(1, present, value) == ClockResult::BindingChanged && !present && Untouched(value),
          "native root publication during an absent observation refuses the mixed lifetime");
    pool.Reset();
    std::memset(pool.objects, 0xcd, sizeof(std::uintptr_t));
    present = false;
    value = Sentinel();
    Check(reader.Probe(1, present, value) != ClockResult::Ok && !present && Untouched(value),
          "an occupied malformed ped is a read/binding failure, never qualified absence");
    PoolTestHost::image = 0;
    std::puts("Production native ped pool presence primitive fixture completed");
    std::printf("Passed %u native ped-pool presence checks (64-bit; no native engine code executed)\n", checks);
    return EXIT_SUCCESS;
}
