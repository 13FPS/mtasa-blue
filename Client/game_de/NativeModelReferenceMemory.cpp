/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reacquire an occupied native key before a single counter mutation
 ****************************************************************************/
#include "NativeModelReferenceMemory.h"
#include "NativeModelReferenceProfile.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelBankProfile.h"
#include "NativeStreamingModelContext.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeImageAccess.h"
#include <game/CModelRequestPolicy.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace mta::game_de::NativeModelReferenceMemory
{
    namespace
    {
        namespace P = NativeModelReferenceProfile;
        namespace C = NativeStreamingModelContext;
        struct Row
        {
            std::uint32_t key = 0, count = 0;
            std::int32_t  next = -1, bucket = -1;
        };
        static_assert(sizeof(Row) == P::RowBytes);
        struct Snapshot
        {
            std::uintptr_t                            map = 0, rowsAddress = 0, bitsAddress = 0, hashAddress = 0;
            std::array<unsigned char, P::HeaderBytes> header{};
            std::vector<Row>                          rows;
            std::vector<std::uint32_t>                bits;
            std::vector<std::int32_t>                 hash;
            std::int32_t                              selected = -1;
        };
        template <class T>
        T Field(const Snapshot& value, std::size_t offset) noexcept
        {
            T field{};
            std::memcpy(&field, value.header.data() + offset, sizeof(field));
            return field;
        }
        bool PrivateWritable(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            void*      allocation = nullptr;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) ||
                    ((page.Protect & 0xff) != PAGE_READWRITE && (page.Protect & 0xff) != PAGE_WRITECOPY) || !page.RegionSize)
                    return false;
                if (!allocation)
                    allocation = page.AllocationBase;
                if (allocation != page.AllocationBase)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
        {
            if (!PrivateWritable(address, bytes))
                return false;
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, bytes, &copied) && copied == bytes;
        }
        bool Disjoint(std::uintptr_t a, std::size_t an, std::uintptr_t b, std::size_t bn) noexcept
        {
            return a && b && an && bn && an <= (std::numeric_limits<std::uintptr_t>::max)() - a && bn <= (std::numeric_limits<std::uintptr_t>::max)() - b &&
                   (a + an <= b || b + bn <= a);
        }
        bool Occupied(const Snapshot& value, std::size_t index) noexcept
        {
            return (value.bits[index / 32] & (std::uint32_t{1} << (index % 32))) != 0;
        }
        bool Pins(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            for (const auto& pin : P::Code)
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            return C::QualifyProfile(image);
        }
        ClockResult Capture(const C::Value& context, const NativeModelBankReader::Value& model, Snapshot& output)
        {
            Snapshot value;
            if (P::MapOffset + P::HeaderBytes > (std::numeric_limits<std::uintptr_t>::max)() - context.root)
                return ClockResult::BindingChanged;
            value.map = context.root + P::MapOffset;
            if (!Copy(value.map, value.header.data(), value.header.size()))
                return ClockResult::ReadFailure;
            const auto num = Field<std::int32_t>(value, 8), capacity = Field<std::int32_t>(value, 0xc);
            const auto numBits = Field<std::int32_t>(value, 0x28), maxBits = Field<std::int32_t>(value, 0x2c);
            const auto freeHead = Field<std::int32_t>(value, 0x30), freeCount = Field<std::int32_t>(value, 0x34);
            const auto hashCount = Field<std::int32_t>(value, 0x48);
            // These are conservative admission bounds, not a claim that the
            // native allocator promises a particular maximum capacity.
            if (num <= 0 || num > P::MaximumRows || capacity < num || numBits != num || maxBits < numBits || maxBits % 32 || freeCount < 0 ||
                freeCount >= num || hashCount <= 0 || hashCount > P::MaximumBuckets || (hashCount & (hashCount - 1)))
                return ClockResult::BindingChanged;
            value.rowsAddress = Field<std::uintptr_t>(value, 0);
            const auto externalBits = Field<std::uintptr_t>(value, 0x20), externalHash = Field<std::uintptr_t>(value, 0x40);
            if ((!externalBits && maxBits > 128) || (!externalHash && hashCount != 1))
                return ClockResult::BindingChanged;
            value.bitsAddress = externalBits ? externalBits : value.map + 0x10;
            value.hashAddress = externalHash ? externalHash : value.map + 0x38;
            const auto rowBytes = static_cast<std::size_t>(num) * sizeof(Row), bitBytes = ((static_cast<std::size_t>(num) + 31) / 32) * 4;
            const auto hashBytes = static_cast<std::size_t>(hashCount) * 4;
            if (value.rowsAddress % alignof(std::uintptr_t) || value.bitsAddress % 4 || value.hashAddress % 4 ||
                !Disjoint(value.rowsAddress, rowBytes, context.root, P::MapOffset + P::HeaderBytes) ||
                !Disjoint(value.rowsAddress, rowBytes, context.provider, NativeStreamingRequestProfile::ProviderCountOffset + 4) ||
                !Disjoint(value.rowsAddress, rowBytes, context.rows,
                          static_cast<std::size_t>(context.count) * NativeStreamingRequestProfile::ProviderRowStride) ||
                !Disjoint(value.rowsAddress, rowBytes, model.nativeModelInfo, NativeStreamingRequestProfile::ModelPrefixBytes) ||
                !Disjoint(value.rowsAddress, rowBytes, value.bitsAddress, bitBytes) || !Disjoint(value.rowsAddress, rowBytes, value.hashAddress, hashBytes) ||
                !Disjoint(value.bitsAddress, bitBytes, value.hashAddress, hashBytes) ||
                (externalBits && !Disjoint(value.bitsAddress, bitBytes, context.root, P::MapOffset + P::HeaderBytes)) ||
                (externalHash && !Disjoint(value.hashAddress, hashBytes, context.root, P::MapOffset + P::HeaderBytes)))
                return ClockResult::BindingChanged;
            value.rows.resize(static_cast<std::size_t>(num));
            value.bits.resize(bitBytes / 4);
            value.hash.resize(static_cast<std::size_t>(hashCount));
            if (!Copy(value.rowsAddress, value.rows.data(), rowBytes) || !Copy(value.bitsAddress, value.bits.data(), bitBytes) ||
                !Copy(value.hashAddress, value.hash.data(), hashBytes))
                return ClockResult::ReadFailure;
            std::array<unsigned char, P::HeaderBytes> headerAfter{};
            if (!Copy(value.map, headerAfter.data(), headerAfter.size()))
                return ClockResult::ReadFailure;
            if (value.header != headerAfter)
                return ClockResult::BindingChanged;
            std::vector<unsigned char> visited(static_cast<std::size_t>(num));
            std::vector<std::uint32_t> keys;
            keys.reserve(static_cast<std::size_t>(num - freeCount));
            std::size_t active = 0;
            for (std::int32_t bucket = 0; bucket != hashCount; ++bucket)
                for (auto index = value.hash[static_cast<std::size_t>(bucket)]; index != -1;)
                {
                    if (index < 0 || index >= num || !Occupied(value, static_cast<std::size_t>(index)) || visited[static_cast<std::size_t>(index)])
                        return ClockResult::BindingChanged;
                    visited[static_cast<std::size_t>(index)] = 1;
                    const auto& row = value.rows[static_cast<std::size_t>(index)];
                    if (row.bucket != bucket || (row.key & static_cast<std::uint32_t>(hashCount - 1)) != static_cast<std::uint32_t>(bucket))
                        return ClockResult::BindingChanged;
                    if (row.key == model.modelIndex)
                    {
                        if (value.selected != -1)
                            return ClockResult::BindingChanged;
                        value.selected = index;
                    }
                    keys.push_back(row.key);
                    ++active;
                    index = row.next;
                }
            std::sort(keys.begin(), keys.end());
            if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
                return ClockResult::BindingChanged;
            // Free rows reuse key/value as previous/next free indices. They
            // must never be mistaken for an occupied model with a small count.
            std::int32_t previous = -1, freeSeen = 0;
            for (auto index = freeHead; index != -1;)
            {
                if (index < 0 || index >= num || Occupied(value, static_cast<std::size_t>(index)) || visited[static_cast<std::size_t>(index)] ||
                    freeSeen >= freeCount)
                    return ClockResult::BindingChanged;
                const auto&  row = value.rows[static_cast<std::size_t>(index)];
                std::int32_t prev = 0, next = 0;
                std::memcpy(&prev, &row.key, 4);
                std::memcpy(&next, &row.count, 4);
                if (prev != previous)
                    return ClockResult::BindingChanged;
                visited[static_cast<std::size_t>(index)] = 1;
                previous = index;
                index = next;
                ++freeSeen;
            }
            if (freeSeen != freeCount || active + static_cast<std::size_t>(freeCount) != static_cast<std::size_t>(num) ||
                std::find(visited.begin(), visited.end(), 0) != visited.end())
                return ClockResult::BindingChanged;
            if (value.selected < 0)
                return ClockResult::NotInitialized;
            output = std::move(value);
            return ClockResult::Ok;
        }
        bool Same(const Snapshot& before, const Snapshot& after, bool changed, std::uint32_t target) noexcept
        {
            if (before.map != after.map || before.header != after.header || before.bits != after.bits || before.hash != after.hash ||
                before.selected != after.selected || before.rows.size() != after.rows.size())
                return false;
            for (std::size_t i = 0; i != before.rows.size(); ++i)
            {
                const auto& a = before.rows[i];
                const auto& b = after.rows[i];
                if (a.key != b.key || a.next != b.next || a.bucket != b.bucket ||
                    (changed && i == static_cast<std::size_t>(before.selected) ? target : a.count) != b.count)
                    return false;
            }
            return true;
        }
