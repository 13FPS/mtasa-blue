/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guard current typed IMGFile, selected resource and list inputs
 ****************************************************************************/
#include "NativeStreamingResourceContext.h"
#include "NativeStreamingResourceProfile.h"
#include "NativeImageAccess.h"
#include "NativeUObjectMembershipProfile.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
namespace mta::game_de::NativeStreamingResourceContext
{
    namespace ResourceContextDetail
    {
        namespace P = NativeStreamingResourceProfile;
        namespace L = NativeModelLoadedProfile;
        bool Readable(std::uintptr_t at, std::size_t bytes, bool writable = false) noexcept
        {
            if (!at || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - at)
                return false;
            const auto end = at + bytes;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !page.RegionSize)
                    return false;
                const auto access = page.Protect & 0xff;
                if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                    return false;
                if (writable && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                    return false;
                if (page.Type == MEM_IMAGE)
                {
                    if (page.AllocationBase != GetModuleHandleW(nullptr))
                        return false;
                }
                else if (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (base > at || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + page.RegionSize <= at)
                    return false;
                at = (std::min)(end, base + page.RegionSize);
            }
            return true;
        }
        bool Copy(std::uintptr_t at, void* output, std::size_t bytes) noexcept
        {
            if (!Readable(at, bytes))
                return false;
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), output, bytes, &copied) && copied == bytes;
        }
        bool Span(Value& value, std::uintptr_t at, std::size_t bytes, bool stable = true)
        {
            if (!bytes)
                return true;
            Value::Token token;
            token.address = at;
            token.stableAfterEntry = stable;
            token.bytes.resize(bytes);
            if (!Copy(at, token.bytes.data(), bytes))
                return false;
            value.tokens.push_back(std::move(token));
            return true;
        }
        template <class T>
        bool At(Value& value, std::uintptr_t base, std::size_t offset, T& output, bool stable = true)
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base || !Span(value, base + offset, sizeof(T), stable))
                return false;
            std::memcpy(&output, value.tokens.back().bytes.data(), sizeof(T));
            return true;
        }
        template <class T>
        T Field(const unsigned char* data, std::size_t offset) noexcept
        {
            T result{};
            std::memcpy(&result, data + offset, sizeof(result));
            return result;
        }
        bool Member(Value& value, std::uintptr_t image, std::uintptr_t object, std::uintptr_t vtable = 0)
        {
            NativeUObjectMembershipAccess::Snapshot next;
            if (object % alignof(std::uintptr_t) || !NativeUObjectMembershipAccess(image, &Copy).Capture(object, next) ||
                (vtable && next.vtableAddress != vtable))
                return false;
            value.memberships.push_back(next);
            return true;
        }
        struct Array
        {
            std::uintptr_t data = 0;
            std::int32_t   count = 0, capacity = 0;
        };
        static_assert(sizeof(Array) == 16);
        bool Header(Value& value, std::uintptr_t at, Array& array, std::int32_t budget, bool stable = true)
        {
            return At(value, at, 0, array, stable) && array.count >= 0 && array.capacity >= array.count && array.capacity <= budget &&
                   (!array.count || array.data);
        }
        bool Buffer(Value& value, std::uintptr_t at, bool stable = true)
        {
            Array array;
            if (!Header(value, at, array, P::BufferBudget, stable))
                return false;
            if (!array.count)
                return true;
            if (!Readable(array.data, static_cast<std::size_t>(array.count)))
                return false;
            value.buffers.push_back({array.data, static_cast<std::size_t>(array.count)});
            return true;
        }
        bool Wide(Value& value, std::uintptr_t at, std::string* ascii = nullptr)
        {
            Array array;
            if (!Header(value, at, array, P::StringBudget))
                return false;
            if (!array.count)
            {
                if (ascii)
                    ascii->clear();
                return true;
            }
            if (!Span(value, array.data, static_cast<std::size_t>(array.count) * sizeof(std::uint16_t)))
                return false;
            const auto& bytes = value.tokens.back().bytes;
            if (Field<std::uint16_t>(bytes.data(), bytes.size() - 2))
                return false;
            if (ascii)
            {
                ascii->clear();
                for (std::int32_t index = 0; index < array.count - 1; ++index)
                {
                    const auto character = Field<std::uint16_t>(bytes.data(), static_cast<std::size_t>(index) * 2);
                    if (!character)
                        break;
                    // This restriction applies only to the admission lookup
                    // while a native lazy map is empty; no guessed locale or
                    // replacement native filename conversion is introduced.
                    if (character > 0x7f)
                        return false;
                    ascii->push_back(static_cast<char>(character));
                }
            }
            return true;
        }
        bool CString(Value& value, std::uintptr_t at, std::size_t budget, std::string& text)
        {
            text.clear();
            for (std::size_t index = 0; index < budget; ++index)
            {
                unsigned char character = 0;
                if (!At(value, at, index, character))
                    return false;
                if (!character)
                    return true;
                text.push_back(static_cast<char>(character));
            }
            return false;
        }
        std::uint32_t Hash(const std::string& text) noexcept
        {
            std::uint32_t hash = 0;
            for (auto character : text)
            {
                auto byte = static_cast<unsigned char>(character);
                if (byte >= 'a' && byte <= 'z')
                    byte = static_cast<unsigned char>(byte - ('a' - 'A'));
                const auto index = static_cast<unsigned char>(hash ^ byte);
                hash = (hash >> 8) ^ Field<std::uint32_t>(P::FilenameHashTable.data(), static_cast<std::size_t>(index) * 4);
            }
            return hash;
        }
        bool Link(Value& value, std::uintptr_t image, std::int32_t index, std::array<std::int16_t, 2>& links)
        {
            return index >= 0 && index < static_cast<std::int32_t>(L::ModelCount) &&
                   Readable(image + L::ArrayRva + static_cast<std::uintptr_t>(index) * L::RowStride + 8, 4, true) &&
                   At(value, image + L::ArrayRva, static_cast<std::size_t>(index) * L::RowStride + 8, links, false) && links[0] >= -1 &&
                   links[0] < static_cast<std::int32_t>(L::ModelCount) && links[1] >= -1 && links[1] < static_cast<std::int32_t>(L::ModelCount);
        }
        bool Lists(Value& value, std::uintptr_t image, std::uint32_t model)
        {
            std::uintptr_t array = 0;
            if (!At(value, image, P::ArrayPointerRva, array) || array != image + L::ArrayRva)
                return false;
            const std::array<std::uintptr_t, 4> globals{P::LoadedHeadRva, P::LoadedTailRva, P::RequestHeadRva, P::RequestTailRva};
            for (std::size_t offset = 0; offset < globals.size(); offset += 2)
            {
                const auto                  head = static_cast<std::int32_t>(P::ResourceEnd + offset), tail = head + 1;
                std::uintptr_t              headPointer = 0, tailPointer = 0;
                std::array<std::int16_t, 2> first{}, last{}, neighbor{};
                if (!At(value, image, globals[offset], headPointer) || !At(value, image, globals[offset + 1], tailPointer) ||
                    headPointer != array + static_cast<std::uintptr_t>(head) * L::RowStride ||
                    tailPointer != array + static_cast<std::uintptr_t>(tail) * L::RowStride || !Link(value, image, head, first) ||
                    !Link(value, image, tail, last) || first[1] != -1 || last[0] != -1 || first[0] < 0 || last[1] < 0 ||
                    !Link(value, image, first[0], neighbor) || neighbor[1] != head || !Link(value, image, last[1], neighbor) || neighbor[0] != tail)
                    return false;
            }
            std::array<std::int16_t, 2> selected{}, neighbor{};
            if (!Link(value, image, static_cast<std::int32_t>(model), selected))
                return false;
            if (selected[0] != -1 || selected[1] != -1)
                if (selected[0] < 0 || selected[1] < 0 || !Link(value, image, selected[0], neighbor) || neighbor[1] != static_cast<std::int32_t>(model) ||
                    !Link(value, image, selected[1], neighbor) || neighbor[0] != static_cast<std::int32_t>(model))
                    return false;
            for (auto global : {P::RequestCountRva, P::PriorityCountRva})
            {
                std::int32_t count = 0;
                if (!Readable(image + global, sizeof(count), true) || !At(value, image, global, count, false) || count < 0 ||
                    count == (std::numeric_limits<std::int32_t>::max)())
                    return false;
            }
            return true;
        }
        bool Filename(Value& value, std::uintptr_t image, std::uint32_t model, std::string& text)
        {
            if (model < P::DatFirst)
            {
                const bool        collision = model < P::IplFirst;
                const auto        first = collision ? P::ColFirst : P::IplFirst, end = collision ? P::IplFirst : P::DatFirst;
                const std::size_t stride = collision ? 44 : 52;
                std::uintptr_t    pool = 0, objects = 0, flags = 0;
                std::int32_t      count = 0;
                unsigned char     generation = 0;
                if (!At(value, image, collision ? P::ColPoolRva : P::IplPoolRva, pool) || !At(value, pool, 0, objects) || !At(value, pool, 8, flags) ||
                    !At(value, pool, 0x10, count) || count <= 0 || count > static_cast<std::int32_t>(end - first) ||
                    model - first >= static_cast<std::uint32_t>(count) || !Readable(objects, static_cast<std::size_t>(count) * stride, true) ||
                    !Readable(flags, static_cast<std::size_t>(count), true) || !At(value, flags, model - first, generation) || (generation & 0x80) ||
                    !CString(value, objects + static_cast<std::uintptr_t>(model - first) * stride + 0x10, stride - 0x10, text))
                    return false;
                text += collision ? ".col" : ".ipl";
                return true;
            }
            if (model < P::RrrFirst)
            {
                if (!CString(value, image + P::IfpRowsRva + static_cast<std::uintptr_t>(model - P::IfpFirst) * 32, 16, text))
                    return false;
                text += ".ifp";
                return true;
            }
            if (model < P::ScmFirst)
            {
                std::int32_t         id = 0;
                std::array<char, 32> buffer{};
                if (!At(value, image + P::RrrRowsRva, static_cast<std::size_t>(model - P::RrrFirst) * 24, id))
                    return false;
                const auto length = std::snprintf(buffer.data(), buffer.size(), "carrec%03d.rrr", id);
                if (length < 0 || static_cast<std::size_t>(length) >= buffer.size())
                    return false;
                text.assign(buffer.data(), static_cast<std::size_t>(length));
                return true;
            }
            const auto   row = image + P::ScmRowsRva + static_cast<std::uintptr_t>(model - P::ScmFirst) * 40;
            std::int32_t size = 0;
            if (!At(value, row, 0x20, size) || size < 0 || size > P::BufferBudget || !CString(value, row + 0xc, 0x20 - 0xc, text))
                return false;
            text += ".scm";
            return true;
        }
        bool Cache(Value& value, std::uintptr_t object, std::uint32_t hash)
        {
            value.fileChecks.push_back({object, hash});
            Array rows;
            if (!Header(value, object + 0x30, rows, P::RowBudget) || (rows.count && !Readable(rows.data, static_cast<std::size_t>(rows.count) * 32, true)) ||
                !Span(value, object + 0x58, 0x50, false))
                return false;
            const auto header = value.tokens.back().bytes;
            const auto data = Field<std::uintptr_t>(header.data(), 0), bits = Field<std::uintptr_t>(header.data(), 0x20),
                       buckets = Field<std::uintptr_t>(header.data(), 0x40);
            const auto count = Field<std::int32_t>(header.data(), 8), capacity = Field<std::int32_t>(header.data(), 12),
                       bitCount = Field<std::int32_t>(header.data(), 0x28), bitCapacity = Field<std::int32_t>(header.data(), 0x2c),
                       firstFree = Field<std::int32_t>(header.data(), 0x30), freeCount = Field<std::int32_t>(header.data(), 0x34),
                       bucketCount = Field<std::int32_t>(header.data(), 0x48);
            if (count < 0 || count > P::RowBudget || capacity < count || capacity > P::RowBudget || freeCount < 0 || freeCount > count ||
                (freeCount && (firstFree < 0 || firstFree >= count)) || bitCount != count || bitCapacity < bitCount || bitCapacity > P::RowBudget + 127 ||
                (count && (!Readable(data, static_cast<std::size_t>(count) * 24, true) || (!bits && bitCount > 128) ||
                           !Readable(bits ? bits : object + 0x68, static_cast<std::size_t>((bitCount + 31) / 32) * 4, true))))
                return false;
            if (count == freeCount)
            {
                // Let the real IMGFile rebuild its own native map. Only the
                // source names it will scan and matching input buffers are
                // inspected; no hash entry is allocated or repaired here.
                for (std::int32_t index = 0; index < rows.count; ++index)
                {
                    const auto  row = rows.data + static_cast<std::uintptr_t>(index) * 32;
                    std::string name;
                    if (!Wide(value, row, &name) || (Hash(name) == hash && !Buffer(value, row + 0x10)))
                        return false;
                }
                return true;
            }
            if (bucketCount <= 0 || bucketCount > P::RowBudget * 2 || (bucketCount & (bucketCount - 1)) || (!buckets && bucketCount > 2) ||
                !Readable(buckets ? buckets : object + 0x90, static_cast<std::size_t>(bucketCount) * 4, true))
                return false;
            std::int32_t index = -1;
            if (!At(value, buckets ? buckets : object + 0x90, static_cast<std::size_t>((static_cast<std::uint32_t>(bucketCount) - 1) & hash) * 4, index, false))
                return false;
            for (std::int32_t remaining = count; index != -1; --remaining)
            {
                if (!remaining || index < 0 || index >= count)
                    return false;
                std::uint32_t allocation = 0;
                if (!At(value, bits ? bits : object + 0x68, static_cast<std::size_t>(index / 32) * 4, allocation, false) ||
                    !(allocation & (std::uint32_t{1} << (index & 31))) || !Span(value, data + static_cast<std::uintptr_t>(index) * 24, 24, false))
                    return false;
                const auto entry = value.tokens.back().bytes;
                if (Field<std::uint32_t>(entry.data(), 0) == hash)
                {
                    const auto row = Field<std::uintptr_t>(entry.data(), 8);
                    if (row < rows.data || row - rows.data >= static_cast<std::size_t>(rows.count) * 32 || (row - rows.data) % 32)
                        return false;
                    return Buffer(value, row + 0x10);
                }
                index = Field<std::int32_t>(entry.data(), 0x10);
            }
            return true;
        }
        bool Files(Value& value, std::uintptr_t image, std::uint32_t model)
        {
            std::string filename;
            if (!Filename(value, image, model, filename))
                return false;
            const auto     hash = Hash(filename);
            std::uintptr_t files = 0, imgClass = 0;
            if (!At(value, value.root, P::FileArrayOffset, files) || !At(value, value.root, P::FileCountOffset, value.imgFileCount) || value.imgFileCount < 0 ||
                value.imgFileCount > P::FileBudget ||
                (value.imgFileCount && (!files || !Readable(files, static_cast<std::size_t>(value.imgFileCount) * 8, true))) ||
                !At(value, image, P::ImgClassCacheRva, imgClass))
                return false;
            for (std::int32_t index = 0; index < value.imgFileCount; ++index)
            {
                std::uintptr_t object = 0;
                if (!At(value, files, static_cast<std::size_t>(index) * 8, object) || !Readable(object, P::ImgBytes, true) ||
                    !Member(value, image, object, image + P::ImgVtableRva) || !imgClass || value.memberships.back().classAddress != imgClass ||
                    !Cache(value, object, hash))
                    return false;
            }
            // The native fallback has the same pointer/length prefix, without
            // an array capacity. It is not silently treated as an absent file.
            std::uintptr_t buffer = 0;
            std::int32_t   bytes = 0;
            if (!At(value, image, P::FallbackRva, buffer) || !At(value, image, P::FallbackRva + 8, bytes) || bytes < 0 || bytes > P::BufferBudget)
                return false;
            if (bytes)
            {
                if (!Readable(buffer, static_cast<std::size_t>(bytes)))
                    return false;
                value.buffers.push_back({buffer, static_cast<std::size_t>(bytes)});
            }
            return true;
        }
        bool Dat(Value& value, std::uintptr_t image, std::uint32_t model)
        {
            Array rows;
            if (!At(value, value.root, P::DatProviderOffset, value.datProvider) || !Member(value, image, value.datProvider) ||
                !Header(value, value.datProvider + 0x58, rows, static_cast<std::int32_t>(P::IfpFirst - P::DatFirst)) ||
                model - P::DatFirst >= static_cast<std::uint32_t>(rows.count) || !Readable(rows.data, static_cast<std::size_t>(rows.count) * 40, true))
                return false;
            const auto record = rows.data + static_cast<std::uintptr_t>(model - P::DatFirst) * 40;
            // The weak-cache prefix is legitimately changed by native async
            // resolution. Request copies the FName and FString at+10 only.
            std::uint64_t name = 0;
            return At(value, record, 0x10, name) && name && Wide(value, record + 0x18);
        }
    }
    bool QualifyProfile(std::uintptr_t image) noexcept
    {
        namespace P = NativeStreamingResourceProfile;
        std::array<unsigned char, P::MaximumPinBytes> actual{};
        for (const auto& pin : P::Pins)
        {
            if (!pin.size || pin.size > actual.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
            if (!pin.code)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(image + pin.rva), &page, sizeof(page)) != sizeof(page) || page.Protect != PAGE_READONLY)
                    return false;
            }
        }
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            if (pin.size > actual.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        return true;
    }
    ClockResult CheckCurrentLists(std::uintptr_t image, std::uint32_t model) noexcept
    {
        try
        {
            Value observed;
            return ResourceContextDetail::Lists(observed, image, model) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult Capture(std::uintptr_t image, const NativeModelLoadedReader::Value& model, Value& output) noexcept
    {
        namespace D = ResourceContextDetail;
        namespace P = NativeStreamingResourceProfile;
        if (model.modelIndex < P::ResourceFirst || model.modelIndex >= P::ResourceEnd)
            return ClockResult::InvalidArgument;
        try
        {
            Value next;
            if (!D::At(next, image, P::RootRva, next.root) || !D::Readable(next.root, P::FileCountOffset + 8, true) ||
                !D::Member(next, image, next.root, image + NativeStreamingRequestProfile::RootVtableRva) || !D::Lists(next, image, model.modelIndex))
                return ClockResult::BindingChanged;
            // Preserve the native flag short circuit and pending/loaded paths.
            // Fresh TXD needs no metadata, whereas DAT owns a separate soft
            // asset path; the remaining first loads use genuine IMGFile data.
            if (!(model.rawFlags & 0x80) && model.rawLoadState == 0 && model.modelIndex >= P::ColFirst)
            {
                const bool dat = model.modelIndex >= P::DatFirst && model.modelIndex < P::IfpFirst;
                if (!(dat ? D::Dat(next, image, model.modelIndex) : D::Files(next, image, model.modelIndex)))
                    return ClockResult::BindingChanged;
            }
            output = std::move(next);
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult Recheck(std::uintptr_t image, const Value& previous, bool afterEntry) noexcept
    {
        namespace D = ResourceContextDetail;
        try
        {
            for (const auto& token : previous.tokens)
            {
                if (afterEntry && !token.stableAfterEntry)
                    continue;
                std::vector<unsigned char> actual(token.bytes.size());
                if (!D::Copy(token.address, actual.data(), actual.size()) || actual != token.bytes)
                    return ClockResult::BindingChanged;
            }
            for (const auto& buffer : previous.buffers)
                if (!D::Readable(buffer.address, buffer.bytes))
                    return ClockResult::BindingChanged;
            NativeUObjectMembershipAccess membership(image, &D::Copy);
            for (const auto& object : previous.memberships)
                if (!membership.Recheck(object))
                    return ClockResult::BindingChanged;
            if (afterEntry)
                for (const auto& file : previous.fileChecks)
                {
                    Value current;
                    if (!D::Cache(current, file.object, file.hash))
                        return ClockResult::BindingChanged;
                }
            return ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
}
