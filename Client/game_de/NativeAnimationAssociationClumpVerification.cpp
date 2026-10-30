/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify only original clump-defined receiver and dynamic40 node bytes
 ****************************************************************************/
#include "NativeAnimationAssociationClumpVerification.h"
#include "NativeAnimationClumpInitializationPlan.h"
#include "NativeAnimationAssociationProfile.h"
#include "NativeModelReferenceMap.h"
#include "NativeModelReferenceHeap.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <cstring>
#include <limits>
namespace mta::game_de::NativeAnimationAssociationClumpVerification
{
    namespace
    {
        namespace P = NativeAnimationAssociationProfile;
        namespace M = NativeModelReferenceMap;
        namespace InputPlan = NativeAnimationClumpInitializationPlan;
        constexpr std::uintptr_t                 ConstructorRva = 0xfeb910, InitializeRva = 0xfeba90;
        constexpr std::array<unsigned char, 89>  ConstructorBytes{{
            0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xd9, 0x48, 0x8d, 0x05, 0xa8, 0x68, 0x2f, 0x03, 0x48, 0x89, 0x01, 0xb8, 0xff, 0xff, 0xff,
            0xff, 0x66, 0x89, 0x41, 0x1a, 0x33, 0xc9, 0x48, 0x89, 0x4b, 0x20, 0x89, 0x4b, 0x38, 0x88, 0x4b, 0x60, 0x48, 0x89, 0x4b, 0x08, 0x48, 0x89,
            0x4b, 0x10, 0x48, 0x8b, 0xcb, 0x48, 0xc7, 0x43, 0x30, 0x00, 0x00, 0x80, 0x3f, 0x48, 0xc7, 0x43, 0x3c, 0x00, 0x00, 0x80, 0x3f, 0x48, 0xc7,
            0x43, 0x44, 0xff, 0xff, 0x00, 0x00, 0xe8, 0x30, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xc3, 0x48, 0x83, 0xc4, 0x20, 0x5b, 0xc3,
        }};
        constexpr std::array<unsigned char, 370> InitializeBytes{{
            0x48, 0x89, 0x6c, 0x24, 0x18, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x57, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xba, 0x00, 0x03, 0x00, 0x00,
            0x4c, 0x8b, 0xea, 0x4d, 0x8b, 0xf8, 0x48, 0x8b, 0xf1, 0x0f, 0xbf, 0x47, 0x10, 0x8b, 0xd0, 0x66, 0x89, 0x41, 0x18, 0xe8, 0x50, 0x01, 0x00, 0x00,
            0x45, 0x33, 0xe4, 0x45, 0x8b, 0xcc, 0x66, 0x44, 0x3b, 0x66, 0x18, 0x7d, 0x1c, 0x41, 0x8b, 0xcc, 0x48, 0x8b, 0x46, 0x20, 0x48, 0x8d, 0x49, 0x28,
            0x41, 0xff, 0xc1, 0x48, 0x89, 0x74, 0x08, 0xf0, 0x0f, 0xbf, 0x46, 0x18, 0x44, 0x3b, 0xc8, 0x7c, 0xe7, 0x4c, 0x89, 0x7e, 0x28, 0x41, 0x8b, 0xec,
            0x66, 0x45, 0x3b, 0x67, 0x10, 0x0f, 0x8d, 0xf5, 0x00, 0x00, 0x00, 0x48, 0x89, 0x5c, 0x24, 0x50, 0x4c, 0x89, 0x74, 0x24, 0x58, 0x4d, 0x8b, 0xf4,
            0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x8b, 0x5f, 0x08, 0x49, 0x03, 0xde, 0x0f, 0xb6, 0x43, 0x42, 0xc0, 0xe8, 0x04, 0xa8, 0x01,
            0x74, 0x7a, 0x8b, 0x87, 0x40, 0x14, 0x00, 0x00, 0x3b, 0x87, 0x6c, 0x14, 0x00, 0x00, 0x0f, 0x84, 0x9f, 0x00, 0x00, 0x00, 0x48, 0x63, 0x8f, 0x80,
            0x14, 0x00, 0x00, 0x4c, 0x8d, 0x87, 0x70, 0x14, 0x00, 0x00, 0x49, 0x8b, 0x50, 0x08, 0x48, 0xff, 0xc9, 0x4c, 0x0f, 0xbf, 0x4b, 0x40, 0x49, 0x23,
            0xc9, 0x48, 0x85, 0xd2, 0x4c, 0x0f, 0x45, 0xc2, 0x41, 0x8b, 0x04, 0x88, 0x83, 0xf8, 0xff, 0x74, 0x72, 0x48, 0x8b, 0x97, 0x38, 0x14, 0x00, 0x00,
            0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x98, 0x48, 0xc1, 0xe0, 0x04, 0x48, 0x03, 0xc2, 0x44, 0x39, 0x08, 0x74, 0x0a, 0x8b, 0x40,
            0x08, 0x83, 0xf8, 0xff, 0x75, 0xea, 0xeb, 0x4b, 0x48, 0x85, 0xc0, 0x48, 0x8d, 0x48, 0x04, 0x49, 0x0f, 0x44, 0xcc, 0x48, 0x85, 0xc9, 0x74, 0x3b,
            0x8b, 0x01, 0xeb, 0x0b, 0x48, 0x8b, 0xd3, 0x49, 0x8b, 0xcd, 0xe8, 0x89, 0xfa, 0x00, 0x00, 0x83, 0xf8, 0xff, 0x74, 0x27, 0x48, 0x63, 0xc8, 0x48,
            0x8b, 0xc1, 0x48, 0xc1, 0xe0, 0x05, 0x48, 0x83, 0xc0, 0x28, 0x48, 0x03, 0xc7, 0x74, 0x14, 0x66, 0x44, 0x39, 0x63, 0x44, 0x7e, 0x0d, 0x48, 0x8b,
            0x46, 0x20, 0x48, 0x8d, 0x0c, 0x89, 0x48, 0x89, 0x5c, 0xc8, 0x10, 0x41, 0x0f, 0xbf, 0x47, 0x10, 0xff, 0xc5, 0x49, 0x83, 0xc6, 0x50, 0x3b, 0xe8,
            0x0f, 0x8c, 0x2a, 0xff, 0xff, 0xff, 0x4c, 0x8b, 0x74, 0x24, 0x58, 0x48, 0x8b, 0x5c, 0x24, 0x50, 0x48, 0x8b, 0x6c, 0x24, 0x60, 0x48, 0x83, 0xc4,
            0x20, 0x41, 0x5f, 0x41, 0x5d, 0x41, 0x5c, 0x5f, 0x5e, 0xc3,
        }};

        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        template <class T>
        void Put(Bytes& bytes, std::size_t offset, T value) noexcept
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }
        template <std::size_t N>
        bool Pin(std::uintptr_t image, std::uintptr_t rva, const std::array<unsigned char, N>& expected) noexcept
        {
            std::array<unsigned char, N> actual{};
            return NativeImageAccess::ReadImage(image, image + rva, actual.data(), actual.size(), false, true) && actual == expected;
        }
        bool DefinedReceiver(std::uintptr_t image, Operation operation, const Bytes& before, const Bytes& after, const InputPlan::Snapshot& input) noexcept
        {
            Bytes expected = before;
            // Init changes count, nodes and H only. The two constructor routes
            // retain their different actual source stores; opaque padding and
            // callbacks must not accidentally become zero-filled defaults.
            if (operation != Operation::Initialize)
            {
                Put(expected, 0, image + P::VtableRva);
                Put(expected, 8, std::uint64_t{0});
                Put(expected, 0x10, std::uint64_t{0});
                Put(expected, 0x1a, std::uint16_t{0xffff});
                Put(expected, 0x30, std::uint64_t{0x3f800000});
                Put(expected, 0x38, std::uint32_t{0});
                Put(expected, 0x3c, std::uint64_t{0x3f800000});
                if (operation == Operation::Construct)
                {
                    Put(expected, 0x44, std::uint64_t{0xffff});
                    expected[0x60] = 0;
                }
                else
                {
                    Put(expected, 0x44, std::uint16_t{0xffff});
                    Put(expected, 0x46, std::uint16_t{0});
                    Put(expected, 0x48, std::uint32_t{0});
                }
            }
            Put(expected, 0x18, input.frameCount);
            Put(expected, 0x20, Field<std::uintptr_t>(after, 0x20));
            Put(expected, 0x28, input.hierarchy.interfaceAddress);
            return after == expected && Field<std::uintptr_t>(after, 0) == image + P::VtableRva;
        }
    }
    bool Qualify(std::uintptr_t image) noexcept
    {
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::VtableRva || !Pin(image, ConstructorRva, ConstructorBytes) ||
            !Pin(image, InitializeRva, InitializeBytes))
            return false;
        std::array<unsigned char, P::MaximumPinBytes> actual{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > actual.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        std::uintptr_t destructor = 0;
        // Reuse the established original node allocator/destructor and common
        // provider/name/import profile; there is no copied matcher execution.
        return NativeModelReferenceHeap::ReadOnlyImage(image, image + P::VtableRva, sizeof(destructor)) &&
               NativeImageAccess::ReadImage(image, image + P::VtableRva, &destructor, sizeof(destructor)) && destructor == image + P::DestructorRva &&
               InputPlan::QualifyProfile(image);
    }
    ClockResult Observe(std::uintptr_t image, std::uintptr_t receiver, Operation operation, const Bytes& before, const InputPlan::Snapshot& input,
                        Observation& output) noexcept
    {
        if ((operation != Operation::Construct && operation != Operation::Initialize && operation != Operation::InitializeCustom) || !receiver ||
            receiver % 8 || output.sequenceMap.size() != input.frameCount || input.frameCount > NativeStaticAssociationInitializeProfile::FrameCapacity ||
            input.sequenceRows.size() != input.hierarchy.sequenceCount || !input.hierarchy.interfaceAddress)
            return ClockResult::InvalidArgument;
        if (!Qualify(image))
            return ClockResult::BindingChanged;
        if (!M::Copy(receiver, output.row.data(), output.row.size()))
            return ClockResult::ReadFailure;
        if (!DefinedReceiver(image, operation, before, output.row, input))
            return ClockResult::BindingChanged;
        output.count = Field<std::uint16_t>(output.row, 0x18);
        output.nodes = Field<std::uintptr_t>(output.row, 0x20);
        // FEBC10's unsigned request arithmetic also allocates 64 bytes when
        // the logical count is zero. Null or merely readable nodes do not
        // replace that native header/allocation observation.
        const auto amount = static_cast<std::uint64_t>(output.count) * P::NodeBytes;
        output.requestBytes = amount ? static_cast<std::size_t>(((amount - 1) & ~std::uint64_t{63}) + 128) : 64;
        if (output.nodes < 8 || output.nodes % 64)
            return ClockResult::BindingChanged;
        if (!M::Copy(output.nodes - 8, &output.nodeRaw, sizeof(output.nodeRaw)))
            return ClockResult::ReadFailure;
        if (!output.nodeRaw || output.nodeRaw > (std::numeric_limits<std::uintptr_t>::max)() - 64 ||
            ((output.nodeRaw + 64) & ~std::uintptr_t{63}) != output.nodes || !M::PrivateWritable(output.nodeRaw, output.requestBytes) ||
            output.nodes - output.nodeRaw > output.requestBytes || amount > output.requestBytes - (output.nodes - output.nodeRaw) ||
            !M::Disjoint(output.nodeRaw, output.requestBytes, receiver, output.row.size()) ||
            !M::Disjoint(output.nodeRaw, output.requestBytes, input.container, NativeStaticAssociationInitializeProfile::ContainerBytes))
            return ClockResult::BindingChanged;
        std::uintptr_t clump = 0;
        if (!NativeStreamingCompletionContext::At(input.container, 0x18, clump))
            return ClockResult::ReadFailure;
        if (!M::Disjoint(output.nodeRaw, output.requestBytes, clump, 0x310) ||
            (input.sequences && !M::Disjoint(output.nodeRaw, output.requestBytes, input.sequences - 8, input.sequenceAllocationBytes)) ||
            (!input.map.rows.empty() &&
             !M::Disjoint(output.nodeRaw, output.requestBytes, input.map.rowsAddress, input.map.rows.size() * sizeof(InputPlan::BoneRow))) ||
            (!input.map.bits.empty() &&
             !M::Disjoint(output.nodeRaw, output.requestBytes, input.map.bitsAddress, input.map.bits.size() * sizeof(std::uint32_t))) ||
            (!input.map.buckets.empty() &&
             !M::Disjoint(output.nodeRaw, output.requestBytes, input.map.bucketsAddress, input.map.buckets.size() * sizeof(std::int32_t))))
            return ClockResult::BindingChanged;
        for (std::size_t index = 0; index != output.count; ++index)
        {
            std::array<unsigned char, P::NodeBytes> node{};
            if (!M::Copy(output.nodes + index * P::NodeBytes, node.data(), node.size()))
                return ClockResult::ReadFailure;
            const auto sequence = Field<std::uintptr_t>(node, 0x10);
            if (Field<std::uint32_t>(node, 8) != 0xffffffff || Field<std::uint32_t>(node, 0xc) || Field<std::uintptr_t>(node, 0x18) != receiver ||
                Field<std::uint32_t>(node, 0x20) != 0xffffffff)
                return ClockResult::BindingChanged;
            if (sequence)
            {
                if (!input.sequences || sequence < input.sequences || (sequence - input.sequences) % NativeCustomAnimSequenceStorageProfile::RowBytes)
                    return ClockResult::BindingChanged;
                const auto member = (sequence - input.sequences) / NativeCustomAnimSequenceStorageProfile::RowBytes;
                if (member >= input.sequenceRows.size() ||
                    Field<std::int16_t>(input.sequenceRows[static_cast<std::size_t>(member)], NativeCustomAnimSequenceStorageProfile::FrameCountOffset) <= 0)
                    return ClockResult::BindingChanged;
            }
            // This is the actual native result, not a second CPU name/bone
            // matcher. The caller retains it only after its causal call ACK.
            output.sequenceMap[index] = sequence;
        }
        return ClockResult::Ok;
    }
}
