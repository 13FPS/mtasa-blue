/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine BoundRect source/typed borrows with final original doubles
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeWorldBoundRectHistoricalMatrixMain
#include "NativeWorldPoolMatrixTests.cpp"
#undef main
#pragma pop_macro("main")
#include "NativeWorldBoundRectFixture.h"
#include "../../Client/game_de/NativeWorldBoundRectCalls.h"
#include "../../Client/game_de/NativeWorldBoundRectProfile.h"
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeModelObjectGroupMemory.cpp"
#include "../../Client/game_de/NativeWorldBoundRectMemory.cpp"
#include "../../Client/game_de/NativeWorldBoundRectCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace BoundTest
{
    namespace BM = mta::game_de::NativeWorldBoundRectMemory;
    namespace BC = mta::game_de::NativeWorldBoundRectCalls;
    namespace BP = mta::game_de::NativeWorldBoundRectProfile;
    namespace BF = NativeWorldPoolMatrixFixture;
    namespace BW = NativeWorldIplFixture;
    BF::State* selected = nullptr;
    unsigned   entries = 0, mode = 0;
    template <class T>
    T At(std::uintptr_t address)
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void*>(address), sizeof(value));
        return value;
    }
    ClockResult OriginalDouble(BC::Entry entry, std::uintptr_t entity, BC::Rect& output, BC::Rect*& returned, std::uint32_t& exception) noexcept
    {
        ++entries;
        Check(reinterpret_cast<std::uintptr_t>(entry) == selected->image + BP::EntryRva,
              "explicit final original double receives the exact native GetBoundRect whole entry");
        if (mode == 1)
        {
            exception = 0xe05b0001;
            return ClockResult::OriginalException;
        }
        // This is the labelled original double only, not a replacement geometry
        // policy. Production admission and before/after raw seals remain real.
        std::array<float, 6> box{};
        std::memcpy(box.data(), reinterpret_cast<void*>(selected->image + BP::DefaultBoxRva), sizeof(box));
        output = {1000000, -1000000, -1000000, 1000000};
        const auto matrix = At<std::uintptr_t>(entity + 0x18);
        const auto corner = [&](float x, float y, float z)
        {
            float px = 0, py = 0;
            if (matrix)
            {
                const float firstX = y * At<float>(matrix + 0x10) + x * At<float>(matrix);
                const float secondX = z * At<float>(matrix + 0x20) + At<float>(matrix + 0x30);
                const float firstY = x * At<float>(matrix + 4) + y * At<float>(matrix + 0x14);
                const float secondY = z * At<float>(matrix + 0x24) + At<float>(matrix + 0x34);
                px = firstX + secondX;
                py = firstY + secondY;
            }
            else
            {
                px = x + At<float>(entity + 8);
                py = y + At<float>(entity + 0xc);
            }
            output[0] = (std::min)(output[0], px);
            output[2] = (std::max)(output[2], px);
            output[3] = (std::min)(output[3], py);
            output[1] = (std::max)(output[1], py);
        };
        corner(box[0], box[1], box[2]);
        corner(box[3], box[4], box[5]);
        corner(box[3], box[1], box[2]);
        corner(box[0], box[4], box[5]);
        returned = mode == 2 ? nullptr : &output;
        if (mode == 3)
            output[0] = (std::numeric_limits<float>::quiet_NaN)();
        if (mode == 4)
            BW::Put(NativeModelObjectGroupFixture::Row(selected->image), 0x28, std::uint16_t{99});
        if (mode == 5)
            BW::Put(entity, 0x56, static_cast<unsigned char>(5));
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace BoundTest;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "BoundRect evidence is an explicit workspace path");
    LoadFixture fixture(argv[1]);
    const auto  image = fixture.transform.image.image;
    PoolTestHost::image = image;
    BW::State world(image, &LoadFixture::Write, &fixture);
    BF::State entities(image, &LoadFixture::Write, &fixture);
    selected = &entities;
    const auto reset = [&]
    {
        fixture.Reset();
        world.Reset();
        entities.Reset();
        NativeWorldBoundRectFixture::Install(image, &LoadFixture::Write, &fixture);
        NativeWorldBoundRectFixture::PublishProvider(fixture.graph.root, fixture.graph.Game(), reinterpret_cast<std::uintptr_t>(fixture.rows), 20000);
        entries = mode = 0;
    };
    reset();
    BM::Value  value;
    const auto initialCapture = BM::Capture(image, 1, BM::Kind::Building, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, value);
    if (initialCapture != ClockResult::Ok)
    {
        // Failure-only concrete guard observations identify the next missing
        // owned-fixture input; no production check or result is substituted.
        NativeStreamingCompletionContext::Value diagnosticWorld;
        NativeModelReferenceHeap::Value         diagnosticHeap;
        NativeWorldPoolMatrixMemory::Entity     diagnosticEntity;
        NativeModelObjectGroupMemory::Value     diagnosticModel;
        const auto                              worldResult = NativeStreamingCompletionContext::Capture(image, diagnosticWorld);
        const auto                              heapResult = NativeModelReferenceHeap::Capture(image, diagnosticHeap);
        const auto                              entityResult = NativeWorldPoolMatrixMemory::CaptureEntity(image, BM::Kind::Building, 0, diagnosticEntity);
        const auto modelResult = NativeModelObjectGroupMemory::Capture(image, 1, 17, &PoolTestHost::Owner, &PoolTestHost::Owner, diagnosticModel);
        std::printf("BoundRect diagnostic: capture=%u profile=%d matrixProfile=%d groupProfile=%d world=%u heap=%u entity=%u model=%u thread=%u\n",
                    static_cast<unsigned>(initialCapture), BM::Profile(image), NativeWorldPoolMatrixMemory::Profile(image),
                    NativeModelObjectGroupMemory::Detail::Profile(image), static_cast<unsigned>(worldResult), static_cast<unsigned>(heapResult),
                    static_cast<unsigned>(entityResult), static_cast<unsigned>(modelResult), static_cast<unsigned>(NativeStartGameCalls::CheckThread(image)));
        NativeModelBankReader::Value        bankValue;
        NativeModelBankReader               bank({image}, &PoolTestHost::Owner);
        const auto                          bankResult = bank.Read(17, bankValue);
        NativeStreamingModelContext::Value  modelContext;
        const auto                          contextResult = bankResult == ClockResult::Ok && bankValue.nativeModelInfo
                                                                ? NativeStreamingModelContext::Capture(image, bankValue, modelContext)
                                                                : ClockResult::NotInitialized;
        NativeModelObjectGroupMemory::Value modelStages;
        modelStages.generation = 1;
        modelStages.model = bankValue;
        modelStages.context = modelContext;
        modelStages.present = bankValue.nativeModelInfo != 0;
        std::uint8_t actualType = 0;
        const bool typeRead = contextResult == ClockResult::Ok && NativeModelTypeLeaf::Read(image, modelContext.typeMethod, modelStages.typeBytes, actualType);
        const bool poolRead = typeRead && NativeModelObjectGroupMemory::Detail::Pool(image, modelStages, modelStages.poolCount);
        const bool groupRead = bankValue.nativeModelInfo && NativeModelObjectGroupMemory::Detail::Group(image, bankValue.nativeModelInfo, modelStages.group);
        const auto bankAgain = bankResult == ClockResult::Ok ? bank.Recheck(bankValue) : ClockResult::NotInitialized;
        const bool anchors = poolRead && groupRead && NativeModelObjectGroupMemory::Detail::Anchors(image, modelStages, modelStages.group);
        std::printf(
            "Model guard diagnostic: bank=%u bankAgain=%u bankValid=%d pedValid=%d context=%u typeRead=%d type=%u pool=%d poolCount=%d group=%d anchors=%d "
            "metadata=%llx expected=%llx root=%llx provider=%llx rows=%llx count=%d typeMethod=%llx animMethod=%llx\n",
            static_cast<unsigned>(bankResult), static_cast<unsigned>(bankAgain), NativeModelBankReader::Revalidate({image}),
            NativePedPoolReader::Revalidate({image}), static_cast<unsigned>(contextResult), typeRead, static_cast<unsigned>(actualType), poolRead,
            modelStages.poolCount, groupRead, anchors, static_cast<unsigned long long>(bankValue.nativeModelInfo),
            static_cast<unsigned long long>(NativeModelObjectGroupFixture::Row(image)), static_cast<unsigned long long>(modelContext.root),
            static_cast<unsigned long long>(modelContext.provider), static_cast<unsigned long long>(modelContext.rows), modelContext.count,
            static_cast<unsigned long long>(modelContext.typeMethod), static_cast<unsigned long long>(modelContext.animationMethod));
        const auto inspectPins = [&](const auto& pins, const char* label)
        {
            for (const auto& pin : pins)
            {
                std::array<unsigned char, BP::MaximumPinBytes> actual{};
                const bool read = pin.size <= actual.size() && NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code);
                const bool same = read && !std::memcmp(actual.data(), pin.bytes, pin.size);
                MEMORY_BASIC_INFORMATION page{};
                VirtualQuery(reinterpret_cast<void*>(image + pin.rva), &page, sizeof(page));
                const bool readonly = pin.code || pin.rva == BP::DefaultBoxRva || NativeModelReferenceHeap::ReadOnlyImage(image, image + pin.rva, pin.size);
                if (!read || !same || !readonly)
                    std::printf("BoundRect pin: set=%s rva=%llx size=%llu read=%d same=%d readonly=%d protect=%lx\n", label,
                                static_cast<unsigned long long>(pin.rva), static_cast<unsigned long long>(pin.size), read, same, readonly, page.Protect);
            }
        };
        inspectPins(BP::Pins, "bound");
        inspectPins(NativeModelObjectGroupProfile::Pins, "atomic");
        for (const auto& slot : NativeWorldAddProfile::Slots)
        {
            std::uintptr_t actual = 0;
            const bool     read = NativeImageAccess::ReadImage(image, image + slot.rva, &actual, sizeof(actual));
            const bool     readonly = NativeModelReferenceHeap::ReadOnlyImage(image, image + slot.rva, sizeof(actual));
            if (!read || !readonly || actual != image + slot.target)
                std::printf("BoundRect slot: rva=%llx read=%d readonly=%d observed=%llx expected=%llx\n", static_cast<unsigned long long>(slot.rva), read,
                            readonly, static_cast<unsigned long long>(actual), static_cast<unsigned long long>(image + slot.target));
        }
        std::fflush(stdout);
    }
    Check(initialCapture == ClockResult::Ok && !value.hasMatrix && value.model.present && BM::Seal(image, value),
          "actual current world/heap/occupied Building and allocated normalAtomic null collision admit static fallback + exact inline +0");
    BC::Receipt receipt;
    BC::Rect    output{99, 99, 99, 99};
    BW::Put(entities.buildings.Row(0), 8, 10.0f);
    BW::Put(entities.buildings.Row(0), 0xc, 20.0f);
    Check(BC::Read(image, 1, BM::Kind::Building, 0, output, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) == ClockResult::Ok &&
              output == BC::Rect{9, 21, 11, 19} && receipt.entered && receipt.returned && !receipt.uncertain && entries == 1,
          "genuine whole-entry reader publishes only acknowledged owned four-float output; no engine original is executed");
    reset();
    entities.Move(0, mta::game_de::NativeWorldPoolMatrixMemory::List::Static, entities.dummies.Row(0));
    for (const auto& field : std::array<std::pair<std::size_t, float>, 8>{
             {{0, 2.0f}, {4, 3.0f}, {0x10, 5.0f}, {0x14, 7.0f}, {0x20, -11.0f}, {0x24, -13.0f}, {0x30, 100.0f}, {0x34, 200.0f}}})
        BW::Put(entities.Node(0), field.first, field.second);
    Check(BM::Capture(image, 1, BM::Kind::Dummy, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, value) == ClockResult::Ok && value.hasMatrix &&
              value.matrix.owner == entities.dummies.Row(0),
          "actual occupied Dummy obtains the real current matrix bank/list owner association");
    Check(BC::Read(image, 1, BM::Kind::Dummy, 0, output, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) == ClockResult::Ok &&
              output == BC::Rect{92, 209, 108, 191} && entries == 1,
          "tilted source transform follows exactly native4corners, whose result differs from an invented8corner box");
    for (unsigned scenario = 0; scenario != 11; ++scenario)
    {
        reset();
        if (scenario == 0)
            BW::Put(entities.buildings.Row(0), 0x20, entities.dummies.Row(0));
        if (scenario == 1)
            BW::Put(NativeModelObjectGroupFixture::Row(image), 0x30, entities.dummies.Row(0));
        if (scenario == 2)
            BW::Put(entities.buildings.Row(0), 0x14, std::uint32_t{0x80000000});
        if (scenario == 3)
            BW::Put(entities.buildings.Row(0), 0x14, 0.5f);
        if (scenario == 4)
            BW::Put(entities.buildings.Row(0), 8, (std::numeric_limits<float>::infinity)());
        if (scenario == 5)
            entities.buildings.flags[0] |= 0x80;
        if (scenario == 6)
            BW::Put(entities.buildings.Row(0), 0x3a, std::int16_t{-1});
        if (scenario == 7)
            BW::Put(entities.buildings.Row(0), 0x18, entities.Node(0));
        if (scenario == 8)
            BW::Put(image + mta::game_de::NativeModelObjectGroupProfile::PoolCountRva, 0, std::int32_t{0});
        if (scenario == 9)
            BW::Put(image + BP::DefaultBoxRva, 0, 2.0f);
        if (scenario == 10)
            BW::Put(NativeModelObjectGroupFixture::Row(image), 0, image + BF::EntityP::DummyVtable);
        output = {99, 99, 99, 99};
        Check(BC::Read(image, 1, BM::Kind::Building, 0, output, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) != ClockResult::Ok &&
                  output == BC::Rect{99, 99, 99, 99} && !receipt.entered && !entries,
              "actor/collision/-0/general heading/nonfinite/generation/model/free-matrix/count/default/type refusals preserve output before native entry");
    }
    for (unsigned scenario = 1; scenario != 6; ++scenario)
    {
        reset();
        mode = scenario;
        output = {99, 99, 99, 99};
        Check(BC::Read(image, 1, BM::Kind::Building, 0, output, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) != ClockResult::Ok &&
                  receipt.entered && receipt.uncertain && receipt.returned == (scenario != 1) && entries == 1 && output == BC::Rect{99, 99, 99, 99},
              "entered fault/bad returned pointer/nonfinite output/model change/typed row change never publishes stale results or replays");
    }
    reset();
    Check(BM::Capture(image, 1, BM::Kind::Building, 0, &PoolTestHost::Owner, &PoolTestHost::Owner, value) == ClockResult::Ok,
          "capture shared input observation for the separately owned World Remove controller");
    BW::Put(NativeModelObjectGroupFixture::Row(image), 0x2b, static_cast<unsigned char>(1));
    Check(!BM::Seal(image, value), "full model-row changes fail raw shared borrow rather than renewing a lifetime by same pointer");
    for (const auto& pin : BP::Pins)
    {
        reset();
        const auto byte = static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1);
        fixture.transform.image.Patch(pin.rva + pin.size - 1, &byte, 1,
                                      pin.code                       ? PAGE_EXECUTE_READ
                                      : pin.rva == BP::DefaultBoxRva ? PAGE_READWRITE
                                                                     : PAGE_READONLY);
        output = {99, 99, 99, 99};
        Check(BC::Read(image, 1, BM::Kind::Building, 0, output, &PoolTestHost::Owner, &PoolTestHost::Owner, &receipt, &OriginalDouble) ==
                      ClockResult::UnsupportedBuild &&
                  !receipt.entered && !entries && output == BC::Rect{99, 99, 99, 99},
              "whole BoundRect/transform/+0 math and each literal/default last-byte source mutation refuses before entry");
    }
    std::printf(
        "Passed %u native world bound rect checks (real source/world/typed model/matrix borrows; final original explicit double; no GTA/native execution)\n",
        checks);
    return 0;
}
