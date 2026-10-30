/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the source cache unlink without heap or frame teardown
 ****************************************************************************/
#include "NativeAnimationHierarchyCacheRemove.h"
#include "NativeAnimationHierarchyCacheRemoveProfile.h"
#include "NativeAnimationHierarchyOrigin.h"
#include "NativeAnimationHierarchyReadProfile.h"
#include "NativeModelReferenceMap.h"
#include "NativeStreamingCompletionContext.h"
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace mta::game_de
{
    namespace AnimationHierarchyCacheRemoveDetail
    {
        namespace Origin = NativeAnimationHierarchyOrigin;
        namespace Image = NativeImageAccess;
        namespace Map = NativeModelReferenceMap;
        namespace Context = NativeStreamingCompletionContext;
        namespace Source = NativeAnimationHierarchyCacheRemoveProfile;
        namespace ReadProfile = NativeAnimationHierarchyReadProfile;
        using CheckOwner = NativeAnimationHierarchyCacheRemoveCalls::CheckOwner;
        using Store = NativeAnimationHierarchyCacheRemoveCalls::Store;
        using Receipt = NativeAnimationHierarchyCacheRemoveCalls::Receipt;
        using Binding = NativeAnimationBlockMemory::Binding;
        using Hierarchy = std::array<unsigned char, 40>;
        using NodeBytes = std::array<unsigned char, 24>;
        using Observation = Origin::Observation<Hierarchy>;
        // This limits one callback's preparation, not the native cache size.
        constexpr std::size_t    MaximumCacheNodes = 256;
        constexpr std::uintptr_t ActiveHeadRva = 0x53c08a0, ActiveTailRva = 0x53c08b8;
        constexpr std::uintptr_t FreeHeadRva = 0x53c08d0, FreeTailRva = 0x53c08e8;

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
        bool Profile(std::uintptr_t image, std::uintptr_t hierarchyRva) noexcept
        {
            if (!Origin::Sections({image}, hierarchyRva, 0, Source::Code.data(), Source::Code.size()) ||
                !Origin::Sections({image}, hierarchyRva, 0, ReadProfile::Code.data(), ReadProfile::Code.size()))
                return false;
            std::array<unsigned char, Source::SourceBytes.size()> source{};
            if (!Image::ReadImage(image, image + Source::SourceRva, source.data(), source.size(), false, true) || source != Source::SourceBytes)
                return false;
            for (const auto& pin : ReadProfile::Code)
            {
                std::array<unsigned char, ReadProfile::MaximumPinBytes> bytes{};
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
        struct Node
        {
            std::uintptr_t address = 0;
            NodeBytes      before{}, expected{}, observed{};
        };
        bool ReadNode(std::uintptr_t image, std::uintptr_t address, NodeBytes& output) noexcept
        {
            // Sentinels use writable image data; engine nodes may use private
            // storage. Neither kind is allocated, adopted or freed by unlink.
            return Map::PrivateWritable(address, output.size()) ? Map::Copy(address, output.data(), output.size())
                                                                : Image::ReadImage(image, address, output.data(), output.size(), true);
        }
        ClockResult WriteStores(const Store* stores, std::size_t count, std::uint32_t& exception) noexcept
        {
            if (!stores || count > 8)
                return ClockResult::InvalidArgument;
            __try
            {
                // Each aligned source QWORD store remains ordered. This is
                // not an atomic multi-store transaction, so any lost ACK must
                // retain the prepared journal instead of attempting rollback.
                for (std::size_t index = 0; index != count; ++index)
                    *reinterpret_cast<volatile std::uintptr_t*>(stores[index].address) = stores[index].value;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false, std::memory_order_release); }
        };
    }

    struct NativeAnimationHierarchyCacheRemove::Data
    {
        NativeAnimationHierarchyReferenceToken                         token{};
        NativeAnimationHierarchyOrigin::Location                       location{};
        AnimationHierarchyCacheRemoveDetail::Observation               before{}, observed{};
        AnimationHierarchyCacheRemoveDetail::Hierarchy                 expected{};
        std::vector<AnimationHierarchyCacheRemoveDetail::Node>         nodes;
        std::array<NativeAnimationHierarchyCacheRemoveCalls::Store, 8> stores{};
        std::size_t                                                    storeCount = 0;
    };

    namespace AnimationHierarchyCacheRemoveDetail
    {
        template <class Plan>
        ClockResult Verify(std::uintptr_t image, CheckOwner owner, Plan& plan, bool after) noexcept
        {
            auto result = Capture(image, owner, plan.location, plan.observed);
            if (result != ClockResult::Ok)
                return result;
            if (!Origin::Same(plan.before.origin, plan.observed.origin) || plan.observed.payload != (after ? plan.expected : plan.before.payload))
                return ClockResult::BindingChanged;
            for (auto& node : plan.nodes)
                if (!ReadNode(image, node.address, node.observed))
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
        ClockResult PrepareGraphs(std::uintptr_t image, Plan& plan, std::uintptr_t selected, Receipt& receipt)
        {
            if (image > (std::numeric_limits<std::uintptr_t>::max)() - FreeTailRva - sizeof(NodeBytes))
                return ClockResult::BindingChanged;
            const std::array<std::uintptr_t, 4> anchors{image + ActiveHeadRva, image + ActiveTailRva, image + FreeHeadRva, image + FreeTailRva};
            plan.nodes.reserve(MaximumCacheNodes + anchors.size());
            const auto append = [&](std::uintptr_t address) -> ClockResult
            {
                if (!address || address % alignof(std::uintptr_t) || address > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(NodeBytes) ||
                    !Map::Disjoint(address, sizeof(NodeBytes), plan.token.locator, plan.expected.size()))
                    return ClockResult::BindingChanged;
                // One shared inventory proves disjoint active/free graphs and
                // rejects cycles, crossed sentinels and partially aliased rows.
                for (const auto& node : plan.nodes)
                    if (!Map::Disjoint(address, sizeof(NodeBytes), node.address, node.before.size()))
                        return ClockResult::OwnerConflict;
                Node node;
                node.address = address;
                if (!ReadNode(image, address, node.before))
                    return ClockResult::ReadFailure;
                node.expected = node.before;
                plan.nodes.push_back(node);
                return ClockResult::Ok;
            };
            for (const auto anchor : anchors)
            {
                const auto result = append(anchor);
                if (result != ClockResult::Ok)
                    return result;
            }
            bool found = false;
            for (std::size_t graph = 0; graph != 2; ++graph)
            {
                const auto headIndex = graph * 2;
                const auto tailIndex = headIndex + 1;
                auto       previous = anchors[headIndex];
                auto       current = At<std::uintptr_t>(plan.nodes[headIndex].before, 0x10);
                while (current != anchors[tailIndex])
                {
                    if (plan.nodes.size() - anchors.size() == MaximumCacheNodes)
                        return ClockResult::Busy;
                    const auto result = append(current);
                    if (result != ClockResult::Ok)
                        return result;
                    const auto& node = plan.nodes.back();
                    if (At<std::uintptr_t>(node.before, 8) != previous)
                        return ClockResult::BindingChanged;
                    if (current == selected)
                    {
                        if (graph != 0 || At<std::uintptr_t>(node.before, 0) != plan.token.locator)
                            return ClockResult::BindingChanged;
                        found = true;
                    }
                    previous = current;
                    current = At<std::uintptr_t>(node.before, 0x10);
                }
                if (At<std::uintptr_t>(plan.nodes[tailIndex].before, 8) != previous)
                    return ClockResult::BindingChanged;
            }
            if (!found)
                return ClockResult::BindingChanged;
            bool       valid = true;
            const auto load = [&](std::uintptr_t address) -> std::uintptr_t
            {
                for (const auto& node : plan.nodes)
                    if (address >= node.address && address - node.address <= 16 && (address - node.address) % 8 == 0)
                        return At<std::uintptr_t>(node.expected, static_cast<std::size_t>(address - node.address));
                valid = false;
                return 0;
            };
            const auto store = [&](std::uintptr_t address, std::uintptr_t value)
            {
                if (plan.storeCount == plan.stores.size())
                {
                    valid = false;
                    return;
                }
                for (auto& node : plan.nodes)
                    if (address >= node.address && address - node.address <= 16 && (address - node.address) % 8 == 0)
                    {
                        Put(node.expected, static_cast<std::size_t>(address - node.address), value);
                        plan.stores[plan.storeCount++] = {address, value};
                        return;
                    }
                if (address == plan.token.locator + 0x20)
                {
                    Put(plan.expected, 0x20, value);
                    plan.stores[plan.storeCount++] = {address, value};
                    return;
                }
                valid = false;
            };
            // FEC683..FEC6C9 is an ordered active-to-free splice, followed by
            // H20/node.data clears. Predict aliases from earlier stores, never
            // replace the source order with an abstract list rearrangement.
            store(load(selected + 0x10) + 8, load(selected + 8));
            store(load(selected + 8) + 0x10, load(selected + 0x10));
            store(selected + 0x10, load(anchors[2] + 0x10));
            store(load(anchors[2] + 0x10) + 8, selected);
            store(selected + 8, anchors[2]);
            store(anchors[2] + 0x10, selected);
            store(plan.token.locator + 0x20, std::uintptr_t{0});
            store(selected, std::uintptr_t{0});
            if (!valid || plan.storeCount != plan.stores.size())
                return ClockResult::BindingChanged;
            receipt.cacheNodes = static_cast<std::uint32_t>(plan.nodes.size() - anchors.size());
            return ClockResult::Ok;
        }
        template <class Plan>
        ClockResult Prepare(std::uintptr_t image, CheckOwner owner, Plan& plan, Receipt& receipt)
        {
            auto result = Origin::Locate({image}, plan.token.locator, plan.location);
            if (result != ClockResult::Ok || (result = Capture(image, owner, plan.location, plan.before)) != ClockResult::Ok)
                return result;
            plan.expected = plan.before.payload;
            const auto selected = At<std::uintptr_t>(plan.before.payload, 0x20);
            if (!selected)
            {
                // Source cache0 is no effect for every other H field. Do not
                // inspect children or require a cache graph for this branch.
                receipt.noOp = true;
                return Verify(image, owner, plan, false);
            }
            result = PrepareGraphs(image, plan, selected, receipt);
            return result == ClockResult::Ok ? Verify(image, owner, plan, false) : result;
        }
    }

    NativeAnimationHierarchyCacheRemove::NativeAnimationHierarchyCacheRemove() noexcept = default;
    NativeAnimationHierarchyCacheRemove::~NativeAnimationHierarchyCacheRemove() = default;
    ClockResult NativeAnimationHierarchyCacheRemove::Remove(std::uintptr_t image, const NativeAnimationHierarchyReferenceToken& requested, CheckOwner owner,
                                                            Receipt* output, Write write) noexcept
    {
        namespace Detail = AnimationHierarchyCacheRemoveDetail;
        const auto request = requested;
        Receipt    receipt;
        receipt.generation = request.ownerGeneration;
        receipt.hierarchy = request.locator;
        const auto report = [&](ClockResult result)
        {
            receipt.debt = debt.load(std::memory_order_acquire);
            receipt.result = result;
            if (output)
                *output = receipt;
            return result;
        };
        if (!image || !request.ownerGeneration || !request.locator || !owner)
            return report(ClockResult::InvalidArgument);
        bool expected = false;
        if (!busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return report(ClockResult::Busy);
        Detail::BusyScope operation{busy};
        const auto        finish = [&](ClockResult result)
        {
            if (receipt.entered && result != ClockResult::Ok)
                debt.store(true, std::memory_order_release);
            if (!debt.load(std::memory_order_acquire))
                data.reset();
            return report(result);
        };
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
        if (receipt.noOp)
        {
            receipt.effectsVerified = true;
            return finish(ClockResult::Ok);
        }
#if defined(MTA_DE_ANIMATION_HIERARCHY_CACHE_REMOVE_WRITE)
        const auto entry = write ? write : &MTA_DE_ANIMATION_HIERARCHY_CACHE_REMOVE_WRITE;
#else
        const auto entry = write ? write : &Detail::WriteStores;
#endif
        // Every source store, old/new snapshot and verification buffer already
        // exists. From this boundary onward, uncertainty keeps them all alive.
        receipt.entered = true;
        result = entry(data->stores.data(), data->storeCount, receipt.exceptionCode);
        receipt.returned = result == ClockResult::Ok;
        if (result != ClockResult::Ok)
            return finish(result);
        if ((result = Detail::Verify(image, owner, *data, true)) != ClockResult::Ok || (result = Detail::Verify(image, owner, *data, true)) != ClockResult::Ok)
            return finish(result);
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
