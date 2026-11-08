/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Acknowledge exact matrix entry prefixes and retain incomplete intent
 ****************************************************************************/
#include "NativeWorldPoolMatrixDomain.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mta::game_de
{
    namespace MatrixDomainDetail
    {
        namespace M = NativeWorldPoolMatrixMemory;
        namespace P = NativeWorldPoolMatrixProfile;
        namespace D = NativeStreamingCompletionContext;
        using Calls = NativeWorldPoolEntryCalls::Operation;
        struct Guard
        {
            std::atomic_flag&  operation;
            std::atomic<bool>& active;
            bool               held;
            Guard(std::atomic_flag& flag, std::atomic<bool>& busy) noexcept : operation(flag), active(busy), held(!flag.test_and_set())
            {
                if (held)
                    active.store(true);
            }
            ~Guard()
            {
                if (held)
                {
                    active.store(false);
                    operation.clear();
                }
            }
        };
        struct WorldScope
        {
            bool held = D::TryEnter();
            ~WorldScope()
            {
                if (held)
                    D::Leave();
            }
        };
        std::uint64_t Key(M::Kind kind, std::uint32_t index) noexcept
        {
            return (static_cast<std::uint64_t>(kind) << 32) | index;
        }
        template <class V, class Bytes>
        V Field(const Bytes& bytes, std::size_t offset) noexcept
        {
            V value{};
            if (offset <= bytes.size() && sizeof(value) <= bytes.size() - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        template <class V, class Bytes>
        void Put(Bytes& bytes, std::size_t offset, const V& value) noexcept
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }
        ClockResult Owner(std::uintptr_t image, std::uint64_t generation, NativeWorldPoolMatrixDomain::CheckOwner owner,
                          NativeWorldPoolMatrixDomain::CheckOwner afterApplication) noexcept
        {
            if (!image || !generation || !owner || !afterApplication)
                return ClockResult::InvalidArgument;
            const auto result = afterApplication();
            return result == ClockResult::Ok ? D::Owner(image, owner) : result;
        }
        ClockResult Sample(std::uintptr_t image, std::uint64_t generation, M::Kind kind, std::uint32_t index, NativeWorldPoolMatrixDomain::CheckOwner owner,
                           NativeWorldPoolMatrixDomain::CheckOwner afterApplication, M::Value& bank, M::Entity& entity)
        {
            auto result = Owner(image, generation, owner, afterApplication);
            if (result != ClockResult::Ok || (result = M::CaptureEntity(image, kind, index, entity)) != ClockResult::Ok ||
                (result = M::Capture(image, bank)) != ClockResult::Ok || (result = Owner(image, generation, owner, afterApplication)) != ClockResult::Ok)
                return result;
            return M::SealEntity(image, entity) && M::Seal(image, bank) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult Final(std::uintptr_t image, std::uint64_t generation, NativeWorldPoolMatrixDomain::CheckOwner owner,
                          NativeWorldPoolMatrixDomain::CheckOwner afterApplication, const M::Value& bank, const M::Entity& entity)
        {
            const auto result = Owner(image, generation, owner, afterApplication);
            return result != ClockResult::Ok ? result : M::SealEntity(image, entity) && M::Seal(image, bank) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        bool EntityIdentity(const M::Entity& a, const M::Entity& b) noexcept
        {
            // A new callback requalifies the current slot, not the old cursor or
            // a retained matrix pointer. All selected row bytes are compared by
            // each operation below; free/reused slot generations refuse first.
            return a.kind == b.kind && a.index == b.index && a.generation == b.generation && a.root == b.root && a.pool == b.pool && a.objects == b.objects &&
                   a.flags == b.flags && a.address == b.address && a.vtable == b.vtable && a.capacity == b.capacity && a.stride == b.stride;
        }
        bool EntityExcept(const M::Entity& a, const M::Entity& b, std::size_t begin, std::size_t end) noexcept
        {
            if (!EntityIdentity(a, b))
                return false;
            for (std::size_t at = 0; at < a.bytes.size(); ++at)
                if ((at < begin || at >= end) && a.bytes[at] != b.bytes[at])
                    return false;
            return true;
        }
        bool PutPointer(M::Value& bank, std::uintptr_t image, std::uintptr_t address, std::size_t offset, std::uintptr_t value) noexcept
        {
            if (offset > P::NodeBytes || sizeof(value) > P::NodeBytes - offset)
                return false;
            if (address >= bank.bank && (address - bank.bank) % P::NodeBytes == 0)
            {
                const auto index = (address - bank.bank) / P::NodeBytes;
                if (index < bank.nodes.size())
                {
                    Put(bank.nodes[index].bytes, offset, value);
                    return true;
                }
            }
            if (address < image + P::DynamicStart || address >= image + P::DynamicStart + bank.sentinels.size())
                return false;
            const auto at = address - (image + P::DynamicStart);
            if (at % P::NodeBytes || at + offset + sizeof(value) > bank.sentinels.size())
                return false;
            Put(bank.sentinels, at + offset, value);
            return true;
        }
        std::uintptr_t Pointer(const M::Value& bank, std::uintptr_t image, std::uintptr_t address, std::size_t offset) noexcept
        {
            if (address >= bank.bank && (address - bank.bank) % P::NodeBytes == 0)
            {
                const auto index = (address - bank.bank) / P::NodeBytes;
                if (index < bank.nodes.size())
                    return Field<std::uintptr_t>(bank.nodes[index].bytes, offset);
            }
            if (address >= image + P::DynamicStart && address < image + P::DynamicStart + bank.sentinels.size())
                return Field<std::uintptr_t>(bank.sentinels, address - (image + P::DynamicStart) + offset);
            return 0;
        }
        bool MoveExpected(M::Value& expected, std::uintptr_t image, const M::NodeView& node, M::List target, std::uintptr_t owner) noexcept
        {
            const auto start = image + (target == M::List::Free ? P::FreeStart : P::StaticStart);
            if (!PutPointer(expected, image, node.previous, P::NextOffset, node.next) || !PutPointer(expected, image, node.next, P::PrevOffset, node.previous))
                return false;
            const auto first = Pointer(expected, image, start, P::NextOffset);
            return first && PutPointer(expected, image, node.address, P::PrevOffset, start) &&
                   PutPointer(expected, image, node.address, P::NextOffset, first) && PutPointer(expected, image, first, P::PrevOffset, node.address) &&
                   PutPointer(expected, image, start, P::NextOffset, node.address) && PutPointer(expected, image, node.address, P::OwnerOffset, owner);
        }
        bool RawExpected(const M::Value& expected, const M::Value& after) noexcept
        {
            if (!M::SameBank(expected, after) || expected.sentinels != after.sentinels || expected.nodes.size() != after.nodes.size())
                return false;
            for (std::size_t at = 0; at < expected.nodes.size(); ++at)
                if (expected.nodes[at].address != after.nodes[at].address || expected.nodes[at].bytes != after.nodes[at].bytes)
                    return false;
            return true;
        }
        void IdentityTransform(M::NodeView& node) noexcept
        {
            for (std::size_t row = 0; row != 4; ++row)
                for (std::size_t column = 0; column != 3; ++column)
                    Put(node.bytes, row * 16 + column * 4, float{row == column ? 1.0f : 0.0f});
        }
        M::Transform TransformOf(const M::NodeView& node) noexcept
        {
            M::Transform result{};
            std::copy_n(node.bytes.begin(), result.size(), result.begin());
            return result;
        }
    }
    void NativeWorldPoolMatrixDomain::Publish() noexcept
    {
        bool retained = uncertain.load();
        for (const auto& entry : records)
            retained = retained || entry.second.phase != Phase::Prepared;
        pending.store(retained);
    }
    void NativeWorldPoolMatrixDomain::MarkUnknown(Record& record, ClockResult result) noexcept
    {
        record.phase = Phase::Unknown;
        record.failure = result;
        uncertain.store(true);
        pending.store(true);
    }
    void NativeWorldPoolMatrixDomain::CatchEntered() noexcept
    {
        for (auto at = records.begin(); at != records.end();)
        {
            auto& record = at->second;
            if (record.phase == Phase::Removing || record.phase == Phase::Allocating || record.phase == Phase::Storing)
                MarkUnknown(record, ClockResult::ReadFailure);
            if (record.phase == Phase::Prepared)
                at = records.erase(at);
            else
                ++at;
        }
        Publish();
    }
    NativeWorldPoolMatrixDomain::Record NativeWorldPoolMatrixDomain::Remember(std::uintptr_t image, std::uint64_t generation,
                                                                              const NativeWorldPoolMatrixMemory::Value&  bank,
                                                                              const NativeWorldPoolMatrixMemory::Entity& entity,
                                                                              const Transform&                           transform) noexcept
    {
        Record record;
        record.image = image;
        record.generation = generation;
        record.bank = bank.bank;
        record.cookie = bank.cookie;
        record.heap = bank.heap;
        record.entity = entity;
        record.transform = transform;
        record.root = bank.world.root;
        record.gameMode = bank.world.gameMode;
        record.level = bank.world.level;
        record.world = bank.world.world;
        record.gameClass = bank.world.gameClass;
        record.levelClass = bank.world.levelClass;
        record.mode = bank.world.mode;
        record.memberCount = bank.world.count;
        for (std::size_t at = 0; at < record.memberCount && at < record.members.size(); ++at)
        {
            const auto& member = bank.world.members[at];
            record.members[at] = {member.objectAddress, member.vtableAddress, member.classAddress, member.outerAddress,
                                  member.itemAddress,   member.index,         member.serial};
        }
        return record;
    }
    bool NativeWorldPoolMatrixDomain::Matches(const Record& record, std::uintptr_t image, std::uint64_t generation,
                                              const NativeWorldPoolMatrixMemory::Value& bank, const NativeWorldPoolMatrixMemory::Entity& entity) noexcept
    {
        if (record.image != image || record.generation != generation || record.bank != bank.bank || record.cookie != bank.cookie ||
            !NativeModelReferenceHeap::Same(record.heap, bank.heap) || record.root != bank.world.root || record.gameMode != bank.world.gameMode ||
            record.level != bank.world.level || record.world != bank.world.world || record.gameClass != bank.world.gameClass ||
            record.levelClass != bank.world.levelClass || record.mode != bank.world.mode || record.memberCount != bank.world.count ||
            record.memberCount > record.members.size() || !MatrixDomainDetail::EntityIdentity(record.entity, entity))
            return false;
        for (std::size_t at = 0; at < record.memberCount; ++at)
        {
            const auto& a = record.members[at];
            const auto& b = bank.world.members[at];
            if (a.object != b.objectAddress || a.vtable != b.vtableAddress || a.klass != b.classAddress || a.outer != b.outerAddress ||
                a.item != b.itemAddress || a.index != b.index || a.serial != b.serial)
                return false;
        }
        return true;
    }
    ClockResult NativeWorldPoolMatrixDomain::Read(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner,
                                                  CheckOwner afterApplication, Transform& output, bool& present) noexcept
    {
        using namespace MatrixDomainDetail;
        Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        WorldScope scope;
        if (!scope.held)
            return ClockResult::Busy;
        try
        {
            M::Value  bank;
            M::Entity entity;
            auto      result = Sample(image, generation, kind, index, owner, afterApplication, bank, entity);
            if (result != ClockResult::Ok)
                return result;
            if (!entity.matrix)
            {
                present = false;
                return ClockResult::Ok;
            }
            M::NodeView node;
            result = M::FindOwner(bank, entity, node);
            if (result != ClockResult::Ok)
                return result;
            output = TransformOf(node);
            present = true;
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeWorldPoolMatrixDomain::Detach(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner,
                                                    CheckOwner afterApplication, NativeWorldPoolEntryCalls::Invoker invoke) noexcept
    {
        using namespace MatrixDomainDetail;
        Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        WorldScope scope;
        if (!scope.held)
            return ClockResult::Busy;
        try
        {
            M::Value  bank;
            M::Entity entity;
            auto      result = Sample(image, generation, kind, index, owner, afterApplication, bank, entity);
            if (result != ClockResult::Ok)
                return result;
            const auto key = Key(kind, index);
            const auto prior = records.find(key);
            if (prior != records.end())
                return prior->second.phase == Phase::Detached && Matches(prior->second, image, generation, bank, entity) &&
                               entity.bytes == prior->second.entity.bytes
                           ? ClockResult::Ok
                           : ClockResult::RestoreFailure;
            if (!entity.matrix)
                return ClockResult::Ok;
            M::NodeView node;
            if ((result = M::FindOwner(bank, entity, node)) != ClockResult::Ok)
                return result;
            auto expected = bank;
            if (!MoveExpected(expected, image, node, M::List::Free, 0))
                return ClockResult::BindingChanged;
            auto  found = records.emplace(key, Remember(image, generation, bank, entity, TransformOf(node))).first;
            auto& record = found->second;
            if ((result = Final(image, generation, owner, afterApplication, bank, entity)) != ClockResult::Ok)
            {
                records.erase(found);
                Publish();
                return result;
            }
            record.phase = Phase::Removing;
            Publish();
            result = NativeWorldPoolEntryCalls::Invoke(Calls::RemoveMatrix, image, entity.address, record.entry, invoke);
            if (result != ClockResult::Ok || !record.entry.entered || !record.entry.returned)
            {
                const auto failure = result == ClockResult::Ok ? ClockResult::WriteFailure : result;
                if (record.entry.entered)
                    MarkUnknown(record, failure);
                else
                    records.erase(found);
                Publish();
                return failure;
            }
            M::Value  after;
            M::Entity current;
            result = Sample(image, generation, kind, index, owner, afterApplication, after, current);
            float heading = 0;
            std::memcpy(&heading, current.bytes.data() + 0x14, sizeof(heading));
            if (result == ClockResult::Ok && (!EntityExcept(entity, current, 8, 0x20) || current.matrix || !std::isfinite(heading) ||
                                              std::memcmp(current.bytes.data() + 8, node.bytes.data() + 0x30, 12) || !RawExpected(expected, after)))
                result = ClockResult::BindingChanged;
            if (result != ClockResult::Ok)
            {
                MarkUnknown(record, result);
                return result;
            }
            record.entity = current;
            record.phase = Phase::Detached;
            Publish();
            return ClockResult::Ok;
        }
        catch (...)
        {
            CatchEntered();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeWorldPoolMatrixDomain::Restore(std::uintptr_t image, std::uint64_t generation, Record& record, CheckOwner owner,
                                                     CheckOwner afterApplication, NativeWorldPoolEntryCalls::Invoker invoke)
    {
        using namespace MatrixDomainDetail;
        M::Value  bank;
        M::Entity entity;
        auto      result = Sample(image, generation, record.entity.kind, record.entity.index, owner, afterApplication, bank, entity);
        if (result != ClockResult::Ok)
            return result;
        if (!Matches(record, image, generation, bank, entity) || entity.bytes != record.entity.bytes)
            return ClockResult::BindingChanged;
        if (record.phase == Phase::Detached)
        {
            if (entity.matrix || (result = M::RequireFree(bank, 1)) != ClockResult::Ok)
                return entity.matrix ? ClockResult::BindingChanged : result;
            const auto selected = bank.lists[static_cast<std::size_t>(M::List::Free)].front();
            const auto selectedNode = bank.nodes[selected];
            auto       expected = bank;
            if (!MoveExpected(expected, image, selectedNode, M::List::Static, entity.address))
                return ClockResult::BindingChanged;
            IdentityTransform(expected.nodes[selected]);
            if ((result = Final(image, generation, owner, afterApplication, bank, entity)) != ClockResult::Ok)
                return result;
            record.phase = Phase::Allocating;
            Publish();
            result = NativeWorldPoolEntryCalls::Invoke(Calls::AllocateStaticMatrix, image, entity.address, record.entry, invoke);
            if (result != ClockResult::Ok || !record.entry.entered || !record.entry.returned)
            {
                const auto failure = result == ClockResult::Ok ? ClockResult::WriteFailure : result;
                if (record.entry.entered)
                    MarkUnknown(record, failure);
                else
                    record.phase = Phase::Detached;
                Publish();
                return failure;
            }
            M::Value    after;
            M::Entity   current;
            M::NodeView allocated;
            result = Sample(image, generation, entity.kind, entity.index, owner, afterApplication, after, current);
            if (result == ClockResult::Ok &&
                (!EntityExcept(entity, current, 0x18, 0x20) || current.matrix != selectedNode.address || !RawExpected(expected, after)))
                result = ClockResult::BindingChanged;
            if (result == ClockResult::Ok)
                result = M::FindOwner(after, current, allocated);
            if (result != ClockResult::Ok || allocated.list != M::List::Static)
            {
                if (result == ClockResult::Ok)
                    result = ClockResult::BindingChanged;
                MarkUnknown(record, result);
                return result;
            }
            record.entity = current;
            record.allocatedTransform = TransformOf(allocated);
            record.phase = Phase::Allocated;
            Publish();
        }
        if (record.phase != Phase::Allocated)
            return ClockResult::RestoreFailure;
        // This fresh continuation may run after a known pre-entry store refusal.
        // It never replays the already acknowledged static matrix allocation.
        if ((result = Sample(image, generation, record.entity.kind, record.entity.index, owner, afterApplication, bank, entity)) != ClockResult::Ok)
            return result;
        M::NodeView node;
        if (!Matches(record, image, generation, bank, entity) || entity.bytes != record.entity.bytes ||
            (result = M::FindOwner(bank, entity, node)) != ClockResult::Ok)
            return result == ClockResult::Ok ? ClockResult::BindingChanged : result;
        if (node.list != M::List::Static || TransformOf(node) != record.allocatedTransform)
            return ClockResult::BindingChanged;
        auto       expected = bank;
        const auto selected = (node.address - bank.bank) / NativeWorldPoolMatrixProfile::NodeBytes;
        std::copy(record.transform.begin(), record.transform.end(), expected.nodes[selected].bytes.begin());
        if ((result = Final(image, generation, owner, afterApplication, bank, entity)) != ClockResult::Ok)
            return result;
        record.phase = Phase::Storing;
        Publish();
        result = M::StoreTransform(image, bank, entity, record.transform);
        if (result != ClockResult::Ok)
        {
            if (result == ClockResult::WriteFailure)
                MarkUnknown(record, result);
            else
                record.phase = Phase::Allocated;
            Publish();
            return result;
        }
        M::Value  after;
        M::Entity current;
        result = Sample(image, generation, entity.kind, entity.index, owner, afterApplication, after, current);
        if (result == ClockResult::Ok && (current.bytes != entity.bytes || !EntityIdentity(entity, current) || !RawExpected(expected, after)))
            result = ClockResult::BindingChanged;
        if (result != ClockResult::Ok)
            MarkUnknown(record, result);
        return result;
    }
    ClockResult NativeWorldPoolMatrixDomain::RestoreOne(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner,
                                                        CheckOwner afterApplication, NativeWorldPoolEntryCalls::Invoker invoke) noexcept
    {
        using namespace MatrixDomainDetail;
        Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        WorldScope scope;
        if (!scope.held)
            return ClockResult::Busy;
        try
        {
            const auto result = Owner(image, generation, owner, afterApplication);
            if (result != ClockResult::Ok)
                return result;
            const auto found = records.find(Key(kind, index));
            if (found == records.end())
                return ClockResult::Ok;
            const auto restored = Restore(image, generation, found->second, owner, afterApplication, invoke);
            if (restored == ClockResult::Ok)
                records.erase(found);
            Publish();
            return restored;
        }
        catch (...)
        {
            CatchEntered();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeWorldPoolMatrixDomain::RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, CheckOwner afterApplication,
                                                        NativeWorldPoolEntryCalls::Invoker invoke) noexcept
    {
        using namespace MatrixDomainDetail;
        Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        WorldScope scope;
        if (!scope.held)
            return ClockResult::Busy;
        try
        {
            auto result = Owner(image, generation, owner, afterApplication);
            if (result != ClockResult::Ok)
                return result;
            for (auto at = records.begin(); at != records.end();)
            {
                result = Restore(image, generation, at->second, owner, afterApplication, invoke);
                if (result != ClockResult::Ok)
                {
                    Publish();
                    return result;
                }
                at = records.erase(at);
            }
            Publish();
            return ClockResult::Ok;
        }
        catch (...)
        {
            CatchEntered();
            return ClockResult::ReadFailure;
        }
    }
}
