/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute real common IFP parsing, transactions and resource cleanup
 ****************************************************************************/
#include "IFPParserBackendFixture.h"
#include <initializer_list>
namespace
{
    using namespace ifp_parser_tests;
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Actual IFP runtime check %u failed\n", checks);
            std::abort();
        }
    }
    void Phase(const char* name)
    {
        std::printf("IFP phase: %s\n", name);
        std::fflush(stdout);
    }
    struct InputSequence
    {
        std::int32_t bone;
        const char*  name;
        std::int16_t x = 1024;
    };
    struct File
    {
        std::vector<unsigned char> bytes;
        std::size_t                animationCount = 0, sequenceCount = 0, firstFrameCount = 0, firstFrame = 0, nameSize = 0;
        template <class T>
        void Add(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            const auto* begin = reinterpret_cast<const unsigned char*>(&value);
            bytes.insert(bytes.end(), begin, begin + sizeof(value));
        }
        void Text(const char* value, std::size_t count)
        {
            const auto start = bytes.size();
            bytes.resize(start + count, 0);
            std::memcpy(bytes.data() + start, value, (std::min)(std::strlen(value), count));
        }
        template <class T>
        void Patch(std::size_t at, const T& value)
        {
            Check(at <= bytes.size() && sizeof(value) <= bytes.size() - at);
            std::memcpy(bytes.data() + at, &value, sizeof(value));
        }
        SString Buffer() const { return bytes.empty() ? SString() : SString(reinterpret_cast<const char*>(bytes.data()), bytes.size()); }
    };
    File MakeFile(const char* version, std::initializer_list<InputSequence> sequences, const char* animation = "walk")
    {
        File file;
        file.Text(version, 4);
        const bool anpk = std::strcmp(version, "ANPK") == 0;
        if (!anpk)
        {
            CClientIFP::SIFPHeaderV2 header{};
            header.TotalAnimations = 1;
            std::memcpy(header.InternalFileName, "fixture", 7);
            file.animationCount = file.bytes.size() + offsetof(CClientIFP::SIFPHeaderV2, TotalAnimations);
            file.Add(header);
            file.Text(animation, 24);
            file.sequenceCount = file.bytes.size();
            file.Add(static_cast<std::int32_t>(sequences.size()));
            if (std::strcmp(version, "ANP3") == 0)
            {
                file.Add(std::int32_t{16});
                file.Add(std::int32_t{1});
            }
            for (const auto& input : sequences)
            {
                CClientIFP::SSequenceHeaderV2 sequence{};
                std::strncpy(sequence.Name, input.name, sizeof(sequence.Name) - 1);
                sequence.FrameType = CClientIFP::KRT0_COMPRESSED;
                sequence.TotalFrames = 1;
                sequence.BoneID = input.bone;
                if (!file.firstFrameCount)
                    file.firstFrameCount = file.bytes.size() + offsetof(CClientIFP::SSequenceHeaderV2, TotalFrames);
                file.Add(sequence);
                CClientIFP::SCompressed_KRT0 frame{};
                frame.Rotation.W = 4096;
                frame.Time = 60;
                frame.Translation.X = input.x;
                if (!file.firstFrame)
                    file.firstFrame = file.bytes.size();
                file.Add(frame);
            }
            file.Patch(std::size_t{4}, static_cast<std::uint32_t>(file.bytes.size() - 8));
            return file;
        }
        file.Add(std::uint32_t{0});
        CClientIFP::SBase info{};
        std::memcpy(info.FourCC, "INFO", 4);
        info.Size = 28;
        file.Add(info);
        file.animationCount = file.bytes.size();
        file.Add(std::int32_t{1});
        file.Text("fixture", 24);
        CClientIFP::SBase name{};
        std::memcpy(name.FourCC, "NAME", 4);
        name.Size = 8;
        file.nameSize = file.bytes.size() + offsetof(CClientIFP::SBase, Size);
        file.Add(name);
        file.Text(animation, 8);
        const auto        dganAt = file.bytes.size();
        CClientIFP::SBase dgan{};
        std::memcpy(dgan.FourCC, "DGAN", 4);
        file.Add(dgan);
        info.Size = 4;
        file.Add(info);
        file.sequenceCount = file.bytes.size();
        file.Add(static_cast<std::int32_t>(sequences.size()));
        for (const auto& input : sequences)
        {
            CClientIFP::SBase cpan{};
            std::memcpy(cpan.FourCC, "CPAN", 4);
            cpan.Size = sizeof(CClientIFP::SAnim) + sizeof(CClientIFP::SBase) + sizeof(CClientIFP::SKrt0);
            file.Add(cpan);
            CClientIFP::SAnim sequence{};
            std::memcpy(sequence.Base.FourCC, "ANIM", 4);
            sequence.Base.Size = sizeof(sequence) - sizeof(sequence.Base);
            sequence.Frames = 1;
            std::strncpy(sequence.Name, input.name, sizeof(sequence.Name) - 1);
            if (!file.firstFrameCount)
                file.firstFrameCount = file.bytes.size() + offsetof(CClientIFP::SAnim, Frames);
            file.Add(sequence);
            CClientIFP::SBase frameType{};
            std::memcpy(frameType.FourCC, "KRT0", 4);
            frameType.Size = sizeof(CClientIFP::SKrt0);
            file.Add(frameType);
            CClientIFP::SKrt0 frame{};
            frame.Rotation.W = 1;
            frame.Time = 1;
            frame.Translation.X = static_cast<float>(input.x) / 1024;
            if (!file.firstFrame)
                file.firstFrame = file.bytes.size();
            file.Add(frame);
        }
        file.Patch(dganAt + offsetof(CClientIFP::SBase, Size), static_cast<std::uint32_t>(file.bytes.size() - dganAt - sizeof(CClientIFP::SBase)));
        file.Patch(std::size_t{4}, static_cast<std::uint32_t>(file.bytes.size() - 8));
        return file;
    }
    void Balanced(const Manager& manager)
    {
        Check(manager.Empty());
        Check(manager.frameAllocations == manager.frameFrees);
        Check(manager.hierarchyAllocations == manager.hierarchyFrees);
        Check(manager.detachedAllocations == manager.detachedFrees);
        Check(manager.arrayAllocations == manager.arrayFrees);
    }
    void NormalFormats()
    {
        Phase("actual ANPK/ANP2/ANP3 and duplicate/unknown transfers");
        for (const bool alternate : {false, true})
            for (const auto* version : {"ANPK", "ANP2", "ANP3"})
            {
                Manager manager(alternate);
                {
                    CClientIFP ifp(nullptr, 1, manager);
                    const auto input = MakeFile(version, {{0, "Normal", 1024}});
                    Check(ifp.Load("original", true, input.Buffer()));
                    auto* hierarchy = ifp.GetAnimationHierarchy("walk");
                    Check(hierarchy != nullptr);
                    Check(hierarchy->usNumSequences == 64 && manager.arrays.at(hierarchy->pSequences) == 65);
                    Check(ifp.GetAnimatedBonesMask("walk").test(0));
                    CClientIFP::SCompressed_KRT0 root{};
                    std::memcpy(&root, manager.Row(hierarchy->pSequences, 0)->pKeyFrames, sizeof(root));
                    Check(root.Translation.X == 1024 && root.Time == 60);
                }
                Balanced(manager);
            }
        Manager manager(true);
        {
            CClientIFP ifp(nullptr, 2, manager);
            const auto input = MakeFile("ANP3", {{0, "Normal", 1024}, {0, "Normal", 2048}, {-1, "unmapped", 3072}});
            Check(ifp.Load("duplicates", true, input.Buffer()));
            auto* hierarchy = ifp.GetAnimationHierarchy("walk");
            Check(hierarchy != nullptr && hierarchy->usNumSequences == 65);
            Check(manager.arrays.at(hierarchy->pSequences) == 67 && ifp.GetAnimatedBonesMask("walk").count() == 1);
            CClientIFP::SCompressed_KRT0 root{};
            std::memcpy(&root, manager.Row(hierarchy->pSequences, 0)->pKeyFrames, sizeof(root));
            Check(root.Translation.X == 2048);
            Check(manager.Row(hierarchy->pSequences, 64)->pKeyFrames != nullptr);
            Check(manager.detachedAllocations == 2 && manager.detachedFrees == 2 && manager.frameFrees == 1);
        }
        Balanced(manager);
        std::puts("Actual common IFP opaque layout variants fixture completed");
    }
    void TruncatedAndMalformed()
    {
        Phase("recognized truncation and malformed declared counts/chunks");
        for (const auto* version : {"ANPK", "ANP2", "ANP3"})
        {
            const auto input = MakeFile(version, {{0, "Normal", 1024}});
            for (const auto size : {std::size_t{0}, std::size_t{3}, std::size_t{7}, input.firstFrame - 1, input.bytes.size() - 1})
            {
                Manager manager(true);
                {
                    CClientIFP ifp(nullptr, 3, manager);
                    auto       cut = input;
                    cut.bytes.resize(size);
                    Check(!ifp.Load("cut", true, cut.Buffer()));
                }
                Balanced(manager);
            }
            for (unsigned field = 0; field != 3; ++field)
            {
                Manager manager(true);
                {
                    CClientIFP ifp(nullptr, 4, manager);
                    auto       bad = input;
                    const auto at = field == 0 ? bad.animationCount : (field == 1 ? bad.sequenceCount : bad.firstFrameCount);
                    bad.Patch(at, field == 2 ? std::int32_t{-1} : (std::numeric_limits<std::int32_t>::max)());
                    Check(!ifp.Load("bad", true, bad.Buffer()));
                }
                Balanced(manager);
            }
        }
        for (const auto size : {std::uint32_t{0}, (std::numeric_limits<std::uint32_t>::max)()})
        {
            Manager manager(true);
            {
                CClientIFP ifp(nullptr, 5, manager);
                auto       bad = MakeFile("ANPK", {{0, "Normal", 1024}});
                bad.Patch(bad.nameSize, size);
                Check(!ifp.Load("bad-name", true, bad.Buffer()));
            }
            Balanced(manager);
        }
        Manager partial(true);
        {
            CClientIFP ifp(nullptr, 5, partial);
            auto       cut = MakeFile("ANP3", {{0, "Normal", 1024}, {-1, "unmapped", 2048}});
            cut.bytes.resize(cut.bytes.size() - 1);
            Check(!ifp.Load("partial-unknown", true, cut.Buffer()));
        }
        Balanced(partial);
    }
    void ResourceFailures()
    {
        Phase("allocation and before/after publication failures");
        const auto input = MakeFile("ANP3", {{0, "Normal", 1024}});
        for (const bool alternate : {false, true})
        {
            for (const auto failure : {std::size_t{1}, std::size_t{2}})
            {
                Manager manager(alternate);
                manager.failFrameAt = failure;
                {
                    CClientIFP ifp(nullptr, 6, manager);
                    Check(ifp.Load("frame-oom", true, input.Buffer()));
                    auto* hierarchy = ifp.GetAnimationHierarchy("walk");
                    Check(hierarchy != nullptr);
                    Check(ifp.GetAnimatedBonesMask("walk").test(0) == (failure != 1));
                    if (failure == 2)
                        Check(manager.Row(hierarchy->pSequences, 1)->pKeyFrames == nullptr);
                }
                Balanced(manager);
            }
            Manager unknown(alternate);
            unknown.failFrameAt = 1;
            {
                CClientIFP ifp(nullptr, 6, unknown);
                Check(ifp.Load("unknown-frame-oom", true, MakeFile("ANP3", {{-1, "unmapped", 1024}}).Buffer()));
                auto* hierarchy = ifp.GetAnimationHierarchy("walk");
                Check(hierarchy != nullptr && hierarchy->usNumSequences == 65);
                Check(unknown.Row(hierarchy->pSequences, 64)->pKeyFrames == nullptr);
            }
            Balanced(unknown);
            Manager allocation(alternate);
            allocation.failArrayAt = 1;
            {
                CClientIFP ifp(nullptr, 7, allocation);
                Check(!ifp.Load("array-oom", true, input.Buffer()));
            }
            Balanced(allocation);
            for (const bool after : {false, true})
            {
                Manager transfer(alternate);
                transfer.failCopyAt = 1;
                transfer.copyFailureAfterWrite = after;
                {
                    CClientIFP ifp(nullptr, 8, transfer);
                    Check(!ifp.Load("copy-fail", true, input.Buffer()));
                }
                Balanced(transfer);
                Manager attachment(alternate);
                attachment.failAttachAt = 1;
                attachment.attachFailureAfterWrite = after;
                {
                    CClientIFP ifp(nullptr, 9, attachment);
                    Check(!ifp.Load("attach-fail", true, input.Buffer()));
                }
                Balanced(attachment);
            }
        }
    }
    void DetachedConstruction()
    {
        Phase("owned-array rows finish before hierarchy attachment");
        for (const bool alternate : {false, true})
            for (const auto* version : {"ANPK", "ANP2", "ANP3"})
            {
                Manager manager(alternate);
                manager.requireConstructedBeforeAttach = true;
                {
                    CClientIFP ifp(nullptr, 20, manager);
                    Check(ifp.Load("detached", true, MakeFile(version, {{0, "Normal", 1024}}).Buffer()));
                    auto* hierarchy = ifp.GetAnimationHierarchy("walk");
                    Check(hierarchy != nullptr);
                    Check(manager.allCapacityInitialized && manager.hierarchyAttachAttempts == 1 && manager.hierarchyAttachAccepted == 1);
                    Check(manager.atBeforeAttachment >= 64 && !manager.hierarchyRowRequests && !manager.populationsAfterAttachment);
                    Check(alternate ? manager.opaqueAtCalls > 0 : manager.classicAtCalls > 0);
                    std::size_t attachedAt = manager.events.size();
                    for (std::size_t index = 0; index < manager.events.size(); ++index)
                        if (manager.events[index].kind == Manager::EventKind::Attach && manager.events[index].array == hierarchy->pSequences)
                        {
                            attachedAt = index;
                            break;
                        }
                    Check(attachedAt < manager.events.size());
                    for (std::size_t index = 0; index < manager.events.size(); ++index)
                        if (manager.events[index].kind == Manager::EventKind::Populate && manager.events[index].array == hierarchy->pSequences)
                            Check(index < attachedAt);
                    const auto capacity = manager.arrays.at(hierarchy->pSequences);
                    Check(capacity == 65 && hierarchy->usNumSequences == 64);
                    const auto* unused = manager.Row(hierarchy->pSequences, capacity - 1);
                    Check(!unused->pKeyFrames && !unused->sFlags && !unused->sNumKeyFrames && unused->m_boneId == 0xffff);
                }
                Balanced(manager);
                Check(!manager.hierarchyRowRequests && !manager.populationsAfterAttachment);
            }
        std::puts("Actual common IFP detached construction order fixture completed");
    }
    void DetachedFailureCleanup()
    {
        Phase("unattached partial construction owns frames through full capacity");
        const auto input = MakeFile("ANP3", {{0, "Normal", 1024}});
        for (const bool alternate : {false, true})
            for (unsigned failure = 0; failure != 3; ++failure)
            {
                Manager manager(alternate);
                manager.requireConstructedBeforeAttach = true;
                if (!failure)
                    manager.failArrayRowAt = 3;
                else
                {
                    manager.failHierarchyAttachAt = 1;
                    manager.hierarchyAttachFailureAfterWrite = failure == 2;
                }
                {
                    CClientIFP ifp(nullptr, 21, manager);
                    Check(!ifp.Load("unattached-failure", true, input.Buffer()));
                    Check(manager.frameAllocations >= 2 && manager.frameAllocations == manager.frameFrees);
                    Check(manager.arrayAllocations == 1 && manager.arrayFrees == 1 && manager.hierarchyAllocations == 1 && manager.hierarchyFrees == 1);
                    Check(manager.allCapacityInitialized && !manager.cacheRemovals && !manager.hierarchyRowRequests);
                    if (!failure)
                        Check(!manager.hierarchyAttachAttempts && manager.atBeforeAttachment >= 65);
                    else
                        Check(manager.hierarchyAttachAttempts == 1 && manager.hierarchyAttachAccepted == (failure == 2 ? 1u : 0u));
                }
                Balanced(manager);
            }
        for (const bool alternate : {false, true})
        {
            Manager manager(alternate);
            manager.requireConstructedBeforeAttach = true;
            {
                CClientIFP ifp(nullptr, 22, manager);
                Check(ifp.Load("old", true, input.Buffer()));
                auto       retained = ifp.GetIFPAnimationsPointer();
                auto*      old = ifp.GetAnimationHierarchy("walk");
                const auto oldFrames = manager.frames.size();
                const auto allocations = manager.frameAllocations;
                const auto frees = manager.frameFrees;
                manager.failArrayRowAt = manager.arrayAtAttempts + 3;
                Check(!ifp.Load("candidate", true, input.Buffer()));
                Check(ifp.GetIFPAnimationsPointer() == retained && ifp.GetAnimationHierarchy("walk") == old && ifp.GetBlockName() == "old");
                Check(manager.frames.size() == oldFrames && manager.frameAllocations - allocations == manager.frameFrees - frees &&
                      manager.hierarchies.count(old) == 1);
                manager.failArrayRowAt = 0;
                Check(ifp.Load("replacement", true, MakeFile("ANP2", {{0, "Normal", 2048}}, "next").Buffer()));
                Check(ifp.GetIFPAnimationsPointer() != retained && ifp.GetAnimationHierarchy("next") && manager.hierarchies.count(old) == 1);
                retained.reset();
                Check(!manager.hierarchies.count(old));
            }
            Balanced(manager);
        }
        std::puts("Actual common IFP unattached cleanup and retained set fixture completed");
    }
    void ReloadAndSharedCleanup()
    {
        Phase("transactional reload, retained old set and shared chunk cleanup");
        Manager manager(true);
        {
            CClientIFP ifp(nullptr, 10, manager);
            const auto input = MakeFile("ANPK", {{0, "Normal", 1024}});
            Check(ifp.Load("old", true, input.Buffer()));
            auto  retained = ifp.GetIFPAnimationsPointer();
            auto* old = ifp.GetAnimationHierarchy("walk");
            auto  invalid = MakeFile("ANP3", {{0, "Normal", 2048}});
            invalid.bytes.resize(invalid.bytes.size() - 1);
            Check(!ifp.Load("failed", true, invalid.Buffer()));
            Check(ifp.GetBlockName() == "old");
            Check(ifp.GetIFPAnimationsPointer() == retained && ifp.GetAnimationHierarchy("walk") == old);
            Check(ifp.Load("new", true, MakeFile("ANP2", {{0, "Normal", 3072}}, "replacement").Buffer()));
            Check(ifp.GetIFPAnimationsPointer() != retained && ifp.GetAnimationHierarchy("replacement") != nullptr);
            Check(ifp.GetAnimationHierarchy("walk") == nullptr && manager.hierarchies.count(old) == 1);
            retained.reset();
            Check(manager.hierarchies.count(old) == 0);
        }
        Balanced(manager);
        Manager shared(true);
        {
            CIFPAnimations animations(shared);
            animations.vecAnimations.emplace_back();
            auto& animation = animations.vecAnimations.back();
            animation.pHierarchy = shared.GetCustomAnimBlendHierarchy();
            animation.pHierarchy->Initialize();
            animation.pSequences = shared.AllocateCustomAnimSequences(3);
            animation.sequenceCapacity = 3;
            animation.pHierarchy->SetSequences(animation.pSequences);
            animation.sequencesAttached = true;
            animation.pHierarchy->SetNumSequences(1);
            auto* frames = shared.AllocateKeyFramesMemory(48);
            shared.Row(animation.pSequences, 0)->pKeyFrames = frames;
            shared.Row(animation.pSequences, 0)->sFlags = 8;
            shared.Row(animation.pSequences, 1)->pKeyFrames = frames + 16;
            shared.Row(animation.pSequences, 1)->sFlags = 8;
            animations.DeleteAnimations();
            Balanced(shared);
            Check(shared.frameFrees == 1 && shared.arrayFrees == 1 && shared.hierarchyFrees == 1);
            animations.DeleteAnimations();
            Check(shared.frameFrees == 1 && shared.arrayFrees == 1);
        }
        Balanced(shared);
    }
}
int main()
{
    NormalFormats();
    TruncatedAndMalformed();
    ResourceFailures();
    ReloadAndSharedCleanup();
    DetachedConstruction();
    DetachedFailureCleanup();
    std::puts("Actual common IFP parser, transaction and cleanup fixture completed");
    std::printf("Passed %u actual common IFP runtime checks (%zu-bit; test animation environment, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
