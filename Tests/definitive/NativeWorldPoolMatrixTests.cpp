/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual matrix source/pool/list memory and explicit original doubles
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldPoolMatrixHistoricalIplMain
#include "NativeWorldIplTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldPoolMatrixFixture.h"
#include "../../Client/game_de/NativeWorldPoolEntryCalls.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeWorldPoolMatrixMemory.cpp"
#include "../../Client/game_de/NativeWorldPoolEntryCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace MatrixTest
{
    namespace MatrixP = mta::game_de::NativeWorldPoolMatrixProfile;
    namespace MatrixMem = mta::game_de::NativeWorldPoolMatrixMemory;
    namespace Entries = mta::game_de::NativeWorldPoolEntryCalls;
    namespace Fixture = NativeWorldPoolMatrixFixture;
    namespace W = NativeWorldIplFixture;
    Fixture::State* selected = nullptr;
    unsigned        nativeEntries = 0, mode = 0;
    ClockResult     Entry(Entries::Entry original, void* entity, std::uint32_t& exception) noexcept
    {
        ++nativeEntries;
        const auto address = reinterpret_cast<std::uintptr_t>(original);
        const auto target = reinterpret_cast<std::uintptr_t>(entity);
        Check(address == selected->image + MatrixP::RemoveMatrixRva || address == selected->image + MatrixP::AllocateStaticRva,
              "explicit matrix original double sees only the exact whole detach/static allocation entry");
        if (mode == 1)
        {
            exception = 0xe0540001;
            return ClockResult::OriginalException;
        }
        const auto okay = address == selected->image + MatrixP::RemoveMatrixRva ? selected->Remove(target) : selected->Allocate(target);
        Check(okay, "owned native-entry double applies its acknowledged list/owner effects without native engine execution");
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace MatrixTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "matrix evidence has one explicit workspace-owned output directory");
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    W::State       world(image, &LoadFixture::Write, &fixture);
    Fixture::State matrix(image, &LoadFixture::Write, &fixture);
    selected = &matrix;
    Check(world.Ready() && matrix.Ready(), "own exact native Building/Dummy rows and1800 full matrix-link allocations");
    const auto reset = [&]
    {
        fixture.Reset();
        world.Reset();
        matrix.Reset();
        nativeEntries = mode = 0;
    };
    reset();
    MatrixMem::Value    value;
    MatrixMem::Entity   building, dummy;
    MatrixMem::NodeView node;
    Check(MatrixMem::Profile(image) && MatrixMem::Capture(image, value) == ClockResult::Ok && value.nodes.size() == 1800 && value.lists[2].size() == 1800,
          "real source/world/heap/bank cookie and every node in three reciprocal disjoint lists qualify without a native call");
    Check(MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok &&
              MatrixMem::CaptureEntity(image, MatrixMem::Kind::Dummy, 0, dummy) == ClockResult::Ok && !building.matrix && !dummy.matrix &&
              MatrixMem::RequireFree(value, 1800) == ClockResult::Ok && MatrixMem::RequireFree(value, 1801) == ClockResult::NotInitialized,
          "actual current occupied native generations qualify separately; an exhausted prefix refuses native owner eviction");
    reset();
    matrix.Move(0, MatrixMem::List::Dynamic, matrix.buildings.Row(0));
    matrix.Move(1, MatrixMem::List::Static, matrix.dummies.Row(0));
    Check(MatrixMem::Capture(image, value) == ClockResult::Ok && value.lists[0].size() == 1 && value.lists[1].size() == 1 && value.lists[2].size() == 1798 &&
              MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok &&
              MatrixMem::FindOwner(value, building, node) == ClockResult::Ok && node.list == MatrixMem::List::Dynamic && node.owner == building.address,
          "native bank membership plus independent typed pool identity admits a real Building matrix owner");
    const auto originalNode = node;
    W::Put(matrix.Node(0), 0x30, 125.0f);
    W::Put(matrix.Node(0), 0x34, -20.0f);
    W::Put(matrix.Node(0), 0x38, 15.0f);
    Check(!MatrixMem::Seal(image, value), "transform edits fail the final raw matrix-bank equality fence");
    Check(MatrixMem::Capture(image, value) == ClockResult::Ok && MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok &&
              MatrixMem::FindOwner(value, building, node) == ClockResult::Ok,
          "capture current finite transform before native detachment");
    MatrixMem::Transform transform{};
    std::copy_n(node.bytes.begin(), transform.size(), transform.begin());
    Entries::Receipt receipt;
    Check(Entries::Invoke(Entries::Operation::RemoveMatrix, image, building.address, receipt, &Entry) == ClockResult::Ok && receipt.entered &&
              receipt.returned && nativeEntries == 1,
          "real whole-entry thunk exposes exact accepted/returned native matrix removal boundaries");
    MatrixMem::Value removed;
    Check(MatrixMem::Capture(image, removed) == ClockResult::Ok && MatrixMem::SameBank(value, removed) &&
              MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok && !building.matrix && removed.nodes[0].owner == 0 &&
              removed.nodes[0].list == MatrixMem::List::Free && MatrixMem::RequireFree(removed, 1) == ClockResult::Ok,
          "returned detach acknowledges a cleared owner pointer and the same original link in the genuine free list");
    Check(Entries::Invoke(Entries::Operation::AllocateStaticMatrix, image, building.address, receipt, &Entry) == ClockResult::Ok && receipt.entered &&
              receipt.returned && nativeEntries == 2,
          "restoration invokes the actual static-list allocator entry, with a qualified free prefix and no dynamic fallback");
    MatrixMem::Value restored;
    Check(MatrixMem::Capture(image, restored) == ClockResult::Ok &&
              MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok &&
              MatrixMem::FindOwner(restored, building, node) == ClockResult::Ok && node.list == MatrixMem::List::Static,
          "restored matrix has a new current typed-owner association in native list2");
    const auto linkBefore = node.bytes;
    Check(MatrixMem::StoreTransform(image, restored, building, transform) == ClockResult::Ok &&
              std::memcmp(reinterpret_cast<void*>(node.address), transform.data(), transform.size()) == 0 &&
              std::memcmp(reinterpret_cast<void*>(node.address + 0x40), linkBefore.data() + 0x40, linkBefore.size() - 0x40) == 0,
          "actual transform write preserves the allocated node's attachment metadata, owner and intrusive prev/next bytes");
    Check(originalNode.address == node.address, "test double reused the released free-head link, without assuming this for production restoration");
    for (unsigned scenario = 0; scenario != 13; ++scenario)
    {
        reset();
        matrix.Move(0, MatrixMem::List::Dynamic, matrix.buildings.Row(0));
        if (scenario == 0)
            W::Put(reinterpret_cast<std::uintptr_t>(matrix.allocation), 0, std::uint64_t{1799});
        if (scenario == 1)
            matrix.Global(MatrixP::BankRva, std::uintptr_t{0});
        if (scenario == 2)
            W::Put(matrix.Node(0), MatrixP::PrevOffset, matrix.Node(0));
        if (scenario == 3)
            W::Put(matrix.Node(0), MatrixP::NextOffset, matrix.Node(0));
        if (scenario == 4)
            W::Put(matrix.Head(MatrixMem::List::Static), MatrixP::NextOffset, matrix.Node(0));
        if (scenario == 5)
            W::Put(matrix.Tail(MatrixMem::List::Free), MatrixP::PrevOffset, matrix.Node(1));
        if (scenario == 6)
            W::Put(matrix.Node(1), MatrixP::OwnerOffset, matrix.buildings.Row(0));
        if (scenario == 7)
            W::Put(matrix.Node(0), MatrixP::OwnerOffset, std::uintptr_t{0});
        if (scenario == 8)
            W::Put(matrix.buildings.Row(0), 0x18, matrix.Node(1));
        if (scenario == 9)
            W::Put(matrix.Node(0), MatrixP::OwnerOffset, std::uintptr_t{0x1234});
        if (scenario == 10)
            W::Put(matrix.Head(MatrixMem::List::Dynamic), MatrixP::NextOffset, matrix.Tail(MatrixMem::List::Dynamic));
        if (scenario == 11)
            W::Put(matrix.Node(1799), MatrixP::NextOffset, matrix.Node(1798));
        if (scenario == 12)
            W::Put(matrix.Head(MatrixMem::List::Free), MatrixP::NextOffset, matrix.Bank() + 1);
        value.bank = 0xdeadbeef;
        Check(MatrixMem::Capture(image, value) != ClockResult::Ok && value.bank == 0xdeadbeef && !nativeEntries,
              "bad cookie/root/reciprocity/cycle/list alias/owner route refuses unchanged observation before native effects");
    }
    for (unsigned scenario = 0; scenario != 7; ++scenario)
    {
        reset();
        matrix.Move(0, MatrixMem::List::Dynamic, matrix.buildings.Row(0));
        if (scenario == 0)
            matrix.buildings.flags[0] |= 0x80;
        if (scenario == 1)
            W::Put(matrix.buildings.Address(), 0x10, std::int32_t{13999});
        if (scenario == 2)
            W::Put(matrix.buildings.Row(0), 0x6a, static_cast<unsigned char>(5));
        if (scenario == 3)
            W::Put(matrix.buildings.Row(0), 0, image + Fixture::EntityP::DummyVtable);
        if (scenario == 4)
            W::Put(matrix.buildings.Address(), 8, matrix.buildings.Row(0));
        if (scenario == 5)
            matrix.Global(Fixture::EntityP::BuildingRoot, std::uintptr_t{0});
        if (scenario == 6)
            W::Put(matrix.buildings.Address(), 0x19, static_cast<unsigned char>(1));
        building.address = 0xdeadbeef;
        Check(MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) != ClockResult::Ok && building.address == 0xdeadbeef && !nativeEntries,
              "heap/readability or matrix-owner association cannot replace exact current typed native pool generation/layout");
    }
    for (unsigned scenario = 0; scenario != 4; ++scenario)
    {
        reset();
        matrix.Move(0, MatrixMem::List::Dynamic, matrix.buildings.Row(0));
        if (scenario == 0)
            W::Put(matrix.Node(0), 0x40, matrix.Node(1));
        if (scenario == 1)
            W::Put(matrix.Node(0), 0x48, std::uint64_t{1});
        if (scenario == 2)
            W::Put(matrix.Node(0), 0x30, (std::numeric_limits<float>::quiet_NaN)());
        if (scenario == 3)
            W::Put(matrix.Node(0), 0x14, (std::numeric_limits<float>::infinity)());
        Check(MatrixMem::Capture(image, value) == ClockResult::Ok &&
                  MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok &&
                  MatrixMem::FindOwner(value, building, node) == (scenario == 1 ? ClockResult::Ok : ClockResult::UnsupportedBuild) && !nativeEntries,
              "actual attached pointer and nonfinite values refuse; opaque48 bytes alone do not invent a flag-width precondition");
    }
    reset();
    W::Put(matrix.Node(1), 0x40, matrix.buildings.Row(0));
    Check(MatrixMem::Capture(image, value) == ClockResult::Ok && MatrixMem::RequireFree(value, 1) == ClockResult::Ok &&
              MatrixMem::RequireFree(value, 2) == ClockResult::UnsupportedBuild,
          "ordered free-prefix admission refuses an attached second consumed node even though many later free nodes are safe");
    reset();
    W::Put(matrix.Node(0), 0x49, static_cast<unsigned char>(0xa5));
    Check(MatrixMem::Capture(image, value) == ClockResult::Ok && MatrixMem::RequireFree(value, 1) == ClockResult::Ok,
          "nonzero opaque padding49 with zero low byte48 does not reject the actual ordered free prefix");
    matrix.Move(0, MatrixMem::List::Dynamic, matrix.buildings.Row(0));
    Check(MatrixMem::Capture(image, value) == ClockResult::Ok && MatrixMem::CaptureEntity(image, MatrixMem::Kind::Building, 0, building) == ClockResult::Ok &&
              MatrixMem::FindOwner(value, building, node) == ClockResult::Ok && node.opaque48 != 0 && MatrixMem::Seal(image, value),
          "selected matrix qualification preserves opaque48..4f without requiring padding to be zero");
    W::Put(matrix.Node(0), 0x49, static_cast<unsigned char>(0xa6));
    Check(!MatrixMem::Seal(image, value), "changing the admitted opaque padding still fails its full raw byte seal");
    for (const auto& pin : MatrixP::MatrixPins)
    {
        reset();
        const auto byte = static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1);
        fixture.transform.image.Patch(pin.rva + pin.size - 1, &byte, 1, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        value.bank = 0xdeadbeef;
        Check(MatrixMem::Capture(image, value) == ClockResult::UnsupportedBuild && value.bank == 0xdeadbeef && !nativeEntries,
              "exact initializer/static allocator/whole removal/sign-mask last-byte mutations refuse before output/effects");
    }
    reset();
    matrix.Move(0, MatrixMem::List::Dynamic, matrix.buildings.Row(0));
    mode = 1;
    Check(Entries::Invoke(Entries::Operation::RemoveMatrix, image, matrix.buildings.Row(0), receipt, &Entry) == ClockResult::OriginalException &&
              receipt.entered && !receipt.returned && receipt.exception == 0xe0540001,
          "SEH-equivalent explicit original fault remains visibly entered for the consuming retained-intent controller");
    const auto count = nativeEntries;
    Check(Entries::Invoke(static_cast<Entries::Operation>(255), image, matrix.buildings.Row(0), receipt, &Entry) == ClockResult::InvalidArgument &&
              !receipt.entered && nativeEntries == count,
          "an invalid whole-entry role refuses before native acceptance");
    std::printf("Passed %u world pool matrix checks (actual memory/source/pool/list/write; final native entries explicit doubles; no GTA/native execution)\n",
                checks);
    return 0;
}
