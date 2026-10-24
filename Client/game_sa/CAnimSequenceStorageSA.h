/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep custom sequence storage paired with its actual backend layout
 ****************************************************************************/
#pragma once
#include "CAnimBlendSequenceSA.h"
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
namespace MtaGame::CAnimSequenceStorageSA
{
    using Sequence = CAnimBlendSequenceSAInterface;
    inline constexpr std::size_t CookieBytes = sizeof(std::size_t);
    static_assert(CookieBytes % alignof(Sequence) == 0);
    static_assert(std::is_trivially_destructible_v<Sequence>);
    inline void Initialize(Sequence& sequence) noexcept
    {
        // Match the existing SDK policy exactly. In particular, the upper
        // hash-union bytes are not invented initialization or a native name.
        sequence.m_boneId = static_cast<std::uint16_t>(-1);
        sequence.sFlags = 0;
        sequence.sNumKeyFrames = 0;
        sequence.pKeyFrames = nullptr;
    }
    inline Sequence* Allocate(std::size_t capacity)
    {
        if (!capacity)
            return nullptr;
        if (capacity > ((std::numeric_limits<std::size_t>::max)() - CookieBytes) / sizeof(Sequence))
            throw std::bad_array_new_length();
        // Classic runtime is x86: this is its DWORD cookie and 12-byte row.
        // The x64 source layout is not a claim that classic GTA runs as x64.
        // A trivial C++ array need not get a compiler cookie, so own the
        // backend prefix explicitly and pair it with the same allocator.
        auto* allocation = static_cast<unsigned char*>(::operator new(CookieBytes + capacity * sizeof(Sequence)));
        std::memcpy(allocation, &capacity, CookieBytes);
        auto* sequences = ::new (static_cast<void*>(allocation + CookieBytes)) Sequence[capacity];
        // Cleanup may inspect unused capacity after a partial parse. Every
        // row must therefore start without flags, frames or a frame pointer.
        for (std::size_t index = 0; index != capacity; ++index)
            Initialize(sequences[index]);
        return sequences;
    }
    inline void Free(Sequence* sequences) noexcept
    {
        // Array storage and keyframes have independent explicit owners. Do
        // not run an engine destructor or free any frame pointer here.
        if (sequences)
            ::operator delete(reinterpret_cast<unsigned char*>(sequences) - CookieBytes);
    }
}
