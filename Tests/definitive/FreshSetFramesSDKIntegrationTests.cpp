/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual common IFP construction over the real fresh sequence bridge
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalFreshSetFramesControllerFixtureMain
#include "NativeFreshUncompressedSequenceControllerTests.cpp"
#undef main
#pragma pop_macro("main")
#include "AnimationSequenceDEFixture.h"
#include "../../Client/mods/deathmatch/logic/CIFPAnimationStorage.h"
namespace FreshSetFramesIntegration
{
    using namespace mta::game_de;
    namespace Host = UncompressedControllerHost;
    using Controller = Host::Controller;
    using Token = NativeAnimationSequenceReferenceToken;
    unsigned bridgeChecks = 0;
    void     Verify(bool value, const char* message)
    {
        ++bridgeChecks;
        Check(value, message);
    }
    class Service final : public NativeAnimationSequenceAccess
    {
    public:
        Service(Controller& value, std::uintptr_t imageValue) noexcept : controller(value), image(imageValue) {}
        unsigned                     compressedCalls = 0, uncompressedCalls = 0;
        Controller::FrameBindReceipt receipt;
        ClockResult PrepareFreshCompressedAnimationSequence(const Token& token, std::size_t count, bool root, std::uintptr_t base) noexcept override
        {
            ++compressedCalls;
            return controller.PrepareFreshCompressedSequence(image, token, count, root, base, &Host::Owner, &receipt);
        }
        ClockResult PrepareFreshUncompressedAnimationSequence(const Token& token, std::size_t count, bool root, std::uintptr_t base) noexcept override
        {
            ++uncompressedCalls;
            return controller.PrepareFreshUncompressedSequence(image, token, count, root, base, &Host::Owner, &receipt);
        }
        ClockResult InitializeAnimationSequence(const Token& token) noexcept override { return controller.InitializeSequence(image, token, &Host::Owner); }
        ClockResult SetAnimationSequenceName(const Token& token, const NativeAnimationSequenceName& name) noexcept override
        {
            return controller.SetSequenceName(image, token, name, &Host::Owner);
        }
        ClockResult SetAnimationSequenceBoneTag(const Token& token, std::int32_t bone) noexcept override
        {
            return controller.SetSequenceBoneTag(image, token, bone, &Host::Owner);
        }
        ClockResult ReadAnimationSequence(const Token& token, NativeAnimationSequenceValues& output) noexcept override
        {
            return controller.ReadSequence(image, token, &Host::Owner, output);
        }
        ClockResult ReadAnimationSequenceHash(const Token& token, std::uint32_t& output) noexcept override
        {
            return controller.ReadSequenceHash(image, token, &Host::Owner, output);
        }
        ClockResult CopyAnimationSequenceProperties(const Token& token, std::uintptr_t source) noexcept override
        {
            return controller.CopySequenceProperties(image, token, source, &Host::Owner);
        }
        ClockResult ReadAnimationSequenceFrame(const Token& token, std::size_t index, std::uint32_t stride, std::uintptr_t& output) noexcept override
        {
            return controller.ReadSequenceFrame(image, token, index, stride, &Host::Owner, output);
        }