#if !defined(MTA_DE_MODEL_REFERENCE_EXCHANGE)
        ClockResult NativeExchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(std::uint32_t));
            LONG e = 0, t = 0;
            std::memcpy(&e, &expected, sizeof(e));
            std::memcpy(&t, &target, sizeof(t));
            __try
            {
                const auto result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), t, e);
                std::memcpy(&observed, &result, sizeof(observed));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Adjust(std::uintptr_t image, std::uint32_t modelIndex, int delta, CheckOwner owner, Receipt* output, Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.modelIndex = modelIndex;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount || (delta != 1 && delta != -1))
            return finish(ClockResult::InvalidArgument);
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(modelIndex))
            return finish(ClockResult::UnsupportedBuild);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        try
        {
            NativeModelBankReader          bank({image}, owner);
            NativeModelLoadedReader        loaded({image}, owner);
            NativeModelBankReader::Value   model;
            NativeModelLoadedReader::Value state;
            auto                           result = bank.Read(modelIndex, model);
            if (result != ClockResult::Ok || (result = loaded.Read(modelIndex, state)) != ClockResult::Ok)
                return finish(result);
            if (!state.streamedIn || !model.nativeModelInfo)
                return finish(ClockResult::NotInitialized);
            if (!Pins(image))
                return finish(ClockResult::UnsupportedBuild);
            C::Value context;
            if ((result = C::Capture(image, model, context)) != ClockResult::Ok)
                return finish(result);
            Snapshot first, second;
            if ((result = Capture(context, model, first)) != ClockResult::Ok)
                return finish(result);
            receipt.root = context.root;
            receipt.map = first.map;
            receipt.row = first.rowsAddress + static_cast<std::uintptr_t>(first.selected) * P::RowBytes;
            receipt.before = first.rows[static_cast<std::size_t>(first.selected)].count;
            if ((delta < 0 && !receipt.before) || (delta > 0 && receipt.before == (std::numeric_limits<std::uint32_t>::max)()))
                return finish(ClockResult::InvalidArgument);
            // Reaching zero enters native type/streaming/removal work. Until
            // that complete operation is callable, the interior slice refuses.
            if (delta < 0 && receipt.before == 1)
                return finish(ClockResult::UnsupportedBuild);
            receipt.target = delta > 0 ? receipt.before + 1 : receipt.before - 1;
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                (result = C::Recheck(image, model, context)) != ClockResult::Ok || (result = Capture(context, model, second)) != ClockResult::Ok)
                return finish(result);
            if (!Same(first, second, false, 0) || !Pins(image))
                return finish(ClockResult::BindingChanged);
            if ((result = owner()) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_MODEL_REFERENCE_EXCHANGE)
            const auto entry = exchange ? exchange : &MTA_DE_MODEL_REFERENCE_EXCHANGE;
#else
            const auto entry = exchange ? exchange : &NativeExchange;
#endif
            // One compare/exchange closes the final count race without
            // replaying accepted native intent or guessing at another row.
            receipt.entered = true;
            result = entry(receipt.row + 4, receipt.before, receipt.target, receipt.observed, receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok && receipt.observed == receipt.before;
            if (result != ClockResult::Ok)
                return finish(result);
            if (!receipt.accepted)
                return finish(ClockResult::Busy);
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                (result = C::Recheck(image, model, context)) != ClockResult::Ok || (result = Capture(context, model, second)) != ClockResult::Ok)
                return finish(result);
            if (!Same(first, second, true, receipt.target) || !Pins(image))
                return finish(ClockResult::BindingChanged);
            return finish(owner());
        }
        catch (...)
        {
            // Snapshot storage failure cannot justify replay or undoing a
            // count already accepted by native memory.
            return finish(ClockResult::ReadFailure);
        }
    }
}
