/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the original compressed timestamp conversion in place
 ****************************************************************************/
#include "NativeAnimationHierarchyUncompress.h"
#include "NativeAnimationHierarchyOrigin.h"
#include "NativeAnimationHierarchyReadProfile.h"
#include "NativeAnimationSequenceFrameBindProfile.h"
#include "NativeAnimationAssociationProgressProfile.h"
#include "NativeModelReferenceHeap.h"
#include "NativeModelReferenceMap.h"
#include "NativeStreamingCompletionContext.h"
#include <array>
#include <cstring>
#include <limits>
#include <vector>
#include <initializer_list>
#include <xmmintrin.h>

namespace mta::game_de
{
    namespace AnimationHierarchyUncompressDetail
    {
        namespace Origin = NativeAnimationHierarchyOrigin;
        namespace Image = NativeImageAccess;
        namespace Map = NativeModelReferenceMap;
        namespace Context = NativeStreamingCompletionContext;
        namespace ReadProfile = NativeAnimationHierarchyReadProfile;
        namespace FrameProfile = NativeAnimationSequenceFrameBindProfile;
        namespace TimeProfile = NativeAnimationAssociationProgressProfile;
        using Calls = NativeAnimationHierarchyUncompressCalls::Receipt;
        using CheckOwner = NativeAnimationHierarchyUncompressCalls::CheckOwner;
        using Hierarchy = std::array<unsigned char, 40>;
        using Row = std::array<unsigned char, 80>;
        using Binding = NativeAnimationBlockMemory::Binding;
        using Observation = Origin::Observation<Hierarchy>;
        constexpr std::size_t MaximumPreparedBytes = 4 * 1024 * 1024;
        template <class T, class Bytes>
        T At(const Bytes& bytes, std::size_t offset) noexcept
        {
            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        template <class T, class Bytes>
        void Put(Bytes& bytes, std::size_t offset, T value) noexcept
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }
        float Float(std::uint32_t bits) noexcept
        {
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        std::uint32_t Bits(float value) noexcept
        {
            std::uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
        bool Finite(std::uint32_t bits) noexcept
        {
            return (bits & 0x7f800000u) != 0x7f800000u;
        }
        bool Profile(std::uintptr_t image, std::uintptr_t hierarchyRva) noexcept
        {
            if (!Origin::Sections({image}, hierarchyRva, 0))
                return false;
            std::array<unsigned char, TimeProfile::HierarchyTimeBytes.size()> body{};
            if (!Image::ReadImage(image, image + TimeProfile::HierarchyTimeRva, body.data(), body.size(), false, true) ||
                body != TimeProfile::HierarchyTimeBytes)
                return false;
            // This branch consumes only the saved time scale. It does not
            // import progress's trigonometry, decompressor or cache helpers.
            std::array<unsigned char, 4> scale{};
            if (!NativeModelReferenceHeap::ReadOnlyImage(image, image + 0x4bc80ac, scale.size()) ||
                !Image::ReadImage(image, image + 0x4bc80ac, scale.data(), scale.size()) || scale != TimeProfile::TimeScaleBytes)
                return false;
            for (const auto& pin : ReadProfile::Code)
            {
                std::array<unsigned char, ReadProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool FrameProducerProfile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : FrameProfile::Code)
            {
                std::array<unsigned char, FrameProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        ClockResult Observe(Binding binding, std::uintptr_t row, Hierarchy& output) noexcept
        {
            return Image::ReadImage(binding.image, row, output.data(), output.size(), true) ? ClockResult::Ok : ClockResult::ReadFailure;
        }
        ClockResult Capture(std::uintptr_t image, CheckOwner owner, const Origin::Location& location, Observation& output) noexcept
        {
            if (!Profile(image, location.rva))
                return ClockResult::BindingChanged;
            auto result = Origin::Capture({image}, owner, location.ordinal, location.rva, &Observe, output);
            if (result != ClockResult::Ok)
                return result;
            return At<std::uint32_t>(output.payload, 0x14) == output.origin.block ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult InvokeOriginal(NativeAnimationHierarchyUncompressCalls::Function function, std::uintptr_t hierarchy, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(hierarchy);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        struct CacheNode
        {
            std::uintptr_t                address = 0;
            std::array<unsigned char, 24> before{}, expected{}, observed{};
        };
        bool ReadCacheNode(std::uintptr_t image, std::uintptr_t address, std::array<unsigned char, 24>& bytes) noexcept
        {
            // Native list sentinels live in writable image data. Ordinary
            // nodes may use the engine's private allocation; neither route
            // grants this command authority to release or adopt their storage.
            return Map::PrivateWritable(address, bytes.size()) ? Map::Copy(address, bytes.data(), bytes.size())
                                                               : Image::ReadImage(image, address, bytes.data(), bytes.size(), true);
        }
        struct Frame
        {
            std::uintptr_t             address = 0;
            std::size_t                stride = 0;
            std::vector<unsigned char> before, expected, observed;
        };
        struct Sequence
        {
            std::uintptr_t address = 0;
            Row            before{}, observed{};
        };
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false, std::memory_order_release); }
        };
    }
    struct NativeAnimationHierarchyUncompress::Data
    {
        using Detail = AnimationHierarchyUncompressDetail::Observation;
        NativeAnimationHierarchyReferenceToken                     token{};
        NativeAnimationHierarchyOrigin::Location                   location{};
        Detail                                                     before{}, observed{};
        AnimationHierarchyUncompressDetail::Hierarchy              expected{};
        std::uintptr_t                                             sequenceFirst = 0, cookieAddress = 0, blockAddress = 0;
        std::uint64_t                                              capacity = 0;
        std::size_t                                                sequenceBytes = 0, preparedBytes = 0;
        unsigned char                                              loaded = 0;
        bool                                                       payload = false, cache = false;
        std::vector<AnimationHierarchyUncompressDetail::CacheNode> cacheNodes;
        std::vector<AnimationHierarchyUncompressDetail::Sequence>  sequences;
        std::vector<AnimationHierarchyUncompressDetail::Frame>     frames;
    };
    namespace AnimationHierarchyUncompressDetail
    {
        template <class Plan>
        ClockResult Verify(std::uintptr_t image, CheckOwner owner, Plan& plan, bool after) noexcept
        {
            auto result = Capture(image, owner, plan.location, plan.observed);
            if (result != ClockResult::Ok)
                return result;
            if (!Origin::Same(plan.before.origin, plan.observed.origin) || plan.observed.payload != (after ? plan.expected : plan.before.payload))
                return ClockResult::BindingChanged;
            if (plan.payload)
            {
                if (!FrameProducerProfile(image))
                    return ClockResult::BindingChanged;
                std::uint64_t capacity = 0;
                unsigned char loaded = 0;
                if (!Map::PrivateWritable(plan.cookieAddress, plan.sequenceBytes) || !Map::Copy(plan.cookieAddress, &capacity, sizeof(capacity)) ||
                    !Image::ReadImage(image, plan.blockAddress + 0x10, &loaded, sizeof(loaded), true))
                    return ClockResult::ReadFailure;
                if (capacity != plan.capacity || loaded != plan.loaded)
                    return ClockResult::BindingChanged;
                for (auto& sequence : plan.sequences)
                    if (!Map::Copy(sequence.address, sequence.observed.data(), sequence.observed.size()))
                        return ClockResult::ReadFailure;
                    else if (sequence.observed != sequence.before)
                        return ClockResult::BindingChanged;
                for (auto& frame : plan.frames)
                    if (!Map::PrivateWritable(frame.address, frame.observed.size()) || !Map::Copy(frame.address, frame.observed.data(), frame.observed.size()))
                        return ClockResult::ReadFailure;
                    else if (frame.observed != (after ? frame.expected : frame.before))
                        return ClockResult::BindingChanged;
            }
            if (plan.cache)
                for (auto& node : plan.cacheNodes)
                    if (!ReadCacheNode(image, node.address, node.observed))
                        return ClockResult::ReadFailure;
                    else if (node.observed != (after ? node.expected : node.before))
                        return ClockResult::BindingChanged;
            Observation final;
            if ((result = Capture(image, owner, plan.location, final)) != ClockResult::Ok)
                return result;
            if (!Origin::Same(plan.before.origin, final.origin) || final.payload != plan.observed.payload)
                return ClockResult::BindingChanged;
            return Origin::Owner({image}, owner);
        }
        template <class Plan>
        ClockResult PrepareCache(std::uintptr_t image, Plan& plan, Calls& receipt)
        {
            constexpr std::size_t MaximumCacheNodes = 256;
            const auto            head = image + 0x53c08a0;
            // FF0F11 reads the last-node slot at53C08C0. The same native
            // node's prev field is+8, hence its terminal anchor is53C08B8.
            // Membership below proves that relationship in the current list;
            // it supplies no allocation, free or lasting cache-node authority.
            const auto tail = image + 0x53c08b8;
            const auto selected = At<std::uintptr_t>(plan.before.payload, 0x20);
            const auto append = [&](std::uintptr_t address) -> ClockResult
            {
                if (!address || address % alignof(std::uintptr_t) || address > (std::numeric_limits<std::uintptr_t>::max)() - 24 ||
                    !Map::Disjoint(address, 24, plan.token.locator, plan.expected.size()))
                    return ClockResult::BindingChanged;
                for (const auto& node : plan.cacheNodes)
                    if (!Map::Disjoint(address, 24, node.address, node.before.size()))
                        return ClockResult::OwnerConflict;
                CacheNode node;
                node.address = address;
                if (!ReadCacheNode(image, address, node.before))
                    return ClockResult::ReadFailure;
                node.expected = node.before;
                plan.cacheNodes.push_back(node);
                return ClockResult::Ok;
            };
            auto result = append(head);
            if (result != ClockResult::Ok || (result = append(tail)) != ClockResult::Ok)
                return result;
            auto previous = head;
            auto current = At<std::uintptr_t>(plan.cacheNodes[0].before, 0x10);
            bool found = false;
            for (std::size_t count = 0; current != tail; ++count)
            {
                if (count == MaximumCacheNodes)
                    return ClockResult::Busy;
                if ((result = append(current)) != ClockResult::Ok)
                    return result;
                const auto& node = plan.cacheNodes.back();
                if (At<std::uintptr_t>(node.before, 8) != previous)
                    return ClockResult::BindingChanged;
                if (current == selected)
                {
                    if (At<std::uintptr_t>(node.before, 0) != plan.token.locator)
                        return ClockResult::BindingChanged;
                    found = true;
                }
                previous = current;
                current = At<std::uintptr_t>(node.before, 0x10);
            }
            if (!found || At<std::uintptr_t>(plan.cacheNodes[1].before, 8) != previous)
                return ClockResult::BindingChanged;
            bool       modelValid = true;
            const auto word = [&](std::uintptr_t address, std::uintptr_t* written) -> std::uintptr_t
            {
                for (auto& node : plan.cacheNodes)
                    if (address >= node.address && address - node.address <= 16 && (address - node.address) % 8 == 0)
                    {
                        const auto offset = static_cast<std::size_t>(address - node.address);
                        if (written)
                            Put(node.expected, offset, *written);
                        return At<std::uintptr_t>(node.expected, offset);
                    }
                modelValid = false;
                return 0;
            };
            const auto load = [&](std::uintptr_t address) { return word(address, nullptr); };
            const auto store = [&](std::uintptr_t address, std::uintptr_t value) { (void)word(address, &value); };
            // Model the original six stores in order, including aliases when
            // the selected entry is already first or is the only live entry.
            // Predicted snapshots are CPU evidence, never replacement writes.
            store(load(selected + 0x10) + 8, load(selected + 8));
            store(load(selected + 8) + 0x10, load(selected + 0x10));
            store(selected + 0x10, load(head + 0x10));
            store(load(head + 0x10) + 8, selected);
            store(selected + 8, head);
            store(head + 0x10, selected);
            if (!modelValid)
                return ClockResult::BindingChanged;
            receipt.cachePath = true;
            receipt.cacheNodes = static_cast<std::uint32_t>(plan.cacheNodes.size() - 2);
            for (const auto& node : plan.cacheNodes)
                for (std::size_t offset : {std::size_t{8}, std::size_t{16}})
                    if (At<std::uintptr_t>(node.before, offset) != At<std::uintptr_t>(node.expected, offset))
                        ++receipt.modifiedCacheWords;
            plan.cache = true;
            return ClockResult::Ok;
        }
        template <class Plan>
        ClockResult Prepare(std::uintptr_t image, CheckOwner owner, Plan& plan, Calls& receipt)
        {
            auto result = Origin::Locate({image}, plan.token.locator, plan.location);
            if (result != ClockResult::Ok || (result = Capture(image, owner, plan.location, plan.before)) != ClockResult::Ok)
                return result;
            plan.expected = plan.before.payload;
            const auto timeBits = At<std::uint32_t>(plan.before.payload, 0x18);
            receipt.beforeTimeBits = timeBits;
            if (!Finite(timeBits))
                return ClockResult::InvalidArgument;
            if (plan.before.payload[0x13] == 0)
            {
                // An existing linked entry moves through the source LRU
                // branch only. Cache allocation/decompression still needs its
                // own returned-allocation and eviction journal.
                if (plan.before.payload[0x12] != 0)
                    return ClockResult::NotInitialized;
                if (At<std::uintptr_t>(plan.before.payload, 0x20) != 0 && (result = PrepareCache(image, plan, receipt)) != ClockResult::Ok)
                    return result;
            }
            else if (Float(timeBits) == 0.0f)
            {
                Put(plan.expected, 0x18, std::uint32_t{0});
                const auto count = At<std::int16_t>(plan.before.payload, 0x10);
                if (count > 0)
                {
                    if (!FrameProducerProfile(image))
                        return ClockResult::BindingChanged;
                    plan.payload = true;
                    plan.sequenceFirst = At<std::uintptr_t>(plan.before.payload, 8);
                    if (plan.sequenceFirst < 8 || plan.sequenceFirst % alignof(std::uint64_t))
                        return ClockResult::BindingChanged;
                    plan.cookieAddress = plan.sequenceFirst - 8;
                    if (!Map::Copy(plan.cookieAddress, &plan.capacity, sizeof(plan.capacity)))
                        return ClockResult::ReadFailure;
                    if (plan.capacity < static_cast<std::uint16_t>(count))
                        return ClockResult::BindingChanged;
                    if (plan.capacity > (MaximumPreparedBytes - 8) / ReadProfile::SequenceStride)
                        return ClockResult::Busy;
                    plan.sequenceBytes = 8 + static_cast<std::size_t>(plan.capacity) * ReadProfile::SequenceStride;
                    if (plan.sequenceBytes > (std::numeric_limits<std::uintptr_t>::max)() - plan.cookieAddress ||
                        !Map::PrivateWritable(plan.cookieAddress, plan.sequenceBytes))
                        return ClockResult::BindingChanged;
                    plan.blockAddress = image + NativeAnimationBlockProfile::BankRva + plan.before.origin.block * NativeAnimationBlockProfile::RowBytes;
                    if (!Image::ReadImage(image, plan.blockAddress + 0x10, &plan.loaded, sizeof(plan.loaded), true))
                        return ClockResult::ReadFailure;
                    if (!plan.loaded)
                        return ClockResult::NotInitialized;
                    plan.sequences.reserve(static_cast<std::size_t>(count));
                    plan.frames.reserve(static_cast<std::size_t>(count));
                    float duration = 0.0f;
                    for (std::int32_t index = 0; index < count; ++index)
                    {
                        Sequence sequence;
                        sequence.address = plan.sequenceFirst + static_cast<std::size_t>(index) * ReadProfile::SequenceStride;
                        if (!Map::Copy(sequence.address, sequence.before.data(), sequence.before.size()))
                            return ClockResult::ReadFailure;
                        const auto flags = At<std::uint16_t>(sequence.before, 0x42);
                        const auto frames = At<std::int16_t>(sequence.before, 0x44);
                        if (!(flags & 4) || frames <= 0)
                            return ClockResult::OwnerConflict;
                        Frame frame;
                        frame.address = At<std::uintptr_t>(sequence.before, 0x48);
                        frame.stride = flags & 2 ? FrameProfile::RootStride : FrameProfile::ChildStride;
                        const auto bytes = static_cast<std::size_t>(frames) * frame.stride;
                        if (bytes > MaximumPreparedBytes - plan.preparedBytes)
                            return ClockResult::Busy;
                        if (!frame.address || frame.address % 2 || bytes > (std::numeric_limits<std::uintptr_t>::max)() - frame.address ||
                            !Map::PrivateWritable(frame.address, bytes) || !Map::Disjoint(frame.address, bytes, plan.cookieAddress, plan.sequenceBytes) ||
                            !Map::Disjoint(frame.address, bytes, plan.token.locator, plan.expected.size()))
                            return ClockResult::BindingChanged;
                        // The loader admits separate row extents. Reject
                        // overlapping payloads rather than silently predicting
                        // an alias's second conversion from stale input.
                        for (const auto& prior : plan.frames)
                            if (!Map::Disjoint(frame.address, bytes, prior.address, prior.before.size()))
                                return ClockResult::OwnerConflict;
                        frame.before.resize(bytes);
                        frame.observed.resize(bytes);
                        if (!Map::Copy(frame.address, frame.before.data(), bytes))
                            return ClockResult::ReadFailure;
                        frame.expected = frame.before;
                        const auto last = At<std::int16_t>(frame.before, static_cast<std::size_t>(frames - 1) * frame.stride + 8);
                        const auto seconds = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(static_cast<float>(last)), _mm_set_ss(Float(0x3c888889))));
                        if (seconds > duration)
                            duration = seconds;
                        // Native FF0E0F converts backwards and subtracts WORDs
                        // modulo16. Non-time bytes and the first time remain.
                        for (std::int32_t frameIndex = frames - 1; frameIndex >= 1; --frameIndex)
                        {
                            const auto here = static_cast<std::size_t>(frameIndex) * frame.stride + 8;
                            const auto previous = here - frame.stride;
                            const auto difference =
                                static_cast<std::uint16_t>(At<std::uint16_t>(frame.before, here) - At<std::uint16_t>(frame.before, previous));
                            Put(frame.expected, here, difference);
                            if (At<std::uint16_t>(frame.before, here) != difference)
                                ++receipt.modifiedFrames;
                        }
                        receipt.preparedFrames += static_cast<std::uint16_t>(frames);
                        plan.preparedBytes += bytes;
                        plan.sequences.push_back(std::move(sequence));
                        plan.frames.push_back(std::move(frame));
                    }
                    Put(plan.expected, 0x18, Bits(duration));
                    receipt.sequences = static_cast<std::uint32_t>(plan.sequences.size());
                }
            }
            // These are planned target facts until effectsVerified confirms
            // the original returned and every current payload bookend agrees.
            receipt.afterTimeBits = At<std::uint32_t>(plan.expected, 0x18);
            receipt.noOp = plan.expected == plan.before.payload && receipt.modifiedFrames == 0 && receipt.modifiedCacheWords == 0;
            return Verify(image, owner, plan, false);
        }
    }
    NativeAnimationHierarchyUncompress::NativeAnimationHierarchyUncompress() noexcept = default;
    NativeAnimationHierarchyUncompress::~NativeAnimationHierarchyUncompress() = default;
    ClockResult NativeAnimationHierarchyUncompress::Uncompress(std::uintptr_t image, const NativeAnimationHierarchyReferenceToken& requested, CheckOwner owner,
                                                               Receipt* output, Invoke invoke) noexcept
    {
        namespace Detail = AnimationHierarchyUncompressDetail;
        const auto request = requested;
        Receipt    receipt;
        receipt.hierarchy = request.locator;
        receipt.generation = request.ownerGeneration;
        const auto finish = [&](ClockResult result)
        {
            if (receipt.entered && result != ClockResult::Ok)
                debt.store(true, std::memory_order_release);
            receipt.debt = debt.load(std::memory_order_acquire);
            receipt.result = result;
            if (!receipt.debt)
                data.reset();
            if (output)
                *output = receipt;
            return result;
        };
        if (!image || !request.ownerGeneration || !request.locator || !owner)
        {
            receipt.result = ClockResult::InvalidArgument;
            receipt.debt = debt.load();
            if (output)
                *output = receipt;
            return receipt.result;
        }
        bool expected = false;
        if (!busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            receipt.result = ClockResult::Busy;
            receipt.debt = debt.load();
            if (output)
                *output = receipt;
            return receipt.result;
        }
        Detail::BusyScope operation{busy};
        if (debt.load(std::memory_order_acquire))
            return finish(ClockResult::OwnerConflict);
        if (!Detail::Context::TryEnter())
            return finish(ClockResult::Busy);
        Detail::Context::Scope gate;
        auto                   result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        try
        {
            data = std::make_unique<Data>();
            data->token = request;
            result = Detail::Prepare(image, owner, *data, receipt);
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        if (result != ClockResult::Ok)
            return finish(result);
        if ((result = Detail::Verify(image, owner, *data, false)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_ANIMATION_HIERARCHY_UNCOMPRESS_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_ANIMATION_HIERARCHY_UNCOMPRESS_INVOKE;
#else
        const auto entry = invoke ? invoke : &Detail::InvokeOriginal;
#endif
        // All rows, expected bytes and post-call buffers exist before entry.
        // Unknown native payload effects retain this actual plan with no replay.
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeAnimationHierarchyUncompressCalls::Function>(image + receipt.entryRva), request.locator, receipt.exceptionCode);
        receipt.returned = result == ClockResult::Ok;
        if (result != ClockResult::Ok)
            return finish(result);
        if ((result = Detail::Verify(image, owner, *data, true)) != ClockResult::Ok)
            return finish(result);
        if ((result = Detail::Verify(image, owner, *data, true)) != ClockResult::Ok)
            return finish(result);
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