    private:
        Controller&    controller;
        std::uintptr_t image;
    };
    struct ResourceOwner
    {
        Controller&    controller;
        std::uintptr_t image;
        std::uint64_t  generation;
        unsigned       frameFrees = 0, interfaceFrees = 0;
        // These two dependencies are real storage operations, not a supplied
        // complete CAnimManager/CGame or a model of allocation authority.
        void FreeKeyFramesMemory(void* value)
        {
            ++frameFrees;
            if (controller.FreeFrames(image, generation, reinterpret_cast<std::uintptr_t>(value), &Host::Owner) != ClockResult::Ok)
                throw std::runtime_error("actual frame retirement refused");
        }
        void DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface* value)
        {
            ++interfaceFrees;
            if (controller.FreeStorage(image, generation, reinterpret_cast<std::uintptr_t>(value), &Host::Owner) != ClockResult::Ok)
                throw std::runtime_error("actual metadata-only retirement refused");
        }
    };
    using Receiver = animation_sequence_fixture::AnimationSequenceReceiver;
    void Initialize(CAnimBlendSequence& sequence)
    {
        sequence.Initialize();
        sequence.SetName("fresh");
        sequence.SetBoneTag(17);
    }
}
int main(int argc, char** argv)
{
    using namespace FreshSetFramesIntegration;
    namespace Heap = NativeCustomAnimSequenceStorageFixture;
    constexpr std::uint64_t generation = 0xfedcba98765432d1ull;
    static_assert(!std::is_abstract_v<CAnimBlendSequenceDE>);
    static_assert(!std::is_constructible_v<CAnimBlendSequenceDE, NativeAnimationSequenceAccess&, const Token&>);
    static_assert(std::is_same_v<decltype(&Receiver::SetKeyFrames), decltype(&CAnimBlendSequenceDE::SetKeyFrames)>);
    Check(argc == 2, "fresh SDK bridge fixture owns its evidence directory");
    SpecialFixture fixture(argv[1]);
    Heap::State    heap;
    Heap::active = &heap;
    const auto image = fixture.transform.image.image;
    const auto reset = [&]
    {
        fixture.Reset();
        PoolTestHost::image = image;
        heap.Reset(image, &SpecialFixture::Patch, &fixture);
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            SpecialFixture::Patch(&fixture, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        PoolTestHost::watched = 0;
        PoolTestHost::after = nullptr;
        Host::writes = Host::failureMode = 0;
        Host::reuseAddress = 0;
    };
    {
        NativeAnimationSequenceAccess missing;
        Receiver                      receiver(missing, {generation, 0x123400u, 0});
        for (bool compressed : {false, true})
        {
            bool refused = false;
            try
            {
                receiver.SetKeyFrames(3, false, compressed, reinterpret_cast<void*>(0x2000u));
            }
            catch (const std::runtime_error& error)
            {
                refused = std::string(error.what()) == "DE custom animation sequence owner refused SetKeyFrames: 2";
            }
            Verify(refused, "missing optional constructor became a successful generic setter");
        }
    }
    struct Shape
    {
        bool        compressed, root;
        std::size_t count;
    };
    for (const Shape shape : {Shape{true, false, 65536}, Shape{false, false, 65536}, Shape{false, true, 3}})
    {
        reset();
        Controller        controller;
        Service           service(controller, image);
        ResourceOwner     owner{controller, image, generation};
        std::uintptr_t    tag = 0, frames = 0;
        const std::size_t stride = shape.compressed ? (shape.root ? 16u : 10u) : (shape.root ? 32u : 20u);
        const auto        bytes = static_cast<std::uint32_t>(shape.count * stride);
        Check(controller.Allocate(image, generation, 1, &Host::Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, bytes, &Host::Owner, frames) == ClockResult::Ok,
              "issue original construction records");
        Receiver            receiver(service, {generation, tag, 0});
        CAnimBlendSequence& sdk = receiver;
        Initialize(sdk);
        if (!shape.compressed)
            Host::Fill(frames, shape.count, shape.root);
        const std::vector<unsigned char> payload(reinterpret_cast<unsigned char*>(frames), reinterpret_cast<unsigned char*>(frames) + bytes);
        MtaGame::IfpStorage::AttachFrames(owner, sdk, shape.count, shape.root, shape.compressed, reinterpret_cast<BYTE*>(frames));
        Controller::AcceptedFrameFormat format;
        Verify(controller.ReadSequenceFrameFormat(image, {generation, tag, 0}, &Host::Owner, format) == ClockResult::Ok &&
                   format.format == (shape.compressed ? Controller::FrameFormat::Compressed : Controller::FrameFormat::Uncompressed) &&
                   format.count == shape.count && format.root == shape.root && format.frameBase == frames,
               "actual common AttachFrames did not route to the qualified current encoding owner");
        Verify(service.compressedCalls == (shape.compressed ? 1u : 0u) && service.uncompressedCalls == (shape.compressed ? 0u : 1u) &&
                   service.receipt.count == shape.count && service.receipt.bytes == bytes && Host::writes == 1 &&
                   sdk.GetKeyFramesCount() == static_cast<std::uint16_t>(shape.count),
               "SDK branch/count/root/base transport changed full source intent");
        Verify(!std::memcmp(payload.data(), reinterpret_cast<void*>(frames), bytes) && !owner.frameFrees && !heap.freeCalls,
               "fresh construction allocated, converted payload or relinquished supplied buffers");
        owner.FreeKeyFramesMemory(reinterpret_cast<void*>(frames));
        owner.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
        Verify(!controller.IsRetained(), "successful bridge hid original storage debt");
    }
    reset();
    {
        Controller     controller;
        Service        service(controller, image);
        ResourceOwner  owner{controller, image, generation};
        std::uintptr_t tag = 0, first = 0, rejected = 0;
        Check(controller.Allocate(image, generation, 1, &Host::Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 60, &Host::Owner, first) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 60, &Host::Owner, rejected) == ClockResult::Ok,
              "issue repeated-construction inputs");
        Receiver receiver(service, {generation, tag, 0});
        Initialize(receiver);
        MtaGame::IfpStorage::AttachFrames(owner, receiver, 3, false, false, reinterpret_cast<BYTE*>(first));
        bool refused = false;
        try
        {
            MtaGame::IfpStorage::AttachFrames(owner, receiver, 3, false, false, reinterpret_cast<BYTE*>(rejected));
        }
        catch (const std::exception&)
        {
            refused = true;
        }
        Verify(refused && Host::writes == 1 && owner.frameFrees == 1 && !heap.Contains(rejected) && heap.Contains(first) &&
                   reinterpret_cast<std::uintptr_t>(receiver.GetKeyFrames()) == first,
               "repeated state guessed replacement policy or common rejection freed the accepted original");
        owner.FreeKeyFramesMemory(reinterpret_cast<void*>(first));
        owner.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag));
    }
    reset();
    {
        Controller     controller;
        Service        service(controller, image);
        ResourceOwner  owner{controller, image, generation};
        std::uintptr_t source = 0, target = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Host::Owner, source) == ClockResult::Ok &&
                  controller.Allocate(image, generation, 1, &Host::Owner, target) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 20, &Host::Owner, frames) == ClockResult::Ok,
              "issue detached temporary and target");
        Receiver destination(service, {generation, target, 0});
        Initialize(destination);
        {
            std::unique_ptr<CAnimBlendSequence>               sequence = std::make_unique<Receiver>(service, Token{generation, source, 0});
            MtaGame::IfpStorage::SequenceOwner<ResourceOwner> detached(owner, std::move(sequence), true);
            Initialize(*detached.Get());
            detached.MarkInitialized();
            MtaGame::IfpStorage::AttachFrames(owner, *detached.Get(), 2, false, true, reinterpret_cast<BYTE*>(frames));
            const auto hash = detached.Get()->GetHash();
            detached.TransferTo(destination);
            Verify(owner.interfaceFrees == 1 && !owner.frameFrees && heap.Contains(frames) && destination.GetHash() == hash &&
                       reinterpret_cast<std::uintptr_t>(destination.GetKeyFrames()) == frames,
                   "actual SequenceOwner transfer freed frames or lost copied source properties");
        }
        Controller::AcceptedFrameFormat format;
        Verify(controller.ReadSequenceFrameFormat(image, {generation, target, 0}, &Host::Owner, format) == ClockResult::Ok &&
                   format.format == Controller::FrameFormat::Compressed && format.frameBase == frames && owner.interfaceFrees == 1 && !owner.frameFrees,
               "source facade destruction invalidated copied frame affinity or doubled metadata disposal");
        owner.FreeKeyFramesMemory(reinterpret_cast<void*>(frames));
        owner.DeleteCustomAnimSequenceInterface(reinterpret_cast<CAnimBlendSequenceSAInterface*>(target));
        Verify(owner.frameFrees == 1 && owner.interfaceFrees == 2 && !controller.IsRetained(), "transferred originals did not retire exactly once");
    }
    reset();
    {
        Controller     controller;
        Service        service(controller, image);
        ResourceOwner  owner{controller, image, generation};
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Host::Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 40, &Host::Owner, frames) == ClockResult::Ok,
              "issue untransferred detached guard inputs");
        {
            std::unique_ptr<CAnimBlendSequence>               sequence = std::make_unique<Receiver>(service, Token{generation, tag, 0});
            MtaGame::IfpStorage::SequenceOwner<ResourceOwner> detached(owner, std::move(sequence), true);
            Initialize(*detached.Get());
            detached.MarkInitialized();
            MtaGame::IfpStorage::AttachFrames(owner, *detached.Get(), 2, false, false, reinterpret_cast<BYTE*>(frames));
        }
        Verify(owner.frameFrees == 1 && owner.interfaceFrees == 1 && heap.freeCalls == 2 && !controller.IsRetained(),
               "actual detached guard failed frame-before-metadata cleanup on successful unpublished construction");
    }
    reset();
    {
        Controller     controller;
        Service        service(controller, image);
        ResourceOwner  owner{controller, image, generation};
        std::uintptr_t tag = 0, frames = 0;
        Check(controller.Allocate(image, generation, 1, &Host::Owner, tag) == ClockResult::Ok &&
                  controller.AllocateFrames(image, generation, 60, &Host::Owner, frames) == ClockResult::Ok,
              "issue accepted-loss common helper inputs");
        Receiver receiver(service, {generation, tag, 0});
        Initialize(receiver);
        Host::failureMode = 2;
        bool refused = false;
        try
        {
            MtaGame::IfpStorage::AttachFrames(owner, receiver, 3, false, false, reinterpret_cast<BYTE*>(frames));
        }
        catch (const std::exception&)
        {
            refused = true;
        }
        Verify(refused && service.receipt.entered && service.receipt.accepted && Host::writes == 1 && !owner.frameFrees && !heap.freeCalls &&
                   controller.IsRetained() && controller.RecoveryPending(),
               "lost acknowledgement freed an accepted buffer or replayed construction");
        Host::failureMode = 0;
        PoolTestHost::ownerResult = ClockResult::Ok;
        Controller::RetireProgress progress;
        Verify(controller.Retire(image, generation, 4, &Host::Owner, progress) == ClockResult::Ok && !progress.retained && heap.freeCalls == 2,
               "owner debt did not recover original storage once after conservative common-helper refusal");
    }
    Heap::active = nullptr;
    std::puts("Actual common IFP fresh SetKeyFrames bridge integration completed");
    std::puts("Actual common SequenceOwner transfer fixture completed");
    std::printf("Passed %u actual common fresh SetFrames SDK integration checks (64-bit; no native engine code executed)\n", bridgeChecks);
    return 0;
}
