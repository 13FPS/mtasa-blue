/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded, exact payload transition plan for the genuine time helper
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationProgressProfile.h"
#include "NativeModelReferenceHeap.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <limits>
#include <xmmintrin.h>

namespace mta::game_de::AnimationAssociationProgressDetail
{
    using Node = std::array<unsigned char, 40>;
    using Row = std::array<unsigned char, 80>;
    template <class T, class A>
    T At(const A& a, std::size_t n) noexcept
    {
        T v;
        std::memcpy(&v, a.data() + n, sizeof(v));
        return v;
    }
    template <class T, class A>
    void Put(A& a, std::size_t n, T v) noexcept
    {
        std::memcpy(a.data() + n, &v, sizeof(v));
    }
    inline float Float(std::uint32_t bits) noexcept
    {
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    }
    inline std::uint32_t Bits(float v) noexcept
    {
        std::uint32_t b;
        std::memcpy(&b, &v, 4);
        return b;
    }
    inline bool Finite(float v) noexcept
    {
        return (Bits(v) & 0x7f800000u) != 0x7f800000u;
    }
    // FF0D10 returns without cache/decompression effects in exactly this
    // phase. Copy admission shares the predicate without importing the time
    // helper's arithmetic or weakening Build's earlier finite-value checks.
    template <class A>
    bool IsWarmNoEffect(const A& hierarchy) noexcept
    {
        const auto length = At<float>(hierarchy, 0x18);
        return hierarchy[0x13] != 0 ? Finite(length) && length != 0.0f : hierarchy[0x12] == 0 && At<std::uintptr_t>(hierarchy, 0x20) == 0;
    }
    // Explicit scalar SSE operations preserve the native helper's rounding and
    // ordering; the admission calculation must not become fused arithmetic.
    inline float Mul(float a, float b) noexcept
    {
        return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }
    inline float Add(float a, float b) noexcept
    {
        return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }
    inline float Sub(float a, float b) noexcept
    {
        return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }
    inline float Div(float a, float b) noexcept
    {
        return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }
    inline float Neg(float a) noexcept
    {
        return Float(Bits(a) ^ 0x80000000u);
    }
    struct Math
    {
        using Function = float (*)(float);
        Function acos = nullptr, sin = nullptr;
        bool     operator==(const Math& other) const noexcept { return acos == other.acos && sin == other.sin; }
    };
    inline bool Qualify(std::uintptr_t image, Math& output) noexcept
    {
        namespace Layout = NativeAnimationAssociationProgressProfile;
        for (const auto& pin : Layout::Code)
        {
            std::array<unsigned char, Layout::MaximumPinBytes> bytes{};
            if (!NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        for (const auto& pin : Layout::ReadOnlyData)
        {
            std::array<unsigned char, 16> bytes{};
            if (!NativeModelReferenceHeap::ReadOnlyImage(image, image + pin.rva, pin.size) ||
                !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        }
        // The original main image imports these UCRT functions for its entire
        // lifetime. Resolve only the already loaded module; never load a DLL or
        // infer a function ABI from an arbitrary executable pointer.
        const auto module = GetModuleHandleW(L"ucrtbase.dll");
        if (!module)
            return false;
        Math next;
        for (std::size_t i = 0; i < Layout::MathBindings.size(); ++i)
        {
            const auto&              binding = Layout::MathBindings[i];
            const auto               expected = reinterpret_cast<std::uintptr_t>(GetProcAddress(module, binding.symbol));
            std::uintptr_t           actual = 0;
            MEMORY_BASIC_INFORMATION page{};
            if (!expected || !NativeModelReferenceHeap::ReadOnlyImage(image, image + binding.iatRva, 8) ||
                !NativeImageAccess::ReadImage(image, image + binding.iatRva, &actual, 8) || actual != expected ||
                VirtualQuery(reinterpret_cast<void*>(actual), &page, sizeof(page)) != sizeof(page) || page.Type != MEM_IMAGE || page.State != MEM_COMMIT ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || ((page.Protect & 0xff) != PAGE_EXECUTE_READ && (page.Protect & 0xff) != PAGE_EXECUTE))
                return false;
            if (i == 0)
                next.acos = reinterpret_cast<Math::Function>(actual);
            else
                next.sin = reinterpret_cast<Math::Function>(actual);
        }
        output = next;
        return true;
    }
    struct Cache
    {
        std::array<std::uint32_t, 4> words{{0, 0, 0xffffffff, 0}};
        unsigned char                defined = 0x0c;
    };
    struct FrameJournal
    {
        std::uintptr_t               row = 0, address = 0;
        Row                          header{};
        std::array<unsigned char, 8> quaternion{};
    };
    struct RowState
    {
        std::uintptr_t address = 0;
        Row            bytes{};
    };
    struct FrameState
    {
        std::uintptr_t             address = 0;
        std::size_t                stride = 0;
        bool                       compressed = false;
        std::vector<unsigned char> before, after;
    };
    struct Plan
    {
        std::uintptr_t                 hierarchy = 0;
        std::array<unsigned char, 40>  hierarchyBytes{};
        std::array<unsigned char, 104> receiverBefore{};
        std::vector<Node>              beforeNodes, afterNodes;
        std::vector<unsigned char>     written;
        std::vector<RowState>          rows;
        std::vector<FrameState>        frames;
        std::vector<FrameJournal>      journal;
        std::uint32_t                  absoluteBits = 0, finalTimeBits = 0;
        std::size_t                    preparedFrames = 0, modifiedFrames = 0, work = 0, bytes = 0;
    };
    inline ClockResult Step(Plan& p) noexcept
    {
        return ++p.work > NativeAnimationAssociationProgressProfile::MaximumWalkSteps ? ClockResult::Busy : ClockResult::Ok;
    }
    inline bool CopyJournal(const FrameJournal& item) noexcept
    {
        Row                          row{};
        std::array<unsigned char, 8> quaternion{};
        return NativeModelReferenceMap::Copy(item.row, row.data(), row.size()) && row == item.header &&
               NativeModelReferenceMap::Copy(item.address, quaternion.data(), quaternion.size()) && quaternion == item.quaternion;
    }
    inline bool Verify(const Plan& p, bool after) noexcept
    {
        std::array<unsigned char, 40> hierarchy{};
        // H is an already-qualified native bank image row, while its sequence
        // payloads are private native heap data. Do not conflate those channels.
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(p.hierarchy), hierarchy.data(), hierarchy.size(), &copied) ||
            copied != hierarchy.size() || hierarchy != p.hierarchyBytes)
            return false;
        for (const auto& row : p.rows)
        {
            Row bytes{};
            if (!NativeModelReferenceMap::Copy(row.address, bytes.data(), bytes.size()) || bytes != row.bytes)
                return false;
        }
        for (const auto& frame : p.frames)
        {
            const auto& expected = after ? frame.after : frame.before;
            // Fixed-size blocks avoid any post-entry allocation or large stack.
            std::array<unsigned char, 256> bytes{};
            for (std::size_t n = 0; n < expected.size(); n += bytes.size())
            {
                const auto count = (std::min)(bytes.size(), expected.size() - n);
                if (!NativeModelReferenceMap::Copy(frame.address + n, bytes.data(), count) || std::memcmp(bytes.data(), expected.data() + n, count))
                    return false;
            }
        }
        return true;
    }
    inline ClockResult CacheQuaternion(Plan& p, Node& node, FrameState& frames, std::size_t start, std::size_t end, bool compressed, const Math& math,
                                       const RowState& row)
    {
        float a[4]{}, b[4]{};
        for (std::size_t i = 0; i < 4; ++i)
        {
            if (compressed)
            {
                a[i] = Mul(static_cast<float>(At<std::int16_t>(frames.after, start * frames.stride + i * 2)), Float(0x39800000));
                b[i] = Mul(static_cast<float>(At<std::int16_t>(frames.after, end * frames.stride + i * 2)), Float(0x39800000));
            }
            else
            {
                a[i] = At<float>(frames.after, start * frames.stride + i * 4);
                b[i] = At<float>(frames.after, end * frames.stride + i * 4);
                if (!Finite(a[i]) || !Finite(b[i]))
                    return ClockResult::InvalidArgument;
            }
        }
        if (compressed)
        {
            // FED420 first tests the raw signed-WORD dot, then packs a negated
            // second quaternion. Aliased nodes observe this planned prior write.
            float raw[4]{};
            for (std::size_t i = 0; i < 4; ++i)
                raw[i] = Mul(static_cast<float>(At<std::int16_t>(frames.after, start * frames.stride + i * 2)),
                             static_cast<float>(At<std::int16_t>(frames.after, end * frames.stride + i * 2)));
            const auto dot = Mul(Add(Add(raw[3], raw[2]), Add(raw[1], raw[0])), Float(0x33800000));
            if (dot < 0)
            {
                for (std::size_t i = 0; i < 4; ++i)
                {
                    b[i] = Neg(b[i]);
                    const auto value = _mm_cvttss_si32(_mm_set_ss(Mul(b[i], Float(0x45800000))));
                    Put(frames.after, end * frames.stride + i * 2, static_cast<std::uint16_t>(value));
                }
                const auto address = frames.address + end * frames.stride;
                auto       found = std::find_if(p.journal.begin(), p.journal.end(), [&](const FrameJournal& j) { return j.address == address; });
                if (found == p.journal.end())
                {
                    p.journal.push_back({row.address, address, row.bytes, {}});
                    found = p.journal.end() - 1;
                }
                std::copy_n(frames.after.data() + end * frames.stride, 8, found->quaternion.data());
            }
        }
        const auto first = Add(Mul(a[3], b[3]), Mul(a[2], b[2]));
        const auto second = compressed ? Add(Mul(a[0], b[0]), Mul(a[1], b[1])) : Add(Mul(a[1], b[1]), Mul(a[0], b[0]));
        const auto dot = Add(first, second);
        if (!Finite(dot))
            return ClockResult::InvalidArgument;
        if (Float(Bits(dot) & 0x7fffffff) >= 1.0f)
        {
            Put(node, 0, std::uint64_t{0});
            return ClockResult::Ok;
        }
        const auto angle = math.acos((std::max)(-1.0f, (std::min)(1.0f, dot)));
        Put(node, 0, Bits(angle));
        Put(node, 4, Bits(angle == 0.0f ? 0.0f : Div(1.0f, math.sin(angle))));
        return ClockResult::Ok;
    }
    // All allocations and numerical work occur before FEBD90's first38 store.
    // Limits are explicit admission budgets, not native allocation capacities.
    inline ClockResult Build(std::uintptr_t image, std::uintptr_t hierarchy, std::uintptr_t sequences, std::uint64_t capacity, std::uintptr_t receiver,
                             std::uintptr_t nodeRaw, std::size_t nodeBytes, std::uint16_t count, std::uint32_t progress,
                             const std::vector<FrameJournal>& journal, const Math& math, Plan& p)
    {
        namespace Layout = NativeAnimationAssociationProgressProfile;
        namespace Map = NativeModelReferenceMap;
        p.hierarchy = hierarchy;
        p.journal = journal;
        if (!NativeImageAccess::ReadImage(image, hierarchy, p.hierarchyBytes.data(), p.hierarchyBytes.size(), true))
            return ClockResult::ReadFailure;
        const auto length = At<float>(p.hierarchyBytes, 0x18), value = Float(progress);
        if (!Finite(length) || !Finite(value))
            return ClockResult::InvalidArgument;
        const bool compressed = p.hierarchyBytes[0x13] != 0;
        if (!IsWarmNoEffect(p.hierarchyBytes))
            return ClockResult::OwnerConflict;
        const auto absolute = Mul(length, value);
        if (!Finite(absolute))
            return ClockResult::InvalidArgument;
        p.absoluteBits = Bits(absolute);
        std::array<unsigned char, 104> receiverBytes{};
        if (!Map::Copy(receiver, receiverBytes.data(), receiverBytes.size()))
            return ClockResult::ReadFailure;
        p.receiverBefore = receiverBytes;
        const bool looping = (At<std::uint16_t>(receiverBytes, 0x46) & 2) != 0;
        auto       time = absolute;
        if (time >= length)
        {
            if (!looping)
                time = length;
            else
            {
                if (!(length > 0.0f))
                    return ClockResult::InvalidArgument;
                while (time >= length)
                {
                    if (Step(p) != ClockResult::Ok)
                        return ClockResult::Busy;
                    const auto next = Sub(time, length);
                    if (!Finite(next) || next == time)
                        return ClockResult::Busy;
                    time = next;
                }
            }
        }
        p.finalTimeBits = Bits(time);
        p.beforeNodes.resize(count);
        p.afterNodes.resize(count);
        p.written.assign(count, 0);
        const auto nodes = At<std::uintptr_t>(receiverBytes, 0x20);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (Step(p) != ClockResult::Ok)
                return ClockResult::Busy;
            if (!Map::Copy(nodes + i * 40, p.beforeNodes[i].data(), 40))
                return ClockResult::ReadFailure;
            p.afterNodes[i] = p.beforeNodes[i];
            auto&      target = p.afterNodes[i];
            const auto sequence = At<std::uintptr_t>(target, 0x10);
            if (!sequence)
                continue;
            if (!capacity || sequence < sequences || (sequence - sequences) % 80 || (sequence - sequences) / 80 >= capacity)
                return ClockResult::BindingChanged;
            auto rowIt = std::find_if(p.rows.begin(), p.rows.end(), [&](const RowState& r) { return r.address == sequence; });
            if (rowIt == p.rows.end())
            {
                RowState row;
                row.address = sequence;
                if (!Map::Copy(sequence, row.bytes.data(), row.bytes.size()))
                    return ClockResult::ReadFailure;
                p.rows.push_back(row);
                rowIt = p.rows.end() - 1;
            }
            const auto row = *rowIt;
            const auto n = At<std::int16_t>(row.bytes, 0x44);
            if (n <= 0)
                continue;
            const auto flags = At<std::uint16_t>(row.bytes, 0x42);
            if (((flags & 4) != 0) != compressed)
                return ClockResult::OwnerConflict;
            const std::size_t stride = compressed ? ((flags & 2) ? 16 : 10) : ((flags & 2) ? 32 : 20);
            const std::size_t needed = compressed ? (std::min)(std::size_t{2}, static_cast<std::size_t>(n)) : static_cast<std::size_t>(n);
            const auto        bytes = needed * stride;
            const auto        frames = At<std::uintptr_t>(row.bytes, 0x48);
            if (!frames || frames % 2 || !Map::PrivateWritable(frames, bytes) || !Map::Disjoint(frames, bytes, receiver, 104) ||
                !Map::Disjoint(frames, bytes, nodeRaw, nodeBytes) || !Map::Disjoint(frames, bytes, sequences - 8, 8 + static_cast<std::size_t>(capacity) * 80))
                return ClockResult::BindingChanged;
            auto frameIt = std::find_if(p.frames.begin(), p.frames.end(), [&](const FrameState& f) { return f.address == frames; });
            if (frameIt == p.frames.end())
            {
                for (const auto& f : p.frames)
                    if (!Map::Disjoint(frames, bytes, f.address, f.before.size()))
                        return ClockResult::OwnerConflict;
                if (bytes > Layout::MaximumPreparedBytes - p.bytes)
                    return ClockResult::Busy;
                FrameState frame;
                frame.address = frames;
                frame.stride = stride;
                frame.compressed = compressed;
                frame.before.resize(bytes);
                if (!Map::Copy(frames, frame.before.data(), bytes))
                    return ClockResult::ReadFailure;
                frame.after = frame.before;
                p.frames.push_back(std::move(frame));
                frameIt = p.frames.end() - 1;
                p.bytes += bytes;
                p.preparedFrames += needed;
            }
            else if (frameIt->stride != stride || frameIt->compressed != compressed || frameIt->before.size() != bytes)
                return ClockResult::OwnerConflict;
            auto&       payload = *frameIt;
            std::size_t start = 0, end = n == 1 ? 0 : 1;
            float       delta = 0;
            if (compressed)
            {
                if (n > 1)
                    delta = Mul(static_cast<float>(At<std::int16_t>(payload.after, stride + 8)), Float(0x3c888889));
            }
            else if (n > 1)
            {
                auto left = time;
                for (;;)
                {
                    if (Step(p) != ClockResult::Ok)
                        return ClockResult::Busy;
                    const auto duration = At<float>(payload.after, end * stride + 0x10);
                    if (!Finite(duration))
                        return ClockResult::InvalidArgument;
                    if (left <= duration)
                    {
                        delta = Sub(duration, left);
                        break;
                    }
                    const auto next = Sub(left, duration);
                    if (!Finite(next))
                        return ClockResult::Busy;
                    if (end + 1 < static_cast<std::size_t>(n))
                    {
                        start = end;
                        ++end;
                    }
                    else if (!looping)
                    {
                        delta = 0;
                        break;
                    }
                    else
                    {
                        if (start == 0 && end == 1 && next == left)
                            return ClockResult::Busy;
                        start = 0;
                        end = 1;
                    }
                    left = next;
                }
            }
            Put(target, 8, static_cast<std::uint16_t>(end));
            Put(target, 0xa, static_cast<std::uint16_t>(start));
            Put(target, 0xc, Bits(delta));
            p.written[i] = 0x0c;
            if (flags & 1)
            {
                const auto result = CacheQuaternion(p, target, payload, start, end, compressed, math, row);
                if (result != ClockResult::Ok)
                    return result;
                p.written[i] |= 3;
            }
        }
        for (const auto& frame : p.frames)
            for (std::size_t offset = 0; offset < frame.before.size(); offset += frame.stride)
                p.modifiedFrames += std::memcmp(frame.before.data() + offset, frame.after.data() + offset, 8) != 0;
        return Verify(p, false) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
