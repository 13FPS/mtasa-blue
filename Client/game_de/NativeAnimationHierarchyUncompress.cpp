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
#include "NativeAnimationHierarchyDecompressProfile.h"
#include "NativeAnimationHierarchyEvictionProfile.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelReferenceHeap.h"
#include "NativeModelReferenceMap.h"
#include "NativeStreamingCompletionContext.h"
#include <array>
#include <cstring>
#include <limits>
#include <vector>
#include <initializer_list>
#include <xmmintrin.h>
#include <emmintrin.h>

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
        namespace ConversionProfile = NativeAnimationHierarchyDecompressProfile;
        namespace EvictionProfile = NativeAnimationHierarchyEvictionProfile;
        namespace Heap = NativeModelReferenceHeap;
        namespace StorageProfile = NativeCustomAnimSequenceStorageProfile;
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
        bool ConversionSource(std::uintptr_t image, std::uintptr_t hierarchyRva) noexcept
        {
            if (!Origin::Sections({image}, hierarchyRva, 0, ConversionProfile::Code.data(), ConversionProfile::Code.size()))
                return false;
            for (const auto& pin : ConversionProfile::Code)
            {
                std::array<unsigned char, ConversionProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            const auto literal = [&](std::uintptr_t rva, const auto& expected)
            {
                std::array<unsigned char, 4> bytes{};
                return Heap::ReadOnlyImage(image, image + rva, bytes.size()) && Image::ReadImage(image, image + rva, bytes.data(), bytes.size()) &&
                       bytes == expected;
            };
            std::array<unsigned char, 16> sign{};
            return Heap::ReadOnlyImage(image, image + ConversionProfile::SignMask.rva, sign.size()) &&
                   Image::ReadImage(image, image + ConversionProfile::SignMask.rva, sign.data(), sign.size()) &&
                   !std::memcmp(sign.data(), ConversionProfile::SignMask.bytes, sign.size()) &&
                   literal(ConversionProfile::QuaternionScaleRva, ConversionProfile::QuaternionScaleBytes) &&
                   literal(ConversionProfile::PositionScaleRva, ConversionProfile::PositionScaleBytes) &&
                   literal(ConversionProfile::TimeScaleRva, ConversionProfile::TimeScaleBytes);
        }
        bool EvictionSource(std::uintptr_t image, std::uintptr_t hierarchyRva) noexcept
        {
            if (!Origin::Sections({image}, hierarchyRva, 0, EvictionProfile::Code.data(), EvictionProfile::Code.size()))
                return false;
            for (const auto& pin : EvictionProfile::Code)
            {
                std::array<unsigned char, EvictionProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            const auto literal = [&](std::uintptr_t rva, const auto& expected)
            {
                std::array<unsigned char, 4> bytes{};
                return Heap::ReadOnlyImage(image, image + rva, bytes.size()) && Image::ReadImage(image, image + rva, bytes.data(), bytes.size()) &&
                       bytes == expected;
            };
            return literal(EvictionProfile::QuaternionScaleRva, EvictionProfile::QuaternionScaleBytes) &&
                   literal(EvictionProfile::TimeScaleRva, EvictionProfile::TimeScaleBytes) &&
                   literal(EvictionProfile::PositionScaleRva, EvictionProfile::PositionScaleBytes) &&
                   literal(EvictionProfile::HalfRva, EvictionProfile::HalfBytes);
        }
        bool PredictPacked(std::uint32_t bits, std::size_t word, std::uint16_t& output) noexcept
        {
            if (!Finite(bits))
                return false;
            const auto scale = word < 4    ? At<std::uint32_t>(EvictionProfile::QuaternionScaleBytes, 0)
                               : word == 4 ? At<std::uint32_t>(EvictionProfile::TimeScaleBytes, 0)
                                           : At<std::uint32_t>(EvictionProfile::PositionScaleBytes, 0);
            auto       result = _mm_mul_ss(_mm_set_ss(Float(bits)), _mm_set_ss(Float(scale)));
            if (word == 4)
                result = _mm_add_ss(result, _mm_set_ss(Float(At<std::uint32_t>(EvictionProfile::HalfBytes, 0))));
            const auto value = _mm_cvtss_f32(result);
            // CVTTSS2SI's low WORD is the source encoding, not saturation.
            // Refuse nonfinite/indefinite conversions before destructive entry.
            if (!Finite(Bits(value)) || value < -2147483648.0f || value >= 2147483648.0f)
                return false;
            output = static_cast<std::uint16_t>(_mm_cvttss_si32(result));
            return true;
        }
        ClockResult CaptureHeap(std::uintptr_t image, Heap::Value& output) noexcept
        {
            // Borrow the constructor-published engine heap and its real slots.
            // No source frame becomes a private-storage allocation record.
            if (!Heap::QualifyProfile(image))
                return ClockResult::BindingChanged;
            for (const auto& pin : StorageProfile::Code)
            {
                std::array<unsigned char, StorageProfile::MaximumPinBytes> bytes{};
                if (!Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return ClockResult::BindingChanged;
            }
            Heap::Value current;
            const auto  result = Heap::Capture(image, current);
            if (result != ClockResult::Ok)
                return result;
            const auto slots = [&](std::uintptr_t table, std::uintptr_t allocate, std::uintptr_t release)
            {
                std::uintptr_t a = 0, b = 0;
                return Image::ReadImage(image, table + 0x10, &a, sizeof(a)) && Image::ReadImage(image, table + 0x30, &b, sizeof(b)) && a == image + allocate &&
                       b == image + release;
            };
            if (!slots(current.authorityVtable, StorageProfile::AuthorityMallocRva, StorageProfile::AuthorityFreeRva) ||
                (current.current != current.authority && !slots(current.currentVtable, StorageProfile::ProxyMallocRva, StorageProfile::ProxyFreeRva)))
                return ClockResult::BindingChanged;
            output = current;
            return ClockResult::Ok;
        }
        float ConvertWord(std::int16_t value, std::uint32_t scale) noexcept
        {
            // Match MOVD/CVTDQ2PS/MULSS rather than letting scalar arithmetic
            // contract, widen, or choose a different rounding itinerary.
            const auto integer = _mm_cvtsi32_si128(static_cast<int>(value));
            return _mm_cvtss_f32(_mm_mul_ss(_mm_cvtepi32_ps(integer), _mm_set_ss(Float(scale))));
        }
        template <class Plan>
        void PredictPostProcess(Plan& plan) noexcept
        {
            float         duration = 0.0f;
            std::uint32_t sign = 0;
            std::memcpy(&sign, ConversionProfile::SignMask.bytes, sizeof(sign));
            for (auto& frame : plan.frames)
            {
                const auto stride = frame.stride * 2;
                const auto count = frame.expected.size() / stride;
                for (std::size_t key = 1; key != count; ++key)
                {
                    const auto current = key * stride;
                    const auto previous = current - stride;
                    const auto multiply = [&](std::size_t word)
                    {
                        return _mm_mul_ss(_mm_set_ss(Float(At<std::uint32_t>(frame.expected, current + word * 4))),
                                          _mm_set_ss(Float(At<std::uint32_t>(frame.expected, previous + word * 4))));
                    };
                    // Preserve FEC4F0's rounding order: (y*prevY + x*prevX)
                    // + (w*prevW + z*prevZ). The possibly flipped current
                    // quaternion becomes the next predecessor.
                    const auto xy = _mm_add_ss(multiply(1), multiply(0));
                    const auto zw = _mm_add_ss(multiply(3), multiply(2));
                    if (_mm_comilt_ss(_mm_add_ss(xy, zw), _mm_setzero_ps()))
                        for (std::size_t word = 0; word != 4; ++word)
                            Put(frame.expected, current + word * 4, At<std::uint32_t>(frame.expected, current + word * 4) ^ sign);
                }
                const auto last = Float(At<std::uint32_t>(frame.expected, (count - 1) * stride + 16));
                if (_mm_comigt_ss(_mm_set_ss(last), _mm_set_ss(duration)))
                    duration = last;
                // FEC1E0 chooses the signed maximum last absolute time before
                // descending SUBSS stores. Backward traversal preserves each
                // predecessor's original timestamp, including signed zeros.
                for (std::size_t key = count - 1; key != 0; --key)
                {
                    const auto current = key * stride + 16;
                    const auto previous = current - stride;
                    const auto difference = _mm_sub_ss(_mm_set_ss(Float(At<std::uint32_t>(frame.expected, current))),
                                                       _mm_set_ss(Float(At<std::uint32_t>(frame.expected, previous))));
                    Put(frame.expected, current, Bits(_mm_cvtss_f32(difference)));
                }
            }
            Put(plan.expected, 0x18, Bits(duration));
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
            std::size_t                stride = 0, outputOffset = 0;
            std::vector<unsigned char> before, expected, observed;
        };
        struct Sequence
        {
            std::uintptr_t address = 0;
            Row            before{}, expected{}, observed{};
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
        std::size_t                                                outputBytes = 0;
        std::uintptr_t                                             outputBase = 0, selectedCache = 0;
        std::uint32_t                                              sseControl = 0;
        unsigned char                                              loaded = 0;
        bool                                                       payload = false, cache = false, conversion = false, sharedInput = false, compression = false;
        std::unique_ptr<Data>                                      victim;
        Data*                                                      joint = nullptr;
        NativeAnimationHierarchyUncompressCalls::CheckOwner        afterApplicationFrameOwner = nullptr;
        NativeModelReferenceHeap::Value                            heap{};
        NativeModelLoadedReader::Value                             stream{};
        std::array<unsigned char, 32>                              blockHeader{};
        std::array<unsigned char, 16>                              streamHeader{};
        std::vector<AnimationHierarchyUncompressDetail::CacheNode> cacheNodes;
        std::vector<AnimationHierarchyUncompressDetail::Sequence>  sequences;
        std::vector<AnimationHierarchyUncompressDetail::Frame>     frames;
    };
    namespace AnimationHierarchyUncompressDetail
    {
        template <class Plan>
        ClockResult ConversionOwner(std::uintptr_t image, CheckOwner owner, const Plan& plan) noexcept
        {
            if (!plan.afterApplicationFrameOwner)
                return ClockResult::NotInitialized;
            const auto result = plan.afterApplicationFrameOwner();
            return result == ClockResult::Ok ? Origin::Owner({image}, owner) : result;
        }
        template <class Plan>
        bool AwayFromHeap(std::uintptr_t address, std::size_t bytes, const Plan& plan) noexcept
        {
            return Map::Disjoint(address, bytes, plan.heap.authority, NativeModelReferenceTransactionProfile::HeapBytes) &&
                   (plan.heap.current == plan.heap.authority ||
                    Map::Disjoint(address, bytes, plan.heap.current, NativeModelReferenceTransactionProfile::ProxyBytes));
        }
        template <class Plan>
        bool FrameRange(std::uintptr_t address, std::size_t bytes, const Plan& plan, bool output) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address ||
                !Map::Disjoint(address, bytes, plan.token.locator, plan.expected.size()) ||
                !Map::Disjoint(address, bytes, plan.cookieAddress, plan.sequenceBytes) || !AwayFromHeap(address, bytes, plan))
                return false;
            for (const auto& node : plan.cacheNodes)
                if (!Map::Disjoint(address, bytes, node.address, node.before.size()))
                    return false;
            const auto otherRange = [&](const auto& other, bool oldInputs)
            {
                if (!Map::Disjoint(address, bytes, other.token.locator, other.expected.size()) ||
                    (other.cookieAddress && other.sequenceBytes && !Map::Disjoint(address, bytes, other.cookieAddress, other.sequenceBytes)))
                    return false;
                for (const auto& node : other.cacheNodes)
                    if (!Map::Disjoint(address, bytes, node.address, node.before.size()))
                        return false;
                if (oldInputs)
                    for (const auto& frame : other.frames)
                        if (!Map::Disjoint(address, bytes, frame.address, frame.before.size()))
                            return false;
                return !output || !other.outputBase || Map::Disjoint(address, bytes, other.outputBase, other.outputBytes);
            };
            // Victim compact allocation precedes both input frees. Target
            // expansion follows victim input release and may reuse that base.
            // These checks use numeric journals, never query freed old spans.
            if ((plan.victim && !otherRange(*plan.victim, !output)) || (plan.joint && !otherRange(*plan.joint, !output || plan.compression)))
                return false;
            // These are numeric old extents after entry. Never query or copy a
            // freed input again merely to prove that the new output is disjoint.
            if (output)
                for (const auto& frame : plan.frames)
                    if (!Map::Disjoint(address, bytes, frame.address, frame.before.size()))
                        return false;
            return true;
        }
        template <class Plan>
        ClockResult VerifyConversion(std::uintptr_t image, CheckOwner owner, Plan& plan, bool after) noexcept
        {
            auto result = ConversionOwner(image, owner, plan);
            if (result != ClockResult::Ok)
                return result;
            if ((_mm_getcsr() & 0xffc0u) != plan.sseControl || !ConversionSource(image, plan.location.rva) ||
                (plan.compression && !EvictionSource(image, plan.location.rva)))
                return ClockResult::BindingChanged;
            Heap::Value heap;
            if ((result = CaptureHeap(image, heap)) != ClockResult::Ok)
                return result;
            if (!Heap::Same(plan.heap, heap))
                return ClockResult::BindingChanged;
            if ((result = Capture(image, owner, plan.location, plan.observed)) != ClockResult::Ok)
                return result;
            if (!Origin::Same(plan.before.origin, plan.observed.origin) || plan.observed.payload != (after ? plan.expected : plan.before.payload))
                return ClockResult::BindingChanged;
            std::uint64_t                  capacity = 0;
            std::array<unsigned char, 32>  block{};
            std::array<unsigned char, 16>  stream{};
            NativeModelLoadedReader        reader({image}, owner);
            NativeModelLoadedReader::Value state;
            if (!Map::PrivateWritable(plan.cookieAddress, plan.sequenceBytes) || !Map::Copy(plan.cookieAddress, &capacity, sizeof(capacity)) ||
                !Image::ReadImage(image, plan.blockAddress, block.data(), block.size(), true) ||
                !Image::ReadImage(image, plan.stream.rowAddress, stream.data(), stream.size(), true))
                return ClockResult::ReadFailure;
            if (capacity != plan.capacity || block != plan.blockHeader || stream != plan.streamHeader)
                return ClockResult::BindingChanged;
            if ((result = reader.Read(plan.stream.modelIndex, state)) != ClockResult::Ok)
                return result;
            if (state.rowAddress != plan.stream.rowAddress || state.rawFlags != plan.stream.rawFlags || state.rawLoadState != 1 || !state.streamedIn)
                return ClockResult::BindingChanged;
            for (auto& sequence : plan.sequences)
                if (!Map::Copy(sequence.address, sequence.observed.data(), sequence.observed.size()))
                    return ClockResult::ReadFailure;
            if (after)
            {
                const auto base = At<std::uintptr_t>(plan.sequences[0].observed, 0x48);
                // Row0 receives the actual aggregate return. A null aggregate
                // takes FED6E0's fallback and clears bit08, so it cannot become
                // a fabricated successful one-allocation result here.
                if (At<std::uint16_t>(plan.sequences[0].observed, 0x42) != At<std::uint16_t>(plan.sequences[0].expected, 0x42) || !base || base % 16 ||
                    !FrameRange(base, plan.outputBytes, plan, true) || !Map::PrivateWritable(base, plan.outputBytes) ||
                    (plan.outputBase && plan.outputBase != base))
                    return ClockResult::BindingChanged;
                plan.outputBase = base;
            }
            for (std::size_t index = 0; index != plan.sequences.size(); ++index)
            {
                auto& sequence = plan.sequences[index];
                auto& frame = plan.frames[index];
                if (after)
                    Put(sequence.expected, 0x48, plan.outputBase + frame.outputOffset);
                if (sequence.observed != (after ? sequence.expected : sequence.before))
                    return ClockResult::BindingChanged;
                const auto address = after ? plan.outputBase + frame.outputOffset : frame.address;
                const auto bytes = after ? frame.expected.size() : frame.before.size();
                if (!Map::PrivateWritable(address, bytes) || !Map::Copy(address, frame.observed.data(), bytes))
                    return ClockResult::ReadFailure;
                const auto& expected = after ? frame.expected : frame.before;
                if (std::memcmp(frame.observed.data(), expected.data(), bytes))
                    return ClockResult::BindingChanged;
            }
            for (auto& node : plan.cacheNodes)
                if (!ReadCacheNode(image, node.address, node.observed))
                    return ClockResult::ReadFailure;
                else if (node.observed != (after ? node.expected : node.before))
                    return ClockResult::BindingChanged;
            Observation final;
            if ((result = Capture(image, owner, plan.location, final)) != ClockResult::Ok || (result = reader.Recheck(plan.stream)) != ClockResult::Ok ||
                (result = Heap::Recheck(image, plan.heap)) != ClockResult::Ok)
                return result;
            if (!Origin::Same(plan.before.origin, final.origin) || final.payload != plan.observed.payload || (_mm_getcsr() & 0xffc0u) != plan.sseControl)
                return ClockResult::BindingChanged;
            return ConversionOwner(image, owner, plan);
        }
        template <class Plan>
        ClockResult Verify(std::uintptr_t image, CheckOwner owner, Plan& plan, bool after) noexcept
        {
            if (plan.conversion)
            {
                // Compression publishes its output before target expansion.
                // Bookend both current banks around the one shared cache graph.
                if (plan.victim)
                {
                    const auto victim = VerifyConversion(image, owner, *plan.victim, after);
                    if (victim != ClockResult::Ok)
                        return victim;
                }
                const auto result = VerifyConversion(image, owner, plan, after);
                return result != ClockResult::Ok || !plan.victim ? result : VerifyConversion(image, owner, *plan.victim, after);
            }
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
        ClockResult PrepareConversionCache(std::uintptr_t image, Plan& plan, Calls& receipt)
        {
            constexpr std::size_t MaximumCacheNodes = 256;
            if (image > (std::numeric_limits<std::uintptr_t>::max)() - 0x53c08e8 - 24)
                return ClockResult::BindingChanged;
            const std::array<std::uintptr_t, 4> anchors{image + 0x53c08a0, image + 0x53c08b8, image + 0x53c08d0, image + 0x53c08e8};
            plan.cacheNodes.reserve(MaximumCacheNodes + anchors.size());
            const auto append = [&](std::uintptr_t address) -> ClockResult
            {
                if (!address || address % alignof(std::uintptr_t) || address > (std::numeric_limits<std::uintptr_t>::max)() - 24 ||
                    !Map::Disjoint(address, 24, plan.token.locator, plan.expected.size()) ||
                    !Map::Disjoint(address, 24, plan.cookieAddress, plan.sequenceBytes) || !AwayFromHeap(address, 24, plan))
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
            for (const auto anchor : anchors)
            {
                const auto result = append(anchor);
                if (result != ClockResult::Ok)
                    return result;
            }
            for (std::size_t graph = 0; graph != 2; ++graph)
            {
                const auto head = graph * 2, tail = head + 1;
                auto       previous = anchors[head];
                auto       current = At<std::uintptr_t>(plan.cacheNodes[head].before, 16);
                if (graph == 1)
                {
                    if (current == anchors[tail])
                    {
                        const auto selected = At<std::uintptr_t>(plan.cacheNodes[1].before, 8);
                        if (selected == anchors[0])
                            return ClockResult::NotInitialized;
                        std::uintptr_t candidate = 0;
                        for (const auto& node : plan.cacheNodes)
                            if (node.address == selected)
                                candidate = At<std::uintptr_t>(node.before, 0);
                        if (!candidate || candidate == plan.token.locator)
                            return ClockResult::OwnerConflict;
                        // Cache data supplies only a candidate. PrepareVictim
                        // must independently prove its actual current bank.
                        plan.victim = std::make_unique<Plan>();
                        plan.victim->token = {plan.token.ownerGeneration, candidate};
                        plan.victim->joint = &plan;
                        plan.selectedCache = selected;
                    }
                    else
                        plan.selectedCache = current;
                }
                while (current != anchors[tail])
                {
                    if (plan.cacheNodes.size() - anchors.size() == MaximumCacheNodes)
                        return ClockResult::Busy;
                    const auto result = append(current);
                    if (result != ClockResult::Ok)
                        return result;
                    const auto& node = plan.cacheNodes.back();
                    if (At<std::uintptr_t>(node.before, 8) != previous || (graph == 1 && At<std::uintptr_t>(node.before, 0) != 0))
                        return ClockResult::BindingChanged;
                    previous = current;
                    current = At<std::uintptr_t>(node.before, 16);
                }
                if (At<std::uintptr_t>(plan.cacheNodes[tail].before, 8) != previous)
                    return ClockResult::BindingChanged;
            }
            bool       valid = true;
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
                if (written && address == plan.token.locator + 0x20)
                {
                    Put(plan.expected, 0x20, *written);
                    return *written;
                }
                if (written && plan.victim && address == plan.victim->token.locator + 0x20)
                {
                    Put(plan.victim->expected, 0x20, *written);
                    return *written;
                }
                valid = false;
                return 0;
            };
            const auto load = [&](std::uintptr_t address) { return word(address, nullptr); };
            const auto store = [&](std::uintptr_t address, std::uintptr_t value) { (void)word(address, &value); };
            const auto selected = plan.selectedCache;
            if (plan.victim)
            {
                // FF0F30..FF0F6F removes the tail victim, makes it the first
                // free node, clears victimH20 and node.data. The following
                // genuine FFAEF0 body performs the same eight insert/H stores
                // modeled below. Ordered snapshots preserve all aliases.
                store(load(selected + 16) + 8, load(selected + 8));
                store(load(selected + 8) + 16, load(selected + 16));
                store(selected + 16, load(anchors[2] + 16));
                store(load(anchors[2] + 16) + 8, selected);
                store(selected + 8, anchors[2]);
                store(anchors[2] + 16, selected);
                store(plan.victim->token.locator + 0x20, 0);
                store(selected, 0);
            }
            // FF0ED4..FF0F08, then FF0F7D: node.data first, the exact six
            // free-to-active link stores, then H20. The eviction case reaches
            // the same sequence through whole FFAEF0, which is not an allocator.
            store(selected, plan.token.locator);
            store(load(selected + 16) + 8, load(selected + 8));
            store(load(selected + 8) + 16, load(selected + 16));
            store(selected + 16, load(anchors[0] + 16));
            store(load(anchors[0] + 16) + 8, selected);
            store(selected + 8, anchors[0]);
            store(anchors[0] + 16, selected);
            store(plan.token.locator + 0x20, selected);
            if (!valid)
                return ClockResult::BindingChanged;
            plan.cache = true;
            receipt.cachePath = true;
            receipt.cacheStores = plan.victim ? 16 : 8;
            receipt.cacheNodes = static_cast<std::uint32_t>(plan.cacheNodes.size() - anchors.size());
            for (const auto& node : plan.cacheNodes)
                for (const auto offset : {std::size_t{0}, std::size_t{8}, std::size_t{16}})
                    if (At<std::uintptr_t>(node.before, offset) != At<std::uintptr_t>(node.expected, offset))
                        ++receipt.modifiedCacheWords;
            return ClockResult::Ok;
        }
        template <class Plan>
        ClockResult PrepareVictim(std::uintptr_t image, CheckOwner owner, Plan& target, Calls& receipt)
        {
            auto& plan = *target.victim;
            auto  result = ConversionOwner(image, owner, target);
            if (result != ClockResult::Ok)
                return result;
            plan.afterApplicationFrameOwner = target.afterApplicationFrameOwner;
            plan.heap = target.heap;
            plan.sseControl = target.sseControl;
            plan.compression = true;
            if ((result = Origin::Locate({image}, plan.token.locator, plan.location)) != ClockResult::Ok ||
                (result = Capture(image, owner, plan.location, plan.before)) != ClockResult::Ok)
                return result;
            if (!EvictionSource(image, plan.location.rva) || !ConversionSource(image, plan.location.rva) || plan.before.payload[0x13] != 0 ||
                plan.before.payload[0x12] != 0 || At<std::uintptr_t>(plan.before.payload, 0x20) != target.selectedCache ||
                !Finite(At<std::uint32_t>(plan.before.payload, 0x18)))
                return ClockResult::OwnerConflict;
            plan.expected = plan.before.payload;
            Put(plan.expected, 0x12, static_cast<unsigned char>(1));
            Put(plan.expected, 0x20, std::uintptr_t{0});
            const auto count = At<std::int16_t>(plan.before.payload, 0x10);
            if (count <= 0)
                return ClockResult::OwnerConflict;
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
            if (!Map::PrivateWritable(plan.cookieAddress, plan.sequenceBytes) || !AwayFromHeap(plan.cookieAddress, plan.sequenceBytes, plan) ||
                !Map::Disjoint(plan.token.locator, plan.expected.size(), target.token.locator, target.expected.size()) ||
                !Map::Disjoint(plan.cookieAddress, plan.sequenceBytes, target.cookieAddress, target.sequenceBytes) ||
                !Map::Disjoint(plan.cookieAddress, plan.sequenceBytes, target.token.locator, target.expected.size()) ||
                !Map::Disjoint(target.cookieAddress, target.sequenceBytes, plan.token.locator, plan.expected.size()))
                return ClockResult::OwnerConflict;
            for (const auto& node : target.cacheNodes)
                if (!Map::Disjoint(plan.cookieAddress, plan.sequenceBytes, node.address, node.before.size()) ||
                    !Map::Disjoint(plan.token.locator, plan.expected.size(), node.address, node.before.size()))
                    return ClockResult::OwnerConflict;
            for (const auto& frame : target.frames)
                if (!FrameRange(frame.address, frame.before.size(), target, false))
                    return ClockResult::OwnerConflict;
            plan.blockAddress = image + NativeAnimationBlockProfile::BankRva + plan.before.origin.block * NativeAnimationBlockProfile::RowBytes;
            if (!Image::ReadImage(image, plan.blockAddress, plan.blockHeader.data(), plan.blockHeader.size(), true))
                return ClockResult::ReadFailure;
            plan.loaded = plan.blockHeader[NativeAnimationBlockProfile::LoadedOffset];
            if (plan.loaded != 1)
                return ClockResult::NotInitialized;
            NativeModelLoadedReader reader({image}, owner);
            if ((result = reader.Read(NativeStreamingResourceProfile::IfpFirst + plan.before.origin.block, plan.stream)) != ClockResult::Ok)
                return result;
            if (plan.stream.rawLoadState != 1 || !plan.stream.streamedIn)
                return ClockResult::NotInitialized;
            if (!Image::ReadImage(image, plan.stream.rowAddress, plan.streamHeader.data(), plan.streamHeader.size(), true))
                return ClockResult::ReadFailure;
            plan.sequences.reserve(static_cast<std::size_t>(count));
            plan.frames.reserve(static_cast<std::size_t>(count));
            for (std::int32_t index = 0; index < count; ++index)
            {
                Sequence sequence;
                sequence.address = plan.sequenceFirst + static_cast<std::size_t>(index) * ReadProfile::SequenceStride;
                if (!Map::Copy(sequence.address, sequence.before.data(), sequence.before.size()))
                    return ClockResult::ReadFailure;
                sequence.expected = sequence.before;
                const auto flags = At<std::uint16_t>(sequence.before, 0x42);
                const auto frames = At<std::int16_t>(sequence.before, 0x44);
                if ((flags & 4) || frames <= 0)
                    return ClockResult::OwnerConflict;
                if (!index)
                    plan.sharedInput = (flags & 8) != 0;
                else if (((flags & 8) != 0) != plan.sharedInput)
                    return ClockResult::OwnerConflict;
                Frame frame;
                frame.address = At<std::uintptr_t>(sequence.before, 0x48);
                frame.stride = (flags & 2 ? FrameProfile::RootStride : FrameProfile::ChildStride) * 2;
                const auto inputBytes = static_cast<std::size_t>(frames) * frame.stride;
                const auto outputBytes = inputBytes / 2;
                if (inputBytes > MaximumPreparedBytes - target.preparedBytes - plan.preparedBytes ||
                    outputBytes > MaximumPreparedBytes - target.outputBytes - plan.outputBytes)
                    return ClockResult::Busy;
                if (!FrameRange(frame.address, inputBytes, plan, false) || frame.address % 4 || ((!plan.sharedInput || !index) && frame.address % 16) ||
                    !Map::PrivateWritable(frame.address, inputBytes))
                    return ClockResult::BindingChanged;
                for (const auto& previous : plan.frames)
                    if (!Map::Disjoint(frame.address, inputBytes, previous.address, previous.before.size()))
                        return ClockResult::OwnerConflict;
                if (plan.sharedInput && index &&
                    (plan.preparedBytes > (std::numeric_limits<std::uintptr_t>::max)() - plan.frames[0].address ||
                     frame.address != plan.frames[0].address + plan.preparedBytes))
                    return ClockResult::OwnerConflict;
                frame.outputOffset = plan.outputBytes;
                frame.before.resize(inputBytes);
                frame.expected.resize(outputBytes);
                frame.observed.resize(inputBytes);
                if (!Map::Copy(frame.address, frame.before.data(), frame.before.size()))
                    return ClockResult::ReadFailure;
                const auto predictionSse = _mm_getcsr();
                bool       valid = true;
                for (std::int32_t key = 0; key < frames && valid; ++key)
                    for (std::size_t word = 0; word != frame.stride / 4; ++word)
                    {
                        std::uint16_t packed = 0;
                        if (!PredictPacked(At<std::uint32_t>(frame.before, static_cast<std::size_t>(key) * frame.stride + word * 4), word, packed))
                        {
                            valid = false;
                            break;
                        }
                        Put(frame.expected, static_cast<std::size_t>(key) * (frame.stride / 2) + word * 2, packed);
                    }
                _mm_setcsr(predictionSse);
                if (!valid)
                    return ClockResult::OwnerConflict;
                Put(sequence.expected, 0x42, static_cast<std::uint16_t>(flags | 12u));
                plan.preparedBytes += inputBytes;
                plan.outputBytes += outputBytes;
                receipt.victimPreparedFrames += static_cast<std::uint16_t>(frames);
                plan.sequences.push_back(std::move(sequence));
                plan.frames.push_back(std::move(frame));
            }
            if (plan.sharedInput && !Map::PrivateWritable(plan.frames[0].address, plan.preparedBytes))
                return ClockResult::BindingChanged;
            // Both banks remain engine-owned. The joint CPU journal grants no
            // private allocation adoption or adapter authority to free a base.
            receipt.evictionPath = true;
            receipt.victimHierarchy = plan.token.locator;
            receipt.victimSharedInput = plan.sharedInput;
            receipt.victimSequences = static_cast<std::uint32_t>(plan.sequences.size());
            receipt.victimInputBytes = static_cast<std::uint32_t>(plan.preparedBytes);
            receipt.victimOutputBytes = static_cast<std::uint32_t>(plan.outputBytes);
            return VerifyConversion(image, owner, plan, false);
        }
        template <class Plan>
        ClockResult PrepareConversion(std::uintptr_t image, CheckOwner owner, Plan& plan, Calls& receipt)
        {
            auto result = ConversionOwner(image, owner, plan);
            if (result != ClockResult::Ok)
                return result;
            if (At<std::uintptr_t>(plan.before.payload, 0x20))
                return ClockResult::NotInitialized;
            const auto count = At<std::int16_t>(plan.before.payload, 0x10);
            if (count <= 0)
                return ClockResult::OwnerConflict;
            if (!ConversionSource(image, plan.location.rva) || !FrameProducerProfile(image))
                return ClockResult::BindingChanged;
            if ((result = CaptureHeap(image, plan.heap)) != ClockResult::Ok)
                return result;
            plan.sseControl = _mm_getcsr() & 0xffc0u;
            // CPU prediction must not raise an unmasked precision exception
            // before the protected original call. Other rounding controls are
            // preserved and bookended for the actual native SSE itinerary.
            if ((plan.sseControl & 0x1f80u) != 0x1f80u)
                return ClockResult::OwnerConflict;
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
            if (!Map::PrivateWritable(plan.cookieAddress, plan.sequenceBytes) || !AwayFromHeap(plan.cookieAddress, plan.sequenceBytes, plan))
                return ClockResult::BindingChanged;
            plan.blockAddress = image + NativeAnimationBlockProfile::BankRva + plan.before.origin.block * NativeAnimationBlockProfile::RowBytes;
            if (!Image::ReadImage(image, plan.blockAddress, plan.blockHeader.data(), plan.blockHeader.size(), true))
                return ClockResult::ReadFailure;
            plan.loaded = plan.blockHeader[NativeAnimationBlockProfile::LoadedOffset];
            if (plan.loaded != 1)
                return ClockResult::NotInitialized;
            NativeModelLoadedReader reader({image}, owner);
            if ((result = reader.Read(NativeStreamingResourceProfile::IfpFirst + plan.before.origin.block, plan.stream)) != ClockResult::Ok)
                return result;
            if (plan.stream.rawLoadState != 1 || !plan.stream.streamedIn)
                return ClockResult::NotInitialized;
            if (!Image::ReadImage(image, plan.stream.rowAddress, plan.streamHeader.data(), plan.streamHeader.size(), true))
                return ClockResult::ReadFailure;
            if ((result = PrepareConversionCache(image, plan, receipt)) != ClockResult::Ok)
                return result;
            plan.sequences.reserve(static_cast<std::size_t>(count));
            plan.frames.reserve(static_cast<std::size_t>(count));
            for (std::int32_t index = 0; index < count; ++index)
            {
                Sequence sequence;
                sequence.address = plan.sequenceFirst + static_cast<std::size_t>(index) * ReadProfile::SequenceStride;
                if (!Map::Copy(sequence.address, sequence.before.data(), sequence.before.size()))
                    return ClockResult::ReadFailure;
                sequence.expected = sequence.before;
                const auto flags = At<std::uint16_t>(sequence.before, 0x42);
                const auto frames = At<std::int16_t>(sequence.before, 0x44);
                if (!(flags & 4) || frames <= 0)
                    return ClockResult::OwnerConflict;
                if (!index)
                    plan.sharedInput = (flags & 8) != 0;
                else if (((flags & 8) != 0) != plan.sharedInput)
                    return ClockResult::OwnerConflict;
                Frame frame;
                frame.address = At<std::uintptr_t>(sequence.before, 0x48);
                frame.stride = flags & 2 ? FrameProfile::RootStride : FrameProfile::ChildStride;
                const auto inputBytes = static_cast<std::size_t>(frames) * frame.stride;
                const auto outputBytes = inputBytes * 2;
                if (outputBytes > MaximumPreparedBytes - plan.outputBytes)
                    return ClockResult::Busy;
                if (!FrameRange(frame.address, inputBytes, plan, false) || frame.address % 2 || ((!plan.sharedInput || !index) && frame.address % 16) ||
                    !Map::PrivateWritable(frame.address, inputBytes))
                    return ClockResult::BindingChanged;
                for (const auto& previous : plan.frames)
                    if (!Map::Disjoint(frame.address, inputBytes, previous.address, previous.before.size()))
                        return ClockResult::OwnerConflict;
                if (plan.sharedInput && index &&
                    (plan.preparedBytes > (std::numeric_limits<std::uintptr_t>::max)() - plan.frames[0].address ||
                     frame.address != plan.frames[0].address + plan.preparedBytes))
                    return ClockResult::OwnerConflict;
                frame.outputOffset = plan.outputBytes;
                frame.before.resize(inputBytes);
                frame.expected.resize(outputBytes);
                frame.observed.resize(outputBytes);
                if (!Map::Copy(frame.address, frame.before.data(), frame.before.size()))
                    return ClockResult::ReadFailure;
                const auto predictionSse = _mm_getcsr();
                for (std::int32_t key = 0; key < frames; ++key)
                {
                    const auto input = static_cast<std::size_t>(key) * frame.stride;
                    const auto output = input * 2;
                    const auto words = frame.stride / 2;
                    for (std::size_t word = 0; word != words; ++word)
                    {
                        const auto scale = word < 4 ? 0x39800000u : word == 4 ? 0x3c888889u : 0x3a800000u;
                        Put(frame.expected, output + word * 4, Bits(ConvertWord(At<std::int16_t>(frame.before, input + word * 2), scale)));
                    }
                }
                // Prediction is CPU scratch, so retire its exception-status
                // flags before a possible refusal. This is before native entry;
                // the original's own floating-point state is never repaired.
                _mm_setcsr(predictionSse);
                Put(sequence.expected, 0x42, static_cast<std::uint16_t>((flags | 8u) & ~4u));
                plan.preparedBytes += inputBytes;
                plan.outputBytes += outputBytes;
                receipt.preparedFrames += static_cast<std::uint16_t>(frames);
                plan.sequences.push_back(std::move(sequence));
                plan.frames.push_back(std::move(frame));
            }
            if (plan.sharedInput && !Map::PrivateWritable(plan.frames[0].address, plan.preparedBytes))
                return ClockResult::BindingChanged;
            if (plan.victim && (result = PrepareVictim(image, owner, plan, receipt)) != ClockResult::Ok)
                return result;
            if (Float(At<std::uint32_t>(plan.before.payload, 0x18)) == 0.0f)
            {
                const auto predictionSse = _mm_getcsr();
                PredictPostProcess(plan);
                _mm_setcsr(predictionSse);
            }
            // The native H remains owner before and after replacement. These
            // CPU snapshots grant no private-storage adoption or raw free.
            plan.conversion = true;
            Put(plan.expected, 0x12, static_cast<unsigned char>(0));
            receipt.decompressionPath = true;
            receipt.sharedInput = plan.sharedInput;
            receipt.sequences = static_cast<std::uint32_t>(plan.sequences.size());
            receipt.encodedBytes = static_cast<std::uint32_t>(plan.preparedBytes);
            receipt.outputBytes = static_cast<std::uint32_t>(plan.outputBytes);
            return Verify(image, owner, plan, false);
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
                if (plan.before.payload[0x12] != 0)
                {
                    if ((result = PrepareConversion(image, owner, plan, receipt)) != ClockResult::Ok)
                        return result;
                }
                else if (At<std::uintptr_t>(plan.before.payload, 0x20) != 0 && (result = PrepareCache(image, plan, receipt)) != ClockResult::Ok)
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
                                                               Receipt* output, Invoke invoke, CheckOwner afterApplicationFrameOwner) noexcept
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
            data->afterApplicationFrameOwner = afterApplicationFrameOwner;
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
        if (data->conversion && (result = Detail::ConversionOwner(image, owner, *data)) != ClockResult::Ok)
            return finish(result);
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeAnimationHierarchyUncompressCalls::Function>(image + receipt.entryRva), request.locator, receipt.exceptionCode);
        receipt.returned = result == ClockResult::Ok;
        const auto postOwner = data->conversion ? Detail::ConversionOwner(image, owner, *data) : ClockResult::Ok;
        if (result != ClockResult::Ok)
            return finish(result);
        if (postOwner != ClockResult::Ok)
            return finish(postOwner);
        if ((result = Detail::Verify(image, owner, *data, true)) != ClockResult::Ok)
            return finish(result);
        if ((result = Detail::Verify(image, owner, *data, true)) != ClockResult::Ok)
            return finish(result);
        if (data->conversion)
        {
            receipt.outputBase = data->outputBase;
            receipt.convertedFrames = receipt.preparedFrames;
            if (data->victim)
            {
                receipt.victimOutputBase = data->victim->outputBase;
                receipt.victimCompressedFrames = receipt.victimPreparedFrames;
            }
        }
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
