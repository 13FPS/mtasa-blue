/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit every selected whole WorldAdd write before its genuine entry
 ****************************************************************************/
#include "NativeWorldAddEntryMemory.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <xmmintrin.h>
namespace mta::game_de::NativeWorldAddEntryMemory
{
    namespace EntryBudget = NativeWorldAddBudgetMemory;
    namespace EntryBounds = NativeWorldBoundRectMemory;
    namespace EntryP = NativeWorldAddProfile;
    namespace EntryC = NativeStreamingCompletionContext;
    namespace
    {
        template <class T, class Bytes>
        T EntryField(const Bytes& bytes, std::size_t at) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + at, sizeof(value));
            return value;
        }
        template <class T, class Bytes>
        void EntryPut(Bytes& bytes, std::size_t at, T value) noexcept
        {
            std::memcpy(bytes.data() + at, &value, sizeof(value));
        }
        bool SameEntity(const NativeWorldPoolMatrixMemory::Entity& a, const NativeWorldPoolMatrixMemory::Entity& b) noexcept
        {
            return a.kind == b.kind && a.index == b.index && a.generation == b.generation && a.root == b.root && a.pool == b.pool && a.objects == b.objects &&
                   a.flags == b.flags && a.address == b.address && a.vtable == b.vtable && a.capacity == b.capacity && a.stride == b.stride &&
                   a.header == b.header && a.bytes == b.bytes;
        }
        bool BoundAndBudget(const Value& value) noexcept
        {
            return SameEntity(value.bounds.entity, value.budget.entity) && EntryC::Same(value.bounds.world, value.budget.world, true) &&
                   NativeModelReferenceHeap::Same(value.bounds.heap, value.budget.heap);
        }
        std::size_t EntryStride(const Value& value) noexcept
        {
            return value.budget.plan.pool == NativeWorldAddPlan::PoolKind::Single ? EntryP::SingleBytes : EntryP::DoubleBytes;
        }
        bool EntryHead(const Value& value, std::uintptr_t rva, const std::vector<unsigned char>*& bytes, std::size_t& at) noexcept
        {
            if (rva >= EntryP::SectorsRva && rva - EntryP::SectorsRva <= value.sectors.size() &&
                sizeof(std::uintptr_t) <= value.sectors.size() - (rva - EntryP::SectorsRva))
            {
                bytes = &value.sectors;
                at = rva - EntryP::SectorsRva;
                return at % 8 == 0;
            }
            if (rva >= EntryP::LodRva && rva - EntryP::LodRva <= value.lod.size() && sizeof(std::uintptr_t) <= value.lod.size() - (rva - EntryP::LodRva))
            {
                bytes = &value.lod;
                at = rva - EntryP::LodRva;
                return at % 8 == 0;
            }
            return false;
        }
        bool EntryNode(const Value& value, std::uintptr_t address, std::size_t& at) noexcept
        {
            const auto base = value.budget.pool.objects, stride = EntryStride(value);
            if (address < base || (address - base) % stride || address - base > value.bank.size() || stride > value.bank.size() - (address - base))
                return false;
            at = address - base;
            return true;
        }
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner after, Value& output,
                        NativeWorldBoundRectCalls::Invoker rectInvoke, NativeWorldBoundRectCalls::Receipt* rectangleReceipt)
    {
        if (rectangleReceipt)
            *rectangleReceipt = {};
        if (kind != Kind::Building && kind != Kind::Dummy)
            return ClockResult::InvalidArgument;
        // This nested capture contains the full current world/matrix
        // observations. Allocate locally before entry rather than consuming
        // another large native callback stack frame.
        auto  storage = std::make_unique<Value>();
        auto& value = *storage;
        value.fpControls = _mm_getcsr() & 0xffc0u;
        auto result = EntryBounds::Capture(image, generation, kind, index, owner, after, value.bounds);
        if (result != ClockResult::Ok)
            return result;
        NativeWorldAddPlan::Rect rectangle{};
        result = NativeWorldBoundRectCalls::Read(image, generation, kind, index, rectangle, owner, after, &value.rectRead, rectInvoke);
        if (rectangleReceipt)
            *rectangleReceipt = value.rectRead;
        if (result != ClockResult::Ok)
            return result;
        if (!EntryBounds::Seal(image, value.bounds))
            return ClockResult::BindingChanged;
        const auto budgetKind = kind == Kind::Building ? NativeWorldAddPlan::Kind::Building : NativeWorldAddPlan::Kind::Dummy;
        if ((result = EntryBudget::CaptureForRect(image, budgetKind, index, rectangle, value.budget)) != ClockResult::Ok)
            return result;
        if (!BoundAndBudget(value))
            return ClockResult::BindingChanged;
        value.bank.resize(static_cast<std::size_t>(value.budget.pool.capacity) * EntryStride(value));
        value.sectors.resize(EntryP::SectorSide * EntryP::SectorSide * 16);
        value.lod.resize(EntryP::LodSide * EntryP::LodSide * 8);
        if (!EntryC::Copy(value.budget.pool.objects, value.bank.data(), value.bank.size()) ||
            !NativeImageAccess::ReadImage(image, image + EntryP::SectorsRva, value.sectors.data(), value.sectors.size(), true) ||
            !NativeImageAccess::ReadImage(image, image + EntryP::LodRva, value.lod.data(), value.lod.size(), true))
            return ClockResult::ReadFailure;
        const auto stride = EntryStride(value);
        for (std::size_t i = 0; i < value.budget.plan.steps.size(); ++i)
        {
            const std::vector<unsigned char>* grid = nullptr;
            std::size_t                       at = 0, node = 0;
            const auto&                       head = value.budget.heads[i];
            const auto&                       allocated = value.budget.allocatedPrefix[i];
            if (!EntryHead(value, head.rva, grid, at) || EntryField<std::uintptr_t>(*grid, at) != head.value || !EntryNode(value, allocated.address, node) ||
                std::memcmp(value.bank.data() + node, allocated.bytes.data(), stride))
                return ClockResult::BindingChanged;
            if (head.value && (!EntryNode(value, head.value, node) || std::memcmp(value.bank.data() + node, head.node.bytes.data(), stride)))
                return ClockResult::BindingChanged;
        }
        if ((result = owner()) != ClockResult::Ok || (result = after()) != ClockResult::Ok)
            return result;
        if (!Seal(image, value))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool Seal(std::uintptr_t image, const Value& value) noexcept
    {
        if ((_mm_getcsr() & 0xffc0u) != value.fpControls || !BoundAndBudget(value) || !EntryBounds::Seal(image, value.bounds) ||
            !EntryBudget::Seal(image, value.budget))
            return false;
        try
        {
            std::vector<unsigned char> bytes(value.bank.size());
            if (!EntryC::Copy(value.budget.pool.objects, bytes.data(), bytes.size()) || bytes != value.bank)
                return false;
            bytes.resize(value.sectors.size());
            if (!NativeImageAccess::ReadImage(image, image + EntryP::SectorsRva, bytes.data(), bytes.size(), true) || bytes != value.sectors)
                return false;
            bytes.resize(value.lod.size());
            return NativeImageAccess::ReadImage(image, image + EntryP::LodRva, bytes.data(), bytes.size(), true) && bytes == value.lod &&
                   (_mm_getcsr() & 0xffc0u) == value.fpControls;
        }
        catch (...)
        {
            return false;
        }
    }
    bool Expected(const Value& before, Value& output)
    {
        // Prepare all fallible CPU snapshot storage before the mutating
        // original; keep the same local lifetime and unchanged-output failure.
        auto       storage = std::make_unique<Value>(before);
        auto&      next = *storage;
        const auto stride = EntryStride(next);
        const bool isDouble = next.budget.plan.pool == NativeWorldAddPlan::PoolKind::Double;
        if (next.budget.plan.steps.size() != next.budget.heads.size() || next.budget.plan.steps.size() != next.budget.allocatedPrefix.size() ||
            next.budget.pool.free < next.budget.plan.steps.size())
            return false;
        for (std::size_t i = 0; i < next.budget.plan.steps.size(); ++i)
        {
            const auto&                       step = next.budget.plan.steps[i];
            const std::vector<unsigned char>* grid = nullptr;
            std::size_t                       at = 0, node = 0, oldNode = 0;
            if (!EntryHead(next, step.headRva, grid, at))
                return false;
            const auto old = EntryField<std::uintptr_t>(*grid, at);
            if (step.slot >= next.budget.pool.states.size())
                return false;
            const auto address = next.budget.pool.objects + step.slot * stride;
            if (!EntryNode(next, address, node) || (old && !EntryNode(next, old, oldNode)))
                return false;
            EntryPut(next.bank, node, next.budget.entity.address);
            EntryPut(next.bank, node + 8, old);
            if (isDouble)
            {
                EntryPut(next.bank, node + 0x10, std::uintptr_t{0});
                if (old)
                    EntryPut(next.bank, oldNode + 0x10, address);
            }
            auto& changed = step.headRva >= EntryP::LodRva && step.headRva - EntryP::LodRva < next.lod.size() ? next.lod : next.sectors;
            EntryPut(changed, at, address);
            auto& allocated = next.budget.allocatedPrefix[i];
            allocated.address = address;
            allocated.bytes.fill(0);
            std::memcpy(allocated.bytes.data(), next.bank.data() + node, stride);
            auto& head = next.budget.heads[i];
            head.value = address;
            head.node = allocated;
        }
        // The native entity thunk writes exactly four cached byte ranges.
        // Transform/model/actor pointers and all opaque row bytes survive.
        for (std::size_t i = 0; i < next.budget.plan.ranges.size(); ++i)
        {
            next.budget.entity.bytes[0x3c + i] = next.budget.plan.ranges[i];
            next.bounds.entity.bytes[0x3c + i] = next.budget.plan.ranges[i];
        }
        next.budget.pool.states = next.budget.plan.flagsAfter;
        next.budget.pool.cursor = next.budget.plan.cursorAfter;
        EntryPut(next.budget.pool.header, 0x14, next.budget.pool.cursor);
        next.budget.pool.free -= static_cast<std::uint32_t>(next.budget.plan.steps.size());
        output = std::move(next);
        return true;
    }
}
